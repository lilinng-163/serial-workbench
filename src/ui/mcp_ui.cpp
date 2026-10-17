// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "agent_config.h"
#include "serial_control_panel.h"
#include "serial_monitor_dialog.h"
#include <algorithm>

namespace sw
{
namespace
{
struct SearchView
{
    QString id;
    QString kind;
    QString title;
    FilterBox *filter;
    LogView *log = nullptr;
    Instrument *instrument = nullptr;
    Pane *pane = nullptr;
};

QList<SearchView> searchViews(Window *window, Channel *channel)
{
    QList<SearchView> result;
    for (auto host : window->hosts)
    {
        if (!host) { continue; }
        for (auto pane : host->panes)
        {
            if (pane->channel != channel) { continue; }
            if (pane->filter && !pane->terminalMode)
            {
                result.append({pane->searchViewId, "log", pane->pageTitle,
                    pane->filter, pane->log, nullptr, pane});
            }
            if (pane->instrument && pane->instrument->filter)
            {
                result.append({pane->searchViewId, pane->instrument->kind, pane->pageTitle,
                    pane->instrument->filter, nullptr, pane->instrument, pane});
            }
            if (pane->instrument && pane->instrument->controls)
            {
                auto monitor = pane->instrument->controls->receiveMonitor();
                if (monitor && monitor->channelId() == channel->id)
                {
                    result.append({monitor->searchViewId, "serial-monitor", "串口收发小窗",
                        monitor->searchBox(), monitor->logView(), nullptr, pane});
                }
            }
        }
    }
    return result;
}

QJsonArray visibleRows(const SearchView &view)
{
    QJsonArray rows;
    if (view.log)
    {
        for (auto block = view.log->document()->begin(); block.isValid(); block = block.next())
        {
            if (block.isVisible() && !block.text().isEmpty())
            {
                rows.append(QJsonObject {{"text", block.text().left(32768)}, {"line", block.blockNumber()},
                    {"truncated", block.text().size() > 32768}});
            }
        }
    }
    else
    {
        auto table = view.instrument->table;
        for (int at = 0; at < table->topLevelItemCount(); ++at)
        {
            auto item = table->topLevelItem(at);
            if (item->isHidden()) { continue; }
            const auto record = item->data(0, Qt::UserRole).toJsonObject();
            const auto text = record["text"].toString();
            rows.append(QJsonObject {{"text", text.left(32768)}, {"time", record["time"]},
                {"format", record["format"]}, {"truncated", text.size() > 32768}});
        }
    }
    return rows;
}
}

void Window::installMcpUi()
{
    const QPointer<Window> guard(this);
    mcp->searchHandler = [guard](Channel *channel, const QJsonObject &a)
    {
        if (!guard) { fail("GUI 窗口已关闭"); }
        const auto views = searchViews(guard, channel);
        if (a["action"] == "list_search_views")
        {
            QJsonArray list;
            for (const auto &view : views)
            {
                list.append(QJsonObject {{"view_id", view.id}, {"kind", view.kind}, {"title", view.title},
                    {"filter", view.filter->settings()}, {"error", view.filter->criteria().error}});
            }
            return QJsonObject {{"channel_id", channel->id}, {"views", list}, {"scope", "gui_display"}};
        }
        const auto id = a["view_id"].toString();
        const auto found = std::find_if(views.begin(), views.end(), [&](const auto &view) { return view.id == id; });
        if (id.isEmpty() || found == views.end())
        {
            fail("view_id 不属于该连接或页面已关闭；先 list_search_views。交互终端请使用 read_events。");
        }
        const auto &view = *found;
        view.pane->tick(true);
        const int offset = a["offset"].toInt();
        const int limit = a["limit"].toInt(200);
        if (offset < 0 || offset > 4000 || limit < 1 || limit > 200)
        {
            fail("offset 范围 0..4000，limit 范围 1..200");
        }
        if (a.contains("filter") && a.contains("snapshot"))
        {
            fail("更新 filter 时不要传旧 snapshot；先更新搜索，再使用返回的 snapshot 分页");
        }
        if (a.contains("filter"))
        {
            if (!a["filter"].isObject()) { fail("filter 必须是对象"); }
            auto next = view.filter->settings();
            const auto patch = a["filter"].toObject();
            for (auto item = patch.begin(); item != patch.end(); ++item)
            {
                if (!next.contains(item.key())) { fail("未知的搜索参数：" + item.key()); }
                if ((item.key() == "text" && !item->isString())
                    || (item.key() != "text" && !item->isBool()))
                {
                    fail("搜索参数类型错误：" + item.key());
                }
                next[item.key()] = item.value();
            }
            if (next["text"].toString().size() > 2048) { fail("搜索内容最多 2048 字符"); }
            SearchFilter check;
            check.restore(next);
            if (!check.error.isEmpty()) { fail(check.error); }
            view.filter->restore(next);
            guard->saveSettings();
        }
        const auto rows = visibleRows(view);
        const auto snapshotData = QJsonDocument(QJsonObject {{"filter", view.filter->settings()}, {"rows", rows}}).toJson(QJsonDocument::Compact);
        const auto snapshot = QString::fromLatin1(QCryptographicHash::hash(snapshotData, QCryptographicHash::Sha256).toHex());
        if (a.contains("snapshot") && a["snapshot"].toString() != snapshot)
        {
            fail("显示内容已变化，请从 offset=0 重新搜索");
        }
        QJsonArray page;
        int bytes = 0;
        for (int at = offset; at < rows.size() && page.size() < limit; ++at)
        {
            const auto row = rows[at];
            const int size = QJsonDocument(row.toObject()).toJson(QJsonDocument::Compact).size();
            if (!page.isEmpty() && bytes + size > 512 * 1024) { break; }
            page.append(row);
            bytes += size;
        }
        return QJsonObject {{"channel_id", channel->id}, {"view_id", view.id}, {"scope", "gui_display"},
            {"filter", view.filter->settings()}, {"error", view.filter->criteria().error}, {"rows", page},
            {"snapshot", snapshot}, {"total_matches", rows.size()}, {"next_offset", offset + page.size()},
            {"has_more", offset + page.size() < rows.size()}};
    };

    mcp->scriptHandler = [guard](Channel *channel, const QString &code, bool enabled)
    {
        if (!guard) { fail("GUI 窗口已关闭"); }
        auto owner = guard->findPane(channel);
        if (!owner) { fail("没有找到该串口的收发页面"); }
        for (auto host : guard->hosts)
        {
            if (!host) { continue; }
            for (auto pane : host->panes)
            {
                if (pane->channel != channel || !pane->scriptDialog) { continue; }
                auto editor = pane->scriptDialog->findChild<QPlainTextEdit *>("scriptCode");
                auto toggle = pane->scriptDialog->findChild<QCheckBox *>("scriptEnabled");
                if ((editor && editor->toPlainText() != channel->scriptCode)
                    || (toggle && toggle->isChecked() != channel->scriptEnabled))
                {
                    fail("GUI 脚本编辑器有未保存内容，请先保存或关闭编辑器");
                }
                auto test = pane->scriptDialog->findChild<QPushButton *>("testScript");
                if (test && !test->isEnabled()) { fail("GUI 正在试解析，请等待完成后再应用脚本"); }
            }
        }
        owner->applyScript(code, enabled);
        for (auto host : guard->hosts)
        {
            if (!host) { continue; }
            for (auto pane : host->panes)
            {
                if (pane->channel != channel || pane->instrument) { continue; }
                pane->scriptCode = code;
                pane->scriptRequested = enabled;
                pane->refreshScriptResults();
                if (pane->scriptDialog)
                {
                    auto editor = pane->scriptDialog->findChild<QPlainTextEdit *>("scriptCode");
                    if (editor) { editor->setPlainText(code); }
                    auto toggle = pane->scriptDialog->findChild<QCheckBox *>("scriptEnabled");
                    if (toggle) { toggle->setChecked(enabled); }
                    auto results = pane->scriptDialog->findChild<QTreeWidget *>("scriptTestResults");
                    if (results) { results->clear(); }
                    auto status = pane->scriptDialog->findChild<QLabel *>("scriptEditorStatus");
                    if (status) { status->setText(enabled ? "AI 已同步并启用脚本 · 从新收到的数据开始解析" : "AI 已同步并停用脚本"); }
                }
            }
        }
    };
    connect(mcp, &McpServer::portChanged, this, [this] { checkAgentEndpoints(); });
}

void Window::checkAgentEndpoints()
{
    if (primary) { return; }
    QJsonArray checks;
    const auto bridge = QCoreApplication::applicationDirPath() + "/SerialWorkbenchMCP.exe";
    for (const auto &agent : AgentConfig::discover(agentEnvironment()))
    {
        if (!agent.installed()) { continue; }
        QString status;
        try { status = AgentConfig::check(agent, mcp->url(), bridge); }
        catch (const std::exception &)
        {
            // Parser diagnostics can contain user configuration fragments.
            status = "配置无法读取或解析，请在 Agent 接入页检查";
        }
        checks.append(QJsonObject {{"agent", agent.id}, {"name", agent.name}, {"status", status},
            {"matches", status == "配置地址一致"}, {"config_path", agent.configPath}, {"expected_url", mcp->url()}});
    }
    mcp->agentChecks = checks;
    updateStatus();
}
}
