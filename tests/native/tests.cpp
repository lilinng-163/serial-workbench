// SPDX-License-Identifier: Apache-2.0

#include "window.h"
#include "ssh_profiles.h"
#include <QtTest>
#include <algorithm>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

static int pageIndex(sw::Window *window, sw::Pane *page)
{
    for (int i = 0; i < window->tabs->count(); ++i)
    {
        if (window->tabs->tabData(i).value<QObject *>() == page) { return i; }
    }
    return -1;
}

class NativeTests : public QObject
{
    Q_OBJECT
private slots:
    void waveformDragMatchesScreen_data()
    {
        QTest::addColumn<int>("plotWidth");
        QTest::addColumn<qreal>("range");
        QTest::addColumn<qreal>("scale");

        QTest::newRow("compact-short-history") << 212 << 100.0 << 4.0;
        QTest::newRow("wide-short-history") << 912 << 100.0 << 4.0;
        QTest::newRow("wide-long-history") << 912 << 5000.0 << 4.0;
        QTest::newRow("wide-high-zoom") << 912 << 5000.0 << 20.0;
    }

    void waveformDragMatchesScreen()
    {
        QFETCH(int, plotWidth);
        QFETCH(qreal, range);
        QFETCH(qreal, scale);

        sw::WavePlot plot;
        plot.resize(plotWidth + 88, 360);
        plot.series["signal"] = {{0, 0}, {range, 1}};
        plot.zoom = scale;

        const auto mouse = [&plot](QEvent::Type type, const QPointF &position,
                                  Qt::MouseButton button, Qt::MouseButtons buttons)
        {
            QMouseEvent event(type, position, plot.mapToGlobal(position.toPoint()),
                              button, buttons, Qt::NoModifier);
            QApplication::sendEvent(&plot, &event);
        };
        const auto drag = [&mouse](const QPointF &start, const QPointF &end,
                                  Qt::MouseButton button = Qt::LeftButton)
        {
            mouse(QEvent::MouseButtonPress, start, button, button);
            mouse(QEvent::MouseMove, end, Qt::NoButton, button);
            mouse(QEvent::MouseButtonRelease, end, button, Qt::NoButton);
        };
        const QPointF start(100, 100);

        drag(start, start + QPointF(10.25, 0));
        const qreal displacement = -plot.offset / (range / scale) * plotWidth;
        QVERIFY2(qAbs(displacement - 10.25) < 0.000001,
                 "The curve must move the same number of screen pixels as the mouse.");
        QCOMPARE(plot.cursor().shape(), Qt::ArrowCursor);

        const qreal once = plot.offset;
        plot.offset = 0;
        mouse(QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        for (int step = 1; step <= 5; ++step)
        {
            mouse(QEvent::MouseMove, start + QPointF(step * 2.05, 0), Qt::NoButton, Qt::LeftButton);
        }
        mouse(QEvent::MouseButtonRelease, start + QPointF(10.25, 0), Qt::LeftButton, Qt::NoButton);
        QVERIFY(qAbs(plot.offset - once) < 0.000001);

        mouse(QEvent::MouseMove, start + QPointF(40, 0), Qt::NoButton, Qt::NoButton);
        drag(QPointF(20, 100), QPointF(30, 100));
        drag(start, start + QPointF(10, 0), Qt::RightButton);
        QVERIFY(qAbs(plot.offset - once) < 0.000001);

        drag(start, start + QPointF(plotWidth * scale, 0));
        QVERIFY(qAbs(plot.offset - (range / scale - range)) < 0.000001);
        drag(start, start - QPointF(plotWidth * scale, 0));
        QCOMPARE(plot.offset, 0.0);

        drag(start, start + QPointF(plotWidth * scale, 0));
        for (int step = 0; step < 30; ++step)
        {
            QWheelEvent wheel(start, plot.mapToGlobal(start.toPoint()), QPoint(), QPoint(0, -120),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(&plot, &wheel);
        }
        QCOMPARE(plot.zoom, 1.0);
        QCOMPARE(plot.offset, 0.0);
        drag(start, start + QPointF(10, 0));
        QCOMPARE(plot.offset, 0.0);

        plot.zoom = scale;
        plot.offset = -range / 2;
        mouse(QEvent::MouseButtonDblClick, start, Qt::LeftButton, Qt::LeftButton);
        QCOMPARE(plot.zoom, 1.0);
        QCOMPARE(plot.offset, 0.0);
    }

    void sshProfilesAndSession()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QDir().mkpath(directory.filePath("includes"));

        QFile included(directory.filePath("includes/hosts.conf"));
        QVERIFY(included.open(QIODevice::WriteOnly));
        included.write("Host lab-server lab-backup\n  HostName 192.0.2.10\nInclude ../config\n");
        included.close();

        QFile configFile(directory.filePath("config"));
        QVERIFY(configFile.open(QIODevice::WriteOnly));
        configFile.write("# local configuration\nHost local-host *.example !blocked\n  User tester\nInclude includes/*.conf\n");
        configFile.close();

        QCOMPARE(sw::SshProfiles::aliases(configFile.fileName()),
            QStringList({"local-host", "lab-server", "lab-backup"}));

        sw::Config config;
        config.kind = "ssh";
        config.sshHost = "lab-server";
        config.sshIdentity = directory.filePath("test key");
        QCOMPARE(sw::Config::fromJson(config.json()).key(), config.key());
        config.validate();
        config.sshHost = "-oProxyCommand=evil";
        QVERIFY_EXCEPTION_THROWN(config.validate(), std::runtime_error);
        config.sshHost = "lab-server";

        sw::Engine engine;
        engine.sshExecutable = QCoreApplication::applicationDirPath() + "/fake_ssh.exe";
        QVERIFY(QFile::exists(engine.sshExecutable));
        auto channel = engine.open(config);
        QTRY_COMPARE(channel->state, QString("connected"));
        QTRY_VERIFY(channel->rx >= 6);

        const QByteArray input("private-input\n");
        const auto sent = channel->send(input);
        QCOMPARE(sent["bytes_sent"].toInt(), input.size());
        QTRY_VERIFY(channel->rx >= 6 + input.size());
        const auto events = engine.journal.read(0)["events"].toArray();
        bool received = false;
        for (const auto &value : events)
        {
            const auto event = value.toObject();
            QVERIFY(event["direction"].toString() != "tx");
            received |= event["text"].toString().contains("private-input");
        }
        QVERIFY(received);
        channel->close();
        QCOMPARE(channel->state, QString("closed"));

        sw::McpServer mcp(&engine);
        QCOMPARE(mcp.call("get_status", {}).toObject()["counts_by_protocol"].toObject()["ssh"].toInt(), 0);

        sw::Engine uiEngine;
        uiEngine.sshExecutable = engine.sshExecutable;
        sw::McpServer uiMcp(&uiEngine);
        sw::Window window(&uiEngine, &uiMcp, directory.filePath("settings.json"));
        window.show();
        window.protocol->setCurrentIndex(4);
        QCOMPARE(window.configStack->currentIndex(), 2);
        window.sshTarget->setCurrentText("lab-server");
        window.sshIdentity->setText(directory.filePath("test key"));
        QCOMPARE(window.form().sshHost, QString("lab-server"));
        window.connectionAction();
        QTRY_COMPARE(window.active->channel->state, QString("connected"));
        QVERIFY(window.active->terminalMode);
        QCOMPARE(window.active->channel->config.sshIdentity, directory.filePath("test key"));
        window.active->tick();
        auto terminal = window.active->terminal;
        terminal->setFocus();
        QTRY_VERIFY(terminal->hasFocus());
        QSignalSpy transmitted(terminal, &sw::TerminalView::transmit);
        const auto receivedBeforeTab = window.active->channel->rx;
        QTest::keyClick(terminal, Qt::Key_Tab);
        QCOMPARE(transmitted.count(), 1);
        QCOMPARE(transmitted.first().first().toByteArray(), QByteArray("\t"));
        QVERIFY(terminal->hasFocus());
        QTRY_COMPARE(window.active->channel->rx, receivedBeforeTab + 1);
        QCOMPARE(window.active->channel->tx, 1);
        QTest::keyClick(terminal, Qt::Key_Tab, Qt::ShiftModifier);
        QCOMPARE(transmitted.count(), 2);
        QCOMPARE(transmitted.last().first().toByteArray(), QByteArray("\x1b[Z"));
        QVERIFY(terminal->hasFocus());
        QTest::keyClick(terminal, Qt::Key_Home, Qt::KeypadModifier);
        QCOMPARE(transmitted.count(), 3);
        QCOMPARE(transmitted.last().first().toByteArray(), QByteArray("\x1b[7~"));
        QTest::keyClick(terminal, Qt::Key_End, Qt::KeypadModifier);
        QCOMPARE(transmitted.count(), 4);
        QCOMPARE(transmitted.last().first().toByteArray(), QByteArray("\x1b[4~"));
        QVERIFY(terminal->inputMethodQuery(Qt::ImEnabled).toBool());
        const QRectF inputRectangle = terminal->inputMethodQuery(Qt::ImCursorRectangle).toRectF();
        QVERIFY(terminal->rect().contains(inputRectangle.toRect()));
        QInputMethodEvent preedit("ni", {});
        QApplication::sendEvent(terminal, &preedit);
        QCOMPARE(terminal->preeditText, QString("ni"));
        QInputMethodEvent commit;
        commit.setCommitString("你");
        QApplication::sendEvent(terminal, &commit);
        QCOMPARE(transmitted.count(), 5);
        QCOMPARE(transmitted.last().first().toByteArray(), QString("你").toUtf8());
        terminal->enabledInput = false;
        QTest::keyClick(terminal, Qt::Key_Tab);
        QCOMPARE(transmitted.count(), 5);
        QVERIFY(!terminal->hasFocus());
        window.close();
    }

    void initTestCase()
    {
        QApplication::setStyle("Fusion");
        QApplication::setFont(sw::Theme::interfaceFont());
    }

    void gearRotationAndToolbarHints()
    {
        sw::IconButton gear("gear", "设置");
        QSignalSpy turns(&gear.rotationAnimation, &QVariantAnimation::finished);
        QEnterEvent enter(gear.rect().center(), gear.rect().center(), gear.rect().center());
        QEvent leave(QEvent::Leave);

        QApplication::sendEvent(&gear, &enter);
        QTest::qWait(190);
        QVERIFY(gear.progress > 0.9);
        QVERIFY(gear.rotation > 0 && gear.rotation < 360);
        QTRY_COMPARE(turns.count(), 1);
        QCOMPARE(gear.rotationAnimation.endValue().toReal(), 360.0);
        QCOMPARE(gear.rotation, 0.0);

        QApplication::sendEvent(&gear, &leave);
        QTest::qWait(190);
        QVERIFY(gear.progress < 0.1);
        QVERIFY(gear.rotation > 0 && gear.rotation < 360);
        QTRY_COMPARE(turns.count(), 2);
        QCOMPARE(gear.rotation, 0.0);

        QApplication::sendEvent(&gear, &enter);
        QTest::qWait(60);
        const qreal beforeLeave = gear.rotation;
        QApplication::sendEvent(&gear, &leave);
        QCOMPARE(gear.rotation, beforeLeave);
        QTRY_COMPARE(turns.count(), 4);
        QCOMPARE(gear.rotation, 0.0);

        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        for (const auto control : window.active->monitorPage->findChildren<sw::IconButton *>())
        {
            QVERIFY(control->text().isEmpty());
            QVERIFY(!control->toolTip().isEmpty());
        }
        QCOMPARE(window.active->modeButton->size(), QSize(36, 36));
        window.active->setTerminal(true);
        QVERIFY(window.active->modeButton->text().isEmpty());
        QCOMPARE(window.active->modeButton->toolTip(), QString("返回收发日志"));
        QCOMPARE(window.active->modeButton->accessibleName(), window.active->modeButton->toolTip());
        window.close();
    }

    void readableFontsAndMigration()
    {
        const QFont font = sw::Theme::defaultDataFont();
        const auto raw = QRawFont::fromFont(font);
        QVERIFY(raw.isValid());
        QVERIFY(raw.supportsCharacter('M'));
        QCOMPARE(font.styleStrategy(), QFont::PreferAntialias);
        const auto brandRaw = QRawFont::fromFont(QFont(sw::Theme::fontFamily()));
        QVERIFY(brandRaw.isValid());
        QVERIFY(brandRaw.supportsCharacter('S'));
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
        writeAppearance(sw::Theme::fontFamily(12), 18);
        {
            sw::Window window(&engine, &mcp, path);
            window.ensurePolished();
            auto title = window.findChild<QLabel *>("brandTitle");
            QVERIFY(title);
            QVERIFY(!title->font().family().contains("Fusion Pixel", Qt::CaseInsensitive));
            QVERIFY(window.active->terminal->font().families().contains("Microsoft YaHei UI"));
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
    void directToolbarActions()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.port->setCurrentText("loop://");
        window.connectionAction();
        auto pane = window.active;
        auto channel = pane->channel;
        channel->receive("first message\n");
        pane->tick();
        QVERIFY(pane->log->toPlainText().contains("first message"));

        QTest::mouseClick(pane->pause, Qt::LeftButton);
        QVERIFY(pane->pause->toolTip().contains("继续显示"));
        channel->receive("received while paused\n");
        pane->tick();
        QVERIFY(!pane->log->toPlainText().contains("received while paused"));
        QVERIFY(engine.journal.read(0)["events"].toArray().last().toObject()["text"].toString().contains("received while paused"));
        QTest::mouseClick(pane->pause, Qt::LeftButton);
        pane->tick();
        QVERIFY(pane->log->toPlainText().contains("received while paused"));

        QTest::mouseClick(pane->follow, Qt::LeftButton);
        QVERIFY(!pane->log->follow);
        QTest::mouseClick(pane->follow, Qt::LeftButton);
        QVERIFY(pane->log->follow);
        const auto history = engine.journal.sequence;
        auto clear = pane->findChild<sw::IconButton *>("clearDisplay");
        QTest::mouseClick(clear, Qt::LeftButton);
        QVERIFY(pane->log->toPlainText().isEmpty());
        QCOMPARE(engine.journal.sequence, history);
        QVERIFY(pane->exportButton->isEnabled());

        pane->payload->setPlainText("ping");
        pane->newline->setCurrentIndex(0);
        pane->interval->setValue(40);
        const auto sent = channel->tx;
        QTest::mouseClick(pane->repeat, Qt::LeftButton);
        QVERIFY(pane->repeatTimer.isActive());
        QVERIFY(pane->repeat->toolTip().contains("停止"));
        QTRY_VERIFY(channel->tx >= sent + 8);
        pane->interval->setValue(60);
        QCOMPARE(pane->repeatTimer.interval(), 60);
        QTest::mouseClick(pane->repeat, Qt::LeftButton);
        QVERIFY(!pane->repeatTimer.isActive());
        const auto stopped = channel->tx;
        QTest::qWait(150);
        QCOMPARE(channel->tx, stopped);
        window.close();
    }

    void connectionIndicatorFollowsSelectedPage()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        auto capture = [&window](const QString &name)
        {
            const auto directory = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
            if (!directory.isEmpty())
            {
                QCoreApplication::processEvents();
                window.sidebar->grab().save(QDir(directory).filePath(name));
                window.grab().save(QDir(directory).filePath("workspace-" + name));
            }
        };

        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Offline);
        QCOMPARE(window.connectionState->text(), QString("未连接"));
        QCOMPARE(window.connectButton->text(), QString("建立连接"));
        capture("connection-offline.png");

        window.port->setCurrentText("loop://");
        QTest::mouseClick(window.connectButton, Qt::LeftButton);
        auto receive = window.active;
        auto channel = receive->channel;
        QVERIFY(channel);
        QCOMPARE(channel->state, QString("connected"));
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Online);
        QCOMPARE(window.connectionState->text(), QString("已连接"));
        QCOMPARE(window.connectButton->text(), QString("断开连接"));
        capture("connection-online.png");
        window.applyTheme(true);
        capture("connection-online-dark.png");
        window.applyTheme(false);

