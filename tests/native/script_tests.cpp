// SPDX-License-Identifier: Apache-2.0

#include "window.h"
#include <QtTest>

namespace
{
const QByteArray Frame = QByteArray::fromHex("AA55C409E40CBC0D");

QJsonArray run(const QString &body, const QByteArray &input = {})
{
    sw::FrameScriptRuntime runtime(sw::FrameScriptProgram::compile(
        "void main(const uint8_t data[], int length) {\n" + body + "\n}"));
    return runtime.feed(input);
}

QJsonArray records(sw::Channel *channel, const QString &peer = "default")
{
    return channel->scriptResults()["sources"].toObject()[peer].toObject()["records"].toArray();
}
}

class ScriptTests : public QObject
{
    Q_OBJECT
private slots:
    void fragmentedFrames_data()
    {
        QTest::addColumn<int>("split");
        for (int i = 0; i <= Frame.size(); ++i)
        {
            QTest::newRow(qPrintable(QString::number(i))) << i;
        }
    }

    void fragmentedFrames()
    {
        QFETCH(int, split);
        sw::FrameScriptRuntime runtime(sw::FrameScriptProgram::compile(sw::FrameScriptProgram::example()));
        auto output = runtime.feed(Frame.left(split));
        for (const auto &frame : runtime.feed(Frame.mid(split))) { output.append(frame); }
        QCOMPARE(output.size(), 1);
        QCOMPARE(output[0].toObject()["温度"].toDouble(), 25.0);
        QCOMPARE(output[0].toObject()["电压"].toDouble(), 3.3);
        QCOMPARE(runtime.bufferedBytes(), 0);
    }

    void resynchronizesAndGroupsFrames()
    {
        sw::FrameScriptRuntime runtime(sw::FrameScriptProgram::compile(sw::FrameScriptProgram::example()));
        auto bad = Frame;
        bad[6] = 0;
        const auto output = runtime.feed(QByteArray::fromHex("00AA01") + bad + Frame + Frame + Frame.left(3));
        QCOMPARE(output.size(), 2);
        QCOMPARE(runtime.bufferedBytes(), 3);
        QCOMPARE(runtime.feed(Frame.mid(3)).size(), 1);
        QCOMPARE(runtime.bufferedBytes(), 0);
    }

    void controlFlowAndNumericTypes()
    {
        const auto fields = run(R"(
            int sum = 0;
            for (int i = 0; i < length; i++)
            {
                if (i == 1) { continue; }
                sum += data[i];
            }
            int n = 0;
            while (true)
            {
                ++n;
                if (n == 3) { break; }
            }
            emit("sum", sum);
            emit("loops", n);
            emit("short", (int16_t)65535);
            emit("byte", (int8_t)255);
            emit("wrapped", (uint8_t)257);
            emit("division", -7 / 2);
            emit("floating", 7 / 2.0);
            emit("literal", 1.5f);
            emit("bits", (0x80 << 8) | 3);
            emit("logical", (false && data[999]) || (true || data[999]));
            emit("ternary", length > 0 ? data[0] : data[999]);
            const int fixed = 12;
            emit("math", max(abs(-fixed), min(20, 3)));
        )", QByteArray::fromHex("010203")).first().toObject();
        QCOMPARE(fields["sum"].toInt(), 4);
        QCOMPARE(fields["loops"].toInt(), 3);
        QCOMPARE(fields["short"].toInt(), -1);
        QCOMPARE(fields["byte"].toInt(), -1);
        QCOMPARE(fields["wrapped"].toInt(), 1);
        QCOMPARE(fields["division"].toInt(), -3);
        QCOMPARE(fields["floating"].toDouble(), 3.5);
        QCOMPARE(fields["literal"].toDouble(), 1.5);
        QCOMPARE(fields["bits"].toInt(), 32771);
        QCOMPARE(fields["logical"].toInt(), 1);
        QCOMPARE(fields["ternary"].toInt(), 1);
        QCOMPARE(fields["math"].toInt(), 12);
    }

