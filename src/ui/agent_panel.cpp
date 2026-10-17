// SPDX-License-Identifier: Apache-2.0
#include "agent_panel.h"
#include "agent_skill.h"
#include "controls.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace sw
{
namespace
{
class AgentLogo : public QLabel
{
public:
    explicit AgentLogo(const QString &asset) : asset(asset)
    {
        setObjectName("agentLogo");
        setFixedSize(34, 34);
        setAlignment(Qt::AlignCenter);
        refreshImage();
    }

protected:
    void changeEvent(QEvent *event) override
    {
        QLabel::changeEvent(event);
        if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        {
            refreshImage();
        }
    }

private:
    QString asset;

    void refreshImage()
    {
        const bool dark = palette().color(QPalette::WindowText).lightness() > 128;
        QString file = asset;
        if (dark && (asset == "opencode.svg" || asset == "hermes.svg"))
        {
            file.replace(".svg", "-dark.svg");
        }
        auto image = QIcon(":/agents/" + file).pixmap(QSize(30, 30), devicePixelRatioF());
        if (asset == "codex.svg" || asset == "grok.svg")
        {
            QPainter painter(&image);
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(image.rect(), dark ? Qt::white : Qt::black);
        }
        setPixmap(image);
    }
};
}

AgentPanel::AgentPanel(McpServer *owner, QWidget *parent, std::function<AgentEnvironment()> provider)
    : QWidget(parent), environment(std::move(provider)), server(owner)
{
    auto body = new QVBoxLayout(this);
    body->setContentsMargins(12, 12, 12, 12);
    body->setSpacing(12);
    auto toolbar = new QHBoxLayout();
    auto scan = new QPushButton("重新检测");
    scan->setObjectName("agentScan");
    updateAll = new QPushButton("一键更新已安装 Agent");
    updateAll->setObjectName("updateAllAgents");
    toolbar->addWidget(scan);
    toolbar->addWidget(updateAll);
    auto skills = new QPushButton("配置 skill");
    skills->setObjectName("configureAgentSkills");
    toolbar->addWidget(skills);
    connect(skills, &QPushButton::clicked, this, &AgentPanel::configureSkills);
    toolbar->addStretch();
    body->addLayout(toolbar);
    summary = new QLabel();
    summary->setObjectName("muted");
    summary->setWordWrap(true);
    summary->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    body->addWidget(summary);
    summary->hide();
    auto grid = new QGridLayout();
    grid->setObjectName("agentGrid");
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    agents = AgentConfig::discover(environment());
    for (int index = 0; index < agents.size(); ++index)
    {
        const auto &agent = agents[index];
        auto frame = new QFrame();
        frame->setObjectName("agentCard");
        frame->setProperty("agentId", agent.id);
        frame->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        auto layout = new QVBoxLayout(frame);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(8);
        auto heading = new QHBoxLayout();
        auto logo = new AgentLogo(agent.logo);
        heading->addWidget(logo);
        auto title = new QLabel(agent.name);
        title->setObjectName("agentTitle");
        title->setWordWrap(true);
        title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        heading->addWidget(title, 1);
        layout->addLayout(heading);
        auto state = new QLabel();
        state->setObjectName("muted");
        state->setWordWrap(true);
        layout->addWidget(state);
        auto row = new QHBoxLayout();
        auto configure = new QPushButton("配置");
        auto check = new QPushButton("检查");
        configure->setObjectName("configure_" + agent.id);
        check->setObjectName("check_" + agent.id);
        configure->setAutoDefault(false);
        check->setAutoDefault(false);
        row->addWidget(configure);
        row->addWidget(check);
        layout->addLayout(row);
        auto result = new QLabel();
        result->setTextFormat(Qt::PlainText);
        result->setWordWrap(true);
        result->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        result->setVisible(false);
        layout->addWidget(result);
        cards.append({state, result, configure, check});
        grid->addWidget(frame, index / 2, index % 2);
        connect(configure, &QPushButton::clicked, this, [this, index] { checkAgent(index, true); });
        connect(check, &QPushButton::clicked, this, [this, index] { checkAgent(index, false); });
    }
    body->addLayout(grid);
    auto hint = new QLabel("配置前备份原文件，保留其他 MCP 服务。更新后请在 Agent 中重载；Pi 使用 /reload。检测范围：Windows 程序与 CLI。");
    hint->setWordWrap(true);
    hint->setObjectName("muted");
    body->addWidget(hint);
    body->addStretch();
    connect(scan, &QPushButton::clicked, this, &AgentPanel::refresh);
    connect(updateAll, &QPushButton::clicked, this, &AgentPanel::configureAll);
    connect(server, &McpServer::portChanged, this, [this] { refresh(); });
    refresh();
}

void AgentPanel::configureSkills()
{
    auto dialog = new QDialog(this);
    dialog->setObjectName("agentSkillDialog");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("配置 Agent skill");
    dialog->resize(760, 570);
    auto body = new QVBoxLayout(dialog);
    auto hint = new QLabel("为 Agent 安装或更新软件、MCP 和脚本使用指南。已检测到的 Agent 默认勾选；不同内容先备份，更新后请重载 Agent。");
    hint->setWordWrap(true);
    hint->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    body->addWidget(hint);
    const auto e = environment();
    struct Choice { QString id; QCheckBox *toggle; QLabel *status; };
    QList<Choice> choices;
    for (const auto &value : AgentSkill::targets(e))
    {
        const auto target = value.toObject();
        const auto id = target["agent"].toString();
        auto row = new QHBoxLayout();
        auto toggle = new QCheckBox(target["name"].toString());
        toggle->setObjectName("skill_" + id);
        toggle->setChecked(target["installed"].toBool());
        row->addWidget(toggle);
        auto state = new QLabel((target["installed"].toBool() ? "已检测到 · " : "未检测到 · ") + target["skill_state"].toString());
        state->setObjectName("skillState_" + id);
        row->addWidget(state);
        row->addStretch();
        body->addLayout(row);
        auto path = new QLabel(QDir::toNativeSeparators(target["path"].toString()));
        path->setTextFormat(Qt::PlainText);
        path->setTextInteractionFlags(Qt::TextSelectableByMouse);
        path->setWordWrap(true);
        path->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        body->addWidget(path);
        choices.append({id, toggle, state});
    }
    auto note = new QLabel("这里只更新 skill 文件。MCP 地址仍在 Agent 接入页配置。Claude Desktop 的自定义 skill 需在客户端导入，也可通过 MCP 的 read_usage_guide 读取本指南。");
    note->setWordWrap(true);
    note->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    body->addWidget(note);
    auto actions = new QHBoxLayout();
    auto install = new QPushButton("安装 / 更新所选");
    install->setObjectName("installSelectedSkills");
    auto close = new QPushButton("关闭");
    actions->addStretch();
    actions->addWidget(install);
    actions->addWidget(close);
    body->addLayout(actions);
    const auto update = [choices, install]
    {
        bool selected = false;
        for (const auto &choice : choices) { selected = selected || choice.toggle->isChecked(); }
        install->setEnabled(selected);
    };
    for (const auto &choice : choices) { connect(choice.toggle, &QCheckBox::toggled, dialog, update); }
    update();
    connect(install, &QPushButton::clicked, dialog, [choices, e]
    {
        for (const auto &choice : choices)
        {
            if (!choice.toggle->isChecked()) { continue; }
            try
            {
                const auto result = AgentSkill::install(choice.id, e);
                choice.status->setText(QString("已更新 · %1 个备份 · 请重载 Agent").arg(result["backups"].toArray().size()));
            }
            catch (const std::exception &error)
            {
                choice.status->setText("未完成：" + QString::fromUtf8(error.what()));
            }
        }
    });
    connect(close, &QPushButton::clicked, dialog, &QDialog::accept);
    makeDialogScrollable(dialog, QSize(560, 320));
    dialog->show();
}

void AgentPanel::refresh()
{
    if (busy) { return; }
    agents = AgentConfig::discover(environment());
    int installed = 0;
    for (int index = 0; index < agents.size(); ++index)
    {
        const auto &agent = agents[index];
        auto &card = cards[index];
        installed += agent.installed();
        card.state->setText(agent.installed() ? "已安装" : "未检测到安装");
        card.state->setToolTip(agent.executable);
        card.configure->setEnabled(agent.installed() && server->isListening());
        card.check->setEnabled(agent.installed() && server->isListening());
        card.configure->setToolTip(agent.installed() ? "保存并检查：" + agent.configPath : "安装后重新检测");
        card.check->setToolTip(agent.installed() ? "只读检查配置与 MCP 服务" : "安装后才可检查配置");
        card.result->setVisible(agent.installed());
        if (agent.installed())
        {
            try
            {
                card.result->setText(AgentConfig::check(agent, server->url(),
                    QCoreApplication::applicationDirPath() + "/SerialWorkbenchMCP.exe"));
            }
            catch (const std::exception &error)
            {
                card.result->setText("检查未完成：" + QString::fromUtf8(error.what()));
            }
        }
    }
    updateAll->setEnabled(installed > 0 && server->isListening());
    findChild<QPushButton *>("agentScan")->setToolTip(QString("检测到 %1 / %2 个 Agent").arg(installed).arg(agents.size()));
    emit configurationUpdated();
}

void AgentPanel::prepareProbe(const std::function<void(const QString &)> &finished)
{
    const auto &openCode = agents[2];
    if (!openCode.installed() || !openCode.executable.endsWith(".exe", Qt::CaseInsensitive))
    {
        probe(finished);
        return;
    }
    auto process = new QProcess(this);
    auto timeout = new QTimer(process);
    timeout->setSingleShot(true);
    timeout->setInterval(2000);
    connect(timeout, &QTimer::timeout, process, &QProcess::kill);
    const auto done = std::make_shared<bool>(false);
    const auto complete = [this, process, timeout, finished, done]
    {
        if (*done) { return; }
        *done = true;
        timeout->stop();
        const auto version = QString::fromUtf8(process->readAllStandardOutput().left(1024));
        const auto match = QRegularExpression(R"((?:^|\s|v)(\d+)\.\d+\.\d+)").match(version);
        if (match.hasMatch()) { agents[2].modernOpenCode = match.captured(1).toInt() >= 2; }
        process->deleteLater();
        probe(finished);
    };
    connect(process, &QProcess::finished, this, [complete](int, QProcess::ExitStatus) { complete(); });
    connect(process, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError) { complete(); });
    process->start(openCode.executable, {"--version"});
    timeout->start();
}

