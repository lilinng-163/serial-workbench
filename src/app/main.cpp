#include "window.h"

int main(int argc, char *argv[])
{
    QElapsedTimer startup;
    startup.start();
    QApplication app(argc, argv);
    app.setApplicationName("SerialWorkbench");
    app.setApplicationVersion(sw::Version);
    app.setStyle("Fusion");
    app.setFont(sw::Theme::interfaceFont());
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"mcp-port", "Local MCP port", "port", "8765"});
    parser.addOption({"settings", "Settings file", "path", QCoreApplication::applicationDirPath() + "/settings.json"});
    parser.addOption({"demo", "Show generated example data"});
    parser.addOption({"view", "Preview waveform, values, structured, settings, dark, search, terminal, detached, floating-wave, narrow or protocol", "name"});
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
        const auto view = parser.value("view");
        if (view == "settings")
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