    void checksumsAndEndianReads()
    {
        auto fields = run(R"(
            emit("crc", crc16_modbus(data, 0, length));
            emit("sum", sum8(data, 0, length));
            emit("xor", xor8(data, 0, length));
        )", "123456789").first().toObject();
        QCOMPARE(fields["crc"].toInt(), 0x4b37);
        QCOMPARE(fields["sum"].toInt(), 221);
        QCOMPARE(fields["xor"].toInt(), 49);
        fields = run(R"(
            emit("f_le", read_f32_le(data, 0));
            emit("f_be", read_f32_be(data, 4));
            emit("signed", read_i32_be(data, 8));
            emit("unsigned", read_u32_le(data, 8));
            emit("i8", read_i8(data, 8));
            emit("i16", read_i16_be(data, 8));
        )", QByteArray::fromHex("0000803F3F800000FFFFFFFF")).first().toObject();
        QCOMPARE(fields["f_le"].toDouble(), 1.0);
        QCOMPARE(fields["f_be"].toDouble(), 1.0);
        QCOMPARE(fields["signed"].toInt(), -1);
        QCOMPARE(fields["unsigned"].toDouble(), 4294967295.0);
        QCOMPARE(fields["i8"].toInt(), -1);
        QCOMPARE(fields["i16"].toInt(), -1);
    }

    void persistentStateAndMultipleOutputs()
    {
        const auto program = sw::FrameScriptProgram::compile(R"(
            void main(const uint8_t data[], int length)
            {
                static int count = 0;
                if (length == 0) { return; }
                emit("count", ++count);
                emit_frame();
                { static int count = 100; emit("other", ++count); }
            }
        )");
        sw::FrameScriptRuntime runtime(program);
        QVERIFY(runtime.feed({}).isEmpty());
        auto output = runtime.feed("a");
        QCOMPARE(output.size(), 2);
        QCOMPARE(output[0].toObject()["count"].toInt(), 1);
        QCOMPARE(output[1].toObject()["other"].toInt(), 101);
        output = runtime.feed("b");
        QCOMPARE(output[0].toObject()["count"].toInt(), 2);
        QCOMPARE(output[1].toObject()["other"].toInt(), 102);
        sw::FrameScriptRuntime fresh(program);
        QCOMPARE(fresh.feed("a")[0].toObject()["count"].toInt(), 1);
    }

    void rejectsInvalidSyntax_data()
    {
        QTest::addColumn<QString>("body");
        QTest::newRow("array-write") << "data[0] = 2;";
        QTest::newRow("unknown-function") << "system(1);";
        QTest::newRow("bad-arity") << "emit(1);";
        QTest::newRow("missing-semicolon") << "int x = 1";
        QTest::newRow("parameter-shadow") << "int data = 1;";
        QTest::newRow("break-outside-loop") << "break;";
        QTest::newRow("bad-number") << "emit(\"x\", 1e+);";
        QTest::newRow("reserved-name") << "int true = 2;";
        QTest::newRow("invalid-type") << "unsigned float x = 1;";
    }

    void rejectsInvalidSyntax()
    {
        QFETCH(QString, body);
        QVERIFY_EXCEPTION_THROWN(sw::FrameScriptProgram::compile(
            "void main(const uint8_t data[], int length) {\n" + body + "\n}"), sw::ScriptError);
    }

    void runtimeLimits_data()
    {
        QTest::addColumn<QString>("body");
        QTest::newRow("bounds") << "emit(\"x\", data[1]);";
        QTest::newRow("division-zero") << "int x = 0; emit(\"x\", 1 / x);";
        QTest::newRow("infinite-loop") << "while (true) {}";
        QTest::newRow("negative-length") << "buffer_drop(-1);";
        QTest::newRow("buffer-limit") << "while (true) { buffer_append(data); }";
        QTest::newRow("output-limit") << "for (int i = 0; i < 1025; ++i) { emit(\"x\", i); emit_frame(); }";
        QTest::newRow("const-write") << "const int x = 1; x++;";
        QTest::newRow("shift-range") << "emit(\"x\", 1 << 32);";
        QTest::newRow("float-overflow") << "float x = 1e100; emit(\"x\", x);";
        QTest::newRow("integer-precision") << "emit(\"x\", 4294967295 * 4294967295);";
    }

