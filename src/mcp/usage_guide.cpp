// SPDX-License-Identifier: Apache-2.0
#include "usage_guide.h"
#include "core.h"

static void initializeGuideResources()
{
    Q_INIT_RESOURCE(workbench_guides);
}

namespace sw
{
QMap<QString, QByteArray> UsageGuide::files()
{
    static const bool ready = [] { initializeGuideResources(); return true; }();
    Q_UNUSED(ready);
    QMap<QString, QByteArray> result;
    QDirIterator entries(":/guides", {"*.md"}, QDir::Files, QDirIterator::Subdirectories);
    while (entries.hasNext())
    {
        QFile file(entries.next());
        if (!file.open(QIODevice::ReadOnly)) { fail("无法读取内置使用指南"); }
        result.insert(file.fileName().mid(QString(":/guides/").size()), file.readAll());
    }
    if (!result.contains("SKILL.md")) { fail("内置 skill 缺少入口文件"); }
    return result;
}

QJsonObject UsageGuide::manifest()
{
    QJsonObject hashes;
    const auto contents = files();
    for (auto file = contents.begin(); file != contents.end(); ++file)
    {
        hashes[file.key()] = QString::fromLatin1(QCryptographicHash::hash(file.value(), QCryptographicHash::Sha256).toHex());
    }
    return {{"version", Version}, {"files", hashes}};
}

QString UsageGuide::text(const QString &topic)
{
    QString name;
    if (topic == "overview") { name = "SKILL.md"; }
    else if (topic == "frame_script") { name = "references/frame-script.md"; }
    else if (topic == "cli") { name = "references/cli.md"; }
    else if (topic == "model3d") { name = "references/model3d.md"; }
    else if (topic == "agents") { name = "references/agents.md"; }
    else if (topic == "openocd") { name = "references/openocd.md"; }
    else { fail("未知指南主题，请使用 overview、frame_script、cli、model3d、agents 或 openocd"); }

    const auto contents = files();
    if (!contents.contains(name)) { fail("无法读取内置使用指南"); }
    return QString::fromUtf8(contents[name]);
}

QJsonObject UsageGuide::read(const QString &topic)
{
    const QString skill = QDir(QCoreApplication::applicationDirPath()).filePath("skills/serial-workbench/SKILL.md");
    return {{"topic", topic}, {"guide", text(topic)},
        {"skill_path", QFileInfo::exists(skill) ? QJsonValue(QDir::toNativeSeparators(skill)) : QJsonValue(QJsonValue::Null)},
        {"skill_path_scope", "server_host"}, {"skill_auto_installed", false},
        {"skill_installation", "optional_installer_components"},
        {"available_topics", QJsonArray {"overview", "frame_script", "cli", "model3d", "agents", "openocd"}}};
}

}
