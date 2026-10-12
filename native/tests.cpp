#include "window.h"
#include <QtTest>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

class NativeTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QApplication::setFont(sw::Theme::interfaceFont());
    }

    void pixelFontAndMigration()
    {
        const QFont font = sw::Theme::defaultDataFont();
        const auto raw = QRawFont::fromFont(font);
        QVERIFY(raw.isValid());
        QVERIFY(raw.supportsCharacter(0x4e2d));
        QVERIFY(raw.supportsCharacter(0x7aef));
        const auto interfaceRaw = QRawFont::fromFont(sw::Theme::interfaceFont());
        for (const QChar character : QString("连接参数端口波特率数据校验停止位流控设置发送搜索"))
        {
            QVERIFY2(interfaceRaw.supportsCharacter(character.unicode()), qPrintable(QString(character)));
        }
        const QFontMetrics metrics(font);
        QCOMPARE(metrics.horizontalAdvance("iiii"), metrics.horizontalAdvance("WWWW"));
        QCOMPARE(metrics.horizontalAdvance("0000"), metrics.horizontalAdvance("WWWW"));

        QTemporaryDir directory;
        const QString path = directory.filePath("settings.json");
        const auto writeAppearance = [&](const QString &family, int size)
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QJsonDocument(QJsonObject {{"appearance", QJsonObject {
                {"font_family", family}, {"font_size", size}
            }}}).toJson());
        };
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        writeAppearance("Cascadia Mono", 11);
        {
            sw::Window window(&engine, &mcp, path);
            QCOMPARE(window.dataFont.family(), font.family());
            QCOMPARE(window.dataFont.pointSize(), font.pointSize());
            window.close();
        }
        writeAppearance("Consolas", 14);
        {
            sw::Window window(&engine, &mcp, path);
            QCOMPARE(window.dataFont.family(), QString("Consolas"));
            QCOMPARE(window.dataFont.pointSize(), 14);
            window.close();
        }
    }

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
            QVERIFY(!window.active->modeButton->isChecked());
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
    void toolbarAndPortUpdates()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        QTest::qWait(200);
        QVERIFY(window.bits->isVisible());
        QVERIFY(window.flow->isVisible());
        QVERIFY(window.dtr->isVisible());
        auto pane = window.active;
        QTest::mouseClick(pane->timestamps, Qt::LeftButton);
        QVERIFY(!pane->log->timestamps);
        QTest::mouseClick(pane->hexDisplay, Qt::LeftButton);
        QVERIFY(pane->log->hex);
        auto search = qobject_cast<sw::SearchBox *>(pane->filter);
        QVERIFY(search);
        search->setText("device");
        QVERIFY(search->clearButton->isVisible());
        QVERIFY(qAbs(search->clearButton->geometry().center().y() - search->rect().center().y()) <= 1);
        QTest::mouseClick(search->clearButton, Qt::LeftButton);
        QVERIFY(search->text().isEmpty());
        QVERIFY(pane->log->filter.isEmpty());
        QTest::mouseClick(pane->modeButton, Qt::LeftButton);
        QVERIFY(pane->terminalMode);
        QVERIFY(pane->sender->isHidden());
        QSignalSpy transmitted(pane->terminal, &sw::TerminalView::transmit);
        QTest::keyClick(pane->terminal, Qt::Key_Insert);
        QVERIFY(!pane->terminal->blockCursor);
        QVERIFY(!pane->cursorShape->isChecked());
        QCOMPARE(transmitted.count(), 0);
        QTest::mouseClick(pane->cursorShape, Qt::LeftButton);
        QVERIFY(pane->terminal->blockCursor);
        QTest::mouseClick(pane->modeButton, Qt::LeftButton);
        QVERIFY(!pane->terminalMode);
        QVERIFY(pane->sender->isVisible());
        if (QGuiApplication::platformName() == "windows")
        {
            // CJK fonts contain half-width Latin and full-width Han glyphs.
            // Validate the actual cell advances rather than font metadata.
            const QFontMetrics terminalMetrics(pane->terminal->font());
            const int cell = terminalMetrics.horizontalAdvance('M');
            for (int character = 32; character < 127; ++character)
            {
                QCOMPARE(terminalMetrics.horizontalAdvance(QChar(character)), cell);
            }
            QCOMPARE(terminalMetrics.horizontalAdvance(QChar(0x4e2d)), cell * 2);
        }
        QVERIFY(pane->newline->itemText(3).contains("\\r\\n"));
        QVERIFY(pane->newline->itemText(3).contains("CRLF"));

        QJsonArray available {QJsonObject {{"port", "COM44"}, {"description", "Test device"}}};
        window.portProvider = [&available] { return available; };
        window.refreshPorts();
        QVERIFY(window.port->findText("COM44") >= 0);
        window.port->setCurrentText("COM44");
        available = {QJsonObject {{"port", "COM45"}}};
        window.port->lineEdit()->setSelection(1, 3);
        window.portTimer.setInterval(30);
        QTRY_VERIFY(window.port->findText("COM45") >= 0);
        QCOMPARE(window.port->currentText(), "COM44");
        QCOMPARE(window.port->lineEdit()->selectedText(), "OM4");
        QVERIFY(window.port->findText("COM44") < 0);
        window.port->setCurrentText("loop://");
        available = {};
        window.refreshPorts();
        QCOMPARE(window.port->currentText(), "loop://");
        window.connectionAction();
        QVERIFY(window.active->channel);
        auto channel = window.active->channel;
        window.bits->setCurrentText("7");
        emit window.bits->activated(window.bits->currentIndex());
        QCOMPARE(channel->config.bytesize, 7);
        window.dtr->setChecked(!window.dtr->isChecked());
        QCOMPARE(channel->config.dtr, window.dtr->isChecked());
        pane->payload->setPlainText("line");
        pane->newline->setCurrentIndex(3);
        pane->sendCurrent();
        QTRY_COMPARE(channel->rx, 6);
        QCOMPARE(QByteArray::fromHex(engine.journal.read(0)["events"].toArray().last().toObject()["hex"].toString().toLatin1()), QByteArray("line\r\n"));
        window.close();
    }

    void detachableConnectionsAndPages()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        QTest::qWait(180);
        window.port->setCurrentText("loop://");
        window.connectionAction();
        auto pane = window.active;
        auto channel = pane->channel;
        const auto id = channel->id;
        pane->payload->setPlainText("preserved draft");
        auto wave = pane->addInstrument("waveform");
        auto structured = pane->addInstrument("structured");
        auto detached = window.detachConnection(window.tabs->currentIndex(), QPoint(180, 160));
        QVERIFY(detached);
        QTest::qWait(100);
        QCOMPARE(detached->active, pane);
        QCOMPARE(pane->window, detached);
        QCOMPARE(channel->id, id);
        QCOMPARE(engine.channels.size(), 1);
        window.syncConnections();
        QCOMPARE(window.panes.size(), 1);
        QVERIFY(!window.active->channel);
        QCOMPARE(window.findPane(channel), pane);
        auto waveWindow = pane->pages->detachPage(pane->pages->indexOf(wave), QPoint(250, 200));
        auto parseWindow = pane->pages->detachPage(pane->pages->indexOf(structured), QPoint(300, 220));
        auto monitorWindow = pane->pages->detachPage(pane->pages->indexOf(pane->monitorPage), QPoint(350, 240));
        QVERIFY(waveWindow && parseWindow && monitorWindow);
        detached->newConnection();
        QVERIFY(pane->isHidden());
        QVERIFY2(wave->isVisible(), "Detached waveform must stay visible when its source tab is hidden");
        QVERIFY(parseWindow->isVisible());
        QVERIFY(monitorWindow->isVisible());
        channel->receive("temp=23,voltage=3.3\n{\"answer\":42}\n");
        QVERIFY(!channel->waveform()["sources"].toObject().isEmpty());
        QTRY_VERIFY(!wave->plot->series.isEmpty());
        QTRY_VERIFY(structured->table->topLevelItemCount() > 0);
        QTRY_VERIFY(pane->log->toPlainText().contains("answer"));
        QCOMPARE(pane->payload->toPlainText(), "preserved draft");
        window.saveSettings();
        QVERIFY(window.viewSettings[channel->config.key()].toObject()["instruments"].toArray().size() == 2);