        const auto captureDirectory = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
        if (!captureDirectory.isEmpty())
        {
            receive->setTerminal(true);
            channel->receive("\r\nuser@device:~$ status\r\n"
                             "Serial Workbench | connection online\r\n"
                             "CPU 8%   Memory 124 MB   Latency 2 ms\r\n"
                             "终端已就绪，支持中文输入与复制粘贴。\r\n"
                             "user@device:~$ ");
            QTest::qWait(160);
            capture("terminal-controls.png");
            auto toolbar = receive->monitorPage->findChild<QWidget *>("monitorToolbar");
            QVERIFY(toolbar);
            QVERIFY(toolbar->grab().save(QDir(captureDirectory).filePath("toolbar-terminal.png")));
            const QRect gearRect(window.gear->mapTo(&window, QPoint()), window.gear->size());
            QVERIFY(window.grab().copy(gearRect).save(QDir(captureDirectory).filePath("gear.png")));
            auto title = window.findChild<QLabel *>("brandTitle");
            QVERIFY(title);
            const QRect titleRect(title->mapTo(&window, QPoint()), title->size());
            QVERIFY(window.grab().copy(titleRect).save(QDir(captureDirectory).filePath("brand-title.png")));
            QTest::mouseMove(window.gear, window.gear->rect().center());
            QEnterEvent enter(window.gear->rect().center(), window.gear->rect().center(),
                              window.gear->mapToGlobal(window.gear->rect().center()));
            QApplication::sendEvent(window.gear, &enter);
            QTest::qWait(200);
            QVERIFY(window.grab().copy(gearRect).save(QDir(captureDirectory).filePath("gear-hover.png")));
            QTest::mouseMove(toolbar, toolbar->rect().center());
            QEvent leave(QEvent::Leave);
            QApplication::sendEvent(window.gear, &leave);
            window.applyTheme(true);
            capture("terminal-controls-dark.png");
            window.applyTheme(false);
            receive->setTerminal(false);
            QTest::qWait(120);
            capture("controls-log.png");
            window.applyTheme(true);
            capture("controls-log-dark.png");
            window.applyTheme(false);
        }