    void runtimeLimits()
    {
        QFETCH(QString, body);
        QVERIFY_EXCEPTION_THROWN(run(body, "a"), sw::ScriptError);
    }

    void sourceLimitsAndCompileFailurePreserveActiveProgram()
    {
        const QString oversized(24000, QChar(0x6e29));
        QVERIFY_EXCEPTION_THROWN(sw::FrameScriptProgram::compile("/*" + oversized + "*/"), sw::ScriptError);
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->configureScript(sw::FrameScriptProgram::example(), true);
        QVERIFY_EXCEPTION_THROWN(channel->configureScript("void main() {}", true), sw::ScriptError);
        QVERIFY(channel->scriptEnabled);
        QCOMPARE(channel->scriptCode, sw::FrameScriptProgram::example());
        channel->receive(Frame);
        QTRY_COMPARE(channel->scriptFrames, 1);
    }

    void channelIsolationAndRawJournal()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->configureScript(sw::FrameScriptProgram::example(), true);
        channel->receive(Frame.left(3), "peer-a");
        channel->receive(Frame, "peer-b");
        channel->receive(Frame.mid(3), "peer-a");
        QTRY_COMPARE(channel->scriptFrames, 2);
        QCOMPARE(channel->state, "connected");
        const auto sources = channel->waveform()["sources"].toObject();
        QCOMPARE(sources.size(), 2);
        QCOMPARE(sources["peer-a"].toObject()["total_samples"].toInt(), 0);
        QCOMPARE(sources["peer-b"].toObject()["total_samples"].toInt(), 0);
        QCOMPARE(records(channel, "peer-a").size(), 1);
        QCOMPARE(records(channel, "peer-a")[0].toObject()["format"].toString(), "script");
        QVERIFY(records(channel, "peer-a")[0].toObject()["event_seq"].toInteger() > 0);
        sw::McpServer mcp(&engine);
        const QJsonObject arguments {{"channel_id", channel->id}};
        QCOMPARE(mcp.call("read_waveform", arguments).toObject(), channel->waveform());
        QCOMPARE(mcp.call("read_structured", arguments).toObject(), channel->structured());
        QCOMPARE(mcp.call("read_script_results", arguments).toObject(), channel->scriptResults());
        QVERIFY(channel->structured()["sources"].toObject()["peer-a"].toObject()["records"].toArray().isEmpty());
        QVERIFY(channel->describe()["script_enabled"].toBool());
        const auto gui = engine.journal.read(0);
        const auto ai = engine.journal.read(0);
        QCOMPARE(gui, ai);
        QByteArray raw;
        for (const auto &value : gui["events"].toArray())
        {
            const auto event = value.toObject();
            if (event["direction"] == "rx") { raw += QByteArray::fromHex(event["hex"].toString().toLatin1()); }
        }
        QCOMPARE(raw, Frame.left(3) + Frame + Frame.mid(3));
        auto other = new sw::Channel(&engine, config);
        other->configureScript(sw::FrameScriptProgram::example(), true);
        other->receive(Frame.mid(3));
        other->receive(Frame);
        QTRY_COMPARE(other->scriptFrames, 1);
        QCOMPARE(channel->scriptFrames, 2);
    }

    void failureAndResetKeepConnectionAlive()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->configureScript("void main(const uint8_t data[], int length) { emit(\"bad\", data[length]); }", true);
        channel->receive(Frame);
        QTRY_VERIFY(!channel->scriptEnabled);
        QVERIFY(channel->scriptError.contains("越界"));
        QCOMPARE(channel->state, "connected");
        QCOMPARE(channel->rx, qint64(8));
        channel->configureScript(sw::FrameScriptProgram::example(), true);
        channel->receive(Frame.left(3));
        channel->close();
        channel->open();
        channel->receive(Frame.mid(3));
        channel->receive(Frame);
        QTRY_COMPARE(channel->scriptFrames, 1);
        QVERIFY(channel->scriptError.isEmpty());
        channel->receive(Frame);
        channel->configureScript(channel->scriptCode, false);
        QTest::qWait(50);
        QCOMPARE(channel->scriptFrames, 0);
        channel->receive("1,2\n");
        QCOMPARE(channel->sources["default"]->samples.back()["values"].toObject().size(), 2);
    }

    void scriptsAreSerialOnly()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        auto pane = window.active;
        pane->applyScript(sw::FrameScriptProgram::example(), true);
        const auto serialSettings = pane->settings();
        QVERIFY(window.scriptControls->isVisible());

        for (int index = 1; index <= 4; ++index)
        {
            window.protocol->setCurrentIndex(index);
            QVERIFY(!pane->supportsScript());
            QVERIFY(!window.scriptControls->isVisible());
            QVERIFY(!window.scriptHint->isVisible());
            QVERIFY(!pane->settings().contains("script_code"));
            pane->openScriptEditor();
            QVERIFY(!pane->scriptDialog);
            QVERIFY_EXCEPTION_THROWN(pane->applyScript(pane->scriptCode, true), sw::ScriptError);
        }
        window.protocol->setCurrentIndex(0);
        QVERIFY(pane->supportsScript());
        QVERIFY(window.scriptControls->isVisible());
        QVERIFY(window.scriptToggle->isChecked());

        for (const auto &kind : QStringList {"tcp-client", "tcp-server", "udp", "ssh", "demo"})
        {
            sw::Config config;
            config.kind = kind;
            auto channel = new sw::Channel(&engine, config);
            QVERIFY_EXCEPTION_THROWN(channel->configureScript(sw::FrameScriptProgram::example(), true), sw::ScriptError);
            QVERIFY(!channel->scriptEnabled);
            pane->setChannel(channel);
            pane->restore(serialSettings);
            QVERIFY(!pane->scriptRequested);
            QVERIFY(!channel->scriptEnabled);
            QVERIFY(!pane->settings().contains("script_enabled"));
            delete channel;
        }
        window.close();
    }

    void mcpHttpReadsScriptResultsWithIndependentCursors()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->configureScript(sw::FrameScriptProgram::example(), true);
        channel->receive(Frame.left(3));
        channel->receive(Frame.mid(3));
        QTRY_COMPARE(channel->scriptFrames, 1);
        sw::McpServer mcp(&engine);
        QVERIFY(mcp.start(0));
        QVERIFY(!engine.allowControl);
        QNetworkAccessManager client;
        QByteArray session;
        int id = 0;
        const auto post = [&](const QString &method, const QJsonObject &params)
        {
            QNetworkRequest request(QUrl(mcp.url()));
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            request.setRawHeader("Accept", "application/json, text/event-stream");
            if (!session.isEmpty()) { request.setRawHeader("Mcp-Session-Id", session); }
            return client.post(request, QJsonDocument(QJsonObject {{"jsonrpc", "2.0"}, {"id", ++id},
                {"method", method}, {"params", params}}).toJson(QJsonDocument::Compact));
        };
        auto initialized = post("initialize", {{"protocolVersion", "2025-03-26"},
            {"clientInfo", QJsonObject {{"name", "Script test"}, {"version", "1"}}}});
        QTRY_VERIFY(initialized->isFinished());
        QCOMPARE(initialized->error(), QNetworkReply::NoError);
        session = initialized->rawHeader("Mcp-Session-Id");
        QVERIFY(!session.isEmpty());
        const auto info = QJsonDocument::fromJson(initialized->readAll()).object()["result"].toObject();
        QVERIFY(info["instructions"].toString().contains("read_script_results"));
        initialized->deleteLater();

        QJsonObject first;
        for (int reader = 0; reader < 2; ++reader)
        {
            auto reply = post("tools/call", {{"name", "read_script_results"},
                {"arguments", QJsonObject {{"channel_id", channel->id}, {"cursors", QJsonObject {{"default", 0}}}}}});
            QTRY_VERIFY(reply->isFinished());
            QCOMPARE(reply->error(), QNetworkReply::NoError);
            const auto result = QJsonDocument::fromJson(reply->readAll()).object()["result"].toObject();
            QVERIFY(!result["isError"].toBool());
            const auto output = result["structuredContent"].toObject();
            if (!reader) { first = output; }
            else { QCOMPARE(output, first); }
            const auto record = output["sources"].toObject()["default"].toObject()["records"].toArray()[0].toObject();
            QCOMPARE(record["format"].toString(), "script");
            QCOMPARE(record["value"].toObject()["温度"].toDouble(), 25.0);
            QCOMPARE(record["value"].toObject()["电压"].toDouble(), 3.3);
            QVERIFY(record["event_seq"].toInteger() > 0);
            reply->deleteLater();
        }
        auto reply = post("tools/call", {{"name", "read_waveform"}, {"arguments", QJsonObject {{"channel_id", channel->id}}}});
        QTRY_VERIFY(reply->isFinished());
        QCOMPARE(reply->error(), QNetworkReply::NoError);
        const auto result = QJsonDocument::fromJson(reply->readAll()).object()["result"].toObject();
        QCOMPARE(result["structuredContent"].toObject(), channel->waveform());
        reply->deleteLater();
    }

    void mixedScriptAndStandardFormats_data()
    {
        QTest::addColumn<bool>("fragmented");
        QTest::newRow("one-receive") << false;
        QTest::newRow("byte-by-byte") << true;
    }

    void mixedScriptAndStandardFormats()
    {
        QFETCH(bool, fragmented);
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->configureScript(sw::FrameScriptProgram::example(), true);
        channel->configureParser("json");
        const QByteArray stream = Frame + "I (1) app_main: tick\r\n"
            + QByteArray("temperature=80,voltage=12\r\n") + QString("{\"温度\":99}\r\n").toUtf8() + Frame;
        if (fragmented)
        {
            for (const char byte : stream) { channel->receive(QByteArray(1, byte)); }
        }
        else { channel->receive(stream); }
        QTRY_COMPARE(channel->scriptFrames, 2);
        const auto samples = channel->waveform()["sources"].toObject()["default"].toObject()["samples"].toArray();
        QCOMPARE(samples.size(), 2);
        QCOMPARE(samples[0].toObject()["values"].toObject()["temperature"].toInt(), 80);
        QCOMPARE(samples[1].toObject()["values"].toObject()["温度"].toInt(), 99);
        QCOMPARE(records(channel).size(), 2);
        QCOMPARE(records(channel)[0].toObject()["value"].toObject()["温度"].toInt(), 25);
        const auto json = channel->structured()["sources"].toObject()["default"].toObject()["records"].toArray();
        QCOMPARE(json.size(), 1);
        QCOMPARE(json[0].toObject()["format"].toString(), "json");
        QCOMPARE(json[0].toObject()["value"].toObject()["温度"].toInt(), 99);
        const auto first = channel->scriptResults(0, 1)["sources"].toObject()["default"].toObject();
        QVERIFY(first["has_more"].toBool());
        const auto second = channel->scriptResults(first["next_cursor"].toInteger(), 1)["sources"].toObject()["default"].toObject();
        QCOMPARE(second["records"].toArray().size(), 1);
        QVERIFY(!second["has_more"].toBool());
        QVERIFY_EXCEPTION_THROWN(channel->scriptResults(0, 1, {{"default", -1}}), std::runtime_error);
        channel->receive("temperature=8");
        channel->configureScript(channel->scriptCode, false);
        channel->receive("0,voltage=12\r\n");
        QCOMPARE(channel->waveform()["sources"].toObject()["default"].toObject()["total_samples"].toInt(), 3);
        QCOMPARE(records(channel).size(), 2);
    }

    void scriptRetentionDoesNotPruneOtherResults()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->configureScript(sw::FrameScriptProgram::example(), true);
        channel->configureParser("json");
        channel->receive("{\"wave\":7}\n");
        channel->receive(Frame.repeated(510));
        QTRY_COMPARE(channel->scriptFrames, 510);
        const auto batch = channel->scriptResults(0, 500)["sources"].toObject()["default"].toObject();
        QCOMPARE(batch["records"].toArray().size(), 500);
        QVERIFY(batch["gap"].toBool());
        QCOMPARE(batch["oldest_cursor"].toInt(), 11);
        QCOMPARE(batch["next_cursor"].toInt(), 510);
        QCOMPARE(channel->structured()["sources"].toObject()["default"].toObject()["records"].toArray().size(), 1);
        QCOMPARE(channel->waveform()["sources"].toObject()["default"].toObject()["total_samples"].toInt(), 1);
    }

    void receivePanelPauseClearCollapseAndPersistence()
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
        pane->applyScript(sw::FrameScriptProgram::example(), true);
        channel->receive(Frame + "plot=42\r\n");
        QTRY_COMPARE(channel->scriptFrames, 1);
        pane->tick();
        QVERIFY(pane->scriptPanel->isVisible());
        QCOMPARE(pane->scriptTable->topLevelItemCount(), 1);
        QCOMPARE(channel->waveform()["sources"].toObject()["default"].toObject()["total_samples"].toInt(), 1);
        pane->pause->setChecked(true);
        channel->receive(Frame);
        QTRY_COMPARE(channel->scriptFrames, 2);
        pane->tick();
        QCOMPARE(pane->scriptTable->topLevelItemCount(), 1);
        pane->pause->setChecked(false);
        pane->tick();
        QCOMPARE(pane->scriptTable->topLevelItemCount(), 2);
        channel->receive(Frame);
        pane->clearView();
        QTRY_COMPARE(channel->scriptFrames, 3);
        pane->tick();
        QCOMPARE(pane->scriptTable->topLevelItemCount(), 0);
        QCOMPARE(records(channel).size(), 3);
        channel->receive(Frame);
        QTRY_COMPARE(channel->scriptFrames, 4);
        pane->tick();
        QCOMPARE(pane->scriptTable->topLevelItemCount(), 1);
        QTest::mouseClick(pane->scriptCollapse, Qt::LeftButton);
        QVERIFY(pane->scriptCollapsed);
        QVERIFY(!pane->scriptTable->isVisible());
        const auto settings = pane->settings();
        QVERIFY(settings["script_collapsed"].toBool());
        QTest::mouseClick(pane->scriptCollapse, Qt::LeftButton);
        QVERIFY(pane->scriptTable->isVisible());
        pane->restore(settings);
        QVERIFY(pane->scriptCollapsed);
        QVERIFY(!pane->scriptTable->isVisible());
        QTest::mouseClick(pane->scriptCollapse, Qt::LeftButton);
        const auto captures = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
        if (!captures.isEmpty())
        {
            QTest::qWait(100);
            QVERIFY(window.grab().save(QDir(captures).filePath("receive-script-results.png")));
            window.applyTheme(true);
            QTest::qWait(100);
            QVERIFY(window.grab().save(QDir(captures).filePath("receive-script-results-dark.png")));
            window.resize(640, 720);
            QTest::qWait(100);
            QVERIFY(window.grab().save(QDir(captures).filePath("receive-script-results-compact.png")));
        }
        window.close();
    }

    void editorPreviewPersistenceAndWidgets()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        auto pane = window.active;
        pane->openScriptEditor();
        auto dialog = pane->scriptDialog;
        QVERIFY(dialog);
        auto editor = dialog->findChild<QPlainTextEdit *>("scriptCode");
        auto runButton = dialog->findChild<QPushButton *>("testScript");
        auto results = dialog->findChild<QTreeWidget *>("scriptTestResults");
        auto status = dialog->findChild<QLabel *>("scriptEditorStatus");
        auto check = dialog->findChild<QPushButton *>("checkScript");
        QVERIFY(editor && runButton && results && status && check);
        runButton->click();
        QTRY_VERIFY(runButton->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 2);
        QVERIFY(results->topLevelItem(0)->text(1).contains("25"));
        QVERIFY(status->text().contains("缓存剩余 0"));

        const auto captureDirectory = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
        auto capture = [&](const QString &name)
        {
            if (!captureDirectory.isEmpty())
            {
                QTest::qWait(100);
                QVERIFY(dialog->grab().save(QDir(captureDirectory).filePath(name)));
            }
        };
        capture("script-editor.png");
        window.applyTheme(true);
        capture("script-editor-dark.png");
        dialog->resize(640, 480);
        capture("script-editor-compact.png");
        window.applyTheme(false);
        editor->setPlainText("void main(const uint8_t data[], int length) {\n emit(\"x\", );\n}");
        check->click();
        QVERIFY(status->text().contains("第 2 行"));
        QCOMPARE(editor->extraSelections().size(), 1);
        editor->setPlainText(sw::FrameScriptProgram::example());
        dialog->findChild<QCheckBox *>()->setChecked(true);
        QPushButton *apply = nullptr;
        for (auto button : dialog->findChildren<QPushButton *>())
        {
            if (button->property("action") == "applyScript") { apply = button; }
        }
        QVERIFY(apply);
        apply->click();
        QTRY_VERIFY(!pane->scriptDialog);
        QVERIFY(pane->scriptRequested);
        QVERIFY(window.scriptToggle->isChecked());
        QCOMPARE(pane->settings()["script_code"].toString(), sw::FrameScriptProgram::example());
        QVERIFY(pane->settings()["script_enabled"].toBool());
        auto draft = window.newConnection();
        QVERIFY(!draft->scriptRequested);
        QVERIFY(!window.scriptToggle->isChecked());
        draft->restore(pane->settings());
        QVERIFY(draft->scriptRequested);
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        pane->setChannel(channel);
        QVERIFY(channel->scriptEnabled);
        auto waveform = window.newInstrument("waveform", channel);
        auto values = window.newInstrument("values", channel);
        auto structured = window.newInstrument("structured", channel);
        channel->receive(Frame);
        QTRY_COMPARE(channel->scriptFrames, 1);
        window.tabs->setCurrentIndex(window.tabs->count() - 3);
        waveform->tick();
        QCOMPARE(waveform->instrument->plot->series.size(), 0);
        window.tabs->setCurrentIndex(window.tabs->count() - 2);
        values->tick();
        QCOMPARE(values->instrument->table->topLevelItemCount(), 0);
        window.tabs->setCurrentIndex(window.tabs->count() - 1);
        structured->tick();
        QCOMPARE(structured->instrument->table->topLevelItemCount(), 0);
        QVERIFY(window.parameterPane == pane);
        QVERIFY(window.scriptToggle->isChecked());
        window.tabs->setCurrentIndex(0);
        pane->tick();
        QVERIFY(pane->scriptPanel->isVisible());
        QCOMPARE(pane->scriptTable->topLevelItemCount(), 1);
        QVERIFY(pane->scriptTable->topLevelItem(0)->text(1).contains("温度 = 25"));
        if (!captureDirectory.isEmpty())
        {
            QVERIFY(window.grab().save(QDir(captureDirectory).filePath("script-workspace.png")));
        }
        window.scriptToggle->setChecked(false);
        QVERIFY(!channel->scriptEnabled);
        window.close();
    }
};

QTEST_MAIN(ScriptTests)
#include "script_tests.moc"
