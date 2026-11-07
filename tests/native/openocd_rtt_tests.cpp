// SPDX-License-Identifier: Apache-2.0
#include "openocd.h"
#include "openocd_dialog.h"
#include "mcp.h"
#include "window_chrome.h"
#include <QtTest>
#include <QtNetwork>

class OpenOcdRttTests : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir directory;

    QJsonObject fixture()
    {
        const auto root = directory.filePath("scripts");
        QDir().mkpath(root + "/board");
        QFile file(root + "/board/fixture.cfg");
        if (file.open(QIODevice::WriteOnly)) { file.write("# Test only\n"); }
#ifdef Q_OS_WIN
        const auto executable = QCoreApplication::applicationDirPath() + "/fake_rtt_openocd.exe";
#else
        const auto executable = QCoreApplication::applicationDirPath() + "/fake_rtt_openocd";
#endif
        return {{"executable", executable}, {"scripts_dir", root}, {"mode", "board"},
            {"board", "board/fixture.cfg"}, {"rtt_address", "0x2000002C"}, {"timeout_ms", 5000}};
    }

    static QByteArray raw(const QJsonObject &page)
    {
        QByteArray result;
        for (const auto &record : page["records"].toArray())
        {
            result += QByteArray::fromHex(record.toObject()["hex"].toString().toLatin1());
        }
        return result;
    }

    static QJsonObject external(const QTcpServer &server)
    {
        return {{"rtt_mode", "external"}, {"rtt_port", server.serverPort()}, {"executable", "missing-openocd"}};
    }

    static void cancel(sw::OpenOcdService &service)
    {
        service.cancel(service.state()["job_id"].toString());
    }

