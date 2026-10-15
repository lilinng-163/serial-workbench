// SPDX-License-Identifier: Apache-2.0

#include "agent_config.h"
#include "core.h"
#include <tomlplusplus/toml.hpp>
#include <fkyaml/fkyaml.hpp>
#include <sstream>

namespace sw
{
// Token-based JSONC edits preserve comments and all unrelated client entries.
struct JsonToken
{
    int start;
    int end;
    QString text;
};

static QList<JsonToken> jsonTokens(const QString &text)
{
    static const QRegularExpression expression(R"(\s+|//[^\r\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_$][A-Za-z0-9_$-]*|[^\s])");
    QList<JsonToken> tokens;
    auto matches = expression.globalMatch(text);
    while (matches.hasNext())
    {
        const auto match = matches.next();
        const auto value = match.captured();
        if (!value.trimmed().isEmpty() && !value.startsWith("//") && !value.startsWith("/*"))
        {
            tokens.append({static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedEnd()), value});
        }
    }
    return tokens;
}

static QString normalizedToken(const QString &token, bool key)
{
    if (token.startsWith('\''))
    {
        QString inner = token.mid(1, token.size() - 2);
        inner.replace("\\'", "'");
        inner.replace(QRegularExpression(R"rx((?<!\\)")rx"), "\\\"");
        return "\"" + inner + "\"";
    }
    if (key && !token.startsWith('"'))
    {
        const auto encoded = QJsonDocument(QJsonArray {token}).toJson(QJsonDocument::Compact);
        return QString::fromUtf8(encoded.mid(1, encoded.size() - 2));
    }
    return token;
}

static QPair<QMap<QString, QPair<int, int>>, int> jsonMembers(const QString &text, int start)
{
    const auto tokens = jsonTokens(text.mid(start));
    if (tokens.isEmpty() || tokens.first().text != "{")
    {
        fail("配置节点需要 JSON 对象");
    }
    QMap<QString, QPair<int, int>> members;
    int i = 1;
    while (i < tokens.size())
    {
        if (tokens[i].text == "}")
        {
            return {members, tokens[i].start + start};
        }
        if (tokens[i].text == ",")
        {
            ++i;
            continue;
        }
        const auto nameDocument = QJsonDocument::fromJson(("[" + normalizedToken(tokens[i].text, true) + "]").toUtf8());
        if (!nameDocument.isArray() || !nameDocument.array()[0].isString() || i + 2 >= tokens.size() || tokens[i + 1].text != ":")
        {
            fail("JSON 配置语法无效");
        }
        const QString name = nameDocument.array()[0].toString();
        i += 2;
        const int begin = tokens[i].start + start;
        int end = begin;
        int depth = 0;
        while (i < tokens.size())
        {
            const QString token = tokens[i].text;
            if ((token == "," || token == "}") && depth == 0)
            {
                break;
            }
            if (token == "{" || token == "[")
            {
                ++depth;
            }
            else if (token == "}" || token == "]")
            {
                --depth;
            }
            end = tokens[i].end + start;
            ++i;
        }
        if (members.contains(name)) { fail("配置对象包含重复字段：" + name); }
        members[name] = {begin, end};
    }
    fail("JSON 配置对象未闭合");
}

static QString setJsonMember(QString text, int start, const QStringList &keys, const QString &encoded)
{
    const auto object = jsonMembers(text, start);
    const QString key = keys.first();
    QString value = encoded;
    if (keys.size() > 1)
    {
        if (object.first.contains(key))
        {
            return setJsonMember(text, object.first[key].first, keys.mid(1), encoded);
        }
        for (int i = keys.size() - 1; i >= 1; --i)
        {
            value = "{\"" + keys[i] + "\": " + value + "}";
        }
    }
    if (object.first.contains(key))
    {
        const auto range = object.first[key];
        text.replace(range.first, range.second - range.first, value);
    }
    else
    {
        const auto preceding = jsonTokens(text.left(object.second));
        const bool comma = !object.first.isEmpty() && !preceding.isEmpty() && preceding.last().text != ",";
        text.insert(object.second, QString(comma ? "," : "") + "\n  \"" + key + "\": " + value + "\n");
    }
    return text;
}

