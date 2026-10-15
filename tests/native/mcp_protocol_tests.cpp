// SPDX-License-Identifier: Apache-2.0

#include "window.h"
#include <QtTest>

static sw::Channel *makeChannel(sw::Engine &engine, const QString &kind)
{
    sw::Config config;
    config.kind = kind;
    config.port = "loop://";
    config.sshHost = "test-host";
    auto channel = new sw::Channel(&engine, config);
    engine.channels.insert(channel->id, channel);
    channel->status("connected");
    return channel;
}

class McpProtocolTests : public QObject
{
    Q_OBJECT

private slots:
    void protocolGates_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("group");
        QTest::newRow("serial") << QString("serial") << QString("serial");
        QTest::newRow("tcp-client") << QString("tcp-client") << QString("tcp");
        QTest::newRow("tcp-server") << QString("tcp-server") << QString("tcp");
        QTest::newRow("udp") << QString("udp") << QString("udp");
        QTest::newRow("ssh") << QString("ssh") << QString("ssh");
    }

    void protocolGates()
    {
        QFETCH(QString, kind);
        QFETCH(QString, group);
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        auto hidden = makeChannel(engine, kind);
        auto demo = makeChannel(engine, "demo");
        hidden->configureParser("auto");
        hidden->receive("@wave temperature=25\n{\"secret\":123}\n");
        demo->receive("public demo\n");
        const QJsonObject args {{"channel_id", hidden->id}};
        QVERIFY(mcp.call("read_waveform", args).toObject()["sources"].toObject().contains("default"));
        QVERIFY(mcp.call("read_structured", args).toObject()["sources"].toObject().contains("default"));

        mcp.setProtocolEnabled(group, false);
        QCOMPARE(mcp.call("list_connections", {}).toArray().size(), 1);
        const auto status = mcp.call("get_status", {}).toObject();
        QCOMPARE(status["connection_count"].toInt(), 1);
        QCOMPARE(status["connected_count"].toInt(), 1);
        QCOMPARE(status["counts_by_protocol"].toObject()[kind].toInt(), 0);
        QCOMPARE(status["protocol_access"].toObject()[group].toBool(), false);
        for (const auto &event : mcp.call("read_events", {}).toObject()["events"].toArray())
        {
            QCOMPARE(event.toObject()["channel_id"].toString(), demo->id);
        }
        for (const auto &tool : {"read_events", "read_waveform", "read_structured", "read_script_results"})
        {
            QVERIFY_EXCEPTION_THROWN(mcp.call(tool, args), std::runtime_error);
        }
        if (kind == "serial")
        {
            QVERIFY_EXCEPTION_THROWN(mcp.call("list_serial_ports", {}), std::runtime_error);
        }

        engine.allowControl = true;
        QVERIFY_EXCEPTION_THROWN(mcp.call("send_data", {{"channel_id", hidden->id}, {"payload", "blocked"}}), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(mcp.call("close_connection", args), std::runtime_error);
        const auto count = engine.channels.size();
        QVERIFY_EXCEPTION_THROWN(mcp.call("open_connection", {{"config", hidden->config.json()}}), std::runtime_error);
        QCOMPARE(engine.channels.size(), count);
        QCOMPARE(hidden->state, QString("connected"));
        // GUI reception and its independent history remain available.
        hidden->receive("still receiving locally\n");
        QVERIFY(engine.journal.read(0, 100, hidden->id)["events"].toArray().size() >= 3);

        mcp.setProtocolEnabled(group, true);
        QCOMPARE(mcp.call("list_connections", {}).toArray().size(), 2);
        QVERIFY(!mcp.call("read_events", args).toObject()["events"].toArray().isEmpty());
        QVERIFY(mcp.call("read_waveform", args).isObject());
        QVERIFY(mcp.call("read_structured", args).isObject());
        QVERIFY(mcp.call("read_script_results", args).isObject());
        mcp.call("close_connection", args);
        QCOMPARE(hidden->state, QString("closed"));
    }

    void paginationAndRemovedHistory()
    {
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        auto serial = makeChannel(engine, "serial");
        auto udp = makeChannel(engine, "udp");
        for (int i = 0; i < 8; ++i)
        {
            serial->receive("private\n");
            udp->receive("public\n");
        }
        const auto serialId = serial->id;
        engine.remove(serialId);
        mcp.setProtocolEnabled("serial", false);

        qint64 cursor = 0;
        int received = 0;
        bool more = true;
        while (more)
        {
            const auto page = mcp.call("read_events", {{"after", cursor}, {"limit", 1}}).toObject();
            const auto events = page["events"].toArray();
            QCOMPARE(events.size(), 1);
            QCOMPARE(events.first().toObject()["channel_id"].toString(), udp->id);
            QVERIFY(page["next_cursor"].toInteger() > cursor);
            cursor = page["next_cursor"].toInteger();
            more = page["has_more"].toBool();
            QVERIFY(++received <= 9);
        }
        QCOMPARE(received, 9);
        QCOMPARE(cursor, engine.journal.sequence);
        mcp.setProtocolEnabled("serial", true);
        bool foundRemoved = false;
        for (const auto &event : mcp.call("read_events", {}).toObject()["events"].toArray())
        {
            foundRemoved = foundRemoved || event.toObject()["channel_id"].toString() == serialId;
        }
        QVERIFY(foundRemoved);
        mcp.setProtocolEnabled("serial", false);
        mcp.setProtocolEnabled("udp", false);
        QVERIFY(mcp.call("read_events", {}).toObject()["events"].toArray().isEmpty());
        QVERIFY(!mcp.call("read_events", {}).toObject()["has_more"].toBool());
    }

    void settingsPersistAndSyncControls()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.json");
        {
            sw::Engine engine;
            sw::McpServer mcp(&engine);
            sw::Window window(&engine, &mcp, path);
            window.openSettings();
            for (const auto &kind : {"serial", "tcp", "udp", "ssh"})
            {
                auto enabled = window.settingsDialog->findChild<QCheckBox *>(QString("mcpAccess_") + kind);
                QVERIFY(enabled);
                QVERIFY(enabled->isChecked());
                enabled->click();
                QVERIFY(!mcp.protocolEnabled(kind));
            }
            QFile saved(path);
            QVERIFY(saved.open(QIODevice::ReadOnly));
            const auto protocols = QJsonDocument::fromJson(saved.readAll()).object()["mcp_protocols"].toObject();
            saved.close();
            QCOMPARE(protocols.size(), 4);
            for (const auto &value : protocols) { QVERIFY(!value.toBool()); }
            // Server policy updates also refresh an already open settings page.
            mcp.setProtocolEnabled("tcp-client", true);
            QVERIFY(window.settingsDialog->findChild<QCheckBox *>("mcpAccess_tcp")->isChecked());
            QVERIFY(mcp.protocolEnabled("tcp-server"));
            window.saveSettings();
        }
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window restored(&engine, &mcp, path);
        QVERIFY(!mcp.protocolEnabled("serial"));
        QVERIFY(mcp.protocolEnabled("tcp-client"));
        QVERIFY(mcp.protocolEnabled("tcp-server"));
        QVERIFY(!mcp.protocolEnabled("udp"));
        QVERIFY(!mcp.protocolEnabled("ssh"));
        QVERIFY(mcp.protocolEnabled("demo"));
        QVERIFY(!engine.allowControl);
    }

    void longPollHonorsLivePermissions()
    {
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        QVERIFY(mcp.start(0));
        mcp.sessions["protocol-test"] = "test";
        auto serial = makeChannel(engine, "serial");
        auto udp = makeChannel(engine, "udp");
        mcp.setProtocolEnabled("serial", false);
        QNetworkAccessManager network;
        const auto request = [&](const QJsonObject &arguments)
        {
            QNetworkRequest http(QUrl(mcp.url()));
            http.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            http.setRawHeader("Mcp-Session-Id", "protocol-test");
            const QJsonObject rpc {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/call"},
                {"params", QJsonObject {{"name", "read_events"}, {"arguments", arguments}}}};
            return network.post(http, QJsonDocument(rpc).toJson(QJsonDocument::Compact));
        };
        const auto after = engine.journal.sequence;
        auto reply = request({{"after", after}, {"wait_ms", 1000}});
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QTimer::singleShot(40, serial, [serial] { serial->receive("private\n"); });
        QTest::qWait(160);
        QVERIFY(!reply->isFinished());
        udp->receive("public\n");
        QVERIFY(finished.wait(1000));
        QCOMPARE(reply->error(), QNetworkReply::NoError);
        const auto result = QJsonDocument::fromJson(reply->readAll()).object()["result"].toObject();
        const auto events = result["structuredContent"].toObject()["events"].toArray();
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.first().toObject()["channel_id"].toString(), udp->id);
        reply->deleteLater();

        auto pending = request({{"after", engine.journal.sequence}, {"channel_id", udp->id}, {"wait_ms", 5000}});
        QSignalSpy stopped(pending, &QNetworkReply::finished);
        QTest::qWait(80);
        QVERIFY(!pending->isFinished());
        mcp.setProtocolEnabled("udp", false);
        QVERIFY(stopped.wait(1000));
        const auto denied = QJsonDocument::fromJson(pending->readAll()).object()["result"].toObject();
        QVERIFY(denied["isError"].toBool());
        QVERIFY(!denied.contains("structuredContent"));
        pending->deleteLater();

        auto immediate = request({{"channel_id", serial->id}, {"wait_ms", 5000}});
        QSignalSpy rejected(immediate, &QNetworkReply::finished);
        QVERIFY(rejected.wait(1000));
        QVERIFY(QJsonDocument::fromJson(immediate->readAll()).object()["result"].toObject()["isError"].toBool());
        immediate->deleteLater();
    }
};

QTEST_MAIN(McpProtocolTests)
#include "mcp_protocol_tests.moc"
