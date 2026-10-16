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
    void explicitConnectionDoesNotReadHistoricalSsh()
    {
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        auto ssh = makeChannel(engine, "ssh");
        ssh->receive("OLD_SSH_OUTPUT\n");
        ssh->status("closed");
        auto serial = makeChannel(engine, "serial");
        serial->config.port = "COM8";
        serial->receive("ESP32 tick\n");
        const auto page = mcp.call("read_events", {{"channel_id", serial->id}}).toObject();
        QCOMPARE(page["scope"].toString(), QString("channel"));
        QCOMPARE(page["channel_id"].toString(), serial->id);
        QCOMPARE(page["protocol"].toString(), QString("serial"));
        QCOMPARE(page["connection_state"].toString(), QString("connected"));
        QVERIFY(!page["events"].toArray().isEmpty());
        for (const auto &event : page["events"].toArray())
        {
            QCOMPARE(event.toObject()["channel_id"].toString(), serial->id);
        }
        QVERIFY(!QJsonDocument(page).toJson().contains("OLD_SSH_OUTPUT"));

        // Previous successful reads must not bind an implicit default for a later call.
        const QList<QJsonObject> invalid {
            {}, {{"channel_id", ""}}, {{"channel_id", "  "}}, {{"channel_id", QJsonValue::Null}},
            {{"channel_id", 123}}, {{"channel_id", "COM8"}}, {{"channel_id", "*"}}, {{"channel_id", "missing-id"}}
        };
        for (const auto &args : invalid)
        {
            QVERIFY_EXCEPTION_THROWN(mcp.call("read_events", args), std::runtime_error);
        }
        for (const auto &tool : {"read_waveform", "read_structured", "read_script_results"})
        {
            QVERIFY_EXCEPTION_THROWN(mcp.call(tool, {}), std::runtime_error);
        }
        const auto history = mcp.call("read_events", {{"channel_id", ssh->id}}).toObject();
        QCOMPARE(history["connection_state"].toString(), QString("closed"));
        QVERIFY(QJsonDocument(history).toJson().contains("OLD_SSH_OUTPUT"));
        const auto all = mcp.call("read_all_events", {}).toObject();
        QCOMPARE(all["scope"].toString(), QString("all_channels"));
        QVERIFY(QJsonDocument(all).toJson().contains("OLD_SSH_OUTPUT"));
        QVERIFY_EXCEPTION_THROWN(mcp.call("read_all_events", {{"channel_id", serial->id}}), std::runtime_error);
        // Independent GUI/AI reads retain the same records.
        QCOMPARE(mcp.call("read_events", {{"channel_id", serial->id}}).toObject(), page);
        QCOMPARE(engine.journal.read(0, 200, serial->id)["events"], page["events"]);
        mcp.setProtocolEnabled("serial", false);
        QVERIFY_EXCEPTION_THROWN(mcp.call("read_events", {{"channel_id", serial->id}}), std::runtime_error);
    }

    void perConnectionCursorsAndFreshData()
    {
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        auto serial = makeChannel(engine, "serial");
        auto ssh = makeChannel(engine, "ssh");
        serial->receive("old serial\n");
        const auto start = mcp.call("get_status", {}).toObject()["latest_cursor"].toInteger();
        ssh->receive("ssh after snapshot\n");
        serial->receive("new ESP32\n");
        const QJsonObject args {{"channel_id", serial->id}, {"after", start}, {"limit", 1}};
        const auto page = mcp.call("read_events", args).toObject();
        QCOMPARE(page["events"].toArray().size(), 1);
        QVERIFY(page["events"].toArray().first().toObject()["text"].toString().contains("new ESP32"));
        QVERIFY(!page["has_more"].toBool());
        QCOMPARE(page["next_cursor"].toInteger(), engine.journal.sequence);
        QVERIFY(mcp.call("read_events", {{"channel_id", serial->id}, {"after", page["next_cursor"]}})
            .toObject()["events"].toArray().isEmpty());
    }

    void guideAndToolDiscovery()
    {
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        const auto guide = mcp.call("read_usage_guide", {}).toObject();
        QVERIFY(!guide["guide"].toString().isEmpty());
        QCOMPARE(guide["skill_path_scope"].toString(), QString("server_host"));
        QVERIFY(!guide["skill_auto_installed"].toBool());
        const auto script = mcp.call("read_usage_guide", {{"topic", "frame_script"}}).toObject()["guide"].toString();
        const QRegularExpression example("```c\\n(.*?)\\n```", QRegularExpression::DotMatchesEverythingOption);
        const auto match = example.match(script);
        QVERIFY(match.hasMatch());
        sw::FrameScriptRuntime runtime(sw::FrameScriptProgram::compile(match.captured(1)));
        const auto frame = QByteArray::fromHex("AA55C409E40CBC0D");
        QVERIFY(runtime.feed("ESP32 log\n" + frame.left(3)).isEmpty());
        auto output = runtime.feed(frame.mid(3) + frame);
        QCOMPARE(output.size(), 2);
        QCOMPARE(output.first().toObject()["温度"].toDouble(), 25.0);
        QCOMPARE(output.first().toObject()["电压"].toDouble(), 3.3);
        auto bad = frame;
        bad[6] = 0;
        QCOMPARE(runtime.feed(bad + "noise\n" + frame).size(), 1);
        bad = frame;
        bad[7] = 0;
        QCOMPARE(runtime.feed(bad + frame).size(), 1);
        QVERIFY_EXCEPTION_THROWN(mcp.call("read_usage_guide", {{"topic", "unknown"}}), std::runtime_error);
        for (const auto &tool : mcp.tools())
        {
            const auto value = tool.toObject();
            if (value["name"] == "read_events")
            {
                QVERIFY(value["inputSchema"].toObject()["required"].toArray().contains("channel_id"));
            }
        }
        // Guides remain readable with every transport closed to AI and no write permission.
        for (const auto &kind : {"serial", "tcp", "udp", "ssh"}) { mcp.setProtocolEnabled(kind, false); }
        QCOMPARE(mcp.call("read_usage_guide", {}).toObject(), guide);
        QVERIFY(!engine.allowControl);
    }

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
        for (const auto &event : mcp.call("read_all_events", {}).toObject()["events"].toArray())
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
            const auto page = mcp.call("read_all_events", {{"after", cursor}, {"limit", 1}}).toObject();
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
        for (const auto &event : mcp.call("read_all_events", {}).toObject()["events"].toArray())
        {
            foundRemoved = foundRemoved || event.toObject()["channel_id"].toString() == serialId;
        }
        QVERIFY(foundRemoved);
        mcp.setProtocolEnabled("serial", false);
        mcp.setProtocolEnabled("udp", false);
        QVERIFY(mcp.call("read_all_events", {}).toObject()["events"].toArray().isEmpty());
        QVERIFY(!mcp.call("read_all_events", {}).toObject()["has_more"].toBool());
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

    void longPollPinsConnectionAndRejectsMissingId()
    {
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        QVERIFY(mcp.start(0));
        mcp.sessions["source-test"] = "test";
        auto serial = makeChannel(engine, "serial");
        auto ssh = makeChannel(engine, "ssh");
        QNetworkAccessManager network;
        const auto request = [&](const QJsonObject &args)
        {
            QNetworkRequest http(QUrl(mcp.url()));
            http.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            http.setRawHeader("Mcp-Session-Id", "source-test");
            const QJsonObject rpc {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/call"},
                {"params", QJsonObject {{"name", "read_events"}, {"arguments", args}}}};
            return network.post(http, QJsonDocument(rpc).toJson(QJsonDocument::Compact));
        };
        auto missing = request({{"wait_ms", 5000}});
        QSignalSpy rejected(missing, &QNetworkReply::finished);
        QVERIFY(rejected.wait(1000));
        const auto error = QJsonDocument::fromJson(missing->readAll()).object()["result"].toObject();
        QVERIFY(error["isError"].toBool());
        QVERIFY(!error.contains("structuredContent"));
        QVERIFY(QJsonDocument(error).toJson().contains("channel_id"));
        missing->deleteLater();

        const auto start = engine.journal.sequence;
        auto pending = request({{"after", start}, {"channel_id", serial->id}, {"wait_ms", 5000}});
        QSignalSpy received(pending, &QNetworkReply::finished);
        ssh->receive("unrelated SSH\n");
        QTest::qWait(120);
        QVERIFY(!pending->isFinished());
        serial->receive("target ESP32\n");
        QVERIFY(received.wait(1000));
        const auto page = QJsonDocument::fromJson(pending->readAll()).object()["result"].toObject()["structuredContent"].toObject();
        QCOMPARE(page["channel_id"].toString(), serial->id);
        QCOMPARE(page["events"].toArray().size(), 1);
        QCOMPARE(page["events"].toArray().first().toObject()["channel_id"].toString(), serial->id);
        pending->deleteLater();

        auto removed = request({{"after", engine.journal.sequence}, {"channel_id", serial->id}, {"wait_ms", 5000}});
        QSignalSpy stopped(removed, &QNetworkReply::finished);
        QTest::qWait(80);
        QVERIFY(!removed->isFinished());
        engine.remove(serial->id);
        ssh->receive("do not switch to SSH\n");
        QVERIFY(stopped.wait(1000));
        const auto result = QJsonDocument::fromJson(removed->readAll()).object()["result"].toObject();
        QVERIFY(result["isError"].toBool());
        QVERIFY(!result.contains("structuredContent"));
        removed->deleteLater();
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
        const auto request = [&](const QJsonObject &arguments, const QString &tool = "read_events")
        {
            QNetworkRequest http(QUrl(mcp.url()));
            http.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            http.setRawHeader("Mcp-Session-Id", "protocol-test");
            const QJsonObject rpc {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/call"},
                {"params", QJsonObject {{"name", tool}, {"arguments", arguments}}}};
            return network.post(http, QJsonDocument(rpc).toJson(QJsonDocument::Compact));
        };
        const auto after = engine.journal.sequence;
        auto reply = request({{"after", after}, {"wait_ms", 1000}}, "read_all_events");
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
