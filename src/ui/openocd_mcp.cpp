// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "openocd_dialog.h"

namespace sw
{
namespace
{
int pageIndex(Window *window, Pane *pane)
{
    for (int index = 0; index < window->tabs->count(); ++index)
    {
        if (window->tabs->tabData(index).value<QObject *>() == pane) { return index; }
    }
    return -1;
}
}
void Window::installOpenOcdMcp()
{
    const QPointer<Window> guard(this);
    mcp->openOcdViewHandler = [guard](const QString &action, const QJsonObject &arguments) -> QJsonObject
    {
        if (!guard) { fail("OpenOCD 页面服务已关闭"); }
        if (action == "create_openocd_view")
        {
            OpenOcdService validation;
            validation.restore(arguments["configuration"].toObject());
            auto pane = guard->newInstrument("openocd", nullptr, arguments["activate"].toBool(false));
            if (!pane) { fail("无法创建 OpenOCD 页面"); }
            auto view = pane->instrument->flasherService;
            view->configure(arguments["configuration"].toObject(), view->revision());
            const auto title = arguments["title"].toString().trimmed().left(200);
            if (!title.isEmpty())
            {
                view->title = title;
                pane->pageTitle = title;
                guard->tabs->setTabText(pageIndex(guard, pane), title);
            }
            guard->saveSettings();
            return view->state();
        }
        auto view = guard->engine->openOcdView(arguments["view_id"].toString());
        if (action == "ensure_openocd_view")
        {
            guard->newInstrument("openocd", nullptr, false, {}, view->viewId);
            guard->saveSettings();
            return view->state();
        }
        const auto id = view->viewId;
        if (view->busy()) { fail("任务运行中，不能关闭 OpenOCD 页面"); }
        for (auto host : guard->hosts)
        {
            if (!host) { continue; }
            for (auto pane : host->panes)
            {
                if (pane->instrument && pane->instrument->flasherService == view)
                {
                    host->closeTab(pageIndex(host, pane));
                    return {{"closed", id}};
                }
            }
        }
        guard->engine->removeOpenOcdView(id);
        guard->saveSettings();
        return {{"closed", id}};
    };
}
}