void AgentPanel::probe(const std::function<void(const QString &)> &finished)
{
    if (!server->isListening()) { finished("MCP 服务未启动"); return; }
    auto manager = new QNetworkAccessManager(this);
    const auto endpoint = server->url();
    QNetworkRequest request {QUrl(endpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(3000);
    const auto rpc = QJsonObject {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", QJsonObject {{"protocolVersion", "2025-03-26"},
            {"clientInfo", QJsonObject {{"name", "Workbench 配置检查"}, {"version", Version}}}, {"capabilities", QJsonObject {}}}}};
    auto reply = manager->post(request, QJsonDocument(rpc).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, manager, reply, finished, endpoint]
    {
        const auto document = QJsonDocument::fromJson(reply->readAll()).object();
        const auto session = reply->rawHeader("Mcp-Session-Id");
        auto failure = reply->error() == QNetworkReply::NoError &&
            endpoint == server->url() && document["result"].toObject()["serverInfo"].toObject()["name"] == "Serial Workbench Native"
            ? QString() : "MCP 服务检查失败：" + reply->errorString();
        if (endpoint != server->url()) { failure = "MCP 端口已改变，请重新检查"; }
        if (!session.isEmpty())
        {
            QNetworkRequest cleanup {QUrl(endpoint)};
            cleanup.setTransferTimeout(3000);
            cleanup.setRawHeader("Mcp-Session-Id", session);
            auto cleanupReply = manager->deleteResource(cleanup);
            connect(cleanupReply, &QNetworkReply::finished, manager, &QObject::deleteLater);
        }
        else { manager->deleteLater(); }
        reply->deleteLater();
        finished(failure);
    });
}

