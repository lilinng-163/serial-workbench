// SPDX-License-Identifier: Apache-2.0
#include "core.h"
#include "openocd.h"
#include <vector>

namespace sw
{
OpenOcdService *Engine::createOpenOcdView(const QString &title, const QJsonObject &configuration)
{
    if (openocdViews.size() >= 32) { fail("OpenOCD 页面最多 32 个，请先关闭不需要的页面"); }
    auto view = std::make_unique<OpenOcdService>();
    QJsonObject initial;
    if (openocd)
    {
        initial["executable"] = openocd->configuration()["executable"];
        initial["scripts_dir"] = openocd->configuration()["scripts_dir"];
    }
    for (auto value = configuration.begin(); value != configuration.end(); ++value) { initial[value.key()] = value.value(); }
    view->restore(initial);
    if (!title.trimmed().isEmpty()) { view->title = title.trimmed().left(200); }
    auto result = view.release();
    result->setParent(this);
    openocdViews.insert(result->viewId, result);
    if (!openocd) { openocd = result; }
    connect(result, &OpenOcdService::configurationChanged, this, &Engine::openOcdViewsChanged);
    emit openOcdViewsChanged();
    return result;
}

OpenOcdService *Engine::openOcdView(const QString &id) const
{
    if (id.isEmpty())
    {
        if (openocdViews.isEmpty()) { fail("尚无 OpenOCD 页面，请先 create_openocd_view 或在 GUI 新建烧录页"); }
        if (openocdViews.size() != 1) { fail("存在多个 OpenOCD 页面，请先 list_openocd_views 并明确指定 view_id"); }
        return openocdViews.first();
    }
    auto view = openocdViews.value(id);
    if (!view) { fail("OpenOCD view_id 不存在，请先 list_openocd_views"); }
    return view;
}

void Engine::removeOpenOcdView(const QString &id)
{
    auto view = openOcdView(id);
    if (view->busy()) { fail("该页面仍在执行 OpenOCD 任务，请先完成或取消再关闭"); }
    openocdViews.remove(id);
    disconnect(view, nullptr, this, nullptr);
    if (openocd == view) { openocd = openocdViews.isEmpty() ? nullptr : openocdViews.first(); }
    view->deleteLater();
    emit openOcdViewsChanged();
}

QJsonArray Engine::openOcdSettings() const
{
    QJsonArray profiles;
    for (auto view : openocdViews)
    {
        profiles.append(QJsonObject {{"view_id", view->viewId}, {"title", view->title}, {"has_page", view->hasPage}, {"configuration", view->configuration()}});
    }
    return profiles;
}

void Engine::restoreOpenOcdViews(const QJsonArray &profiles)
{
    if (profiles.size() > 32) { fail("OpenOCD 配置列表最多 32 个页面"); }
    for (auto view : openocdViews)
    {
        if (view->busy()) { fail("OpenOCD 任务运行中，不能恢复页面"); }
    }
    QSet<QString> identifiers;
    std::vector<std::unique_ptr<OpenOcdService>> prepared;
    static const QRegularExpression validId("^[A-Za-z0-9_-]{1,80}$");
    for (const auto &value : profiles)
    {
        const auto profile = value.toObject();
        const auto id = profile["view_id"].toString();
        if (!validId.match(id).hasMatch() || identifiers.contains(id) || !profile["configuration"].isObject())
        {
            fail("OpenOCD 页面配置不正确：view_id 应唯一且 configuration 为对象");
        }
        identifiers.insert(id);
        auto view = std::make_unique<OpenOcdService>();
        view->viewId = id;
        view->title = profile["title"].toString("OpenOCD 烧录").left(200);
        view->hasPage = profile["has_page"].toBool(true);
        view->restore(profile["configuration"].toObject());
        prepared.push_back(std::move(view));
    }
    for (auto old : openocdViews) { delete old; }
    openocdViews.clear();
    openocd = nullptr;
    for (auto &entry : prepared)
    {
        auto view = entry.release();
        view->setParent(this);
        openocdViews.insert(view->viewId, view);
        if (!openocd) { openocd = view; }
        connect(view, &OpenOcdService::configurationChanged, this, &Engine::openOcdViewsChanged);
    }
    emit openOcdViewsChanged();
}

void Engine::checkOpenOcdProbe(const OpenOcdService *requested) const
{
    const auto serial = requested->configuration()["adapter_serial"].toString().trimmed();
    for (auto other : openocdViews)
    {
        if (other == requested || !other->busy() || other->state()["operation"] == "version") { continue; }
        const auto occupied = other->configuration()["adapter_serial"].toString().trimmed();
        if (serial.isEmpty() || occupied.isEmpty())
        {
            fail("并行连接/烧录前，每个页面都必须填写不同的调试器序列号；无法从 cfg 文件猜测探针归属");
        }
        if (serial.compare(occupied, Qt::CaseInsensitive) == 0)
        {
            fail("调试器 " + serial + " 正被「" + other->title + "」使用，请选择其他探针或等待任务结束");
        }
    }
}
}
