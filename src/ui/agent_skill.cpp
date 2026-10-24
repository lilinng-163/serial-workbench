// SPDX-License-Identifier: Apache-2.0
#include "agent_skill.h"
#include "usage_guide.h"
#include "core.h"

namespace sw
{
QJsonArray AgentSkill::targets(const AgentEnvironment &e)
{
    QJsonArray result {
        QJsonObject {{"agent", "codex"}, {"name", "Codex"},
            {"path", e.variable("CODEX_HOME", e.home + "/.codex") + "/skills/serial-workbench"}},
        QJsonObject {{"agent", "claude-code"}, {"name", "Claude Code"},
            {"path", e.variable("CLAUDE_CONFIG_DIR", e.home + "/.claude") + "/skills/serial-workbench"}},
        QJsonObject {{"agent", "opencode"}, {"name", "OpenCode"},
            {"path", e.variable("OPENCODE_CONFIG_DIR", e.variable("XDG_CONFIG_HOME", e.home + "/.config") + "/opencode")
                + "/skills/serial-workbench"}},
        QJsonObject {{"agent", "pi"}, {"name", "Pi Agent"},
            {"path", e.variable("PI_CODING_AGENT_DIR", e.home + "/.pi/agent") + "/skills/serial-workbench"}},
        QJsonObject {{"agent", "hermes"}, {"name", "Hermes"},
            {"path", e.variable("HERMES_HOME", e.home + "/.hermes") + "/skills/serial-workbench"}},
        QJsonObject {{"agent", "openclaw"}, {"name", "OpenClaw"},
            {"path", e.variable("OPENCLAW_STATE_DIR", e.home + "/.openclaw") + "/skills/serial-workbench"}},
        QJsonObject {{"agent", "grok"}, {"name", "Grok Build"},
            {"path", e.home + "/.grok/skills/serial-workbench"}},
        QJsonObject {{"agent", "gemini"}, {"name", "Gemini CLI"},
            {"path", e.home + "/.gemini/skills/serial-workbench"}}
    };
    const auto agents = AgentConfig::discover(e);
    for (int index = 0; index < result.size(); ++index)
    {
        auto target = result[index].toObject();
        bool installed = false;
        for (const auto &agent : agents)
        {
            if (agent.id == target["agent"].toString()) { installed = agent.installed(); }
        }
        bool present = false;
        bool current = true;
        for (const auto &topic : QStringList {"overview", "frame_script", "cli"})
        {
            const auto relative = topic == "overview" ? "SKILL.md" : topic == "cli" ? "references/cli.md" : "references/frame-script.md";
            QFile file(target["path"].toString() + "/" + relative);
            present = present || file.exists();
            current = file.open(QIODevice::ReadOnly) && file.readAll() == UsageGuide::text(topic).toUtf8() && current;
        }
        target["installed"] = installed;
        target["skill_state"] = current ? "已是最新" : present ? "可更新 / 修复" : "未安装";
        result[index] = target;
    }
    return result;
}

void AgentSkill::writeDetection(const QString &path, const AgentEnvironment &environment)
{
    QByteArray bytes("[agents]\r\n");
    for (const auto &value : targets(environment))
    {
        const auto target = value.toObject();
        bytes += target["agent"].toString().toUtf8() + "=" + (target["installed"].toBool() ? "1" : "0") + "\r\n";
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
    {
        fail("无法写入 Agent 检测结果");
    }
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
        {"references/frame-script.md", UsageGuide::text("frame_script")},
        {"references/cli.md", UsageGuide::text("cli")}};
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
