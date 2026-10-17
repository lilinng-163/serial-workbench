// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "agent_skill.h"
#include "agent_panel.h"
#include "usage_guide.h"
#include "serial_control_panel.h"
#include "serial_monitor_dialog.h"
#include <QtTest>
#include <QNetworkAccessManager>
#include <QNetworkReply>

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

    void scriptTrialInputsSynchronizeAndProtectUnsavedEdits()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        auto pane = window.findPane(channel);
        QVERIFY(pane);
        engine.allowControl = true;
        const auto previous = mcp.call("get_frame_script", {{"channel_id", channel->id}}).toObject();
        const QString code = "void main(const uint8_t data[], int length) { emit(\"count\", length); }";
        QJsonObject request {{"channel_id", channel->id}, {"code", code}, {"enabled", true},
            {"expected_revision", previous["revision"]}, {"sample_chunks", QJsonArray {"10", "20 30"}}};
        const auto applied = mcp.call("set_frame_script", request).toObject();
        QCOMPARE(applied["sample_chunks"].toArray(), QJsonArray({"10", "20 30"}));
        auto hex = pane->scriptDialog->findChild<QPlainTextEdit *>("scriptTestHex");
        QCOMPARE(hex->toPlainText(), QString("10\n20 30"));
        QCOMPARE(pane->settings()["script_test_hex"].toString(), hex->toPlainText());
        auto run = pane->scriptDialog->findChild<QPushButton *>("testScript");
        auto results = pane->scriptDialog->findChild<QTreeWidget *>("scriptTestResults");
        run->click();
        QTRY_VERIFY(run->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 2);
        QVERIFY(results->topLevelItem(1)->text(1).contains("2"));
        QCOMPARE(channel->tx, 0);
        request["expected_revision"] = applied["revision"];
        hex->setPlainText("FF");
        QVERIFY_EXCEPTION_THROWN(mcp.call("set_frame_script", request), std::runtime_error);
        QCOMPARE(pane->scriptTestHex, QString("10\n20 30"));
        hex->setPlainText(pane->scriptTestHex);
        request["sample_chunks"] = QJsonArray {"GG"};
        QVERIFY_EXCEPTION_THROWN(mcp.call("set_frame_script", request), std::runtime_error);
        QCOMPARE(channel->scriptCode, code);
        request["sample_chunks"] = QJsonArray {};
        const auto cleared = mcp.call("set_frame_script", request).toObject();
        QVERIFY(cleared["sample_chunks"].toArray().isEmpty());
        QVERIFY(hex->toPlainText().isEmpty());
        QVERIFY(cleared["revision"] != applied["revision"]);
        const auto captures = qEnvironmentVariable("MCP_WORKFLOW_CAPTURE_DIR");
        if (!captures.isEmpty())
        {
            QDir().mkpath(captures);
            QVERIFY(pane->scriptDialog->grab().save(captures + "/script-without-prompt.png"));
        }
        window.close();
    }

    void skillSettingsDetectSelectAndInstallWithoutMcp()
    {
        QTemporaryDir directory;
        auto e = environment(directory.path());
        write(directory.filePath("bin/codex.cmd"), "@echo off");
        write(directory.filePath("bin/pi.cmd"), "@echo off");
        // A stale configuration directory must not count as an installed Agent.
        write(directory.filePath(".gemini/settings.json"), "{}");
        const auto report = directory.filePath("detected.ini");
        sw::AgentSkill::writeDetection(report, e);
        QSettings detected(report, QSettings::IniFormat);
        QCOMPARE(detected.value("agents/codex").toString(), QString("1"));
        QCOMPARE(detected.value("agents/pi").toString(), QString("1"));
        QCOMPARE(detected.value("agents/gemini").toString(), QString("0"));
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        sw::AgentPanel panel(&mcp, &window, [e] { return e; });
        panel.findChild<QPushButton *>("configureAgentSkills")->click();
        auto dialog = panel.findChild<QDialog *>("agentSkillDialog");
        QVERIFY(dialog);
        auto codex = dialog->findChild<QCheckBox *>("skill_codex");
        auto pi = dialog->findChild<QCheckBox *>("skill_pi");
        auto gemini = dialog->findChild<QCheckBox *>("skill_gemini");
        QVERIFY(codex->isChecked() && pi->isChecked() && !gemini->isChecked());
        QVERIFY(!QFileInfo::exists(directory.filePath(".codex/skills/serial-workbench/SKILL.md")));
        dialog->findChild<QPushButton *>("installSelectedSkills")->click();
        QVERIFY(QFileInfo::exists(directory.filePath(".codex/skills/serial-workbench/SKILL.md")));
        QVERIFY(QFileInfo::exists(directory.filePath(".pi/agent/skills/serial-workbench/SKILL.md")));
        QVERIFY(!QFileInfo::exists(directory.filePath(".gemini/skills/serial-workbench/SKILL.md")));
        QVERIFY(!QFileInfo::exists(directory.filePath(".codex/config.toml")));
        QCOMPARE(dialog->findChildren<QCheckBox *>().size(), 8);
        dialog->findChild<QPushButton *>("installSelectedSkills")->click();
        QVERIFY(dialog->findChild<QLabel *>("skillState_codex")->text().contains("0 个备份"));
        const auto captures = qEnvironmentVariable("MCP_WORKFLOW_CAPTURE_DIR");
        if (!captures.isEmpty())
        {
            QDir().mkpath(captures);
            QTest::qWait(120);
            QVERIFY(dialog->grab().save(captures + "/skill-settings.png"));
            dialog->resize(560, 320);
            QTest::qWait(120);
            QVERIFY(dialog->grab().save(captures + "/skill-settings-compact.png"));
            window.applyTheme(true);
            QVERIFY(dialog->grab().save(captures + "/skill-settings-dark.png"));
        }
        dialog->close();
        window.applyTheme(false);
        window.newInstrument("control");
        auto menu = window.newPageMenu();
        auto widgets = menu->findChild<QMenu *>("newWidgetPage");
        QVERIFY(widgets);
        const auto gridIcon = widgets->menuAction()->icon().pixmap(24, 24).toImage().convertToFormat(QImage::Format_Alpha8);
        const auto controlIcon = window.tabs->tabIcon(window.tabs->count() - 1).pixmap(24, 24).toImage().convertToFormat(QImage::Format_Alpha8);
        QVERIFY(!gridIcon.isNull() && !controlIcon.isNull());
        QVERIFY(gridIcon != controlIcon);
        if (!captures.isEmpty())
        {
            QVERIFY(window.grab().save(captures + "/control-tab-icon.png"));
            menu->popup(window.tabs->mapToGlobal(QPoint(20, 20)));
            QTest::qWait(100);
            QVERIFY(menu->grab().save(captures + "/new-page-menu-icon.png"));
            menu->hide();
        }
        delete menu;
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

    void guiSyncsAiConnectionScriptAndAudit()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        QVERIFY(mcp.start(0));
        engine.allowControl = true;

        // AI opens a serial loop connection; the matching GUI page is activated.
        const auto opened = mcp.call("open_connection", {{"config", QJsonObject {{"kind", "serial"}, {"port", "loop://"}}}}).toObject();
        auto channel = engine.get(opened["id"].toString());
        QVERIFY(channel);
        auto pane = window.findPane(channel);
        QVERIFY(pane);
        QCOMPARE(window.active, pane);
        QCOMPARE(window.stack->currentWidget(), static_cast<QWidget *>(pane));

        // AI writes and enables a frame script; the page is activated and the
        // code is exposed in a non-modal editor.
        const auto match = QRegularExpression("```c\\n(.*?)\\n```", QRegularExpression::DotMatchesEverythingOption)
            .match(sw::UsageGuide::text("frame_script"));
        QVERIFY(match.hasMatch());
        const auto code = match.captured(1);
        const auto current = mcp.call("get_frame_script", {{"channel_id", channel->id}}).toObject();
        const auto applied = mcp.call("set_frame_script", {{"channel_id", channel->id}, {"code", code},
            {"enabled", true}, {"expected_revision", current["revision"]}}).toObject();
        QVERIFY(applied["enabled"].toBool());
        QCOMPARE(pane->scriptCode, code);
        QVERIFY(pane->scriptDialog);
        QCOMPARE(pane->scriptDialog->windowModality(), Qt::NonModal);
        auto editor = pane->scriptDialog->findChild<QPlainTextEdit *>("scriptCode");
        QVERIFY(editor);
        QCOMPARE(editor->toPlainText(), code);
        QCOMPARE(window.active, pane);
        QTRY_VERIFY(window.scriptToggle->isChecked());

        // Every HTTP tool call stores arguments, summary and returned payload.
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
            {"clientInfo", QJsonObject {{"name", "Audit test"}, {"version", "1"}}}});
        QTRY_VERIFY(initialized->isFinished());
        QCOMPARE(initialized->error(), QNetworkReply::NoError);
        session = initialized->rawHeader("Mcp-Session-Id");
        QVERIFY(!session.isEmpty());
        initialized->deleteLater();
        auto reply = post("tools/call", {{"name", "get_status"}, {"arguments", QJsonObject {}}});
        QTRY_VERIFY(reply->isFinished());
        QCOMPARE(reply->error(), QNetworkReply::NoError);
        reply->deleteLater();
        QVERIFY(!mcp.audit.isEmpty());
        const auto entry = mcp.audit.last().toObject();
        QCOMPARE(entry["tool"].toString(), QString("get_status"));
        QVERIFY(!entry["summary"].toString().isEmpty());
        QVERIFY(entry.contains("arguments"));
        QVERIFY(!entry["result"].toString().isEmpty());
        QTRY_VERIFY(window.aiStatus->isVisible());
        QVERIFY(window.aiStatus->text().contains("AI"));
        QTRY_VERIFY(window.aiToast->isVisible());
        QVERIFY(window.aiToastText->text().contains("AI"));

        // The toast opens Settings directly on the audit tab.
        auto toastAll = window.aiToast->findChild<QPushButton *>("aiToastAll");
        QVERIFY(toastAll);
        toastAll->click();
        QVERIFY(window.settingsDialog);
        auto pages = window.settingsDialog->findChild<sw::DetachableTabs *>("settingsPages");
        auto mcpTabs = window.settingsDialog->findChild<sw::DetachableTabs *>("mcpPages");
        QVERIFY(pages && mcpTabs);
        QCOMPARE(pages->currentIndex(), 1);
        QCOMPARE(mcpTabs->currentIndex(), 2);
        QTRY_COMPARE(window.auditTable->topLevelItemCount(), mcp.audit.size());
        auto row = window.auditTable->topLevelItem(window.auditTable->topLevelItemCount() - 1);
        QVERIFY(row);
        const auto stored = row->data(0, Qt::UserRole).toJsonObject();
        QVERIFY(!stored.isEmpty());
        window.showAuditDetail(stored);
        QTRY_VERIFY(window.findChild<QDialog *>("mcpCallDetail"));
        auto detail = window.findChild<QDialog *>("mcpCallDetail");
        auto text = detail->findChild<QPlainTextEdit *>("mcpCallDetailText");
        QVERIFY(text);
        QVERIFY(text->toPlainText().contains("参数"));
        QVERIFY(text->toPlainText().contains("返回"));
        const auto captures = qEnvironmentVariable("MCP_WORKFLOW_CAPTURE_DIR");
        if (!captures.isEmpty())
        {
            QDir().mkpath(captures);
            QTest::qWait(220);
            QApplication::processEvents();
            QVERIFY(window.grab().save(captures + "/ai-gui-sync.png"));
            QVERIFY(pane->scriptDialog->grab().save(captures + "/ai-script-editor.png"));
            QVERIFY(detail->grab().save(captures + "/ai-audit-detail.png"));
        }
        window.close();
    }

    void auditTableFollowsNewestEntries()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.openSettings();
        auto pages = window.settingsDialog->findChild<sw::DetachableTabs *>("settingsPages");
        auto mcpTabs = window.settingsDialog->findChild<sw::DetachableTabs *>("mcpPages");
        QVERIFY(pages && mcpTabs);
        pages->setCurrentIndex(1);
        mcpTabs->setCurrentIndex(2);
        for (int i = 0; i < 40; ++i)
        {
            mcp.audit.append(QJsonObject {{"seq", i + 1}, {"time", "2026-10-17T07:00:00.000Z"}, {"tool", "get_status"},
                {"client", "test"}, {"summary", "get_status"}, {"status", "ok"}, {"duration_ms", 0}});
        }
        mcp.calls = 40;
        QTRY_COMPARE(window.auditTable->topLevelItemCount(), 40);
        auto scroll = window.auditTable->verticalScrollBar();
        QTRY_VERIFY(scroll->maximum() > 0);
        QTRY_COMPARE(scroll->value(), scroll->maximum());

        // Reading an older call must keep both its position and selection when
        // a new call arrives; rebuilding the table used to reset both.
        scroll->setValue(5);
        auto selected = window.auditTable->topLevelItem(8);
        window.auditTable->setCurrentItem(selected);
        scroll->setValue(5);
        const auto top = window.auditTable->itemAt(QPoint(1, 1));
        QVERIFY(top);
        const auto topSequence = top->data(0, Qt::UserRole).toJsonObject()["seq"].toInteger();
        const auto selectedSequence = selected->data(0, Qt::UserRole).toJsonObject()["seq"].toInteger();
        mcp.audit.append(QJsonObject {{"seq", 41}, {"tool", "get_status"}, {"status", "ok"}});
        mcp.calls = 41;
        QTRY_COMPARE(window.auditTable->topLevelItemCount(), 41);
        const auto restoredTop = window.auditTable->itemAt(QPoint(1, 1));
        QVERIFY(restoredTop);
        QCOMPARE(restoredTop->data(0, Qt::UserRole).toJsonObject()["seq"].toInteger(), topSequence);
        QVERIFY(window.auditTable->currentItem());
        QCOMPARE(window.auditTable->currentItem()->data(0, Qt::UserRole).toJsonObject()["seq"].toInteger(), selectedSequence);

        // Evicting the oldest retained row must preserve the same visible call.
        mcp.audit.removeFirst();
        mcp.audit.append(QJsonObject {{"seq", 42}, {"tool", "get_status"}, {"status", "ok"}});
        mcp.calls = 42;
        QTRY_COMPARE(window.auditTable->property("lastSequence").toLongLong(), 42);
        const auto retainedTop = window.auditTable->itemAt(QPoint(1, 1));
        QVERIFY(retainedTop);
        QCOMPARE(retainedTop->data(0, Qt::UserRole).toJsonObject()["seq"].toInteger(), topSequence);
        QVERIFY(window.auditTable->currentItem());
        QCOMPARE(window.auditTable->currentItem()->data(0, Qt::UserRole).toJsonObject()["seq"].toInteger(), selectedSequence);

        // Following resumes once the user returns to the bottom.
        scroll->setValue(scroll->maximum());
        mcp.audit.append(QJsonObject {{"seq", 43}, {"tool", "get_status"}, {"status", "ok"}});
        mcp.calls = 43;
        QTRY_COMPARE(window.auditTable->topLevelItemCount(), 42);
        QTRY_COMPARE(scroll->value(), scroll->maximum());
        window.close();
    }
};

QTEST_MAIN(McpWorkflowTests)
#include "mcp_workflow_tests.moc"
