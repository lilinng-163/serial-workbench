// SPDX-License-Identifier: Apache-2.0
#include "runtime.h"
#include "workbench_version.h"
#include <iostream>
#include <stdexcept>

namespace
{
void print(const QJsonObject &result)
{
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    application.setApplicationName("SerialWorkbenchCLI");
    application.setApplicationVersion(SERIAL_WORKBENCH_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Serial Workbench: shared GUI/background MCP runtime. Output is JSON.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("command", "start | status | show | stop | tools | call TOOL | mcp | install-skill AGENT | configure-agent AGENT");
    parser.addPositionalArgument("name", "Tool or Agent name", "[name]");
    parser.addOption({"settings", "Select a shared settings profile", "path", sw::defaultSettingsPath()});
    parser.addOption({"url", "Use an explicit MCP URL (never auto-start)", "url"});
    parser.addOption({"args", "MCP tool arguments as a JSON object", "json", "{}"});
    parser.addOption({"args-file", "Read MCP tool arguments from a UTF-8 JSON file", "path"});
    parser.addOption({"allow-control", "With start only: explicitly enable and save AI write permission for a new instance"});
    parser.process(application);
    const auto positional = parser.positionalArguments();
    if (positional.isEmpty()) { parser.showHelp(2); }
    const auto command = positional[0];
    const auto settings = QFileInfo(parser.value("settings")).absoluteFilePath();
    try
    {
        if (parser.isSet("allow-control") && command != "start")
        {
            throw std::runtime_error("--allow-control is only valid with start");
        }
        if (command == "install-skill" || command == "configure-agent")
        {
            if (positional.size() != 2) { throw std::runtime_error("Specify one Agent name"); }
            QTemporaryDir temporary;
            const auto report = temporary.filePath("result.json");
            QProcess child;
            child.start(sw::siblingExecutable("SerialWorkbench"),
                {"--" + command, positional[1], "--skill-report", report, "--settings", settings});
            if (!child.waitForFinished(20000)) { throw std::runtime_error("Agent operation did not finish"); }
            QFile file(report);
            if (!file.open(QIODevice::ReadOnly)) { throw std::runtime_error("Agent operation produced no report"); }
            print(QJsonDocument::fromJson(file.readAll()).object());
            return child.exitStatus() == QProcess::NormalExit ? child.exitCode() : 3;
        }
        if (command == "start")
        {
            if (parser.isSet("url")) { throw std::runtime_error("start uses --settings, not --url"); }
            print(sw::ensureInstance(settings, parser.isSet("allow-control")));
            return 0;
        }
        if (command == "status" || command == "stop" || command == "show")
        {
            if (parser.isSet("url")) { throw std::runtime_error("Lifecycle commands use --settings, not --url"); }
            if (command == "show") { sw::ensureInstance(settings); }
            const auto result = sw::instanceRequest(settings, command);
            print(result.isEmpty() ? QJsonObject {{"running", false}, {"settings", settings}} : result);
            return result.isEmpty() && command == "status" ? 3 : 0;
        }
        if (command == "mcp") { return sw::runStdio(settings, parser.value("url")); }
        if (command != "tools" && command != "call") { throw std::runtime_error("Unknown command; use --help"); }
        QJsonObject arguments;
        if (command == "call")
        {
            if (positional.size() != 2) { throw std::runtime_error("call requires exactly one tool name"); }
            if (parser.isSet("args") && parser.isSet("args-file")) { throw std::runtime_error("Choose --args or --args-file"); }
            QByteArray bytes = parser.value("args").toUtf8();
            if (parser.isSet("args-file"))
            {
                QFile file(parser.value("args-file"));
                if (!file.open(QIODevice::ReadOnly) || file.size() > 3 * 1024 * 1024)
                {
                    throw std::runtime_error("Cannot read arguments file, or file exceeds 3 MiB");
                }
                bytes = file.readAll();
            }
            const auto document = QJsonDocument::fromJson(bytes);
            if (!document.isObject()) { throw std::runtime_error("Arguments must be a JSON object"); }
            arguments = document.object();
        }
        const auto url = parser.isSet("url") ? parser.value("url") : sw::ensureInstance(settings)["mcp_url"].toString();
        sw::McpClient client(url);
        client.initialize();
        QJsonObject rpc {{"jsonrpc", "2.0"}, {"id", 2}, {"method", command == "tools" ? "tools/list" : "tools/call"}};
        if (command == "call") { rpc["params"] = QJsonObject {{"name", positional[1]}, {"arguments", arguments}}; }
        const auto response = client.request(rpc);
        client.close();
        const auto result = response.contains("error") ? response : response["result"].toObject();
        print(result);
        return response.contains("error") || result["isError"].toBool() ? 4 : 0;
    }
    catch (const std::exception &error)
    {
        if (command == "mcp")
        {
            std::cerr << error.what() << std::endl;
            return 2;
        }
        print({{"error", QString::fromUtf8(error.what())}});
        return 2;
    }
}
