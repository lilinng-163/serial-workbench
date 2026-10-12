#include "window.h"

int main(int argc, char *argv[])
{
    QElapsedTimer startup;
    startup.start();
    QApplication app(argc, argv);
    app.setApplicationName("SerialWorkbench");
    app.setApplicationVersion(sw::Version);
    app.setStyle("Fusion");
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"mcp-port", "Local MCP port", "port", "8765"});
    parser.addOption({"settings", "Settings file", "path", QCoreApplication::applicationDirPath() + "/settings.json"});
    parser.addOption({"demo", "Show generated example data"});
    parser.addOption({"view", "Preview waveform, values, structured, settings or dark", "name"});
    parser.addOption({"screenshot", "Save window preview", "path"});
    parser.addOption({"ready-file", "Write startup timing", "path"});
    parser.addOption({"quit-after", "Close after milliseconds", "ms"});
    parser.process(app);
    bool validPort = false;
    const auto port = parser.value("mcp-port").toUInt(&validPort);
    if (!validPort || port > 65535)
    {
        return 2;
    }
    const QString instanceName = "SerialWorkbenchNative-" + qEnvironmentVariable("USERNAME") + "-" + QString::number(port);
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
    mcp.start(static_cast<quint16>(port));
    sw::Window window(&engine, &mcp, parser.value("settings"));
    QObject::connect(&instance, &QLocalServer::newConnection, &window, [&]
    {
        while (instance.hasPendingConnections())
        {
            auto socket = instance.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            socket->disconnectFromServer();
        }
        window.showNormal();
        window.raise();
        window.activateWindow();
    });
    window.show();
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
        for (const auto &kind : QStringList {"waveform", "values", "structured"})
        {
            if (std::none_of(window.active->instruments.cbegin(), window.active->instruments.cend(), [&](auto instrument) { return instrument->kind == kind; }))
            {
                window.active->addInstrument(kind, false);
            }
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
        const auto view = parser.value("view");
        if (view == "settings")
        {
            window.openSettings();
        }
        else if (view == "dark")
        {
            window.applyTheme(true);
        }
        else
        {
            for (auto instrument : window.active->instruments)
            {
                if (instrument->kind == view)
                {
                    window.active->pages->setCurrentWidget(instrument);
                }
            }
        }
    });
    if (parser.isSet("screenshot"))
    {
        QTimer::singleShot(1600, &window, [&]
        {
            QWidget *target = window.settingsDialog && window.settingsDialog->isVisible() ? static_cast<QWidget *>(window.settingsDialog) : &window;
            target->grab().save(parser.value("screenshot"));
        });
    }
    if (parser.isSet("quit-after"))
    {
        QTimer::singleShot(parser.value("quit-after").toInt(), &window, &QWidget::close);
    }
    return app.exec();
}
