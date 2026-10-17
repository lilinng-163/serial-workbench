// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "agent_skill.h"
#include "usage_guide.h"
#include "serial_control_panel.h"
#include "serial_monitor_dialog.h"
#include <QtTest>

class McpWorkflowTests : public QObject
{
    Q_OBJECT
private:
    static void write(const QString &path, const QByteArray &bytes)
    {
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    static sw::AgentEnvironment environment(const QString &root)
    {
        sw::AgentEnvironment e;
        e.home = root;
        e.appData = root + "/appdata";
        e.localAppData = root + "/local";
        e.searchPaths = QStringList {root + "/bin"};
        return e;
    }

private slots:
    void structuredAndMonitorViewsShareFilters()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->configureParser("auto");
        auto json = window.newInstrument("json", channel);
        auto xml = window.newInstrument("xml", channel);
        auto controls = window.newInstrument("control", channel);
        QVERIFY(controls->instrument->controls);
        auto toggle = controls->instrument->controls->findChild<QPushButton *>("controlMonitorToggle");
        QVERIFY(toggle);
        toggle->setChecked(true);
        auto monitor = controls->instrument->controls->receiveMonitor();
        QVERIFY(monitor);
        window.show();
        channel->receive("{\"state\":\"READY\"}\n{\"state\":\"idle\"}\n<state>READY</state>\n");
        monitor->refresh();
        const auto views = mcp.call("list_search_views", {{"channel_id", channel->id}}).toObject()["views"].toArray();
        QCOMPARE(views.size(), 4);
        for (auto pane : {json, xml})
        {
            const auto result = mcp.call("search_logs", {{"channel_id", channel->id},
                {"view_id", pane->searchViewId}, {"filter", QJsonObject {{"text", "READY"}, {"case_sensitive", true}}}}).toObject();
            QCOMPARE(result["total_matches"].toInt(), 1);
            QCOMPARE(pane->instrument->filter->text(), QString("READY"));
        }
        const auto result = mcp.call("search_logs", {{"channel_id", channel->id},
            {"view_id", monitor->searchViewId}, {"filter", QJsonObject {{"text", "READY"}, {"case_sensitive", true}}}}).toObject();
        QCOMPARE(result["total_matches"].toInt(), 2);
        QCOMPARE(monitor->searchBox()->text(), QString("READY"));
        window.close();
    }