        window.newConnection();
        QVERIFY(!window.active->channel);
        QCOMPARE(channel->state, QString("connected"));
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Offline);
        QCOMPARE(window.connectionState->text(), QString("未连接"));
        QCOMPARE(window.connectButton->text(), QString("建立连接"));

        auto waveform = window.newInstrument("waveform", channel);
        QCOMPARE(window.parameterPane, receive);
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Online);
        QCOMPARE(window.connectionState->text(), QString("已连接"));
        QTest::mouseClick(window.connectButton, Qt::LeftButton);
        QCOMPARE(channel->state, QString("closed"));
        QCOMPARE(window.active, waveform);
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Offline);
        QCOMPARE(window.connectionState->text(), QString("已断开"));
        QCOMPARE(window.connectButton->text(), QString("建立连接"));

        QTest::mouseClick(window.connectButton, Qt::LeftButton);
        QCOMPARE(channel->state, QString("connected"));
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Online);
        QCOMPARE(window.connectionState->text(), QString("已连接"));
        QCOMPARE(window.connectButton->text(), QString("断开连接"));
        window.close();
    }

    void connectionIndicatorDistinguishesPendingAndFailure()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.port->setCurrentText("loop://");
        window.connectionAction();
        auto channel = window.active->channel;
        QVERIFY(channel);

        // Drive the real state notification without opening a network connection.
        channel->status("connecting");
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Pending);
        QCOMPARE(window.connectionState->text(), QString("连接中"));
        QCOMPARE(window.connectButton->text(), QString("取消连接"));
        QTest::mouseClick(window.connectButton, Qt::LeftButton);
        QCOMPARE(channel->state, QString("closed"));
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Offline);
        QCOMPARE(window.connectionState->text(), QString("已断开"));

        channel->status("reconnecting", "Device temporarily unavailable");
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Pending);
        QCOMPARE(window.connectionState->text(), QString("重连中"));
        QCOMPARE(window.connectButton->text(), QString("停止重连"));

        channel->status("error", "Connection failed");
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Error);
        QCOMPARE(window.connectionState->text(), QString("连接异常"));
        QCOMPARE(window.connectButton->text(), QString("建立连接"));

        channel->status("listening");
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Online);
        QCOMPARE(window.connectionState->text(), QString("监听中"));
        QCOMPARE(window.connectButton->text(), QString("停止监听"));
        QTest::mouseClick(window.connectButton, Qt::LeftButton);
        QCOMPARE(channel->state, QString("closed"));
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Offline);
        QCOMPARE(window.connectionState->text(), QString("已断开"));
        window.close();
    }

    void customReceiveTabTitleSurvivesConnectionAndDetach()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.json");
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        {
            sw::Window window(&engine, &mcp, path);
            window.show();
            auto receive = window.active;
            const int index = pageIndex(&window, receive);
            QTest::mouseDClick(window.tabs, Qt::LeftButton, Qt::NoModifier, window.tabs->tabRect(index).center());
            QTRY_VERIFY(window.tabs->findChild<QLineEdit *>("tabTitleEditor") != nullptr);
            auto editor = window.tabs->findChild<QLineEdit *>("tabTitleEditor");
            QVERIFY(editor);
            QTest::keyClicks(editor, "Bench A");
            QTest::keyClick(editor, Qt::Key_Escape);
            QTRY_COMPARE(window.tabs->tabText(index), QString("新收发页"));
            QTRY_VERIFY(window.tabs->findChild<QLineEdit *>("tabTitleEditor") == nullptr);

            window.beginTabRename(index);
            editor = window.tabs->findChild<QLineEdit *>("tabTitleEditor");
            QVERIFY(editor);
            QTest::keyClicks(editor, "Bench A");
            QTest::keyClick(editor, Qt::Key_Return);
            QCOMPARE(window.tabs->tabText(index), QString("Bench A"));
            QCOMPARE(receive->pageTitle, QString("Bench A"));

            window.port->setCurrentText("loop://");
            window.connectionAction();
            QVERIFY(receive->channel);
            QCOMPARE(window.tabs->tabText(pageIndex(&window, receive)), QString("Bench A"));
            QCOMPARE(receive->channel->name, QString("loop://"));
            window.applyTheme(true);
            QCOMPARE(window.tabs->tabText(pageIndex(&window, receive)), QString("Bench A"));

            auto detached = window.detachConnection(pageIndex(&window, receive), QPoint(200, 180));
            QVERIFY(detached);
            QCOMPARE(detached->tabs->tabText(pageIndex(detached, receive)), QString("Bench A"));
            QCOMPARE(detached->windowTitle(), QString("Bench A · Serial Workbench"));
            detached->close();
            QCOMPARE(window.tabs->tabText(pageIndex(&window, receive)), QString("Bench A"));
            window.tabs->setCurrentIndex(pageIndex(&window, receive));
            window.saveSettings();
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(QJsonDocument::fromJson(file.readAll()).object()["appearance"].toObject()["tab_title"].toString(), QString("Bench A"));
            window.close();
        }
        sw::Window restored(&engine, &mcp, path);
        QCOMPARE(restored.tabs->tabText(0), QString("Bench A"));
        restored.close();
    }

    void customWidgetTabTitleRestores()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.json");
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        {
            sw::Window window(&engine, &mcp, path);
            window.show();
            auto wave = window.newInstrument("waveform");
            QTest::mouseDClick(window.tabs, Qt::LeftButton, Qt::NoModifier,
                window.tabs->tabRect(pageIndex(&window, wave)).center());
            QTRY_VERIFY(window.tabs->findChild<QLineEdit *>("tabTitleEditor") != nullptr);
            auto editor = window.tabs->findChild<QLineEdit *>("tabTitleEditor");
            QVERIFY(editor);
            QVERIFY(editor->hasFocus());
            QTest::keyClicks(editor, "Motor RPM");
            QCOMPARE(editor->text(), QString("Motor RPM"));
            editor->clearFocus();
            QCOMPARE(wave->pageTitle, QString("Motor RPM"));
            QTRY_COMPARE(window.tabs->tabText(pageIndex(&window, wave)), QString("Motor RPM"));
            window.saveSettings();
            window.close();
        }
        sw::Window restored(&engine, &mcp, path);
        auto renamed = std::find_if(restored.panes.begin(), restored.panes.end(), [](auto pane)
        {
            return pane->isInstrument() && pane->pageTitle == "Motor RPM";
        });
        QVERIFY(renamed != restored.panes.end());
        QCOMPARE(restored.tabs->tabText(pageIndex(&restored, *renamed)), QString("Motor RPM"));
        restored.close();
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
    void strictWaveFrames_data()
    {
        QTest::addColumn<QByteArray>("frame");
        QTest::addColumn<QJsonObject>("expected");
        QTest::addColumn<bool>("malformed");

        QTest::newRow("mixed-numeric-types")
            << QByteArray("@wave count=4294967295,temperature=-25.50,precision=1.2345678901234567\r\n")
            << QJsonObject {{"count", 4294967295.0}, {"temperature", -25.5}, {"precision", 1.2345678901234567}} << false;
        QTest::newRow("scientific-and-spacing")
            << QByteArray("@wave  _count = +12 , small=.125, power=-1.25e-3, whole=3.\t\n")
            << QJsonObject {{"_count", 12}, {"small", 0.125}, {"power", -0.00125}, {"whole", 3}} << false;
        QTest::newRow("chinese-names")
            << QString("@wave 温度=25.5,电压=3.3\r").toUtf8()
            << QJsonObject {{"温度", 25.5}, {"电压", 3.3}} << false;
        QTest::newRow("maximum-name")
            << ("@wave " + QByteArray(64, 'a') + "=1\n")
            << QJsonObject {{QString(64, 'a'), 1}} << false;

        QStringList fields;
        QJsonObject values;
        for (int index = 0; index < 16; ++index)
        {
            const QString name = "ch" + QString::number(index);
            fields.append(name + "=" + QString::number(index));
            values.insert(name, index);
        }
        QTest::newRow("sixteen-channels") << ("@wave " + fields.join(',') + "\n").toUtf8() << values << false;
        QTest::newRow("too-many-channels")
            << ("@wave " + fields.join(',') + ",ch16=16\n").toUtf8() << QJsonObject() << true;

        const auto ignored = [](const char *name, const QByteArray &frame)
        {
            QTest::newRow(name) << frame << QJsonObject() << false;
        };
        ignored("plain-integer", "123\n");
        ignored("plain-csv", "1,2,3\n");
        ignored("plain-named", "temperature=25,voltage=3.3\n");
        ignored("json-object", "{\"temperature\":25,\"voltage\":3.3}\n");
        ignored("json-array", "[1,2,3]\n");
        ignored("prefix-inside-log", "I (120) device: @wave count=1\n");
        ignored("indented-prefix", " @wave count=1\n");
        ignored("wrong-case", "@WAVE count=1\n");
        ignored("wrong-separator", "@wave\tcount=1\n");

        const auto rejected = [](const char *name, const QByteArray &frame)
        {
            QTest::newRow(name) << frame << QJsonObject() << true;
        };
        rejected("empty-frame", "@wave \n");
        rejected("unnamed-csv", "@wave 1,2,3\n");
        rejected("prefixed-json", "@wave {\"count\":1}\n");
        rejected("duplicate-name", "@wave count=1,count=2\n");
        rejected("too-long-name", "@wave " + QByteArray(65, 'a') + "=1\n");
        rejected("numeric-name", "@wave 1count=1\n");
        rejected("trailing-comma", "@wave count=1,\n");
        rejected("empty-field", "@wave count=1,,value=2\n");
        rejected("colon-assignment", "@wave count:1\n");
        rejected("semicolon-separator", "@wave count=1;value=2\n");
        rejected("hexadecimal-value", "@wave count=0x10\n");
        rejected("nan-value", "@wave count=1,value=NaN\n");
        rejected("infinite-value", "@wave value=inf\n");
        rejected("overflow-value", "@wave value=1e999\n");
        rejected("unit-in-value", "@wave voltage=3.3V\n");
        rejected("log-after-value", "@wave count=1 device running\n");
        rejected("invalid-utf8-name", "@wave " + QByteArray::fromHex("FF") + "=1\n");
    }

    void strictWaveFrames()
    {
        QFETCH(QByteArray, frame);
        QFETCH(QJsonObject, expected);
        QFETCH(bool, malformed);

        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->receive(frame);
        const auto waveform = channel->waveform();
        QCOMPARE(waveform["protocol"].toString(), "wave/1");
        const auto source = waveform["sources"].toObject()["default"].toObject();
        const auto samples = source["samples"].toArray();
        QCOMPARE(samples.size(), expected.isEmpty() ? 0 : 1);
        QCOMPARE(source["rejected_lines"].toInt(), malformed ? 1 : 0);
        if (!expected.isEmpty())
        {
            QCOMPARE(samples.first().toObject()["values"].toObject(), expected);
        }
        const auto raw = engine.journal.read(0)["events"].toArray().last().toObject();
        QCOMPARE(QByteArray::fromHex(raw["hex"].toString().toLatin1()), frame);
    }

    void strictWaveFragmentationAndRecovery()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->receive("@wa", "peer-a");
        channel->receive("@wave temperature=30,count=2\r\n", "peer-b");
        const auto bytes = QString("ve count=1,温度=25.5\r\n").toUtf8();
        for (const char byte : bytes) { channel->receive(QByteArray(1, byte), "peer-a"); }
        auto sources = channel->waveform()["sources"].toObject();
        QCOMPARE(sources["peer-a"].toObject()["total_samples"].toInt(), 1);
        QCOMPARE(sources["peer-b"].toObject()["total_samples"].toInt(), 1);
        QCOMPARE(sources["peer-a"].toObject()["samples"].toArray().first().toObject()["values"].toObject()["温度"].toDouble(), 25.5);
        channel->receive("@wave count=3");
        QCOMPARE(channel->sources["default"]->count, 0);
        channel->receive("\n");
        QCOMPARE(channel->sources["default"]->count, 1);

        channel->receive(QByteArray(9000, 'x'));
        QVERIFY(channel->sources["default"]->wavePending.size() <= 8192);
        channel->receive("@wave fake=99\n@wave recovered=4\n");
        QCOMPARE(channel->sources["default"]->count, 2);
        QCOMPARE(channel->sources["default"]->rejected, 0);

        channel->receive("@wave value=" + QByteArray(9000, '1'));
        QCOMPARE(channel->sources["default"]->rejected, 1);
        channel->receive(QByteArray(9000, '2'));
        channel->receive("\r\n@wave recovered=5\r\n");
        QCOMPARE(channel->sources["default"]->count, 3);
        QCOMPARE(channel->sources["default"]->rejected, 1);

        channel->receive("@wave value=" + QByteArray(9000, '1') + "\n@wave recovered=6\n");
        QCOMPARE(channel->sources["default"]->count, 4);
        QCOMPARE(channel->sources["default"]->rejected, 2);
    }

    void strictWaveChannelsAndLiveValues()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.port->setCurrentText("loop://");
        window.connectionAction();
        auto channel = window.active->channel;
        auto values = window.newInstrument("values", channel, false)->instrument;
        auto wave = window.newInstrument("waveform", channel, false)->instrument;
        channel->receive("@wave count=4294967295,precision=1.2345678901234567\n");
        channel->receive("@wave precision=2.5,count=4294967294\n");
        wave->tick(channel);
        QCOMPARE(wave->plot->series.size(), 2);
        QCOMPARE(wave->plot->series["count"].size(), 2);
        QCOMPARE(wave->plot->series["count"][0].y(), 4294967295.0);
        QCOMPARE(wave->plot->series["count"][1].y(), 4294967294.0);
        values->tick(channel);
        QCOMPARE(values->table->topLevelItemCount(), 2);
        QCOMPARE(values->table->topLevelItem(0)->text(0), "count");
        QCOMPARE(values->table->topLevelItem(0)->text(1), "4294967294");
        QCOMPARE(values->table->topLevelItem(1)->text(1), "2.5");
        channel->receive("@wave precision=1.2345678901234567\n");
        values->tick(channel);
        QCOMPARE(values->table->topLevelItem(0)->text(1).toDouble(), 1.2345678901234567);
    }

    void fragmentedDecoders()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->receive("@wave temp=24,");
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
        auto page = window.newInstrument("waveform", nullptr, false);
        QVERIFY(page->instrument->plot);
        window.tabs->setCurrentIndex(pageIndex(&window, first));
        QVERIFY(first->findChildren<QTabWidget *>().isEmpty());
        const auto original = first->splitter->sizes();
        first->splitter->setSizes({250, 250});
        QVERIFY(first->splitter->sizes() != original);
        window.openSettings();
        QVERIFY(window.settingsDialog->isVisible());
        window.settingsDialog->hide();
        window.close();
    }
    void logWrappingAndPersistence()
    {
        sw::LogView log;
        log.setFont(sw::Theme::defaultDataFont());
        log.resize(300, 180);
        log.show();
        sw::Journal journal;
        const QByteArray message = QString("长消息保留全部内容 %1 end-of-message\n")
            .arg(QString(240, 'A')).toUtf8();
        log.feed(journal.append("wrap-test", "rx", message));
        const auto original = log.toPlainText();
        QTRY_VERIFY(log.document()->firstBlock().layout()->lineCount() > 1);
        QCOMPARE(log.horizontalScrollBar()->maximum(), 0);
        const int narrowLines = log.document()->firstBlock().layout()->lineCount();
        log.resize(600, 180);
        QTRY_VERIFY(log.document()->firstBlock().layout()->lineCount() < narrowLines);

        QTextCursor selection(log.document());
        selection.select(QTextCursor::Document);
        log.setTextCursor(selection);
        log.setWrapping(false);
        QTRY_VERIFY(log.horizontalScrollBar()->maximum() > 0);
        QVERIFY(log.horizontalScrollBar()->isVisible());
        QCOMPARE(log.document()->firstBlock().layout()->lineCount(), 1);
        log.horizontalScrollBar()->setValue(log.horizontalScrollBar()->maximum());
        QVERIFY(log.horizontalScrollBar()->value() > 0);
        QCOMPARE(log.toPlainText(), original);
        QCOMPARE(log.textCursor().selectedText(), selection.selectedText());
        log.setWrapping(true);
        QTRY_COMPARE(log.horizontalScrollBar()->maximum(), 0);
        QVERIFY(!log.horizontalScrollBar()->isVisible());
        QCOMPARE(log.toPlainText(), original);

        QTemporaryDir directory;
        const auto path = directory.filePath("wrap-settings.json");
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        {
            sw::Window window(&engine, &mcp, path);
            window.show();
            auto pane = window.active;
            QVERIFY(pane->wrapLines->isChecked());
            QTest::mouseClick(pane->wrapLines, Qt::LeftButton);
            QCOMPARE(pane->log->lineWrapMode(), QTextEdit::NoWrap);
            pane->setTerminal(true);
            QVERIFY(pane->wrapLines->isHidden());
            pane->setTerminal(false);
            QVERIFY(!pane->wrapLines->isHidden());
            QVERIFY(!pane->wrapLines->isChecked());
            window.close();
        }
        {
            sw::Window restored(&engine, &mcp, path);
            QVERIFY(!restored.active->wrapLines->isChecked());
            QCOMPARE(restored.active->log->lineWrapMode(), QTextEdit::NoWrap);
            restored.close();
        }
    }

    void buttonFocusFeedback()
    {
        QWidget host;
        host.setStyleSheet(sw::Theme::make(false).stylesheet());
        auto layout = new QHBoxLayout(&host);
        auto action = new sw::IconButton("trash", "清空");
        auto textAction = new QPushButton("导出");
        auto toggle = new sw::IconButton("time", "时间戳");
        toggle->setCheckable(true);
        auto primary = new QPushButton("建立连接");
        primary->setObjectName("primary");
        layout->addWidget(action);
        layout->addWidget(textAction);
        layout->addWidget(toggle);
        layout->addWidget(primary);
        layout->addStretch();
        host.resize(420, 100);
        host.show();
        host.activateWindow();
        QTest::qWait(100);
        QTest::mouseMove(&host, QPoint(400, 80));
        action->setFocus(Qt::OtherFocusReason);
        QTest::qWait(180);
        const auto idle = action->grab().toImage();
        const auto textIdle = textAction->grab().toImage();
        const auto primaryIdle = primary->grab().toImage();

        QSignalSpy activated(action, &QAbstractButton::clicked);
        QTest::mouseClick(action, Qt::LeftButton);
        QTest::mouseMove(&host, QPoint(400, 80));
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(action, &leave);
        QTest::qWait(180);
        QCOMPARE(activated.count(), 1);
        QVERIFY(action->hasFocus());
        QVERIFY(!action->property("keyboardFocus").toBool());
        QCOMPARE(action->grab().toImage(), idle);

        QTest::keyClick(action, Qt::Key_Tab);
        QVERIFY(textAction->hasFocus());
        QVERIFY(textAction->property("keyboardFocus").toBool());
        QTest::keyClick(textAction, Qt::Key_Tab, Qt::ShiftModifier);
        QVERIFY(action->hasFocus());
        QVERIFY(action->property("keyboardFocus").toBool());
        QVERIFY(action->grab().toImage() != idle);
        QTest::keyClick(action, Qt::Key_Space);
        QCOMPARE(activated.count(), 2);

        QTest::mouseClick(textAction, Qt::LeftButton);
        QTest::mouseMove(&host, QPoint(400, 80));
        QVERIFY(!textAction->property("keyboardFocus").toBool());
        QApplication::sendEvent(textAction, &leave);
        QTest::qWait(180);
        QCOMPARE(textAction->grab().toImage(), textIdle);
        QTest::mouseClick(toggle, Qt::LeftButton);
        QTest::mouseMove(&host, QPoint(400, 80));
        QVERIFY(toggle->isChecked());
        QVERIFY(!toggle->property("keyboardFocus").toBool());
        QTest::keyClick(toggle, Qt::Key_Tab);
        QVERIFY(primary->hasFocus());
        QVERIFY(primary->grab().toImage() != primaryIdle);
        QTest::mouseClick(primary, Qt::LeftButton);
        QTest::mouseMove(&host, QPoint(400, 80));
        QApplication::sendEvent(primary, &leave);
        QTest::qWait(180);
        QCOMPARE(primary->grab().toImage(), primaryIdle);
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
            auto source = window.active->channel;
            window.newInstrument("waveform", source, false);
            window.newInstrument("structured", source, false);
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
            QCOMPARE(window.panes.size(), 3);
            QCOMPARE(window.panes[1]->instrument->kind, "waveform");
            QCOMPARE(window.panes[2]->instrument->kind, "structured");
            QCOMPARE(window.panes[1]->sourceKey, "serial:loop://");
            QVERIFY(!window.panes[1]->channel);
            QVERIFY(engine.channels.isEmpty());
        }
    }
    void toolbarAndPortUpdates()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        QJsonArray available;
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
            // ASCII protocol dumps must retain equal character cells.
            const QFontMetrics terminalMetrics(pane->terminal->font());
            const int cell = terminalMetrics.horizontalAdvance('M');
            for (int character = 32; character < 127; ++character)
            {
                QCOMPARE(terminalMetrics.horizontalAdvance(QChar(character)), cell);
            }
        }
        QVERIFY(pane->newline->itemText(3).contains("\\r\\n"));
        QVERIFY(pane->newline->itemText(3).contains("CRLF"));

        available = {QJsonObject {{"port", "COM44"}, {"description", "Test device"}}};
        window.portProvider = [&available] { return available; };
        window.refreshPorts();
        QVERIFY(window.port->findText("COM44") >= 0);
        window.port->setCurrentText("COM44");
        available = {QJsonObject {{"port", "COM45"}}};
        window.port->lineEdit()->setSelection(1, 3);
        window.portTimer.setInterval(30);
        QTRY_VERIFY(window.port->findText("COM45") >= 0);
        QCOMPARE(window.port->currentText(), "NULL");
        QCOMPARE(window.active->draft.port, QString("NULL"));
        QVERIFY(window.portDetails->isHidden());
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

    void livePortDiscovery()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        QJsonArray available {
            QJsonObject {{"port", "COM8"}, {"description", "USB UART"}},
            QJsonObject {{"port", "COM9"}}
        };
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.portProvider = [&available] { return available; };
        window.portTimer.setInterval(30);
        window.refreshPorts();
        window.show();
        window.port->setCurrentText("COM8");
        window.port->lineEdit()->setSelection(1, 2);
        QSignalSpy reset(window.port->model(), &QAbstractItemModel::modelReset);
        QSignalSpy inserted(window.port->model(), &QAbstractItemModel::rowsInserted);
        QSignalSpy removed(window.port->model(), &QAbstractItemModel::rowsRemoved);
        available[0] = QJsonObject {
            {"port", "COM8"}, {"description", "Updated device"}, {"manufacturer", "Silicon Labs"},
            {"vendor_id", 0x10c4}, {"product_id", 0xea60}, {"serial_number", "UART1234"},
            {"system_location", "test-device-path"}
        };
        window.refreshPorts();
        QCOMPARE(window.port->currentText(), "COM8");
        QCOMPARE(window.port->lineEdit()->selectedText(), "OM");
        const auto tooltip = window.port->itemData(1, Qt::ToolTipRole).toString();
        QVERIFY(tooltip.contains("Updated device"));
        QVERIFY(tooltip.contains("Silicon Labs"));
        QVERIFY(tooltip.contains("VID 10C4 · PID EA60"));
        QVERIFY(tooltip.contains("UART1234"));
        QVERIFY(tooltip.contains("test-device-path"));
        QVERIFY(window.portDetails->text().contains("VID 10C4 · PID EA60"));
        QVERIFY(window.portDetails->text().contains("UART1234"));
        QVERIFY(window.portDetails->isVisible());
        for (auto text : window.findChildren<QLabel *>())
        {
            QVERIFY(text->text() != "实时扫描");
        }
        QCOMPARE(reset.count(), 0);
        QCOMPARE(inserted.count(), 0);
        QCOMPARE(removed.count(), 0);

        available.removeAt(0);
        QTRY_COMPARE(window.port->currentText(), QString("NULL"));
        QCOMPARE(window.active->draft.port, QString("NULL"));
        QCOMPARE(window.port->findText("COM8"), -1);
        QCOMPARE(removed.count(), 1);

        window.port->setCurrentText("COM9");
        window.port->showPopup();
        QTRY_VERIFY(window.port->view()->isVisible());
        available.append(QJsonObject {{"port", "COM10"}});
        QTRY_VERIFY(window.port->findText("COM10") > 0);
        QCOMPARE(window.port->currentText(), "COM9");
        available.removeAt(0);
        QTRY_COMPARE(window.port->currentText(), QString("NULL"));
        QVERIFY(window.port->view()->isVisible());
        QCOMPARE(reset.count(), 0);
        window.port->hidePopup();

        // Mouse/keyboard editing must never erase NULL or alter a selected COM.
        for (const QString &selected : {QString("NULL"), QString("COM10")})
        {
            window.port->setCurrentIndex(window.port->findText(selected));
            QTest::mouseClick(window.port->lineEdit(), Qt::LeftButton);
            QTest::keyClick(window.port->lineEdit(), Qt::Key_A, Qt::ControlModifier);
            QTest::keyClick(window.port->lineEdit(), Qt::Key_Backspace);
            QTest::keyClick(window.port->lineEdit(), Qt::Key_Delete);
            QTest::keyClicks(window.port->lineEdit(), "COM99");
            QVERIFY(!window.port->testAttribute(Qt::WA_InputMethodEnabled));
            QVERIFY(!window.port->lineEdit()->testAttribute(Qt::WA_InputMethodEnabled));
            QCOMPARE(window.port->currentText(), selected);
        }
        available.append(QJsonObject {{"port", "COM11"}});
        window.refreshPorts();
        QCOMPARE(window.port->currentText(), "COM10");
        QCOMPARE(window.port->findText("COM99"), -1);
        window.port->showPopup();
        QTRY_VERIFY(window.port->view()->isVisible());
        const auto item = window.port->model()->index(window.port->findText("COM11"), 0);
        QTest::mouseClick(window.port->view()->viewport(), Qt::LeftButton, Qt::NoModifier,
            window.port->view()->visualRect(item).center());
        QCOMPARE(window.port->currentText(), QString("COM11"));

        window.port->setCurrentText("COM8");
        window.refreshPorts();
        QCOMPARE(window.port->currentText(), "NULL");
        sw::Config saved;
        saved.port = "COM8";
        window.loadForm(saved);
        QCOMPARE(window.port->currentText(), "NULL");

        // A lost device must not disable the existing connection's stop action.
        available.append(QJsonObject {{"port", "COM9"}, {"description", "Retained USB UART"}, {"hwid", "OLD-SERIAL"}});
        window.refreshPorts();
        sw::Config loopback;
        loopback.port = "loop://";
        auto channel = engine.open(loopback);
        channel->config.port = "COM9";
        window.tabs->setCurrentIndex(window.stack->indexOf(window.findPane(channel)));
        window.loadForm(channel->config);
        QCOMPARE(window.port->currentText(), "COM9");
        available = {};
        window.refreshPorts();
        QCOMPARE(window.port->currentText(), "COM9");
        QCOMPARE(window.port->findText("COM9"), -1);
        QCOMPARE(window.connectButton->text(), QString("停止重连"));
        QVERIFY(window.portDetails->text().contains("Retained USB UART"));
        QVERIFY(window.portDetails->text().contains("OLD-SERIAL"));
        QCOMPARE(channel->state, QString("reconnecting"));
        QVERIFY(channel->retryTimer.isActive());
        QVERIFY(channel->error.contains("COM9"));
        window.connectionAction();
        QCOMPARE(channel->state, "closed");
        QVERIFY(!channel->retryTimer.isActive());
        QCOMPARE(channel->config.port, QString("COM9"));
        window.close();
    }

    void serialRecoveryIsQuiet()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        // Simulate discovery without opening any physical device.
        QJsonArray available {QJsonObject {{"port", "COM65535"}}};
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.portProvider = [&available] { return available; };
        window.refreshPorts();
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->config.port = "COM65535";
        window.tabs->setCurrentIndex(window.stack->indexOf(window.findPane(channel)));
        window.loadForm(channel->config);
        auto pane = window.active;
        window.show();
        channel->receive("preserved data\r\n");
        const auto identity = channel->id;
        const auto before = engine.journal.sequence;

        available = {};
        window.refreshPorts();
        QCOMPARE(engine.journal.sequence, before + 1);
        const auto reason = channel->error;
        QTRY_VERIFY(channel->attempts >= 3);
        QCOMPARE(engine.journal.sequence, before + 1);
        QCOMPARE(channel->error, reason);
        QCOMPARE(channel->state, QString("reconnecting"));
        QCOMPARE(window.port->currentText(), QString("COM65535"));
        QCOMPARE(window.connectButton->text(), QString("停止重连"));
        QVERIFY(window.serialInfo->text().contains("等待设备恢复"));
        pane->tick();
        QCOMPARE(pane->log->toPlainText().count("已断开"), 1);
        QVERIFY(pane->log->toPlainText().contains("preserved data"));

        window.newConnection();
        window.tabs->setCurrentIndex(window.stack->indexOf(pane));
        window.loadForm(channel->config);
        QCOMPARE(window.port->currentText(), QString("COM65535"));
        QCOMPARE(window.connectButton->text(), QString("停止重连"));
        window.refreshPorts();
        QCOMPARE(window.port->currentText(), QString("COM65535"));

        // A successful recovery retains identity/history and re-arms notification.
        channel->config.port = "loop://";
        channel->open();
        QCOMPARE(channel->state, QString("connected"));
        QCOMPARE(channel->id, identity);
        QVERIFY(!channel->retryTimer.isActive());
        QCOMPARE(engine.journal.sequence, before + 2);
        QVERIFY(engine.journal.read(before + 1)["events"].toArray().first().toObject()["stream_reset"].toBool());
        channel->config.port = "COM65535";
        window.loadForm(channel->config);
        window.refreshPorts();
        QCOMPARE(channel->state, QString("reconnecting"));
        QCOMPARE(engine.journal.sequence, before + 3);
        window.connectionAction();
        QCOMPARE(channel->state, QString("closed"));
        QVERIFY(!channel->retryTimer.isActive());
        const auto stopped = engine.journal.sequence;
        QTest::qWait(400);
        QCOMPARE(engine.journal.sequence, stopped);
        window.refreshPorts();
        QCOMPARE(window.port->currentText(), QString("NULL"));
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
        auto wavePage = window.newInstrument("waveform", channel, false);
        auto parsePage = window.newInstrument("structured", channel, false);
        auto wave = wavePage->instrument;
        auto structured = parsePage->instrument;
        auto detached = window.detachConnection(pageIndex(&window, pane), QPoint(180, 160));
        QVERIFY(detached);
        QTest::qWait(100);
        QCOMPARE(detached->active, pane);
        QCOMPARE(pane->window, detached);
        QCOMPARE(channel->id, id);
        QCOMPARE(engine.channels.size(), 1);
        window.syncConnections();
        QCOMPARE(window.findPane(channel), pane);
        auto waveWindow = window.detachConnection(pageIndex(&window, wavePage), QPoint(250, 200));
        auto parseWindow = window.detachConnection(pageIndex(&window, parsePage), QPoint(300, 220));
        QVERIFY(waveWindow && parseWindow);
        detached->newConnection();
        QVERIFY(pane->isHidden());
        QVERIFY2(wave->isVisible(), "Detached waveform must update while its receive page is hidden");
        QVERIFY(parseWindow->isVisible());
        channel->receive("@wave temp=23,voltage=3.3\n{\"answer\":42}\n");
        QTRY_VERIFY(!wave->plot->series.isEmpty());
        QTRY_VERIFY(structured->table->topLevelItemCount() > 0);
        QCOMPARE(pane->payload->toPlainText(), "preserved draft");
        detached->tabs->setCurrentIndex(pageIndex(detached, pane));
        QTRY_VERIFY(pane->log->toPlainText().contains("answer"));
        window.saveSettings();
        QFile savedFile(directory.filePath("settings.json"));
        QVERIFY(savedFile.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(savedFile.readAll()).object()["widget_pages"].toArray().size(), 2);