static QJsonObject parseJsonc(const QString &text)
{
    const auto tokens = jsonTokens(text);
    QString cleaned;
    for (int i = 0; i < tokens.size(); ++i)
    {
        if (!(tokens[i].text == "," && i + 1 < tokens.size() && (tokens[i + 1].text == "}" || tokens[i + 1].text == "]")))
        {
            cleaned += normalizedToken(tokens[i].text, i + 1 < tokens.size() && tokens[i + 1].text == ":");
        }
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(cleaned.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        fail("现有 JSON/JSONC 配置无效，未修改文件：" + error.errorString());
    }
    return document.object();
}

AgentEnvironment AgentEnvironment::current()
{
    AgentEnvironment environment;
    environment.home = QDir::homePath();
    environment.appData = qEnvironmentVariable("APPDATA", environment.home + "/.config");
    environment.localAppData = qEnvironmentVariable("LOCALAPPDATA");
    const auto system = QProcessEnvironment::systemEnvironment();
    for (const auto &key : system.keys()) { environment.variables[key] = system.value(key); }
    environment.searchPaths = system.value("PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts);
    environment.searchPaths.append(environment.home + "/.local/bin");
    environment.searchPaths.append(environment.home + "/.cargo/bin");
    environment.searchPaths.append(environment.home + "/.hermes/venv/Scripts");
    environment.searchPaths.append(environment.appData + "/npm");
    environment.searchPaths.append(environment.localAppData + "/Microsoft/WinGet/Links");
    environment.searchPaths.append(environment.localAppData + "/Microsoft/WindowsApps");
    environment.inspectWindowsApps = true;
    return environment;
}

QString AgentEnvironment::variable(const QString &name, const QString &fallback) const
{
    const auto value = variables.value(name);
    return value.isEmpty() ? fallback : value;
}

QString AgentEnvironment::executable(const QString &name) const
{
    for (const auto &directory : searchPaths)
    {
        if (directory.isEmpty()) { continue; }
        for (const auto &suffix : QStringList {".exe", ".cmd", ".bat", ".ps1", ""})
        {
            const QFileInfo file(QDir(directory).filePath(name + suffix));
            if (file.isFile() && (suffix != "" || file.isExecutable())) { return file.absoluteFilePath(); }
        }
    }
    return {};
}

static QString desktopExecutable(const AgentEnvironment &environment, const QString &name)
{
    const QStringList directories {
        environment.localAppData + "/" + name,
        environment.localAppData + "/Programs/" + name,
        environment.variable("ProgramFiles", "") + "/" + name
    };
    for (const auto &directory : directories)
    {
        if (QFileInfo::exists(directory + "/" + name + ".exe")) { return directory + "/" + name + ".exe"; }
        for (const auto &version : QDir(directory).entryList({"app-*"}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        {
            const auto file = directory + "/" + version + "/" + name + ".exe";
            if (QFileInfo::exists(file)) { return file; }
        }
    }
#ifdef Q_OS_WIN
    if (environment.inspectWindowsApps)
    {
        // Registered AppX package roots, not leftover configuration directories.
        QSettings registry("HKEY_CURRENT_USER\\Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages", QSettings::NativeFormat);
        for (const auto &package : registry.childGroups())
        {
            if (!package.contains(name, Qt::CaseInsensitive)) { continue; }
            const auto root = registry.value(package + "/PackageRootFolder").toString();
            for (const auto &relative : QStringList {name + ".exe", "app/" + name + ".exe"})
            {
                const auto file = QDir(root).filePath(relative);
                if (!root.isEmpty() && QFileInfo::exists(file)) { return file; }
            }
        }
    }
#endif
    return {};
}

QList<AgentClient> AgentConfig::discover(const AgentEnvironment &environment)
{
    const auto home = environment.home;
    const auto openCodeBase = environment.variable("OPENCODE_CONFIG_DIR",
        environment.variable("XDG_CONFIG_HOME", home + "/.config") + "/opencode");
    const auto openCodePath = environment.variable("OPENCODE_CONFIG", openCodeBase +
        (QFileInfo::exists(openCodeBase + "/opencode.jsonc") ? "/opencode.jsonc" : "/opencode.json"));
    QList<AgentClient> agents {
        {"codex", "Codex", "codex.svg", environment.executable("codex"), environment.variable("CODEX_HOME", home + "/.codex") + "/config.toml", "toml", {"mcp_servers"}},
        {"claude-code", "Claude Code", "claude.svg", environment.executable("claude"), (environment.variables.value("CLAUDE_CONFIG_DIR").isEmpty() ? home + "/.claude.json" : environment.variables.value("CLAUDE_CONFIG_DIR") + "/.claude.json"), "json", {"mcpServers"}},
        {"opencode", "OpenCode CLI", "opencode.svg", environment.executable("opencode"), openCodePath, "json", {"mcp"}},
        {"claude-desktop", "Claude Desktop", "claude.svg", desktopExecutable(environment, "Claude"), environment.appData + "/Claude/claude_desktop_config.json", "json", {"mcpServers"}},
        {"pi", "Pi Agent", "pi.svg", environment.executable("pi"), environment.variable("PI_CODING_AGENT_DIR", home + "/.pi/agent") + "/mcp.json", "json", {"mcpServers"}},
        {"hermes", "Hermes", "hermes.svg", environment.executable("hermes"), environment.variable("HERMES_HOME", home + "/.hermes") + "/config.yaml", "yaml", {"mcp_servers"}},
        {"openclaw", "OpenClaw", "openclaw.svg", environment.executable("openclaw"), environment.variable("OPENCLAW_CONFIG_PATH", environment.variable("OPENCLAW_STATE_DIR", home + "/.openclaw") + "/openclaw.json"), "json", {"mcp", "servers"}},
        {"grok", "Grok Build", "grok.svg", environment.executable("grok"), environment.variable("GROK_HOME", home + "/.grok") + "/config.toml", "toml", {"mcp_servers"}},
        {"gemini", "Gemini CLI", "gemini.svg", environment.executable("gemini"), home + "/.gemini/settings.json", "json", {"mcpServers"}}
    };
    if (!agents[0].installed()) { agents[0].executable = desktopExecutable(environment, "Codex"); }
    if (agents[2].installed())
    {
        const auto directory = QFileInfo(agents[2].executable).absolutePath();
        for (const auto &package : QStringList {directory + "/node_modules/opencode-ai/package.json", directory + "/../package.json"})
        {
            QFile file(package);
            if (!file.open(QIODevice::ReadOnly)) { continue; }
            const auto metadata = QJsonDocument::fromJson(file.readAll()).object();
            if (metadata["name"] == "opencode-ai")
            {
                agents[2].modernOpenCode = metadata["version"].toString().section('.', 0, 0).toInt() >= 2;
                break;
            }
        }
    }
    return agents;
}

QJsonObject AgentConfig::entry(const AgentClient &agent, const QString &url, const QString &bridge)
{
    if (agent.id == "claude-desktop")
    {
        if (!QFileInfo::exists(bridge)) { fail("STDIO 桥接程序不存在：" + bridge); }
        return {{"command", bridge}, {"args", QJsonArray {"--url", url}}};
    }
    if (agent.id == "opencode") { return {{"type", "remote"}, {"url", url}, {"enabled", true}}; }
    if (agent.id == "gemini") { return {{"httpUrl", url}}; }
    if (agent.id == "openclaw") { return {{"transport", "streamable-http"}, {"url", url}, {"enabled", true}}; }
    if (agent.id == "claude-code") { return {{"type", "http"}, {"url", url}}; }
    return {{"url", url}, {"enabled", true}};
}

static QString mergeYaml(QString text, const QString &url)
{
    // Preserve the original YAML instead of reserializing tokens, secrets or
    // comments. Only conventional block mappings are edited; flow mappings,
    // anchors and ambiguous syntax fail without touching the file.
    if (text.trimmed().startsWith("{"))
    {
        parseJsonc(text);
        return setJsonMember(text, 0, {"mcp_servers", "serial-workbench"},
            QString::fromUtf8(QJsonDocument(QJsonObject {{"url", url}, {"enabled", true}}).toJson(QJsonDocument::Compact)));
    }
    const QRegularExpression header(R"((?m)^mcp_servers\s*:[^\n]*$)");
    auto matches = header.globalMatch(text);
    if (!matches.hasNext())
    {
        if (text.contains(QRegularExpression(R"((?m)^["']mcp_servers["']\s*:)"))) { fail("Hermes MCP 节点使用特殊 YAML 语法，请先改为普通块映射"); }
        return text + "\nmcp_servers:\n  serial-workbench:\n    url: \"" + url + "\"\n    enabled: true\n";
    }
    const auto match = matches.next();
    if (matches.hasNext() || !match.captured().contains(QRegularExpression(R"(^mcp_servers\s*:\s*(?:#.*)?$)")))
    {
        fail("Hermes MCP 节点需为唯一的 YAML 块映射；现有文件未修改");
    }
    int end = text.size();
    const auto boundary = QRegularExpression(R"((?m)^[^\s#][^\n]*$)").match(text, match.capturedEnd() + 1);
    if (boundary.hasMatch()) { end = boundary.capturedStart(); }
    QString block = text.mid(match.capturedEnd(), end - match.capturedEnd());
    const auto indentation = QRegularExpression(R"((?m)^( +)[^\s#][^\n]*:)" ).match(block);
    const QString indent = indentation.hasMatch() ? indentation.captured(1) : "  ";
    if (block.contains('\t') || block.contains(QRegularExpression(R"((?m)^ +<<\s*:)"))) { fail("Hermes MCP 映射包含合并键或制表符，未修改文件"); }
    const QRegularExpression owned("(?m)^" + indent + R"((?:serial-workbench|"serial-workbench"|'serial-workbench')\s*:[^\n]*$)");
    auto ownedMatches = owned.globalMatch(block);
    if (ownedMatches.hasNext())
    {
        const auto found = ownedMatches.next();
        if (ownedMatches.hasNext()) { fail("Hermes 的 serial-workbench 配置重复，未修改文件"); }
        const auto next = QRegularExpression("(?m)^ {0," + QString::number(indent.size()) + R"(}[^\s#][^\n]*$)").match(block, found.capturedEnd() + 1);
        block.remove(found.capturedStart(), (next.hasMatch() ? next.capturedStart() : block.size()) - found.capturedStart());
    }
    block += "\n" + indent + "serial-workbench:\n" + indent + "  url: \"" + url + "\"\n" + indent + "  enabled: true\n";
    text.replace(match.capturedEnd(), end - match.capturedEnd(), block);
    return text;
}

QString AgentConfig::updated(const AgentClient &agent, const QString &original, const QString &url, const QString &bridge)
{
    if (agent.format == "yaml")
    {
        try
        {
            const auto before = fkyaml::node::deserialize(original.isEmpty() ? "{}" : original.toStdString());
            if (!before.is_mapping()) { fail("Hermes 配置需要 YAML 映射"); }
            const auto changed = mergeYaml(original, url);
            auto after = fkyaml::node::deserialize(changed.toStdString());
            auto expected = before;
            expected["mcp_servers"]["serial-workbench"] = fkyaml::node {{"url", url.toStdString()}, {"enabled", true}};
            if (after != expected) { fail("YAML 编辑校验失败，未修改文件"); }
            return changed;
        }
        catch (const fkyaml::exception &exception) { fail("YAML 语法无效：" + QString::fromUtf8(exception.what())); }
    }
    if (agent.format == "toml")
    {
        toml::table before;
        try { before = toml::parse(original.toStdString()); }
        catch (const toml::parse_error &) { fail("现有 TOML 语法无效，未修改文件"); }
        QString text = original;
        const QString table = "[mcp_servers.\"serial-workbench\"]\nurl = \"" + url + "\"\nenabled = true\n";
        const QRegularExpression section(R"((?m)^\[mcp_servers\.(?:"serial-workbench"|'serial-workbench'|serial-workbench)\][^\n]*(?:\n|$)[\s\S]*?(?=^\[|\z))");
        auto matches = section.globalMatch(text);
        if (matches.hasNext())
        {
            const auto match = matches.next();
            if (matches.hasNext()) { fail("MCP 表重复，未修改 TOML"); }
            text.replace(match.capturedStart(), match.capturedLength(), table + "\n");
        }
        else { text += "\n" + table; }
        try
        {
            auto after = toml::parse(text.toStdString());
            auto mcp = before["mcp_servers"].as_table();
            if (!mcp)
            {
                before.insert("mcp_servers", toml::table {});
                mcp = before["mcp_servers"].as_table();
            }
            mcp->insert_or_assign("serial-workbench", toml::table {{"url", url.toStdString()}, {"enabled", true}});
            if (before != after) { fail("TOML 编辑校验失败，未修改文件"); }
        }
        catch (const toml::parse_error &) { fail("TOML 节点使用特殊语法，未修改文件"); }
        return text;
    }
    QString text = original.trimmed().isEmpty() ? "{}" : original;
    const auto root = parseJsonc(text);
    auto section = agent.section;
    auto value = entry(agent, url, bridge);
    if (agent.id == "opencode" && agent.modernOpenCode && !root["mcp"].toObject().isEmpty()
        && !root["mcp"].toObject().contains("servers"))
    {
        fail("检测到 OpenCode V2，但文件仍是 V1 MCP 格式，请先在 OpenCode 中迁移配置；原文件未修改");
    }
    if (agent.id == "opencode" && (agent.modernOpenCode || root["mcp"].toObject().contains("servers")))
    {
        section.append("servers");
        value.remove("enabled");
        value["disabled"] = false;
    }
    section.append("serial-workbench");
    return setJsonMember(text, 0, section, QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact)));
}

static QString readConfig(const AgentClient &agent)
{
    QFile file(agent.configPath);
    if (!file.exists()) { return agent.format == "json" ? "{}" : ""; }
    if (!file.open(QIODevice::ReadOnly)) { fail("无法读取配置：" + file.errorString()); }
    return QString::fromUtf8(file.readAll());
}

QString AgentConfig::check(const AgentClient &agent, const QString &url, const QString &bridge)
{
    if (!agent.installed()) { fail("未检测到已安装的 " + agent.name); }
    if (!QFileInfo::exists(agent.configPath)) { return "尚未配置"; }
    const auto text = readConfig(agent);
    if (agent.format != "json")
    {
        // Replacing with the same managed entry must make no semantic change.
        const auto next = updated(agent, text, url, bridge);
        if (agent.format == "toml") { return toml::parse(next.toStdString()) == toml::parse(text.toStdString()) ? "配置地址一致" : "需要更新配置"; }
        return fkyaml::node::deserialize(next.toStdString()) == fkyaml::node::deserialize(text.toStdString()) ? "配置地址一致" : "需要更新配置";
    }
    auto node = parseJsonc(text);
    for (const auto &key : agent.section) { node = node[key].toObject(); }
    const bool v2 = agent.id == "opencode" && (agent.modernOpenCode || node.contains("servers"));
    if (v2) { node = node["servers"].toObject(); }
    const auto saved = node["serial-workbench"].toObject();
    auto expected = entry(agent, url, bridge);
    if (v2) { expected.remove("enabled"); expected["disabled"] = false; }
    for (auto item = expected.begin(); item != expected.end(); ++item)
    {
        if (saved[item.key()] != item.value()) { return "需要更新配置"; }
    }
    return "配置地址一致";
}

QString AgentConfig::configure(const AgentClient &agent, const QString &url, const QString &bridge)
{
    if (!agent.installed()) { fail("未检测到已安装的 " + agent.name); }
    const auto original = readConfig(agent);
    const auto text = updated(agent, original, url, bridge);
    if (!QDir().mkpath(QFileInfo(agent.configPath).absolutePath())) { fail("无法创建配置目录"); }
    if (text != original)
    {
        if (QFileInfo::exists(agent.configPath))
        {
            const auto backup = agent.configPath + "." + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz") + "-" + QUuid::createUuid().toString(QUuid::Id128).left(8) + ".bak";
            if (!QFile::copy(agent.configPath, backup)) { fail("配置备份失败，未修改原文件"); }
        }
        QSaveFile output(agent.configPath);
        const auto bytes = text.toUtf8();
        if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) { fail("配置写入失败"); }
    }
    if (check(agent, url, bridge) != "配置地址一致") { fail("配置写入后校验失败"); }
    const QString reload = agent.id == "pi" ? "请在 Pi 中执行 /reload。" : "请重载客户端配置。";
    return "配置已保存，文件校验通过。" + reload + "\n" + agent.configPath;
}
}
