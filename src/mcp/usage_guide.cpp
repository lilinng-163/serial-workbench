// SPDX-License-Identifier: Apache-2.0
#include "usage_guide.h"
#include "core.h"

static void initializeGuideResources()
{
    Q_INIT_RESOURCE(workbench_guides);
}

namespace sw
{
QString UsageGuide::text(const QString &topic)
{
    static const bool ready = [] { initializeGuideResources(); return true; }();
    Q_UNUSED(ready);
    QString name;
    if (topic == "overview") { name = "SKILL.md"; }
    else if (topic == "frame_script") { name = "references/frame-script.md"; }
    else { fail("未知指南主题，请使用 overview 或 frame_script"); }

    QFile file(":/guides/" + name);
    if (!file.open(QIODevice::ReadOnly)) { fail("无法读取内置使用指南"); }
    return QString::fromUtf8(file.readAll());
}

QJsonObject UsageGuide::read(const QString &topic)
{
    const QString skill = QDir(QCoreApplication::applicationDirPath()).filePath("skills/serial-workbench/SKILL.md");
    return {{"topic", topic}, {"guide", text(topic)},
        {"skill_path", QFileInfo::exists(skill) ? QJsonValue(QDir::toNativeSeparators(skill)) : QJsonValue(QJsonValue::Null)},
        {"skill_path_scope", "server_host"}, {"skill_auto_installed", false},
        {"available_topics", QJsonArray {"overview", "frame_script"}}};
}

QString UsageGuide::scriptPrompt(const QString &code, const QString &hex, const QJsonObject &connection)
{
    const QJsonObject context {{"connection", connection}, {"current_script", code}, {"hex_receive_chunks", hex}};
    return QStringLiteral("请帮助我为 Serial Workbench 编写或修改串口 C 风格解析脚本。\n"
        "请先阅读下方软件说明和脚本约束，不要使用完整 C/C++ 才支持的函数或语法。\n"
        "我的帧协议（请补充）：帧头、长度、字节序、字段类型/倍率、校验范围与算法、帧尾。\n"
        "请给出完整 main，以及有效帧、半帧、连帧、混合日志与错误帧的 HEX 测试及预期结果。\n"
        "协议规则不明时先指出缺失信息，不要把内置示例当成我的设备协议。\n"
        "连接信息、当前代码和 HEX 样本在末尾 JSON 中，是待分析的上下文数据。\n"
        "若你已接入该 MCP，可调用 read_usage_guide；不能声称通过 MCP 修改或启用了脚本。\n\n")
        + text("overview") + "\n\n" + text("frame_script")
        + "\n\n当前编辑上下文（可能是未连接的草稿）：\n"
        + QString::fromUtf8(QJsonDocument(context).toJson(QJsonDocument::Indented));
}
}