#ifdef Q_OS_WIN
        if (QGuiApplication::platformName() == "windows")
        {
            for (auto host : QList<QWidget *> {detached, waveWindow, parseWindow})
            {
                const auto style = GetWindowLongPtr(reinterpret_cast<HWND>(host->winId()), GWL_STYLE);
                QVERIFY(style & WS_MAXIMIZEBOX);
                QVERIFY(style & WS_THICKFRAME);
                QVERIFY(style & WS_CAPTION);
            }
        }
#endif
        waveWindow->close();
        QCOMPARE(wavePage->window, &window);
        const int waveIndex = pageIndex(&window, wavePage);
        QVERIFY(window.tabs->tabButton(waveIndex, QTabBar::RightSide));
        window.tabs->moveTab(waveIndex, 0);
        QCOMPARE(window.tabs->tabData(0).value<QObject *>(), wavePage);
        window.closeTab(0);
        QCOMPARE(channel->state, "connected");
        QCOMPARE(engine.channels.size(), 1);
        detached->close();
        QCOMPARE(pane->window, &window);
        QCOMPARE(channel->state, "connected");
        parseWindow->close();
        window.tabs->setCurrentIndex(pageIndex(&window, pane));
        window.resize(480, 640);
        QTest::qWait(100);
        QCOMPARE(window.width(), 480);
        QVERIFY(window.stack->width() <= 426);
        const int gap = window.plus->geometry().left() - window.tabs->geometry().right();
        QVERIFY(gap >= 0 && gap < 16);
        QVERIFY(window.tabs->rect().contains(window.tabs->tabRect(window.tabs->currentIndex()).center()));
        auto toolbar = pane->monitorPage->findChild<QWidget *>("monitorToolbar");
        QVERIFY(toolbar);
        const int lastToolEdge = pane->exportButton->mapTo(toolbar, QPoint()).x() + pane->exportButton->width();
        QVERIFY(lastToolEdge <= toolbar->width());
        const auto captureDirectory = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
        if (!captureDirectory.isEmpty())
        {
            QVERIFY(window.grab().save(QDir(captureDirectory).filePath("workspace-narrow.png")));
        }
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
        window.portProvider = [] { return QJsonArray {QJsonObject {{"port", "COM77"}}}; };
        window.refreshPorts();
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
        auto waveform = detached->newInstrument("waveform", draft->channel, false);
        auto waveWindow = detached->detachConnection(pageIndex(detached, waveform), QPoint(250, 180));
        QPointer<sw::Window> guarded(waveWindow);
        window.close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(guarded.isNull());
        QFile file(directory.filePath("settings.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto saved = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(saved["widget_pages"].toArray().size(), 1);
    }

    void newPageMenuAndSources()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        QTest::qWait(150);
        auto receive = window.active;
        QTest::mouseClick(window.plus, Qt::LeftButton);
        auto menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        QVERIFY(menu);
        const auto captureDirectory = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
        if (!captureDirectory.isEmpty())
        {
            QCoreApplication::processEvents();
            QVERIFY(menu->grab().save(QDir(captureDirectory).filePath("new-page-menu.png")));
            menu->setActiveAction(menu->actions().first());
            QCoreApplication::processEvents();
            QVERIFY(menu->grab().save(QDir(captureDirectory).filePath("new-page-menu-hover.png")));
        }
        QCOMPARE(window.tabs->count(), 1);
        auto waveAction = menu->findChild<QAction *>("new_waveform");
        QVERIFY(waveAction);
        waveAction->trigger();
        menu->close();
        auto wavePage = window.active;
        QVERIFY(wavePage->instrument);
        QCOMPARE(window.tabs->count(), 2);
        QCOMPARE(window.stack->currentWidget(), wavePage);
        QVERIFY(window.sidebar->isVisible());
        QVERIFY(window.parameterToggle->isEnabled());
        QCOMPARE(window.parameterPane, receive);
        QVERIFY(wavePage->instrument->source->isVisible());
        QVERIFY(engine.channels.isEmpty());
        QVERIFY(!wavePage->channel);

        window.tabs->setCurrentIndex(pageIndex(&window, receive));
        QVERIFY(window.sidebar->isVisible());
        window.port->setCurrentText("loop://");
        window.connectionAction();
        auto first = receive->channel;
        auto picker = wavePage->instrument->source;
        const int sourceIndex = picker->findData(first->config.key());
        QVERIFY(sourceIndex > 0);
        picker->setCurrentIndex(sourceIndex);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, sourceIndex));
        QCOMPARE(wavePage->channel, first);
        QCOMPARE(engine.channels.size(), 1);
        window.tabs->setCurrentIndex(pageIndex(&window, wavePage));
        first->receive("@wave temp=23\n");
        QTRY_VERIFY(!wavePage->instrument->plot->series.isEmpty());
        QCOMPARE(window.parameterPane, receive);
        window.connectionAction();
        QCOMPARE(first->state, "closed");
        QCOMPARE(window.active, wavePage);
        window.connectionAction();
        QCOMPARE(first->state, "connected");
        QCOMPARE(window.active, wavePage);
        window.baud->setCurrentText("1200");
        window.applyBaud();
        QCOMPARE(first->appliedBaud, 1200);

        sw::Config otherConfig;
        otherConfig.kind = "demo";
        otherConfig.name = "第二数据源";
        auto other = engine.open(otherConfig);
        const int otherIndex = picker->findData(other->config.key());
        picker->setCurrentIndex(otherIndex);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, otherIndex));
        QCOMPARE(wavePage->channel, other);
        QCOMPARE(window.parameterPane, window.findPane(other));
        QCOMPARE(window.findPane(first), receive);
        other->receive("@wave voltage=3.3\n");
        QTRY_VERIFY(wavePage->instrument->plot->series.contains("voltage"));
        QVERIFY(!wavePage->instrument->plot->series.contains("temp"));
        engine.remove(other->id);
        QVERIFY(!wavePage->channel);
        QCOMPARE(wavePage->sourceKey, otherConfig.key());
        QVERIFY(!window.parameterPane);
        QVERIFY(!window.configStack->isEnabled());
        QCOMPARE(first->state, "connected");
        QVERIFY(picker->currentText().contains("未建立连接"));
        other = engine.open(otherConfig);
        QCOMPARE(wavePage->channel, other);
        window.closeTab(pageIndex(&window, wavePage));
        QCOMPARE(engine.channels.size(), 2);
        QCOMPARE(first->state, "connected");
        QCOMPARE(other->state, "connected");

        auto newMenu = window.newPageMenu();
        const int count = window.tabs->count();
        newMenu->findChild<QAction *>("newReceivePage")->trigger();
        QCOMPARE(window.tabs->count(), count + 1);
        QVERIFY(!window.active->isInstrument());
        QVERIFY(!window.active->channel);
        delete newMenu;
        window.close();
    }

    void independentWidgetPages()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.port->setCurrentText("loop://");
        window.connectionAction();
        auto receive = window.active;
        auto channel = receive->channel;
        auto json = window.newInstrument("structured", channel);
        json->instrument->parser->setCurrentIndex(1);
        json->instrument->freeze->setChecked(true);
        auto detached = window.detachConnection(pageIndex(&window, json), QPoint(200, 180));
        auto xml = window.newInstrument("structured", channel);
        xml->instrument->parser->setCurrentIndex(2);
        QVERIFY(json->pageTitle != xml->pageTitle);
        QCOMPARE(channel->parserMode, "auto");
        channel->receive("{\"temperature\":24}\n<device><voltage>3.3</voltage></device>\n");
        QTRY_COMPARE(xml->instrument->table->topLevelItemCount(), 1);
        QCOMPARE(json->instrument->table->topLevelItemCount(), 0);
        json->instrument->freeze->setChecked(false);
        QTRY_COMPARE(json->instrument->table->topLevelItemCount(), 1);
        QCOMPARE(json->instrument->table->topLevelItem(0)->text(1), "JSON");
        QCOMPARE(xml->instrument->table->topLevelItem(0)->text(1), "XML");
        detached->close();
        window.closeTab(pageIndex(&window, json));
        QCOMPARE(channel->state, "connected");
        QCOMPARE(channel->parserMode, "auto");
        channel->receive("<device><voltage>3.4</voltage></device>\n");
        QTRY_COMPARE(xml->instrument->table->topLevelItemCount(), 2);
        window.closeTab(pageIndex(&window, receive));
        QVERIFY(engine.channels.isEmpty());
        QVERIFY(!xml->channel);
        QCOMPARE(window.tabs->count(), 1);
        QCOMPARE(window.stack->currentWidget(), xml);
        QVERIFY(!window.parameterPane);
        QVERIFY(!window.configStack->isEnabled());
        QCOMPARE(window.connectButton->text(), QString("新建收发页面"));
        window.connectionAction();
        QVERIFY(!window.active->isInstrument());
        QCOMPARE(window.parameterPane, window.active);
        QVERIFY(window.configStack->isEnabled());
        window.closeTab(pageIndex(&window, window.active));
        QCOMPARE(window.stack->currentWidget(), xml);
        window.closeTab(0);
        QCOMPARE(window.tabs->count(), 1);
        QVERIFY(!window.active->isInstrument());
        QVERIFY(!window.active->channel);
        window.close();
    }

    void legacyInstrumentMigration()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"connection":{"kind":"serial","port":"COM77"},"appearance":{"instruments":[{"kind":"waveform","points":1500},{"kind":"waveform","points":2000},{"kind":"structured","parser":2}]},"views":{"serial:com88":{"instruments":[{"kind":"values","frozen":true}]}}})");
        file.close();
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, path);
        QCOMPARE(window.tabs->count(), 5);
        QVERIFY(!window.active->isInstrument());
        QCOMPARE(window.panes[1]->instrument->points->value(), 1500);
        QCOMPARE(window.panes[2]->instrument->points->value(), 2000);
        QVERIFY(window.panes[1]->pageTitle != window.panes[2]->pageTitle);
        QCOMPARE(window.panes[3]->instrument->parser->currentIndex(), 2);
        QCOMPARE(window.panes[1]->sourceKey, "serial:com77");
        QCOMPARE(window.panes[4]->sourceKey, "serial:com88");
        QCOMPARE(window.panes[4]->instrument->kind, "values");
        QVERIFY(window.panes[4]->instrument->freeze->isChecked());
        QVERIFY(engine.channels.isEmpty());
        window.saveSettings();
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto saved = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(saved["widget_pages"].toArray().size(), 4);
        QVERIFY(!saved["appearance"].toObject().contains("instruments"));
        window.close();
    }

    void tabLabelFollowsPaintRectangle()
    {
        sw::TearTabBar bar;
        bar.setStyleSheet(sw::Theme::make(false).stylesheet());
        bar.resize(700, 48);
        bar.addTab(sw::icon("wave", QColor("#668077")), "数值波形");
        bar.addTab("JSON / XML");
        bar.setCurrentIndex(1);
        bar.show();
        QTest::qWait(30);
        QStyleOptionTab option;
        bar.initStyleOption(&option, 0);
        const auto render = [&](const QStyleOptionTab &tab)
        {
            QImage image(700, 80, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            painter.setFont(bar.font());
            bar.style()->drawControl(QStyle::CE_TabBarTab, &tab, &painter, &bar);
            return image.copy(tab.rect);
        };
        const QImage stationary = render(option);
        option.rect.translate(60, 0);
        QCOMPARE(render(option), stationary);
    }

    void widgetSidebarTracksSource()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.port->setCurrentText("loop://");
        window.connectionAction();
        auto first = window.active;
        auto second = window.newConnection();
        window.protocol->setCurrentIndex(3);
        window.bindPort->setValue(0);
        window.connectionAction();
        auto page = window.newInstrument("structured", first->channel);
        QCOMPARE(window.parameterPane, first);
        QCOMPARE(window.protocol->currentIndex(), 0);
        QCOMPARE(window.port->currentText(), QString("loop://"));
        auto picker = page->instrument->source;
        const int index = picker->findData(second->channel->config.key());
        QVERIFY(index > 0);
        picker->setCurrentIndex(index);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, index));
        QCOMPARE(window.parameterPane, second);
        QCOMPARE(window.protocol->currentIndex(), 3);
        window.parameterToggle->setChecked(false);
        QVERIFY(!window.findChild<QScrollArea *>("parameterScroll")->isVisible());
        window.tabs->setCurrentIndex(pageIndex(&window, first));
        window.tabs->setCurrentIndex(pageIndex(&window, page));
        QVERIFY(!window.findChild<QScrollArea *>("parameterScroll")->isVisible());
        window.parameterToggle->setChecked(true);
        QVERIFY(window.findChild<QScrollArea *>("parameterScroll")->isVisible());
        auto detached = window.detachConnection(pageIndex(&window, page), QPoint(180, 180));
        QCOMPARE(detached->parameterPane, second);
        QCOMPARE(detached->protocol->currentIndex(), 3);
        const int firstIndex = picker->findData(first->channel->config.key());
        picker->setCurrentIndex(firstIndex);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, firstIndex));
        window.tabs->setCurrentIndex(pageIndex(&window, first));
        detached->baud->setCurrentText("9600");
        detached->applyBaud();
        QCOMPARE(first->channel->appliedBaud, 9600);
        QCOMPARE(window.baud->currentText(), QString("9600"));
        picker->setCurrentIndex(index);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, index));
        detached->connectionAction();
        QCOMPARE(second->channel->state, "closed");
        QCOMPARE(detached->active, page);
        detached->close();
        window.close();
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

    void parameterSidebarResizesAndRestores()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        const auto settingsPath = directory.filePath("settings.json");
        int savedWidth = 0;
        {
            sw::Window window(&engine, &mcp, settingsPath);
            window.show();
            QTest::qWait(50);
            auto scroll = window.findChild<QScrollArea *>("parameterScroll");
            auto grip = window.parameterGrip;
            QVERIFY(scroll && grip);
            QVERIFY(grip->isVisible());
            QCOMPARE(scroll->width(), 246);
            const int workspaceWidth = window.stack->width();
            const auto center = grip->rect().center();
            const auto global = grip->mapToGlobal(center);
            QTest::mousePress(grip, Qt::LeftButton, {}, center);
            QMouseEvent move(QEvent::MouseMove, QPointF(center + QPoint(120, 0)),
                QPointF(global + QPoint(120, 0)), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(grip, &move);
            QTest::mouseRelease(grip, Qt::LeftButton, {}, grip->rect().center());
            QTRY_COMPARE(scroll->width(), 366);
            QCOMPARE(window.parameterWidth, 366);
            QVERIFY(window.stack->width() < workspaceWidth);
            QVERIFY(window.sidebar->width() <= scroll->viewport()->width());
            QVERIFY(window.port->width() > 214);
            window.parameterToggle->setChecked(false);
            QVERIFY(!grip->isVisible());
            window.parameterToggle->setChecked(true);
            QVERIFY(grip->isVisible());
            QCOMPARE(scroll->width(), 366);

            window.resizeParameters(700);
            window.resize(900, 900);
            QTest::qWait(50);
            QCOMPARE(window.parameterWidth, 700);
            QVERIFY(scroll->width() < 700);
            QVERIFY(window.stack->width() >= 320);
            window.resize(480, 640);
            QTest::qWait(50);
            QCOMPARE(window.stack->width(), 426);
            window.parameterToggle->setChecked(true);
            QVERIFY(grip->isVisible());
            QVERIFY(grip->mapTo(window.centralWidget(), grip->rect().bottomRight()).x() < window.centralWidget()->width());
            window.resizeParameters(300);
            QCOMPARE(scroll->width(), 300);
            QCOMPARE(grip->mapTo(window.centralWidget(), QPoint()).x(), 54 + 300);
            QTest::mouseDClick(grip, Qt::LeftButton);
            QCOMPARE(window.parameterWidth, 246);
            QCOMPARE(scroll->width(), 246);
            window.resize(1400, 900);
            QTest::qWait(50);
            QVERIFY(grip->isVisible());
            QTest::keyClick(grip, Qt::Key_Right, Qt::ShiftModifier);
            QCOMPARE(window.parameterWidth, 286);
            QCOMPARE(scroll->width(), 286);
            window.resizeParameters(420);
            savedWidth = window.parameterWidth;
            const auto captureDirectory = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
            if (!captureDirectory.isEmpty())
            {
                QTest::qWait(50);
                QVERIFY(window.grab().save(QDir(captureDirectory).filePath("sidebar-resized.png")));
            }
            window.close();
        }
        sw::Window restored(&engine, &mcp, settingsPath);
        restored.show();
        QTest::qWait(50);
        QCOMPARE(restored.parameterWidth, savedWidth);
        QCOMPARE(restored.findChild<QScrollArea *>("parameterScroll")->width(), savedWidth);
        restored.close();
    }

    void settingsResizeAndRestore()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.json");
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        const QSize compactSize(520, 380);
        {
            sw::Window window(&engine, &mcp, path);
            window.openSettings();
            auto dialog = window.settingsDialog;
            auto pages = dialog->findChild<sw::DetachableTabs *>("settingsPages");
            auto mcpPages = dialog->findChild<sw::DetachableTabs *>("mcpPages");
            auto clients = dialog->findChild<QScrollArea *>("clientsScroll");
            auto themeChoice = dialog->findChild<QComboBox *>("themeChoice");
            QVERIFY(pages && mcpPages && clients && themeChoice);

            pages->setCurrentIndex(1);
            mcpPages->setCurrentIndex(1);
            dialog->resize(compactSize);
            QTRY_COMPARE(dialog->size(), compactSize);
            QTRY_VERIFY(clients->verticalScrollBar()->maximum() > 0);
            QCOMPARE(clients->horizontalScrollBar()->maximum(), 0);
            auto piButton = clients->widget()->findChildren<QPushButton *>().last();
            clients->ensureWidgetVisible(piButton);
            QTRY_VERIFY(clients->viewport()->rect().contains(piButton->mapTo(clients->viewport(), piButton->rect().center())));
            const auto captureDirectory = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
            if (!captureDirectory.isEmpty())
            {
                QVERIFY(dialog->grab().save(QDir(captureDirectory).filePath("settings-compact.png")));
            }

            pages->setCurrentIndex(0);
            QCoreApplication::processEvents();
            const int compactFieldWidth = themeChoice->width();
            dialog->resize(980, 760);
            QTRY_VERIFY(themeChoice->width() > compactFieldWidth + 200);
            pages->setCurrentIndex(1);
            mcpPages->setCurrentIndex(2);
            QCoreApplication::processEvents();
            const int expandedTableHeight = window.auditTable->height();
            dialog->resize(compactSize);
            QTRY_VERIFY(window.auditTable->height() < expandedTableHeight - 200);
            QCOMPARE(dialog->size(), compactSize);

            pages->setCurrentIndex(2);
            auto about = dialog->findChild<QScrollArea *>("aboutScroll");
            auto version = dialog->findChild<QLabel *>("aboutVersion");
            QVERIFY(about && version);
            QVERIFY(version->text().contains(sw::Version));
            QTRY_VERIFY(about->verticalScrollBar()->maximum() > 0);
            QCOMPARE(about->horizontalScrollBar()->maximum(), 0);
            if (!captureDirectory.isEmpty())
            {
                QVERIFY(dialog->grab().save(QDir(captureDirectory).filePath("about-compact.png")));
            }
            dialog->resize(980, 760);
            QTRY_COMPARE(about->verticalScrollBar()->maximum(), 0);
            QCOMPARE(about->horizontalScrollBar()->maximum(), 0);
            if (!captureDirectory.isEmpty())
            {
                QTest::qWait(60);
                const QRect viewportRect(about->viewport()->mapTo(dialog, QPoint()), about->viewport()->size());
                QVERIFY(dialog->grab().copy(viewportRect).save(QDir(captureDirectory).filePath("about-page.png")));
                QVERIFY(dialog->grab().save(QDir(captureDirectory).filePath("settings-about.png")));
                window.applyTheme(true);
                QCoreApplication::processEvents();
                QVERIFY(dialog->grab().copy(viewportRect).save(QDir(captureDirectory).filePath("about-page-dark.png")));
                window.applyTheme(false);
            }
            dialog->resize(compactSize);
            QTRY_COMPARE(dialog->size(), compactSize);
            dialog->accept();
            QVERIFY(!dialog->isVisible());
            window.close();
        }
        {
            sw::Window restored(&engine, &mcp, path);
            restored.openSettings();
            QTRY_COMPARE(restored.settingsDialog->size(), compactSize);
            restored.settingsDialog->reject();
            restored.close();
        }
    }

    void piAgentConfiguration()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto agentDirectory = directory.filePath("pi-agent");
        QVERIFY(QDir().mkpath(agentDirectory));
        const auto configPath = QDir(agentDirectory).filePath("mcp.json");
        QFile existing(configPath);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.write("{\n  \"mcpServers\": {\n    \"other-server\": {\"url\": \"http://127.0.0.1:9000/mcp\"}\n  }\n}\n");
        existing.close();

        const bool hadPreviousDirectory = qEnvironmentVariableIsSet("PI_CODING_AGENT_DIR");
        const QByteArray previousDirectory = qgetenv("PI_CODING_AGENT_DIR");
        qputenv("PI_CODING_AGENT_DIR", agentDirectory.toUtf8());

        sw::Engine engine;
        sw::McpServer mcp(&engine);
        QVERIFY(mcp.start(0));
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        QLabel result;
        window.configureClient(4, &result);

        if (hadPreviousDirectory)
        {
            qputenv("PI_CODING_AGENT_DIR", previousDirectory);
        }
        else
        {
            qunsetenv("PI_CODING_AGENT_DIR");
        }

        QFile updatedFile(configPath);
        QVERIFY(updatedFile.open(QIODevice::ReadOnly));
        const auto updated = QJsonDocument::fromJson(updatedFile.readAll()).object();
        const auto servers = updated["mcpServers"].toObject();
        QCOMPARE(servers["other-server"].toObject()["url"].toString(), QString("http://127.0.0.1:9000/mcp"));
        QCOMPARE(servers["serial-workbench"].toObject()["url"].toString(), mcp.url());
        QVERIFY(servers["serial-workbench"].toObject()["enabled"].toBool());
        QVERIFY(result.text().contains(configPath));
        QVERIFY(result.text().contains("服务检查通过"));
        QVERIFY(result.text().contains("/reload"));

        window.openSettings();
        auto themeChoice = window.settingsDialog->findChild<QComboBox *>("themeChoice");
        QVERIFY(themeChoice);
        QCOMPARE(themeChoice->itemText(0), QString("浅色主题"));
        QCOMPARE(themeChoice->itemText(1), QString("深色主题"));
        for (const auto label : window.settingsDialog->findChildren<QLabel *>())
        {
            QVERIFY(!label->text().contains("像素掌机"));
            QVERIFY(!label->text().contains("灰绿面板 / 琥珀按键 / 清晰文字"));
        }

        window.close();
    }

    void mcpPermissions()
    {
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        QCOMPARE(mcp.tools().size(), 10);
        QVERIFY_EXCEPTION_THROWN(mcp.call("open_connection", {{"config", QJsonObject {{"kind", "serial"}, {"port", "loop://"}}}}), std::runtime_error);
        engine.allowControl = true;
        mcp.call("open_connection", {{"config", QJsonObject {{"kind", "serial"}, {"port", "loop://"}}}});
        QCOMPARE(engine.channels.size(), 1);
    }
};
QTEST_MAIN(NativeTests)
#include "tests.moc"
