// SPDX-License-Identifier: Apache-2.0
#include "agent_panel.h"
#include "window.h"
#include <QtTest>

class AgentTests : public QObject
{
    Q_OBJECT
    static void write(const QString &path, const QByteArray &text)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(text) != text.size()) { qFatal("Fixture write failed"); }
    }

    static sw::AgentEnvironment environment(const QTemporaryDir &directory)
    {
        sw::AgentEnvironment result;
        result.home = directory.path();
        result.appData = directory.filePath("Roaming");
        result.localAppData = directory.filePath("Local");
        result.searchPaths = {directory.filePath("bin")};
        return result;
    }

    static void installAll(const sw::AgentEnvironment &env)
    {
        for (const auto &command : QStringList {"codex", "claude", "opencode", "pi", "hermes", "openclaw", "grok", "gemini"})
        {
            write(env.searchPaths.first() + "/" + command + ".cmd", "installation fixture");
        }
        write(env.localAppData + "/Claude/app-1.0/Claude.exe", "desktop fixture");
    }

private slots:
    void installedDetectionIgnoresConfigResidue()
    {
        QTemporaryDir directory;
        auto env = environment(directory);
        write(directory.filePath(".codex/config.toml"), "model = 'test'\n");
        for (const auto &agent : sw::AgentConfig::discover(env)) { QVERIFY(!agent.installed()); }
        installAll(env);
        const auto clients = sw::AgentConfig::discover(env);
        QCOMPARE(clients.size(), 9);
        for (const auto &agent : clients) { QVERIFY2(agent.installed(), qPrintable(agent.name)); }
        env.variables["CODEX_HOME"] = directory.filePath("custom-codex");
        env.variables["PI_CODING_AGENT_DIR"] = directory.filePath("custom-pi");
        env.variables["OPENCODE_CONFIG"] = directory.filePath("custom-opencode.jsonc");
        env.variables["HERMES_HOME"] = directory.filePath("custom-hermes");
        env.variables["GROK_HOME"] = directory.filePath("custom-grok");
        const auto custom = sw::AgentConfig::discover(env);
        QCOMPARE(custom[0].configPath, directory.filePath("custom-codex/config.toml"));
        QCOMPARE(custom[4].configPath, directory.filePath("custom-pi/mcp.json"));
        QCOMPARE(custom[2].configPath, directory.filePath("custom-opencode.jsonc"));
        QCOMPARE(custom[5].configPath, directory.filePath("custom-hermes/config.yaml"));
        QCOMPARE(custom[7].configPath, directory.filePath("custom-grok/config.toml"));
    }

    void configurations_data()
    {
        QTest::addColumn<int>("index");
        QTest::addColumn<QByteArray>("original");
        const QByteArray toml("model = 'keep'\n# preserve me\n[mcp_servers.other]\nurl = 'https://other.invalid/mcp'\n");
        const QByteArray json("{\n// preserve me\n\"otherSetting\": 42, \"mcpServers\": {\"other\": {\"url\": \"https://other.invalid/mcp\"}},\n}\n");
        QTest::newRow("codex") << 0 << toml;
        QTest::newRow("claude-code") << 1 << json;
        QTest::newRow("opencode-v1") << 2 << QByteArray("{ // preserve me\n\"mcp\": {\"other\": {\"type\": \"remote\", \"url\": \"https://other.invalid/mcp\"}},\"otherSetting\": 42 }");
        QTest::newRow("opencode-v2") << 2 << QByteArray("{ // preserve me\n\"mcp\": {\"servers\": {\"other\": {\"type\": \"remote\", \"url\": \"https://other.invalid/mcp\"}}},\"otherSetting\": 42 }");
        QTest::newRow("desktop") << 3 << json;
        QTest::newRow("pi") << 4 << json;
        QTest::newRow("hermes") << 5 << QByteArray("model: keep\n# preserve me\nmcp_servers:\n  other:\n    url: https://other.invalid/mcp\notherSetting: 42\n");
        QTest::newRow("openclaw-json5") << 6 << QByteArray("{ // preserve me\notherSetting: 42, mcp: { servers: { other: {url: 'https://other.invalid/mcp'}, }, }, }");
        QTest::newRow("grok") << 7 << toml;
        QTest::newRow("gemini") << 8 << json;
    }

    void configurations()
    {
        QFETCH(int, index);
        QFETCH(QByteArray, original);
        QTemporaryDir directory;
        auto env = environment(directory);
        installAll(env);
        const auto agent = sw::AgentConfig::discover(env)[index];
        const auto bridge = directory.filePath("SerialWorkbenchMCP.exe");
        write(bridge, "bridge fixture");
        write(agent.configPath, original);
        const QString url = "http://127.0.0.1:23456/mcp";
        QVERIFY(sw::AgentConfig::configure(agent, url, bridge).contains("文件校验通过"));
        QCOMPARE(sw::AgentConfig::check(agent, url, bridge), QString("配置地址一致"));
        QFile file(agent.configPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto updated = file.readAll();
        file.close();
        QVERIFY(updated.contains("preserve me"));
        QVERIFY(updated.contains("https://other.invalid/mcp"));
        QVERIFY(updated.contains("23456"));
        const auto backups = QDir(QFileInfo(agent.configPath).absolutePath()).entryList({QFileInfo(agent.configPath).fileName() + ".*.bak"});
        QCOMPARE(backups.size(), 1);
        QFile backup(QFileInfo(agent.configPath).absolutePath() + "/" + backups.first());
        QVERIFY(backup.open(QIODevice::ReadOnly));
        QCOMPARE(backup.readAll(), original);
        backup.close();
        sw::AgentConfig::configure(agent, "http://127.0.0.1:23457/mcp", bridge);
        QCOMPARE(sw::AgentConfig::check(agent, "http://127.0.0.1:23457/mcp", bridge), QString("配置地址一致"));
        QCOMPARE(sw::AgentConfig::check(agent, url, bridge), QString("需要更新配置"));

        const auto profile = directory.filePath("配置 with 'quotes'/settings.json");
        sw::AgentConfig::configure(agent, QString(), bridge, profile);
        QCOMPARE(sw::AgentConfig::check(agent, QString(), bridge, profile), QString("配置地址一致"));
        QCOMPARE(sw::AgentConfig::check(agent, QString(), bridge, profile + ".other"), QString("需要更新配置"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto automatic = file.readAll();
        file.close();
        QVERIFY(automatic.contains("preserve me"));
        QVERIFY(automatic.contains("https://other.invalid/mcp"));
        QVERIFY(automatic.contains("--settings"));
        const auto before = QDir(QFileInfo(agent.configPath).absolutePath()).entryList({"*.bak"});
        sw::AgentConfig::configure(agent, QString(), bridge, profile);
        QCOMPARE(QDir(QFileInfo(agent.configPath).absolutePath()).entryList({"*.bak"}), before);
    }

    void invalidConfigNeverOverwritten()
    {
        QTemporaryDir directory;
        auto env = environment(directory);
        installAll(env);
        const auto clients = sw::AgentConfig::discover(env);
        for (int index : {0, 1, 5, 6, 7})
        {
            const auto agent = clients[index];
            const QByteArray invalid("broken = [\n");
            write(agent.configPath, invalid);
            QVERIFY_EXCEPTION_THROWN(sw::AgentConfig::configure(agent, "http://127.0.0.1:5555/mcp", "unused"), std::exception);
            QFile file(agent.configPath);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), invalid);
        }
        auto missing = clients[1];
        missing.executable.clear();
        QVERIFY_EXCEPTION_THROWN(sw::AgentConfig::configure(missing, "http://127.0.0.1:5555/mcp", "unused"), std::exception);
    }

    void mcpPortSwitchPreservesSession()
    {
        sw::Engine engine;
        sw::McpServer server(&engine);
        QVERIFY(server.start(0));
        const auto initial = server.serverPort();
        const auto journal = engine.journal.session;
        auto channel = engine.waveformDemo();
        const auto count = channel->sources.first()->count;
        QTcpServer occupied;
        QVERIFY(occupied.listen(QHostAddress::LocalHost, 0));
        QVERIFY(!server.changePort(occupied.serverPort()));
        QCOMPARE(server.serverPort(), initial);
        QVERIFY(server.isListening());
        QVERIFY(!server.error.isEmpty());
        QVERIFY(server.changePort(0));
        QVERIFY(server.serverPort() != initial);
        QVERIFY(server.error.isEmpty());
        QCOMPARE(engine.journal.session, journal);
        QCOMPARE(channel->sources.first()->count, count);
        sw::McpServer fallback(&engine);
        QVERIFY(fallback.startAvailable(occupied.serverPort()));
        QVERIFY(fallback.serverPort() != occupied.serverPort());
    }

    void axesIndependentAndPersisted()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer server(&engine);
        sw::Window window(&engine, &server, directory.filePath("settings.json"));
        auto page = window.newInstrument("waveform");
        auto plot = page->instrument->plot;
        plot->resize(800, 400);
        plot->series["a"] = {{1, 0}, {1000, 1}};
        const auto before = plot->plotArea();
        const auto xRange = plot->horizontalRange();
        const auto yRange = plot->verticalRange();
        page->instrument->waveXAxis->setChecked(false);
        QVERIFY(!plot->xAxisVisible());
        QVERIFY(plot->yAxisVisible());
        QVERIFY(plot->plotArea().height() > before.height());
        page->instrument->waveYAxis->setChecked(false);
        QVERIFY(plot->plotArea().width() > before.width());
        QCOMPARE(plot->horizontalRange(), xRange);
        QCOMPARE(plot->verticalRange(), yRange);
        const auto settings = page->settings();
        auto restored = window.newInstrument("waveform");
        restored->restore(settings);
        QVERIFY(!restored->instrument->plot->xAxisVisible());
        QVERIFY(!restored->instrument->plot->yAxisVisible());
        window.close();
    }

    void portSettingsApplyAndPersist()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer server(&engine);
        QVERIFY(server.start(0));
        const auto path = directory.filePath("settings.json");
        sw::Window window(&engine, &server, path);
        window.openSettings();
        auto spin = window.settingsDialog->findChild<QSpinBox *>("mcpPort");
        auto apply = window.settingsDialog->findChild<QPushButton *>("applyMcpPort");
        auto automatic = window.settingsDialog->findChild<QPushButton *>("availableMcpPort");
        auto address = window.settingsDialog->findChild<QLineEdit *>("mcpAddress");
        QVERIFY(spin && apply && automatic && address);
        const auto initial = server.serverPort();
        QTcpServer occupied;
        QVERIFY(occupied.listen(QHostAddress::LocalHost, 0));
        spin->setValue(occupied.serverPort());
        apply->click();
        QCOMPARE(server.serverPort(), initial);
        QCOMPARE(address->text(), server.url());
        automatic->click();
        QVERIFY(server.serverPort() != initial);
        QCOMPARE(spin->value(), int(server.serverPort()));
        QCOMPARE(address->text(), server.url());
        QFile settings(path);
        QVERIFY(settings.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(settings.readAll()).object()["mcp_port"].toInt(), int(server.serverPort()));
        window.close();
    }

    void logosBlendWithTheme()
    {
        QTemporaryDir directory;
        auto env = environment(directory);
        sw::Engine engine;
        sw::McpServer server(&engine);
        sw::AgentPanel panel(&server, nullptr, [env] { return env; });
        panel.setObjectName("root");
        panel.resize(840, 1020);
        panel.show();
        for (bool dark : {false, true, false})
        {
            const auto theme = sw::Theme::make(dark);
            panel.setStyleSheet(theme.stylesheet());
            QApplication::processEvents();
            auto logos = panel.findChildren<QLabel *>("agentLogo");
            QCOMPARE(logos.size(), 9);
            for (auto logo : logos)
            {
                const auto image = logo->pixmap().toImage().convertToFormat(QImage::Format_ARGB32);
                QVERIFY(!image.isNull());
                QCOMPARE(logo->pixmap().devicePixelRatio(), logo->devicePixelRatioF());
                bool transparent = false;
                for (int y = 0; y < image.height(); ++y)
                {
                    for (int x = 0; x < image.width(); ++x)
                    {
                        transparent |= image.pixelColor(x, y).alpha() == 0;
                    }
                }
                QVERIFY2(transparent, qPrintable(logo->parent()->property("agentId").toString()));
                const auto rendered = logo->grab().toImage();
                QCOMPARE(rendered.pixelColor(0, 0).alpha(), 0);
                if (logo->parent()->property("agentId") == "codex")
                {
                    bool visible = false;
                    for (int y = 0; y < image.height(); ++y)
                    {
                        for (int x = 0; x < image.width(); ++x)
                        {
                            const auto pixel = image.pixelColor(x, y);
                            if (pixel.alpha() < 200) { continue; }
                            QCOMPARE(pixel.lightness() > 128, dark);
                            visible = true;
                        }
                    }
                    QVERIFY(visible);
                }
            }
            const auto capture = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
            if (!capture.isEmpty())
            {
                QDir().mkpath(capture);
                QVERIFY(panel.grab().save(capture + (dark ? "/agent-logos-dark.png" : "/agent-logos-light.png")));
            }
        }
    }

    void cardsAndBatchUpdate()
    {
        QTemporaryDir directory;
        auto env = environment(directory);
        write(env.searchPaths.first() + "/codex.cmd", "fixture");
        write(env.searchPaths.first() + "/gemini.cmd", "fixture");
        sw::Engine engine;
        sw::McpServer server(&engine);
        QVERIFY(server.start(0));
        sw::AgentPanel panel(&server, nullptr, [env] { return env; });
        panel.resize(720, 740);
        panel.show();
        auto grid = panel.findChild<QGridLayout *>("agentGrid");
        QVERIFY(grid);
        QCOMPARE(grid->columnCount(), 2);
        QCOMPARE(grid->count(), 9);
        for (auto logo : panel.findChildren<QLabel *>("agentLogo")) { QVERIFY(!logo->pixmap().isNull()); }
        QVERIFY(panel.findChild<QPushButton *>("configure_codex")->isEnabled());
        QVERIFY(!panel.findChild<QPushButton *>("check_pi")->isEnabled());
        QVERIFY(panel.findChild<QCheckBox *>("agentAutoStart")->isChecked());
        panel.configureAll();
        QTRY_VERIFY(QFileInfo::exists(directory.filePath(".gemini/settings.json")));
        QVERIFY(QFileInfo::exists(directory.filePath(".codex/config.toml")));
        QVERIFY(!QFileInfo::exists(directory.filePath(".pi/agent/mcp.json")));
        QTRY_COMPARE(server.sessions.size(), 0);
        const auto capture = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
        if (!capture.isEmpty()) { QVERIFY(panel.grab().save(capture + "/agent-panel.png")); }
        panel.close();
    }
};
QTEST_MAIN(AgentTests)
#include "agent_tests.moc"
