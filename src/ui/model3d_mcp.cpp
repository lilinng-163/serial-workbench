// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "model3d_panel.h"
#include <QBuffer>

namespace sw
{
namespace
{
QJsonObject snapshot(Pane *pane)
{
    auto model = pane->instrument->model;
    model->tick(pane->channel);
    auto state = model->state();
    state["view_id"] = pane->searchViewId;
    state["title"] = pane->pageTitle;
    state["channel_id"] = pane->channel ? pane->channel->id : QString();
    const QJsonObject revision {{"settings", state["settings"]}, {"channel_id", state["channel_id"]},
        {"pending_file", state["pending_file"]}};
    state["revision"] = QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(revision).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    return state;
}

QString modelPath(const QJsonValue &value)
{
    if (!value.isString() || !QDir::isAbsolutePath(value.toString())) { fail("file_path 必须为工作台所在电脑的绝对路径"); }
    const QFileInfo file(value.toString());
    if (!file.isFile() || !file.isReadable()) { fail("模型文件不存在或不可读"); }
    if (!QStringList {"stl", "obj", "ply"}.contains(file.suffix().toLower())) { fail("支持 STL / OBJ / PLY 模型"); }
    if (file.size() > 64 * 1024 * 1024) { fail("模型文件不能超过 64 MiB"); }
    return file.absoluteFilePath();
}
}

void Window::installModelMcp()
{
    const QPointer<Window> guard(this);
    mcp->modelHandler = [guard](const QString &name, const QJsonObject &arguments) -> QJsonObject
    {
        if (!guard) { fail("3D 页面服务已关闭"); }
        auto server = guard->mcp;
        QList<Pane *> views;
        for (auto host : guard->hosts)
        {
            if (!host) { continue; }
            for (auto pane : host->panes)
            {
                if (pane->instrument && pane->instrument->model) { views.append(pane); }
            }
        }
        const auto exposed = [server](Pane *pane)
        {
            return !pane->channel || server->protocolEnabled(pane->channel->config.kind);
        };
        const auto source = [&]() -> Channel *
        {
            if (!arguments["channel_id"].isString()) { fail("channel_id 必须为字符串，空字符串解除绑定"); }
            const auto id = arguments["channel_id"].toString();
            if (id.isEmpty()) { return nullptr; }
            for (auto channel : guard->engine->channels)
            {
                if (channel->id != id) { continue; }
                if (!server->protocolEnabled(channel->config.kind)) { fail("该连接协议的 AI 访问已关闭"); }
                return channel;
            }
            fail("channel_id 不存在，请先 list_connections");
        };
        if (name == "list_model_views")
        {
            Channel *filter = arguments.contains("channel_id") ? source() : nullptr;
            QJsonArray list;
            for (auto pane : views)
            {
                if (!exposed(pane) || (arguments.contains("channel_id") && pane->channel != filter)) { continue; }
                auto state = snapshot(pane);
                list.append(QJsonObject {{"view_id", state["view_id"]}, {"title", state["title"]},
                    {"channel_id", state["channel_id"]}, {"revision", state["revision"]},
                    {"file", state["settings"].toObject()["file"]}, {"loading", state["loading"]}});
            }
            return {{"views", list}};
        }
        if (name == "open_model_view")
        {
            const auto path = modelPath(arguments["file_path"]);
            auto channel = arguments.contains("channel_id") ? source() : nullptr;
            if (views.size() >= 32) { fail("3D 页面数量已达 32，请先在 GUI 关闭不需要的页面"); }
            auto pane = guard->newInstrument("model3d", channel, false);
            if (!pane) { fail("无法创建 3D 页面"); }
            pane->setChannel(channel);
            pane->instrument->model->loadFile(path);
            guard->saveSettings();
            return snapshot(pane);
        }
        const auto id = arguments["view_id"].toString();
        Pane *pane = nullptr;
        for (auto view : views)
        {
            if (view->searchViewId == id && exposed(view)) { pane = view; break; }
        }
        if (!pane) { fail("3D 页面不存在或协议访问已关闭，请先 list_model_views"); }
        auto model = pane->instrument->model;
        if (name == "get_model_state") { return snapshot(pane); }
        if (model->loading()) { fail("模型仍在后台导入，请等待 get_model_state.loading=false"); }
        if (name == "configure_model_view")
        {
            if (arguments["expected_revision"] != snapshot(pane)["revision"])
            {
                fail("3D 设置已变化，请重新 get_model_state 后使用最新 expected_revision");
            }
            Channel *channel = arguments.contains("channel_id") ? source() : pane->channel.data();
            if (arguments.contains("settings") && !arguments["settings"].isObject()) { fail("settings 必须为对象"); }
            if (arguments.contains("fit") && !arguments["fit"].isBool()) { fail("fit 必须为布尔值"); }
            const auto path = arguments.contains("file_path") ? modelPath(arguments["file_path"]) : QString();
            model->configure(arguments["settings"].toObject());
            if (channel != pane->channel)
            {
                pane->sourceKey = channel ? channel->config.key() : QString();
                pane->setChannel(channel);
                model->tick(channel);
            }
            if (arguments["fit"].toBool()) { model->view->fit(); }
            if (!path.isEmpty()) { model->loadFile(path); }
            guard->saveSettings();
            return snapshot(pane);
        }
        if (name == "capture_model_view")
        {
            const auto size = arguments.value("max_size").toInt(1024);
            if (size < 256 || size > 1600 || (arguments.contains("max_size") && arguments["max_size"].toDouble() != size))
            {
                fail("max_size 必须为 256..1600 的整数");
            }
            auto state = snapshot(pane);
            const auto frame = model->view->capture(size);
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            if (!frame.save(&buffer, "PNG") || bytes.size() > 8 * 1024 * 1024) { fail("3D 截图编码失败或过大"); }
            state["image_width"] = frame.width();
            state["image_height"] = frame.height();
            state["captured_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            state["image_png_base64"] = QString::fromLatin1(bytes.toBase64());
            return state;
        }
        fail("未知 3D 操作");
    };
}
}