void AgentPanel::checkAgent(int index, bool configure)
{
    if (busy) { return; }
    refresh();
    if (!agents[index].installed()) { return; }
    busy = true;
    cards[index].result->show();
    cards[index].result->setText("正在检查 MCP 服务…");
    prepareProbe([this, index, configure](const QString &failure)
    {
        QString result = failure;
        if (failure.isEmpty())
        {
            try
            {
                const auto bridge = QCoreApplication::applicationDirPath() + "/SerialWorkbenchMCP.exe";
                result = configure ? AgentConfig::configure(agents[index], server->url(), bridge)
                    : AgentConfig::check(agents[index], server->url(), bridge) + "；MCP 服务检查通过";
            }
            catch (const std::exception &exception) { result = "未完成：" + QString::fromUtf8(exception.what()); }
        }
        cards[index].result->setText(result);
        busy = false;
        emit configurationUpdated();
    });
}

void AgentPanel::configureAll()
{
    if (busy) { return; }
    refresh();
    busy = true;
    summary->show();
    summary->setText("正在检查 MCP 服务…");
    prepareProbe([this](const QString &failure)
    {
        int success = 0;
        int failed = 0;
        if (failure.isEmpty())
        {
            for (int index = 0; index < agents.size(); ++index)
            {
                if (!agents[index].installed()) { continue; }
                auto &card = cards[index];
                card.result->show();
                try
                {
                    card.result->setText(AgentConfig::configure(agents[index], server->url(),
                        QCoreApplication::applicationDirPath() + "/SerialWorkbenchMCP.exe"));
                    ++success;
                }
                catch (const std::exception &exception)
                {
                    card.result->setText("未完成：" + QString::fromUtf8(exception.what()));
                    ++failed;
                }
            }
            summary->setText(QString("更新完成：%1 成功，%2 失败，%3 未安装跳过。请重载 Agent 配置。")
                .arg(success).arg(failed).arg(agents.size() - success - failed));
        }
        else { summary->setText(failure); }
        busy = false;
        emit configurationUpdated();
    });
}
}
