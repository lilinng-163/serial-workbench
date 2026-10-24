// SPDX-License-Identifier: Apache-2.0

#include "window.h"
#include "window_chrome.h"
#include "agent_skill.h"
#include "runtime.h"

int main(int argc, char *argv[])
{
    QElapsedTimer startup;
    startup.start();
    QApplication app(argc, argv);
    app.setApplicationName("SerialWorkbench");
    app.setApplicationVersion(sw::Version);
    app.setStyle("Fusion");
    app.setFont(sw::Theme::interfaceFont());
    app.setWindowIcon(QIcon(":/brand/lightning.svg"));
    sw::installWindowChrome();
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"mcp-port", "Local MCP port", "port", "8765"});
    parser.addOption({"settings", "Settings file", "path", sw::defaultSettingsPath()});
    parser.addOption({"headless", "Run the shared runtime without showing windows"});
    parser.addOption({"allow-control", "Explicitly enable and save AI control when starting a new background instance"});
    parser.addOption({"configure-agent", "Configure an Agent for automatic background MCP startup", "agent"});
    parser.addOption({"demo", "Show generated example data"});
    parser.addOption({"view", "Preview waveform, json, xml, settings, dark, search, terminal, ssh, detached, floating-wave, narrow or protocol", "name"});
    parser.addOption({"screenshot", "Save window preview", "path"});
    parser.addOption({"ready-file", "Write startup timing", "path"});
    parser.addOption({"quit-after", "Close after milliseconds", "ms"});
    parser.addOption({"install-skill", "Install the bundled skill for codex/claude-code/opencode/pi/hermes/openclaw/grok/gemini without opening the GUI", "agent"});
    parser.addOption({"skill-report", "Write the skill installation result to a JSON file", "path"});
    parser.addOption({"detect-skill-agents", "Detect installed skill-capable Agents without opening the GUI; write INI to --skill-report"});
    parser.process(app);
    const auto settingsPath = QFileInfo(parser.value("settings")).absoluteFilePath();
    app.setProperty("swSettingsPath", settingsPath);
    app.setProperty("swBackground", parser.isSet("headless"));
    if (parser.isSet("allow-control") && !parser.isSet("headless")) { return 2; }
    if (parser.isSet("detect-skill-agents"))
    {
        if (!parser.isSet("skill-report")) { return 2; }
        try { sw::AgentSkill::writeDetection(parser.value("skill-report")); }
        catch (const std::exception &) { return 3; }
        return 0;
    }
    if (parser.isSet("install-skill") || parser.isSet("configure-agent"))
    {
        QJsonObject result;
        int code = 0;
        try
        {
            if (parser.isSet("install-skill")) { result = sw::AgentSkill::install(parser.value("install-skill")); }
            else
            {
                bool found = false;
                for (const auto &agent : sw::AgentConfig::discover())
                {
                    if (agent.id != parser.value("configure-agent")) { continue; }
                    result = {{"agent", agent.id}, {"message", sw::AgentConfig::configure(agent, QString(),
                        sw::siblingExecutable("SerialWorkbenchMCP"), settingsPath)}, {"reload_required", true}};
                    found = true;
                    break;
                }
                if (!found) { sw::fail("Unknown Agent"); }
            }
        }
        catch (const std::exception &error)
        {
            result = {{"error", QString::fromUtf8(error.what())}};
            code = 3;
        }
        if (parser.isSet("skill-report"))
        {
            QSaveFile file(parser.value("skill-report"));
            const auto bytes = QJsonDocument(result).toJson();
            if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) { return 4; }
        }
        return code;
    }
    bool validPort = false;
    auto port = parser.value("mcp-port").toUInt(&validPort);
    if (!parser.isSet("mcp-port"))
    {
        QFile settings(parser.value("settings"));
        if (settings.open(QIODevice::ReadOnly))
        {
            const auto value = QJsonDocument::fromJson(settings.readAll()).object()["mcp_port"].toInt(8765);
            if (value > 0 && value <= 65535) { port = static_cast<uint>(value); }
        }
    }
    if (!validPort || port > 65535)
    {
        return 2;
    }
    // The desktop, CLI and STDIO clients share one runtime per settings file.
    try
    {
        if (!sw::instanceRequest(settingsPath, parser.isSet("headless") ? "status" : "show").isEmpty()) { return 0; }
    }
    catch (const std::exception &) { return 3; }
    const auto instanceName = sw::instanceName(settingsPath);
    QLockFile startupLock(QDir::temp().filePath(instanceName + ".lock"));
    if (!startupLock.tryLock(10000)) { return 3; }
    try
    {
        if (!sw::instanceRequest(settingsPath, parser.isSet("headless") ? "status" : "show").isEmpty()) { return 0; }
    }
    catch (const std::exception &) { return 3; }
    QLocalServer instance;
    instance.setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(instanceName);
    if (!instance.listen(instanceName)) { return 3; }
    sw::Engine engine;
    sw::McpServer mcp(&engine);
    if (parser.isSet("mcp-port")) { mcp.start(static_cast<quint16>(port)); }
    else { mcp.startAvailable(static_cast<quint16>(port)); }
    if (!mcp.isListening()) { return 4; }
    sw::Window window(&engine, &mcp, settingsPath);
    if (parser.isSet("allow-control"))
    {
        engine.allowControl = true;
        window.saveSettings();
    }
    if (!parser.isSet("mcp-port") && mcp.isListening()) { window.saveSettings(); }
    QPointer<QWidget> previewTarget = &window;
    QObject::connect(&instance, &QLocalServer::newConnection, &window, [&]
    {
        while (instance.hasPendingConnections())
        {
            auto socket = instance.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            socket->setReadBufferSize(4096);
            auto buffer = std::make_shared<QByteArray>();
            const auto handle = [&, socket, buffer]
            {
                *buffer += socket->readAll();
                if (!buffer->contains('\n'))
                {
                    if (buffer->size() >= 4096) { socket->disconnectFromServer(); }
                    return;
                }
                const auto command = QJsonDocument::fromJson(*buffer).object()["command"].toString();
                QJsonObject result {{"application", "SerialWorkbench"}, {"version", sw::Version},
                    {"running", true}, {"pid", QCoreApplication::applicationPid()},
                    {"mcp_url", mcp.url()}, {"settings", settingsPath},
                    {"allow_ai_control", engine.allowControl}, {"visible", window.isVisible()}};
                if (command == "show")
                {
                    app.setProperty("swBackground", false);
                    app.setQuitOnLastWindowClosed(true);
                    window.showNormal();
                    window.checkAgentEndpoints();
                    window.raise();
                    window.activateWindow();
                    result["visible"] = true;
                }
                else if (command == "stop")
                {
                    result["stopping"] = true;
                    QTimer::singleShot(100, &window, [&] { window.close(); app.quit(); });
                }
                else if (command != "status") { result["error"] = "Unknown instance command"; }
                socket->write(QJsonDocument(result).toJson(QJsonDocument::Compact) + '\n');
                socket->disconnectFromServer();
            };
            QObject::connect(socket, &QLocalSocket::readyRead, socket, handle);
            QTimer::singleShot(5000, socket, &QLocalSocket::disconnectFromServer);
            if (socket->bytesAvailable()) { handle(); }
        }
    });
    if (parser.isSet("headless")) { app.setQuitOnLastWindowClosed(false); }
    else { window.show(); }
    startupLock.unlock();
    QTimer::singleShot(0, &window, &sw::Window::checkAgentEndpoints);
    if (parser.isSet("demo"))
    {
        sw::Config config;
        config.kind = "demo";
        config.name = "示例数据";
        const auto channel = engine.open(config);
        for (auto pane : window.panes)
        {
            if (pane->channel == channel)
            {
                window.tabs->setCurrentIndex(window.stack->indexOf(pane));
            }
        }
        for (const auto &kind : QStringList {"waveform", "json", "xml"})
        {
            window.newInstrument(kind, channel, false);
        }
        channel->receive("\x1b[32m[INFO]\x1b[0m 设备已就绪 · 示例数据\r\n");
        channel->receive("\x1b[36m[RX]\x1b[0m temperature=24.8,voltage=3.3,current=0.12\r\n");
    }
    QTimer::singleShot(50, &window, [&]
    {
        if (!parser.value("ready-file").isEmpty())
        {
            QFile file(parser.value("ready-file"));
            if (file.open(QIODevice::WriteOnly))
            {
                file.write(QJsonDocument(QJsonObject {{"elapsed_ms", startup.elapsed()}, {"pid", QCoreApplication::applicationPid()}, {"version", sw::Version}}).toJson());
            }
        }
        const auto requestedView = parser.value("view");
        const auto view = requestedView == "structured" ? QString("json")
            : requestedView == "values" ? QString("waveform") : requestedView;
        if (view == "agents")
        {
            window.openSettings();
            window.settingsDialog->findChild<sw::DetachableTabs *>("settingsPages")->setCurrentIndex(1);
        }
        else if (view == "settings")
        {
            window.openSettings();
        }
        else if (view == "dark")
        {
            window.applyTheme(true);
        }
        else if (view == "search")
        {
            window.active->filter->setText("temperature");
        }
        else if (view == "terminal")
        {
            window.active->setTerminal(true);
            window.active->terminal->blockCursor = false;
            window.active->cursorShape->setChecked(false);
        }
        else if (view == "ssh")
        {
            window.protocol->setCurrentIndex(4);
        }
        else if (view == "narrow")
        {
            window.resize(480, 640);
        }
        else if (view == "protocol")
        {
            window.protocol->showPopup();
        }
        else if (view == "detached")
        {
            previewTarget = window.detachConnection(window.tabs->currentIndex(), QPoint(160, 120));
        }
        else
        {
            for (auto page : window.panes)
            {
                if (page->instrument && page->instrument->kind == (view == "floating-wave" ? "waveform" : view))
                {
                    if (requestedView == "values") { page->instrument->valuesToggle->setChecked(true); }
                    for (int i = 0; i < window.tabs->count(); ++i)
                    {
                        if (window.tabs->tabData(i).value<QObject *>() == page)
                        {
                            window.tabs->setCurrentIndex(i);
                            if (view == "floating-wave")
                            {
                                previewTarget = window.detachConnection(i, QPoint(250, 180));
                            }
                            break;
                        }
                    }
                    break;
                }
            }
        }
    });
    if (parser.isSet("screenshot"))
    {
        QTimer::singleShot(1600, &window, [&]
        {
            QWidget *target = window.settingsDialog && window.settingsDialog->isVisible() ? static_cast<QWidget *>(window.settingsDialog) : previewTarget.data();
            if (target)
            {
                auto shot = target->grab();
                if (auto popup = QApplication::activePopupWidget())
                {
                    QPainter painter(&shot);
                    painter.drawPixmap(target->mapFromGlobal(popup->mapToGlobal(QPoint())), popup->grab());
                }
                shot.save(parser.value("screenshot"));
            }
        });
    }
    if (parser.isSet("quit-after"))
    {
        QTimer::singleShot(parser.value("quit-after").toInt(), &window, [&] { window.close(); app.quit(); });
    }
    return app.exec();
}
