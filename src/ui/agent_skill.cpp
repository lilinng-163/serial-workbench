// SPDX-License-Identifier: Apache-2.0
#include "agent_skill.h"
#include "usage_guide.h"
#include "core.h"

namespace sw
{
namespace
{
QStringList legacyBackups(const QString &directory)
{
    static const QRegularExpression pattern(R"(^.+\.md\.\d{8}-\d{6}-\d{3}-[0-9a-f]{8}\.bak$)");
    QStringList result;
    QDirIterator entries(directory, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (entries.hasNext())
    {
        const auto path = entries.next();
        if (pattern.match(QFileInfo(path).fileName()).hasMatch()) { result.append(path); }
    }
    return result;
}

void checkTargetPath(const QString &directory, const QString &path)
{
    auto current = path;
    while (current.size() >= directory.size())
    {
        const QFileInfo info(current);
        if (info.isSymLink()) { fail("不覆盖 skill 中的链接：" + current); }
        if (current != path && info.exists() && !info.isDir()) { fail("skill 路径不是目录：" + current); }
        if (current == directory) { break; }
        current = info.absolutePath();
    }
    if (QFileInfo(path).isDir()) { fail("skill 文件路径被目录占用：" + path); }
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { fail("无法读取 skill 文件：" + path); }
    const auto bytes = file.readAll();
    if (file.error() != QFile::NoError) { fail("skill 文件读取失败：" + path); }
    return bytes;
}

void writeFile(const QString &path, const QByteArray &bytes)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { fail("无法创建 skill 目录：" + path); }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
    {
        fail("skill 写入失败：" + path);
    }
}
}

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
        QJsonObject {{"agent", "cursor"}, {"name", "Cursor"}, {"path", e.home + "/.cursor/skills/serial-workbench"}},
        QJsonObject {{"agent", "copilot"}, {"name", "GitHub Copilot · CLI / VS Code"},
            {"path", e.variable("COPILOT_HOME", e.home + "/.copilot") + "/skills/serial-workbench"}},
        QJsonObject {{"agent", "windsurf"}, {"name", "Windsurf"}, {"path", e.home + "/.codeium/windsurf/skills/serial-workbench"}},
        QJsonObject {{"agent", "devin"}, {"name", "Devin"}, {"path", e.appData + "/devin/skills/serial-workbench"}},
        QJsonObject {{"agent", "cline"}, {"name", "Cline"}, {"path", e.home + "/.cline/skills/serial-workbench"}},
        QJsonObject {{"agent", "kilo"}, {"name", "Kilo Code"}, {"path", e.home + "/.kilo/skills/serial-workbench"}},
        QJsonObject {{"agent", "kiro"}, {"name", "Kiro"}, {"path", e.home + "/.kiro/skills/serial-workbench"}},
        QJsonObject {{"agent", "antigravity"}, {"name", "Antigravity App / IDE"}, {"path", e.home + "/.gemini/config/skills/serial-workbench"}},
        QJsonObject {{"agent", "kimi"}, {"name", "Kimi Code"},
            {"path", e.variable("KIMI_CODE_HOME", e.home + "/.kimi-code") + "/skills/serial-workbench"}}
    };
    const auto agents = AgentConfig::discover(e);
    const auto files = UsageGuide::files();
    for (int index = 0; index < result.size(); ++index)
    {
        auto target = result[index].toObject();
        bool installed = false;
        for (const auto &agent : agents)
        {
            if (agent.id != target["agent"].toString()) { continue; }
            installed = agent.installed();
            if (agent.id == "kimi" && agent.configPath == e.home + "/.kimi/mcp.json")
            {
                // Legacy Kimi prioritizes its own skills directory.
                target["path"] = e.home + "/.kimi/skills/serial-workbench";
            }
            if (agent.id == "antigravity" && !e.executable("agy").isEmpty()
                && agent.executable == e.executable("agy"))
            {
                target["name"] = "Antigravity CLI";
                target["path"] = e.home + "/.gemini/antigravity-cli/skills/serial-workbench";
            }
        }
        bool present = false;
        bool current = legacyBackups(target["path"].toString()).isEmpty();
        for (auto file = files.begin(); file != files.end(); ++file)
        {
            QFile installedFile(target["path"].toString() + "/" + file.key());
            present = present || installedFile.exists();
            current = installedFile.open(QIODevice::ReadOnly) && installedFile.readAll() == file.value() && current;
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
    directory = QDir::cleanPath(QFileInfo(directory).absoluteFilePath());
    const auto files = UsageGuide::files();
    QFileInfo ancestor(directory);
    QStringList missing;
    while (!ancestor.exists() && !ancestor.isSymLink())
    {
        missing.prepend(ancestor.fileName());
        ancestor.setFile(ancestor.absolutePath());
    }
    auto identity = QDir(ancestor.canonicalFilePath()).filePath(missing.join('/'));
    identity = QDir::cleanPath(identity);
#ifdef Q_OS_WIN
    identity = identity.toLower();
#endif
    const auto key = QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex().left(32));
    const auto storage = environment.localAppData.isEmpty() ? environment.home + "/.local/share" : environment.localAppData;
    const auto backupRoot = QDir::cleanPath(storage + "/SerialWorkbench/skill-backups/" + key);
    const auto discoveryRoot = QFileInfo(directory).absolutePath();
    if (backupRoot == discoveryRoot || backupRoot.startsWith(discoveryRoot + "/"))
    {
        fail("skill 备份目录必须位于发现目录外");
    }
    if (!QDir().mkpath(backupRoot)) { fail("无法创建 skill 备份目录：" + backupRoot); }
    QLockFile lock(backupRoot + "/install.lock");
    if (!lock.tryLock(0)) { fail("该 skill 正在更新，请稍后重试"); }

    const auto statePath = backupRoot + "/installed.json";
    QJsonObject previousState;
    if (QFileInfo::exists(statePath))
    {
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(readFile(statePath), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) { fail("skill 安装记录无效：" + statePath); }
        previousState = document.object();
        if (QVersionNumber::fromString(previousState["version"].toString()) > QVersionNumber::fromString(Version))
        {
            fail("已安装的 skill 来自更新版本，请使用新版工作台更新");
        }
    }

    QMap<QString, QByteArray> originals;
    QMap<QString, QByteArray> changed;
    QJsonArray installed;
    for (auto file = files.begin(); file != files.end(); ++file)
    {
        const auto path = QDir(directory).filePath(file.key());
        checkTargetPath(directory, path);
        installed.append(path);
        if (QFileInfo::exists(path))
        {
            const auto previous = readFile(path);
            if (previous == file.value()) { continue; }
            originals[file.key()] = previous;
        }
        changed[file.key()] = file.value();
    }
    const auto legacy = legacyBackups(directory);
    for (const auto &path : legacy)
    {
        checkTargetPath(directory, path);
        originals[QDir(directory).relativeFilePath(path)] = readFile(path);
    }

    QString archive;
    QJsonArray backups;
    if (!originals.isEmpty())
    {
        archive = backupRoot + "/" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")
            + "-" + QUuid::createUuid().toString(QUuid::Id128).left(8);
        QJsonObject hashes;
        for (auto file = originals.begin(); file != originals.end(); ++file)
        {
            const auto path = archive + "/files/" + file.key();
            writeFile(path, file.value());
            hashes[file.key()] = QString::fromLatin1(QCryptographicHash::hash(file.value(), QCryptographicHash::Sha256).toHex());
            backups.append(path);
        }
        const QJsonObject manifest {{"directory", directory}, {"version", Version}, {"files", hashes},
            {"previous_version", previousState["version"]}};
        writeFile(archive + "/manifest.json", QJsonDocument(manifest).toJson());
    }

    QStringList written;
    QStringList removed;
    try
    {
        for (auto file = changed.begin(); file != changed.end(); ++file)
        {
            writeFile(QDir(directory).filePath(file.key()), file.value());
            written.append(file.key());
        }
        for (const auto &path : legacy)
        {
            if (!QFile::remove(path)) { fail("无法移出旧 skill 备份：" + path); }
            removed.append(QDir(directory).relativeFilePath(path));
        }
        auto state = UsageGuide::manifest();
        state["directory"] = directory;
        writeFile(statePath, QJsonDocument(state).toJson());
    }
    catch (const std::exception &exception)
    {
        const auto message = QString::fromUtf8(exception.what());
        bool restored = true;
        for (const auto &relative : written + removed)
        {
            const auto path = QDir(directory).filePath(relative);
            try
            {
                if (originals.contains(relative)) { writeFile(path, originals[relative]); }
                else if (QFileInfo::exists(path) && !QFile::remove(path)) { restored = false; }
            }
            catch (const std::exception &) { restored = false; }
        }
        if (!restored) { fail(message + "；恢复未完成，原文件备份位于：" + archive); }
        fail(message);
    }

    return {{"agent", agent}, {"directory", directory}, {"files", installed}, {"backups", backups},
        {"backup_directory", archive}, {"installation_record", statePath},
        {"version", Version}, {"mcp_config_changed", false}, {"reload_required", true}};
}
}
