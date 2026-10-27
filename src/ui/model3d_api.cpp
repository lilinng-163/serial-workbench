// SPDX-License-Identifier: Apache-2.0
#include "model3d_panel.h"
#include <cmath>

namespace sw
{
namespace
{
void keys(const QJsonObject &object, const QStringList &allowed)
{
    for (auto it = object.begin(); it != object.end(); ++it)
    {
        if (!allowed.contains(it.key())) { fail("未知 3D 设置：" + it.key()); }
    }
}

void number(const QJsonValue &value, double low, double high)
{
    if (!value.isDouble() || !std::isfinite(value.toDouble()) || value.toDouble() < low || value.toDouble() > high)
    {
        fail(QString("3D 数值必须在 %1..%2 之间").arg(low).arg(high));
    }
}

void vector(const QJsonValue &value, double low, double high)
{
    if (!value.isArray() || value.toArray().size() != 3) { fail("3D 向量必须含 3 个数值"); }
    for (const auto &item : value.toArray()) { number(item, low, high); }
}

void booleanFields(const QJsonObject &object, const QStringList &names)
{
    for (const auto &name : names)
    {
        if (object.contains(name) && !object[name].isBool()) { fail(name + " 必须为布尔值"); }
    }
}

void choice(const QJsonObject &object, const QString &name, const QStringList &choices)
{
    if (object.contains(name) && (!object[name].isString() || !choices.contains(object[name].toString())))
    {
        fail(name + " 可选值：" + choices.join(", "));
    }
}

QJsonObject merge(const QJsonObject &base, const QJsonObject &patch)
{
    auto result = base;
    for (auto it = patch.begin(); it != patch.end(); ++it)
    {
        if (it->isObject()) { result[it.key()] = merge(result[it.key()].toObject(), it->toObject()); }
        else { result[it.key()] = *it; }
    }
    return result;
}

void validateMapping(const QJsonObject &mapping)
{
    keys(mapping, {"mode", "order", "intrinsic", "radians", "position", "angles", "quaternion", "axes"});
    choice(mapping, "mode", {"euler", "quaternion"});
    choice(mapping, "order", {"XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX"});
    booleanFields(mapping, {"intrinsic", "radians"});
    for (const auto &name : {"position", "angles", "quaternion"})
    {
        if (!mapping.contains(name)) { continue; }
        const auto value = mapping[name];
        const int count = QString(name) == "quaternion" ? 4 : 3;
        if (!value.isArray() || value.toArray().size() != count) { fail(QString(name) + " 绑定数量错误"); }
        for (const auto &entry : value.toArray())
        {
            if (!entry.isObject()) { fail("绑定必须为对象 {field, gain, offset}"); }
            const auto binding = entry.toObject();
            keys(binding, {"field", "gain", "offset"});
            if (binding.contains("field") && (!binding["field"].isString() || binding["field"].toString().size() > 256))
            {
                fail("绑定字段必须为不超过 256 字符的字符串");
            }
            for (const auto &key : {"gain", "offset"})
            {
                if (binding.contains(key)) { number(binding[key], -1e9, 1e9); }
            }
        }
    }
    if (mapping.contains("axes"))
    {
        vector(mapping["axes"], -3, 3);
        QSet<int> used;
        for (const auto &axis : mapping["axes"].toArray())
        {
            const auto integer = axis.toInt();
            if (axis.toDouble() != integer || !integer || used.contains(qAbs(integer)))
            {
                fail("坐标轴应为 ±1/±2/±3 的不重复排列");
            }
            used.insert(qAbs(integer));
        }
    }
}
}

QJsonObject ModelPanel::state() const
{
    return {{"settings", settings()}, {"loading", loading()}, {"pending_file", loading() ? pendingPath : QString()},
        {"load_error", loadError}, {"using_example_mesh", modelPath.isEmpty()}, {"model", view->modelState()},
        {"telemetry", QJsonObject {{"observed_sequence", lastSample}, {"position_sequence", positionSample},
            {"rotation_sequence", rotationSample}, {"generation", generation}, {"paused", pause->isChecked()},
            {"errors", QJsonArray::fromStringList(mappingErrors)},
            {"connection_state", channel ? channel->state : "unbound"}}}, {"status", status->text()}};
}

void ModelPanel::configure(const QJsonObject &patch)
{
    keys(patch, {"mapping", "feed", "peer", "stream", "paused", "scale", "centered",
        "manual_position", "follow_theme", "color", "alignment", "camera"});
    booleanFields(patch, {"paused", "centered", "follow_theme"});
    choice(patch, "feed", {"wave", "script"});
    for (const auto &name : {"peer", "stream"})
    {
        if (patch.contains(name) && (!patch[name].isString() || patch[name].toString().size() > 256))
        {
            fail(QString(name) + " 必须为不超过 256 字符的字符串");
        }
    }
    if (patch.contains("stream") && !patch["stream"].toString().isEmpty()
        && !QRegularExpression("^[\\p{L}_][\\p{L}\\p{N}_-]{0,31}$").match(patch["stream"].toString()).hasMatch())
    {
        fail("stream 必须为有效的 @wave[名称]，空字符串选择未命名流");
    }
    if (patch.contains("peer") && patch["peer"].toString().isEmpty()) { fail("peer 不可为空；本连接使用 default"); }
    if (patch.contains("scale")) { number(patch["scale"], .000001, 1000000); }
    if (patch.contains("manual_position")) { vector(patch["manual_position"], -1e9, 1e9); }
    if (patch.contains("alignment")) { vector(patch["alignment"], -360, 360); }
    if (patch.contains("color") && (!patch["color"].isString()
        || !QRegularExpression("^#(?:[0-9a-fA-F]{3}|[0-9a-fA-F]{6})$").match(patch["color"].toString()).hasMatch()))
    {
        fail("color 必须为 #RGB 或 #RRGGBB；使用自定义颜色时设置 follow_theme=false");
    }
    if (patch.contains("mapping"))
    {
        if (!patch["mapping"].isObject()) { fail("mapping 必须为对象"); }
        validateMapping(patch["mapping"].toObject());
    }
    if (patch.contains("camera"))
    {
        if (!patch["camera"].isObject()) { fail("camera 必须为对象"); }
        const auto camera = patch["camera"].toObject();
        keys(camera, {"yaw", "elevation", "distance", "target"});
        if (camera.contains("yaw")) { number(camera["yaw"], -360, 360); }
        if (camera.contains("elevation")) { number(camera["elevation"], -89.9, 89.9); }
        if (camera.contains("distance")) { number(camera["distance"], .001, 1e12); }
        if (camera.contains("target")) { vector(camera["target"], -1e12, 1e12); }
    }
    if (auto edit = qobject_cast<QLineEdit *>(QApplication::focusWidget());
        edit && isAncestorOf(edit) && edit->isModified())
    {
        fail("3D 设置正在编辑，请先确认输入或将焦点移出输入框");
    }
    // Validate everything before changing any widget. Reuse the GUI's setters,
    // preserving the mesh and last valid pose when only appearance changes.
    const auto combined = merge(settings(), patch);
    restore(combined, false);
    if (patch.contains("mapping")) { lastSample = -1; }
    tick(channel);
    emit changed();
}
}
