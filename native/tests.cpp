#include "window.h"
#include <QtTest>

class NativeTests : public QObject
{
    Q_OBJECT
private slots:
    void ansiResetAndTerminal()
    {
        sw::AnsiState state;
        state.sgr({32}, sw::Theme::make(false));
        QVERIFY(state.foreground.isValid());
        state.sgr({0}, sw::Theme::make(false));
        QVERIFY(!state.foreground.isValid());
        QVERIFY(!state.background.isValid());
        sw::LogView log;
        sw::Journal journal;
        log.feed(journal.append("a", "rx", "\x1b[32mGREEN\x1b[0m ordinary\r\n"));
        QVERIFY(log.toPlainText().contains("GREEN ordinary"));
        QTextCursor cursor(log.document());
        cursor.setPosition(log.toPlainText().indexOf("ordinary") + 2);
        QCOMPARE(cursor.charFormat().foreground().color(), log.theme.text);
        sw::TerminalView terminal;
        terminal.feed("hello\rworld\x1b[2J\x1b[Hdone");
        QVERIFY(terminal.text().trimmed().startsWith("done"));
        const auto bytes = QString("中文").toUtf8();
        terminal.feed(bytes.left(2));
        terminal.feed(bytes.mid(2));
        QVERIFY(terminal.text().contains("中"));
    }
    void journalCursors()
    {
        sw::Journal journal;
        journal.append("a", "RX", "first");
        journal.append("a", "RX", "second");
        QCOMPARE(journal.read(0, 1)["events"].toArray().size(), 1);
        QCOMPARE(journal.read(0)["events"].toArray().size(), 2);
        QCOMPARE(journal.read(1)["events"].toArray().size(), 1);
        QCOMPARE(journal.sequence, 2);
    }
    void serialReconfiguration()
    {
        sw::Engine engine;
        sw::Config config;
        QVERIFY_EXCEPTION_THROWN(config.validate(), std::runtime_error);
        config.port = "loop://";
        auto channel = engine.open(config);
        QCOMPARE(channel->state, "connected");
        channel->send("hello");
        QTRY_COMPARE(channel->rx, 5);
        config.baudrate = 1200;
        QCOMPARE(engine.open(config), channel);
        QCOMPARE(channel->appliedBaud, 1200);
        QCOMPARE(channel->config.baudrate, 1200);
        channel->close();
        QCOMPARE(engine.open(config), channel);
        QCOMPARE(channel->rx, 5);
        QCOMPARE(sw::Engine::encode("AA 10", true, "utf-8", "none"), QByteArray::fromHex("AA10"));
        QVERIFY_EXCEPTION_THROWN(sw::Engine::encode("GG", true, "utf-8", "none"), std::runtime_error);
    }
    void networkRoundTrip()
    {
        sw::Engine engine;
        sw::Config config;
        config.kind = "tcp-server";
        config.bindPort = 0;
        auto server = engine.open(config);
        QCOMPARE(server->state, "listening");
        config.kind = "tcp-client";
        config.networkPort = server->listener->serverPort();
        auto client = engine.open(config);
        QTRY_COMPARE(client->state, "connected");
        QTRY_COMPARE(server->peers.size(), 1);
        client->send("round trip");
        QTRY_COMPARE(server->rx, 10);
        server->send("reply", server->peers.firstKey());
        QTRY_COMPARE(client->rx, 5);
        config.kind = "udp";
        auto udp = engine.open(config);
        QUdpSocket socket;
        socket.writeDatagram("datagram", QHostAddress::LocalHost, udp->udp->localPort());
        QTRY_COMPARE(udp->rx, 8);
    }
    void fragmentedDecoders()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->receive("temp=24,");
        channel->receive("voltage=3.3\n");
        QCOMPARE(channel->waveform()["sources"].toObject()["default"].toObject()["samples"].toArray().size(), 1);
        channel->configureParser("auto");
        channel->receive("{\"name\":\"");
        const auto chinese = QString("中文").toUtf8();
        channel->receive(chinese.left(2));
        channel->receive(chinese.mid(2) + "\"}\n<root><x>1</x></root>\n");
        QCOMPARE(channel->structured()["sources"].toObject()["default"].toObject()["records"].toArray().size(), 2);
        channel->configureParser("json", "gbk");
        const auto encoded = QTextCodec::codecForName("GBK")->fromUnicode("{\"gbk\":\"中文\"}");
        channel->receive(encoded.left(9));
        channel->receive(encoded.mid(9));
        const auto record = channel->structured()["sources"].toObject()["default"].toObject()["records"].toArray().last().toObject();
        QCOMPARE(record["value"].toObject()["gbk"].toString(), "中文");
    }
    void layoutAndTabs()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        QVERIFY(mcp.start(0));
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        QTest::qWait(200);
        const int distance = window.plus->geometry().left() - window.tabs->geometry().right();
        QVERIFY2(distance >= 0 && distance < 16, qPrintable(QString::number(distance)));
        window.port->setCurrentText("COM9");
        QVERIFY(window.port->lineEdit()->width() > window.port->fontMetrics().horizontalAdvance("COM9") + 12);
        window.active->payload->setPlainText("first tab");
        auto first = window.active;
        auto second = window.newConnection();
        second->payload->setPlainText("second tab");
        window.tabs->setCurrentIndex(0);
        QCOMPARE(window.active, first);
        QCOMPARE(first->payload->toPlainText(), "first tab");
        window.tabs->moveTab(0, 1);
        QCOMPARE(window.active, first);
        QCOMPARE(window.stack->currentWidget(), first);
        QTest::mouseMove(window.gear, window.gear->rect().center());
        // Inject the event as well: headless runners may not own the desktop cursor.
        QEnterEvent enter(window.gear->rect().center(), window.gear->rect().center(), window.gear->mapToGlobal(window.gear->rect().center()));
        QApplication::sendEvent(window.gear, &enter);
        QTest::qWait(190);
        QVERIFY(window.gear->progress > 0.9);
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(window.gear, &leave);
        QTest::qWait(190);
        QVERIFY(window.gear->progress < 0.1);
        auto instrument = first->addInstrument("waveform");
        QVERIFY(instrument->plot);
        first->pages->setCurrentIndex(0);
        const auto original = first->splitter->sizes();
        first->splitter->setSizes({250, 250});
        QVERIFY(first->splitter->sizes() != original);
        window.openSettings();
        QVERIFY(window.settingsDialog->isVisible());
        window.settingsDialog->hide();
        window.close();
    }
    void uiBaudAndPersistence()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.json");
        {
            sw::Engine engine;
            sw::McpServer mcp(&engine);
            mcp.start(0);
            sw::Window window(&engine, &mcp, path);
            window.show();
            QTest::qWait(180);
            window.port->setCurrentText("loop://");
            window.connectionAction();
            QVERIFY(window.active->channel);
            QCOMPARE(window.active->channel->state, "connected");
            window.baud->setCurrentText("1200");
            window.applyBaud();
            QCOMPARE(window.active->channel->appliedBaud, 1200);
            window.active->addInstrument("waveform");
            window.active->addInstrument("structured");
            window.active->setTerminal(true);
            window.active->setTerminal(false);
            QVERIFY(window.active->findChild<QPushButton *>("logMode")->isChecked());
            window.applyTheme(true);
            engine.allowControl = true;
            window.close();
        }
        {
            sw::Engine engine;
            sw::McpServer mcp(&engine);
            mcp.start(0);
            sw::Window window(&engine, &mcp, path);
            QCOMPARE(window.port->currentText(), "NULL");
            QCOMPARE(window.baud->currentText(), "1200");
            QVERIFY(engine.allowControl);
            QVERIFY(window.theme.dark);
            QCOMPARE(window.active->instruments.size(), 2);
            QVERIFY(engine.channels.isEmpty());
        }
    }
    void mcpPermissions()
    {
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        QCOMPARE(mcp.tools().size(), 9);
        QVERIFY_EXCEPTION_THROWN(mcp.call("open_connection", {{"config", QJsonObject {{"kind", "serial"}, {"port", "loop://"}}}}), std::runtime_error);
        engine.allowControl = true;
        mcp.call("open_connection", {{"config", QJsonObject {{"kind", "serial"}, {"port", "loop://"}}}});
        QCOMPARE(engine.channels.size(), 1);
    }
};
QTEST_MAIN(NativeTests)
#include "tests.moc"
