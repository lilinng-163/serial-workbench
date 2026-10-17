// SPDX-License-Identifier: Apache-2.0
#include "agent_skill.h"
#include "usage_guide.h"
#include "core.h"

namespace sw
{
QJsonArray AgentSkill::targets(const AgentEnvironment &e)
{
    return {
        QJsonObject {{"agent", "codex"}, {"name", "Codex"},
            {"path", e.variable("CODEX_HOME", e.home + "/.codex") + "/skills/serial-workbench"}},
        QJsonObject {{"agent", "claude-code"}, {"name", "Claude Code"},
            {"path", e.variable("CLAUDE_CONFIG_DIR", e.home + "/.claude") + "/skills/serial-workbench"}},
        QJsonObject {{"agent", "opencode"}, {"name", "OpenCode"},
            {"path", e.variable("OPENCODE_CONFIG_DIR", e.variable("XDG_CONFIG_HOME", e.home + "/.config") + "/opencode")
                + "/skills/serial-workbench"}},
        QJsonObject {{"agent", "pi"}, {"name", "Pi Agent"},
            {"path", e.variable("PI_CODING_AGENT_DIR", e.home + "/.pi/agent") + "/skills/serial-workbench"}}
    };
}

QJsonObject AgentSkill::install(const QString &agent, const AgentEnvironment &environment)
{
    QString directory;
    for (const auto &target : targets(environment))
    {
        if (target.toObject()["agent"] == agent) { directory = target.toObject()["path"].toString(); }
    }
    if (directory.isEmpty()) { fail("暂不支持该 Agent 的本地 skill 目录"); }
    if (QFileInfo(directory).isSymLink()) { fail("skill 目标是链接，请选择普通目录"); }
    const QMap<QString, QString> files {{"SKILL.md", UsageGuide::text("overview")},
        {"references/frame-script.md", UsageGuide::text("frame_script")}};
    QJsonArray installed;
    QJsonArray backups;
    for (auto file = files.begin(); file != files.end(); ++file)
    {
        const auto path = QDir(directory).filePath(file.key());
        if (QFileInfo(path).isSymLink()) { fail("不覆盖链接文件：" + path); }
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) { fail("无法创建 skill 目录：" + path); }
        const auto bytes = file.value().toUtf8();
        QFile original(path);
        if (original.exists())
        {
            if (!original.open(QIODevice::ReadOnly)) { fail("无法读取已有 skill 文件：" + path); }
            const auto previous = original.readAll();
            original.close();
            if (previous == bytes)
            {
                installed.append(path);
                continue;
            }
            const auto backup = path + "." + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")
                + "-" + QUuid::createUuid().toString(QUuid::Id128).left(8) + ".bak";
            if (!QFile::copy(path, backup)) { fail("skill 备份失败，未覆盖原文件：" + path); }
            backups.append(backup);
        }
        QSaveFile output(path);
        if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit())
        {
            fail("skill 写入失败：" + path);
        }
        installed.append(path);
    }
    return {{"agent", agent}, {"directory", directory}, {"files", installed}, {"backups", backups},
        {"version", Version}, {"mcp_config_changed", false}, {"reload_required", true}};
}
}
