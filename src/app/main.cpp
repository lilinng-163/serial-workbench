// SPDX-License-Identifier: Apache-2.0

#include "window.h"
#include "window_chrome.h"
#include "agent_skill.h"

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
    auto defaultSettings = QCoreApplication::applicationDirPath() + "/settings.json";
    QFile installation(QCoreApplication::applicationDirPath() + "/installation.json");
    if (installation.open(QIODevice::ReadOnly)
        && QJsonDocument::fromJson(installation.readAll()).object()["distribution"] == "installed")
    {
        defaultSettings = qEnvironmentVariable("LOCALAPPDATA", QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
            + "/SerialWorkbench/settings.json";
    }
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"mcp-port", "Local MCP port", "port", "8765"});
    parser.addOption({"settings", "Settings file", "path", defaultSettings});
    parser.addOption({"demo", "Show generated example data"});
    parser.addOption({"view", "Preview waveform, json, xml, settings, dark, search, terminal, ssh, detached, floating-wave, narrow or protocol", "name"});
    parser.addOption({"screenshot", "Save window preview", "path"});
    parser.addOption({"ready-file", "Write startup timing", "path"});
    parser.addOption({"quit-after", "Close after milliseconds", "ms"});
    parser.addOption({"install-skill", "Install the bundled skill for codex/claude-code/opencode/pi/hermes/openclaw/grok/gemini without opening the GUI", "agent"});
    parser.addOption({"skill-report", "Write the skill installation result to a JSON file", "path"});
    parser.addOption({"detect-skill-agents", "Detect installed skill-capable Agents without opening the GUI; write INI to --skill-report"});
    parser.process(app);
    if (parser.isSet("detect-skill-agents"))
    {
        if (!parser.isSet("skill-report")) { return 2; }
        try { sw::AgentSkill::writeDetection(parser.value("skill-report")); }
        catch (const std::exception &) { return 3; }
        return 0;
    }
    if (parser.isSet("install-skill"))
    {
        QJsonObject result;
        int code = 0;
        try { result = sw::AgentSkill::install(parser.value("install-skill")); }
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
    const auto instanceKey = parser.isSet("mcp-port") ? QString::number(port)
        : QString::fromLatin1(QCryptographicHash::hash(QFileInfo(parser.value("settings")).absoluteFilePath().toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
    const QString instanceName = "SerialWorkbenchNative-" + qEnvironmentVariable("USERNAME") + "-" + instanceKey;
    QLocalSocket existing;
    existing.connectToServer(instanceName);
    if (existing.waitForConnected(100))
    {
        existing.write("activate");
        existing.waitForBytesWritten(100);
        return 0;
    }
    QLocalServer instance;
    instance.listen(instanceName);
    sw::Engine engine;
    sw::McpServer mcp(&engine);
    if (parser.isSet("mcp-port")) { mcp.start(static_cast<quint16>(port)); }
    else { mcp.startAvailable(static_cast<quint16>(port)); }
    sw::Window window(&engine, &mcp, parser.value("settings"));
    if (!parser.isSet("mcp-port") && mcp.isListening()) { window.saveSettings(); }
    QPointer<QWidget> previewTarget = &window;
    QObject::connect(&instance, &QLocalServer::newConnection, &window, [&]
    {
        while (instance.hasPendingConnections())
        {
            auto socket = instance.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            socket->disconnectFromServer();
        }
        window.showNormal();
        window.checkAgentEndpoints();
        window.raise();
        window.activateWindow();
    });
    window.show();
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
        QTimer::singleShot(parser.value("quit-after").toInt(), &window, &QWidget::close);
    }
    return app.exec();
}