    void guiSearchAndReaderIsolation()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        auto pane = window.findPane(channel);
        QVERIFY(pane);
        window.tabs->setCurrentIndex(window.panes.indexOf(pane));
        window.activate(window.panes.indexOf(pane));
        window.show();
        channel->receive("Temperature alarm\r\nTemperature normal\r\nmotor fault\r\n");
        pane->tick();
        pane->pause->setChecked(true);
        const auto history = engine.journal.read(0, 1000, channel->id);
        const auto cursor = pane->cursor;
        const auto views = mcp.call("list_search_views", {{"channel_id", channel->id}}).toObject()["views"].toArray();
        QCOMPARE(views.size(), 1);
        const auto id = views.first().toObject()["view_id"];
        const QJsonObject filter {{"text", "Temperature && alarm || fault"}, {"logical", true}};
        QJsonObject request {{"channel_id", channel->id}, {"view_id", id}, {"filter", filter}, {"limit", 1}};
        auto result = mcp.call("search_logs", request).toObject();
        QCOMPARE(result["total_matches"].toInt(), 2);
        QCOMPARE(result["rows"].toArray().size(), 1);
        QVERIFY(result["has_more"].toBool());
        QCOMPARE(pane->filter->text(), filter["text"].toString());
        QVERIFY(pane->filter->findChild<QToolButton *>("filterLogic")->isChecked());
        QCOMPARE(pane->cursor, cursor);
        QCOMPARE(engine.journal.read(0, 1000, channel->id), history);
        QCOMPARE(channel->tx, 0);
        const auto captures = qEnvironmentVariable("MCP_WORKFLOW_CAPTURE_DIR");
        if (!captures.isEmpty())
        {
            QDir().mkpath(captures);
            QTest::qWait(220);
            QApplication::processEvents();
            QVERIFY(window.grab().save(captures + "/mcp-search-sync.png"));
        }
        request.remove("filter");
        request["offset"] = 1;
        request["snapshot"] = result["snapshot"];
        result = mcp.call("search_logs", request).toObject();
        QVERIFY(result["rows"].toArray().first().toObject()["text"].toString().contains("fault"));
        pane->filter->setText("normal");
        QVERIFY_EXCEPTION_THROWN(mcp.call("search_logs", request), std::runtime_error);
        request.remove("snapshot");
        request["offset"] = 0;
        QCOMPARE(mcp.call("search_logs", request).toObject()["total_matches"].toInt(), 1);
        request["filter"] = QJsonObject {{"text", "["}, {"regex", true}};
        QVERIFY_EXCEPTION_THROWN(mcp.call("search_logs", request), std::runtime_error);
        QCOMPARE(pane->filter->text(), QString("normal"));
        QVERIFY_EXCEPTION_THROWN(mcp.call("list_search_views", {}), std::runtime_error);
        mcp.setProtocolEnabled("serial", false);
        QVERIFY_EXCEPTION_THROWN(mcp.call("search_logs", request), std::runtime_error);
        window.close();
    }

    void scriptValidationPermissionAndEditorSync()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        auto pane = window.findPane(channel);
        QVERIFY(pane);
        const auto match = QRegularExpression("```c\\n(.*?)\\n```", QRegularExpression::DotMatchesEverythingOption)
            .match(sw::UsageGuide::text("frame_script"));
        QVERIFY(match.hasMatch());
        const auto code = match.captured(1);
        const auto current = mcp.call("get_frame_script", {{"channel_id", channel->id}}).toObject();
        const auto validation = mcp.call("validate_frame_script", {{"code", code},
            {"sample_chunks", QJsonArray {"AA55C4", "09E40CBC0D"}}}).toObject();
        QVERIFY(validation["valid"].toBool());
        QCOMPARE(validation["frames"].toArray().size(), 1);
        QCOMPARE(validation["frames"].toArray().first().toObject()["温度"].toDouble(), 25.0);
        QCOMPARE(mcp.call("get_frame_script", {{"channel_id", channel->id}}).toObject(), current);
        QVERIFY(!mcp.call("validate_frame_script", {{"code", "bad syntax"}}).toObject()["valid"].toBool());
        QJsonObject request {{"channel_id", channel->id}, {"code", code}, {"enabled", true}, {"expected_revision", current["revision"]}};
        QVERIFY_EXCEPTION_THROWN(mcp.call("set_frame_script", request), std::runtime_error);
        engine.allowControl = true;
        window.show();
        pane->openScriptEditor();
        auto editor = pane->scriptDialog->findChild<QPlainTextEdit *>("scriptCode");
        QVERIFY(editor);
        editor->setPlainText(channel->scriptCode + "\n// local unsaved draft");
        QVERIFY_EXCEPTION_THROWN(mcp.call("set_frame_script", request), std::runtime_error);
        editor->setPlainText(channel->scriptCode);
        const auto applied = mcp.call("set_frame_script", request).toObject();
        QVERIFY(applied["enabled"].toBool());
        QCOMPARE(editor->toPlainText(), code);
        QVERIFY(pane->scriptDialog->findChild<QCheckBox *>("scriptEnabled")->isChecked());
        const auto captures = qEnvironmentVariable("MCP_WORKFLOW_CAPTURE_DIR");
        if (!captures.isEmpty())
        {
            QDir().mkpath(captures);
            QTest::qWait(220);
            QApplication::processEvents();
            QVERIFY(pane->scriptDialog->grab().save(captures + "/mcp-script-sync.png"));
        }
        QVERIFY_EXCEPTION_THROWN(mcp.call("set_frame_script", request), std::runtime_error);
        channel->receive(QByteArray::fromHex("AA55C409E40CBC0D"));
        QTRY_COMPARE(channel->scriptFrames, 1);
        QCOMPARE(channel->tx, 0);
        request["expected_revision"] = applied["revision"];
        request["enabled"] = false;
        mcp.call("set_frame_script", request);
        QVERIFY(!channel->scriptEnabled);
        QVERIFY(!pane->scriptDialog->findChild<QCheckBox *>("scriptEnabled")->isChecked());
        mcp.setProtocolEnabled("serial", false);
        QVERIFY_EXCEPTION_THROWN(mcp.call("get_frame_script", {{"channel_id", channel->id}}), std::runtime_error);
        window.close();
    }

    void skillsInstallBackupsAndEnvironmentOverrides()
    {
        QTemporaryDir directory;
        auto e = environment(directory.path());
        e.variables["CODEX_HOME"] = directory.filePath("custom-codex");
        const auto config = directory.filePath("custom-codex/config.toml");
        write(config, "# unrelated user configuration\n");
        for (const auto &target : sw::AgentSkill::targets(e))
        {
            const auto data = target.toObject();
            const auto result = sw::AgentSkill::install(data["agent"].toString(), e);
            QCOMPARE(result["directory"], data["path"]);
            QVERIFY(QFileInfo::exists(data["path"].toString() + "/SKILL.md"));
            QVERIFY(QFileInfo::exists(data["path"].toString() + "/references/frame-script.md"));
            QVERIFY(result["backups"].toArray().isEmpty());
            QVERIFY(sw::AgentSkill::install(data["agent"].toString(), e)["backups"].toArray().isEmpty());
        }
        const auto skill = directory.filePath("custom-codex/skills/serial-workbench/SKILL.md");
        write(skill, "user edited skill");
        const auto updated = sw::AgentSkill::install("codex", e);
        QCOMPARE(updated["backups"].toArray().size(), 1);
        QFile backup(updated["backups"].toArray().first().toString());
        QVERIFY(backup.open(QIODevice::ReadOnly));
        QCOMPARE(backup.readAll(), QByteArray("user edited skill"));
        QFile untouched(config);
        QVERIFY(untouched.open(QIODevice::ReadOnly));
        QCOMPARE(untouched.readAll(), QByteArray("# unrelated user configuration\n"));
        QVERIFY_EXCEPTION_THROWN(sw::AgentSkill::install("unsupported", e), std::runtime_error);
    }

    void endpointChecksAreReadOnlyAndFollowPortChanges()
    {
        QTemporaryDir directory;
        const auto e = environment(directory.path());
        write(directory.filePath("bin/codex.exe"), "fixture");
        const auto agent = sw::AgentConfig::discover(e).first();
        write(agent.configPath, "[mcp_servers.serial-workbench]\nurl = \"http://127.0.0.1:1/mcp\"\nenabled = true\n");
        QFile original(agent.configPath);
        QVERIFY(original.open(QIODevice::ReadOnly));
        const auto bytes = original.readAll();
        original.close();
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        QVERIFY(mcp.start(0));
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.agentEnvironment = [e] { return e; };
        window.checkAgentEndpoints();
        QCOMPARE(mcp.agentChecks.size(), 1);
        QVERIFY(!mcp.agentChecks.first().toObject()["matches"].toBool());
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), bytes);
        original.close();
        sw::AgentConfig::configure(agent, mcp.url(), QCoreApplication::applicationDirPath() + "/SerialWorkbenchMCP.exe");
        window.checkAgentEndpoints();
        QVERIFY(mcp.agentChecks.first().toObject()["matches"].toBool());
        QVERIFY(mcp.changePort(0));
        QVERIFY(!mcp.agentChecks.first().toObject()["matches"].toBool());
        QCOMPARE(mcp.agentChecks.first().toObject()["expected_url"].toString(), mcp.url());
        const auto captures = qEnvironmentVariable("MCP_WORKFLOW_CAPTURE_DIR");
        if (!captures.isEmpty())
        {
            window.openSettings();
            window.settingsDialog->findChild<sw::DetachableTabs *>("settingsPages")->setCurrentIndex(1);
            window.settingsDialog->resize(780, 540);
            QTest::qWait(220);
            QApplication::processEvents();
            QVERIFY(window.settingsDialog->grab().save(captures + "/agent-endpoint-checks.png"));
        }
        window.close();
    }
};

QTEST_MAIN(McpWorkflowTests)
#include "mcp_workflow_tests.moc"