private slots:
    void initTestCase()
    {
        qApp->setStyle("Fusion");
        qApp->setFont(sw::Theme::interfaceFont());
        sw::installWindowChrome();
        QVERIFY(directory.isValid());
    }

    void plansDoNotFlashOrResetImplicitly()
    {
        sw::OpenOcdService service;
        service.restore(fixture());
        auto plan = service.preview("rtt");
        auto script = plan["script"].toString();
        QVERIFY(!plan["writes_flash"].toBool());
        QVERIFY(plan["persistent"].toBool());
        QVERIFY(script.contains("rtt setup 0x2000002c 0x100 \"SEGGER RTT\""));
        QVERIFY(script.contains("bindto 127.0.0.1"));
        QVERIFY(!script.contains("reset run"));
        QVERIFY(!script.contains("reset halt"));
        QVERIFY(script.endsWith("echo SW_OPENOCD_COMPLETE\n"));
        QCOMPARE(plan["steps"].toArray().first().toString(), QString("init"));
        service.configure({{"rtt_startup", "reset_run"}, {"rtt_gdb_port", 3333}, {"rtt_channel", 2}}, service.revision());
        script = service.preview("rtt")["script"].toString();
        QVERIFY(script.contains("gdb_port 3333"));
        QVERIFY(script.contains("reset run\n    sleep 500"));
        QVERIFY(script.contains("@RTT_PORT@ 2"));
        service.configure({{"rtt_id", "[oops]$x"}}, service.revision());
        QVERIFY(service.preview("rtt")["script"].toString().contains("\\[oops\\]\\$x"));
        const auto document = service.exportConfiguration();
        sw::OpenOcdService restored;
        restored.importConfiguration(document, restored.revision());
        QCOMPARE(restored.configuration(), service.configuration());
        QVERIFY(!restored.busy());
    }

    void invalidConfigurationAndOccupiedPorts()
    {
        sw::OpenOcdService service;
        service.restore(fixture());
        for (const auto &patch : {QJsonObject {{"rtt_port", 65536}}, QJsonObject {{"rtt_channel", -1}},
            QJsonObject {{"rtt_poll_ms", 1.5}}, QJsonObject {{"rtt_id", ""}},
            QJsonObject {{"rtt_startup", "halt"}}, QJsonObject {{"rtt_mode", "shell"}}})
        {
            QVERIFY_EXCEPTION_THROWN(service.configure(patch, service.revision()), std::runtime_error);
        }
        for (const auto &patch : {QJsonObject {{"rtt_address", "0x2000;shutdown"}},
            QJsonObject {{"rtt_size", "0"}}, QJsonObject {{"rtt_size", "0x1000001"}},
            QJsonObject {{"rtt_address", "0xffffffffffffffff"}},
            QJsonObject {{"rtt_port", 3333}, {"rtt_gdb_port", 3333}}})
        {
            service.restore(fixture());
            service.configure(patch, service.revision());
            QVERIFY_EXCEPTION_THROWN(service.preview("rtt"), std::runtime_error);
            QVERIFY(!service.busy());
        }
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        service.restore(fixture());
        service.configure({{"rtt_port", server.serverPort()}}, service.revision());
        QVERIFY_EXCEPTION_THROWN(service.start("rtt", service.revision()), std::runtime_error);
        QVERIFY(!service.busy());
        service.configure({{"rtt_mode", "external"}, {"rtt_port", 0}}, service.revision());
        QVERIFY_EXCEPTION_THROWN(service.preview("rtt"), std::runtime_error);
    }

    void managedSessionPersistsAndReleasesProbe()
    {
        sw::Engine engine;
        auto service = engine.openocd;
        service->restore(fixture());
        service->start("rtt", service->revision());
        QTRY_COMPARE_WITH_TIMEOUT(service->readRtt()["status"].toString(), QString("connected"), 3000);
        QVERIFY(service->ownsProbe());
        QTRY_VERIFY_WITH_TIMEOUT(raw(service->readRtt()).contains("\x1b[32m[INFO]"), 1500);
        auto other = engine.createOpenOcdView("Other", fixture());
        QVERIFY_EXCEPTION_THROWN(other->start("rtt", other->revision()), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(service->configure({{"rtt_channel", 1}}, service->revision()), std::runtime_error);
        // The initial 5-second timeout must not terminate a connected session.
        QTest::qWait(5100);
        QVERIFY(service->busy());
        QCOMPARE(service->readRtt()["status"].toString(), QString("connected"));
        const auto before = service->readRtt()["latest_cursor"].toInteger();
        cancel(*service);
        QTRY_VERIFY_WITH_TIMEOUT(!service->busy(), 2000);
        QVERIFY(!service->ownsProbe());
        QCOMPARE(service->state()["status"].toString(), QString("cancelled"));
        QVERIFY(service->readRtt()["received_bytes"].toInteger() > 0);
        service->start("rtt", service->revision());
        QTRY_VERIFY_WITH_TIMEOUT(service->readRtt()["latest_cursor"].toInteger() > before, 3000);
        QVERIFY(!service->readRtt()["gap"].toBool());
        cancel(*service);
        QTRY_VERIFY_WITH_TIMEOUT(!service->busy(), 2000);
    }

    void externalUtf8AnsiCursorsAndDisconnect()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        sw::OpenOcdService service;
        service.configure(external(server), service.revision());
        QVERIFY(service.preview("rtt")["external"].toBool());
        service.start("rtt", service.revision());
        QTRY_VERIFY(server.hasPendingConnections());
        auto socket = server.nextPendingConnection();
        QVERIFY(!service.ownsProbe());
        const auto bytes = QString("\x1b[31m错误\x1b[0m\r\n").toUtf8();
        socket->write(bytes.left(6)); // Split within the first Chinese character.
        QTRY_COMPARE(service.readRtt()["received_bytes"].toInteger(), qint64(6));
        socket->write(bytes.mid(6));
        QTRY_COMPARE(service.readRtt()["received_bytes"].toInteger(), qint64(bytes.size()));
        const auto first = service.readRtt(0, 1);
        const auto second = service.readRtt(first["next_cursor"].toInteger());
        QCOMPARE(raw(first) + raw(second), bytes);
        QString decoded;
        for (const auto &record : service.readRtt()["records"].toArray()) { decoded += record.toObject()["text"].toString(); }
        QCOMPARE(decoded.toUtf8(), bytes);
        QCOMPARE(raw(service.readRtt()), bytes); // Independent reader starts at 0.
        socket->disconnectFromHost();
        QTRY_VERIFY(!service.busy());
        QCOMPARE(service.state()["status"].toString(), QString("failed"));
        QCOMPARE(raw(service.readRtt()), bytes);
        QVERIFY(server.isListening());
    }

    void externalCancelAndBoundedJournal()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        sw::OpenOcdService service;
        service.configure(external(server), service.revision());
        service.start("rtt", service.revision());
        QTRY_VERIFY(server.hasPendingConnections());
        auto socket = server.nextPendingConnection();
        socket->write(QByteArray(700 * 1024, 'x'));
        QTRY_COMPARE_WITH_TIMEOUT(service.readRtt()["received_bytes"].toInteger(), qint64(700 * 1024), 5000);
        const auto page = service.readRtt(0, 200);
        QVERIFY(page["gap"].toBool());
        QVERIFY(raw(page).size() <= 512 * 1024);
        QVERIFY(raw(page).size() > 0);
        cancel(service);
        QVERIFY(!service.busy());
        QVERIFY(server.isListening());
        QCOMPARE(service.state()["status"].toString(), QString("cancelled"));
        QVERIFY_EXCEPTION_THROWN(service.readRtt(-1), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(service.readRtt(0, 201), std::runtime_error);
    }

    void mcpAndGuiShareReadOnlyHistory()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        auto service = engine.openocd;
        service->restore(external(server));
        sw::OpenOcdDialog dialog(service);
        dialog.setAttribute(Qt::WA_DeleteOnClose, false);
        dialog.resize(1200, 820);
        dialog.show();
        QVERIFY_EXCEPTION_THROWN(mcp.call("read_openocd_rtt", {}), std::runtime_error);
        mcp.setProtocolEnabled("openocd", true);
        QVERIFY(mcp.call("read_openocd_rtt", {}).toObject()["records"].toArray().isEmpty());
        QVERIFY_EXCEPTION_THROWN(mcp.call("start_openocd", {{"view_id", service->viewId},
            {"revision", service->revision()}, {"operation", "rtt"}}), std::runtime_error);
        engine.allowControl = true;
        mcp.call("start_openocd", {{"view_id", service->viewId}, {"revision", service->revision()}, {"operation", "rtt"}});
        QTRY_VERIFY(server.hasPendingConnections());
        auto socket = server.nextPendingConnection();
        auto terminal = dialog.findChild<sw::TerminalView *>("rttTerminal");
        auto pause = dialog.findChild<QPushButton *>("rttPause");
        QTRY_VERIFY(terminal->isVisible());
        socket->write("\x1b[31mERROR\x1b[0m\r\n");
        QTRY_VERIFY(terminal->text().contains("ERROR"));
        const auto red = terminal->grid.first().first().foreground;
        QVERIFY(red.isValid());
        QVERIFY(red.red() > red.green());
        pause->click();
        socket->write("paused sample\r\n");
        QTRY_VERIFY(raw(service->readRtt()).contains("paused sample"));
        QVERIFY(!terminal->text().contains("paused sample"));
        pause->click();
        QTRY_VERIFY(terminal->text().contains("paused sample"));
        dialog.findChild<QPushButton *>("rttClear")->click();
        QVERIFY(!terminal->text().contains("ERROR"));
        engine.allowControl = false;
        QVERIFY(raw(mcp.call("read_openocd_rtt", {{"view_id", service->viewId}}).toObject()).contains("ERROR"));
        QVERIFY(!dialog.findChild<QPushButton *>("rttPopout")->isWindow());
        dialog.findChild<QPushButton *>("rttPopout")->click();
        auto popup = dialog.findChild<QDialog *>("rttFloatingWindow");
        QVERIFY(popup);
        QTRY_VERIFY(popup->isVisible());
        auto floatingTerminal = popup->findChild<sw::TerminalView *>("rttTerminal");
        QTRY_VERIFY(floatingTerminal->text().contains("ERROR"));
        popup->resize(800, 450);
        popup->close();
        QVERIFY(service->busy());
        mcp.setProtocolEnabled("openocd", false);
        QVERIFY_EXCEPTION_THROWN(mcp.call("read_openocd_rtt", {}), std::runtime_error);
        cancel(*service);
        QVERIFY(server.isListening());
    }

    void visualCapture()
    {
        const auto captures = qEnvironmentVariable("SW_RTT_CAPTURES");
        if (captures.isEmpty()) { QSKIP("Optional actual window capture"); }
        QVERIFY(QDir().mkpath(captures));
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        sw::OpenOcdService service;
        service.restore(fixture());
        sw::OpenOcdDialog dialog(&service);
        dialog.setAttribute(Qt::WA_DeleteOnClose, false);
        dialog.resize(1260, 850);
        auto theme = sw::Theme::make(false);
        dialog.setPalette(theme.palette());
        dialog.setStyleSheet(theme.stylesheet());
        dialog.setAppearance(theme, sw::Theme::defaultDataFont());
        dialog.findChild<QTabWidget *>("openocdConfigurationTabs")->setCurrentIndex(3);
        dialog.findChild<QTabWidget *>("openocdResultTabs")->setCurrentIndex(2);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QTest::qWait(200);
        QVERIFY(dialog.grab().save(captures + "/rtt-settings-light.png"));
        service.configure(external(server), service.revision());
        service.start("rtt", service.revision());
        QTRY_VERIFY(server.hasPendingConnections());
        auto socket = server.nextPendingConnection();
        socket->write("\x1b[32m[INFO] SEGGER RTT ready\x1b[0m\r\n\x1b[33m[WARN] temperature: 56.2 C\x1b[0m\r\n\x1b[31m[ERROR] example fault\x1b[0m\r\n");
        auto terminal = dialog.findChild<sw::TerminalView *>("rttTerminal");
        QTRY_VERIFY(terminal->text().contains("example fault"));
        QVERIFY(dialog.grab().save(captures + "/rtt-colors-light.png"));
        theme = sw::Theme::make(true);
        dialog.setPalette(theme.palette());
        dialog.setStyleSheet(theme.stylesheet());
        dialog.setAppearance(theme, sw::Theme::defaultDataFont());
        QTest::qWait(200);
        QVERIFY(dialog.grab().save(captures + "/rtt-colors-dark.png"));
        QVERIFY(!dialog.findChild<QPushButton *>("rttPopout")->isWindow());
        dialog.findChild<QPushButton *>("rttPopout")->click();
        auto popup = dialog.findChild<QDialog *>("rttFloatingWindow");
        QTRY_VERIFY(popup && popup->isVisible());
        QTRY_VERIFY(popup->findChild<sw::TerminalView *>("rttTerminal")->text().contains("example fault"));
        QVERIFY(popup->grab().save(captures + "/rtt-floating.png"));
        popup->close();
        dialog.resize(820, 650);
        QTest::qWait(200);
        QVERIFY(dialog.grab().save(captures + "/rtt-compact.png"));
        cancel(service);
    }
};

QTEST_MAIN(OpenOcdRttTests)
#include "openocd_rtt_tests.moc"