#ifdef Q_OS_WIN
        if (QGuiApplication::platformName() == "windows")
        {
            for (auto host : QList<QWidget *> {detached, waveWindow, parseWindow, monitorWindow})
            {
                const auto style = GetWindowLongPtr(reinterpret_cast<HWND>(host->winId()), GWL_STYLE);
                QVERIFY(style & WS_MAXIMIZEBOX);
                QVERIFY(style & WS_THICKFRAME);
                QVERIFY(style & WS_CAPTION);
            }
        }
#endif
        monitorWindow->close();
        QVERIFY(pane->pages->indexOf(pane->monitorPage) >= 0);
        QVERIFY(!pane->pages->tabBar()->tabButton(pane->pages->indexOf(pane->monitorPage), QTabBar::RightSide));
        waveWindow->close();
        QVERIFY(pane->pages->tabBar()->tabButton(pane->pages->indexOf(wave), QTabBar::RightSide));
        pane->pages->tabBar()->moveTab(pane->pages->indexOf(wave), 0);
        pane->closeInstrument(0);
        QVERIFY(!pane->instruments.contains(wave));
        detached->close();
        QCOMPARE(pane->window, &window);
        QCOMPARE(channel->state, "connected");
        QCOMPARE(engine.channels.size(), 1);
        parseWindow->close();
        window.resize(480, 640);
        QTest::qWait(100);
        QCOMPARE(window.width(), 480);
        QVERIFY(window.stack->width() <= 426);
        const int gap = window.plus->geometry().left() - window.tabs->geometry().right();
        QVERIFY(gap >= 0 && gap < 16);
        QVERIFY(window.tabs->rect().contains(window.tabs->tabRect(window.tabs->currentIndex()).center()));
        QVERIFY(pane->sendButton->mapTo(pane->monitorPage, QPoint()).x() + pane->sendButton->width() <= pane->monitorPage->width());
        QVERIFY(pane->sendButton->parentWidget()->rect().contains(pane->sendButton->geometry()));
        window.close();
        QCOMPARE(channel->state, "closed");
    }

    void detachedDraftAndShutdown()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.port->setCurrentText("COM77");
        window.baud->setCurrentText("230400");
        auto draft = window.active;
        draft->payload->setPlainText("unsent content");
        auto detached = window.detachConnection(0, QPoint(200, 180));
        QCOMPARE(detached->port->currentText(), "COM77");
        QCOMPARE(detached->baud->currentText(), "230400");
        QCOMPARE(detached->active->payload->toPlainText(), "unsent content");
        detached->port->setCurrentText("loop://");
        detached->connectionAction();
        QCOMPARE(engine.channels.size(), 1);
        QCOMPARE(draft->window, detached);
        QCOMPARE(detached->panes.size(), 1);
        QVERIFY(!window.active->channel);
        auto waveform = draft->addInstrument("waveform");
        auto waveWindow = draft->pages->detachPage(draft->pages->indexOf(waveform), QPoint(250, 180));
        QPointer<sw::FloatingPage> guarded(waveWindow);
        window.close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(guarded.isNull());
        QFile file(directory.filePath("settings.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto saved = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(saved["views"].toObject()["serial:loop://"].toObject()["instruments"].toArray().size(), 1);
    }

    void tearGestureAndSettings()
    {
        sw::DetachableTabs tabs;
        tabs.resize(700, 500);
        auto first = new QWidget();
        auto second = new QWidget();
        tabs.addTab(first, "First");
        tabs.addTab(second, "Second");
        tabs.show();
        auto bar = qobject_cast<sw::TearTabBar *>(tabs.tabBar());
        QVERIFY(bar);
        QSignalSpy requested(bar, &sw::TearTabBar::tearRequested);
        // Isolate the gesture from the native OS move loop in an automated test.
        disconnect(bar, &sw::TearTabBar::tearRequested, &tabs, nullptr);
        const QPoint start = bar->tabRect(1).center();
        QTest::mousePress(bar, Qt::LeftButton, Qt::NoModifier, start);
        const QPoint end = start + QPoint(0, 90);
        QMouseEvent move(QEvent::MouseMove, QPointF(end), QPointF(bar->mapToGlobal(end)), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(bar, &move);
        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested.first()[0].toInt(), 1);
        QTest::mouseRelease(bar, Qt::LeftButton, Qt::NoModifier, end);
        tabs.tabBar()->moveTab(1, 0);
        auto floating = tabs.detachPage(0, QPoint(200, 200));
        QCOMPARE(floating->centralWidget(), second);
        floating->close();
        QCOMPARE(tabs.widget(0), second);
        tabs.close();

        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.openSettings();
        const auto allTabs = window.settingsDialog->findChildren<sw::DetachableTabs *>();
        QCOMPARE(allTabs.size(), 2);
        for (auto settingsTabs : allTabs)
        {
            auto page = settingsTabs->widget(0);
            auto floatingSettings = settingsTabs->detachPage(0, QPoint(180, 180));
            QCOMPARE(floatingSettings->centralWidget(), page);
            window.applyTheme(!window.theme.dark);
            QCOMPARE(floatingSettings->palette().color(QPalette::Window), window.theme.background);
            QCOMPARE(floatingSettings->styleSheet(), window.styleSheet());
            floatingSettings->close();
            QCOMPARE(settingsTabs->widget(0), page);
        }
        window.close();
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
