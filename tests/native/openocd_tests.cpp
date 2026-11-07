// SPDX-License-Identifier: Apache-2.0
#include "openocd.h"
#include "openocd_dialog.h"
#include "window.h"
#include "window_chrome.h"
#include <QtTest>

class OpenOcdTests : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir directory;

    static void write(const QString &path, const QByteArray &bytes = "# process fixture\n")
    {
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    QJsonObject fixture()
    {
#ifdef Q_OS_WIN
        const QString executable = QCoreApplication::applicationDirPath() + "/fake_openocd.exe";
#else
        const QString executable = QCoreApplication::applicationDirPath() + "/fake_openocd";
#endif
        const QString scripts = directory.filePath("scripts");
        write(scripts + "/board/fixture.cfg");
        write(scripts + "/interface/stlink.cfg");
        write(scripts + "/interface/cmsis-dap.cfg");
        write(scripts + "/target/stm32f1x.cfg");
        write(scripts + "/target/stm32f4x.cfg");
        write(scripts + "/target/vendor/nested.cfg");
        const QString image = directory.filePath("firmware } { $name [shutdown];.bin");
        write(image, "test firmware bytes");
        return {{"executable", executable}, {"scripts_dir", scripts}, {"mode", "board"}, {"board", "board/fixture.cfg"},
            {"images", QJsonArray {QJsonObject {{"path", image}, {"address", "0x08000000"}}}}};
    }

    static QString output(sw::OpenOcdService &service)
    {
        QString result;
        qint64 after = 0;
        while (true)
        {
            const auto state = service.state(after, 200);
            for (const auto &value : state["records"].toArray()) { result += value.toObject()["text"].toString(); }
            after = state["next_cursor"].toInteger();
            if (!state["has_more"].toBool()) { break; }
        }
        return result;
    }

    static int pageIndex(sw::Window &window, sw::Pane *page)
    {
        for (int index = 0; index < window.tabs->count(); ++index)
        {
            if (window.tabs->tabData(index).value<QObject *>() == page) { return index; }
        }
        return -1;
    }

    static sw::Pane *pageFor(sw::Window &window, const QString &id)
    {
        for (auto page : window.panes)
        {
            if (page->instrument && page->instrument->flasherService && page->instrument->flasherService->viewId == id) { return page; }
        }
        return nullptr;
    }

private slots:
    void initTestCase()
    {
        qApp->setStyle("Fusion");
        qApp->setFont(sw::Theme::interfaceFont());
        sw::installWindowChrome();
    }

    void init()
    {
        QVERIFY(directory.isValid());
        qunsetenv("SW_FAKE_OPENOCD_MODE");
        qunsetenv("SW_FAKE_OPENOCD_LABEL");
    }

    void cleanup()
    {
        qunsetenv("SW_FAKE_OPENOCD_MODE");
        qunsetenv("SW_FAKE_OPENOCD_LABEL");
    }

    void configurationRevisionAndValidation()
    {
        sw::OpenOcdService service;
        service.restore(fixture());
        const auto original = service.revision();
        service.configure({{"speed_khz", 1500}}, original);
        QCOMPARE(service.revision(), original + 1);
        QCOMPARE(service.configuration()["speed_khz"].toInt(), 1500);
        QVERIFY_EXCEPTION_THROWN(service.configure({{"speed_khz", 100}}, original), std::runtime_error);
        const auto revision = service.revision();
        for (const auto &patch : {QJsonObject {{"unknown", true}}, QJsonObject {{"speed_khz", "100"}},
            QJsonObject {{"speed_khz", 1.5}}, QJsonObject {{"timeout_ms", 1000}}, QJsonObject {{"startup", "shell"}},
            QJsonObject {{"images", QJsonArray {"not an object"}}}})
        {
            QVERIFY_EXCEPTION_THROWN(service.configure(patch, revision), std::runtime_error);
            QCOMPARE(service.revision(), revision);
        }
        QCOMPARE(service.configuration()["speed_khz"].toInt(), 1500);
    }

    void configurationImportIsAtomic()
    {
        sw::OpenOcdService source;
        source.restore(fixture());
        source.title = "电机控制板";
        source.configure({{"adapter_serial", "PROBE-A"}, {"startup", "reset_halt"}}, source.revision());
        const auto document = source.exportConfiguration();
        QCOMPARE(document["format"].toString(), QString("serial-workbench.openocd"));

        sw::OpenOcdService target;
        const auto viewId = target.viewId;
        const auto revision = target.revision();
        target.importConfiguration(document, revision);
        QCOMPARE(target.configuration(), source.configuration());
        QCOMPARE(target.title, source.title);
        QCOMPARE(target.viewId, viewId);
        QCOMPARE(target.revision(), revision + 1);
        QCOMPARE(target.state()["status"].toString(), QString("idle"));
        QVERIFY(target.state()["job_id"].toString().isEmpty());

        for (const auto &patch : {QJsonObject {{"format", "other"}}, QJsonObject {{"version", 2}},
            QJsonObject {{"configuration", QJsonObject {{"timeout_ms", 1000}}}},
            QJsonObject {{"configuration", QJsonObject {{"command", "program"}}}}, QJsonObject {{"title", ""}},
            QJsonObject {{"unexpected", true}}})
        {
            auto invalid = document;
            for (auto entry = patch.begin(); entry != patch.end(); ++entry) { invalid[entry.key()] = entry.value(); }
            const auto current = target.revision();
            QVERIFY_EXCEPTION_THROWN(target.importConfiguration(invalid, current), std::runtime_error);
            QCOMPARE(target.exportConfiguration(), document);
            QCOMPARE(target.revision(), current);
        }
        QVERIFY_EXCEPTION_THROWN(target.importConfiguration(document, revision), std::runtime_error);

        auto partial = document;
        partial["configuration"] = QJsonObject {{"mode", "target"}, {"target", "target/stm32f1x.cfg"}};
        target.importConfiguration(partial, target.revision());
        QVERIFY(target.configuration()["images"].toArray().isEmpty());
        QVERIFY(target.configuration()["adapter_serial"].toString().isEmpty());
        target.restore(fixture());
        qputenv("SW_FAKE_OPENOCD_MODE", "hang");
        target.start("connect", target.revision());
        const auto running = target.exportConfiguration();
        QVERIFY_EXCEPTION_THROWN(target.importConfiguration(document, target.revision()), std::runtime_error);
        QCOMPARE(target.exportConfiguration(), running);
        target.cancel(target.state()["job_id"].toString());
        QTRY_VERIFY_WITH_TIMEOUT(!target.busy(), 2500);
    }

    void configurationFileAndMcpUseSameDocument()
    {
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("file-settings.json"));
        mcp.setProtocolEnabled("openocd", true);
        engine.allowControl = true;
        auto service = engine.openocd;
        service->configure(fixture(), service->revision());
        service->configure({{"adapter_serial", "PROBE-A"}}, service->revision());
        auto other = engine.createOpenOcdView("Other board");
        const auto otherConfiguration = other->configuration();
        window.newInstrument("openocd", nullptr, true, {}, service->viewId);
        auto panel = pageFor(window, service->viewId)->instrument->flasher;
        const auto path = directory.filePath("配置/烧录方案.json");
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        panel->saveConfiguration(path);
        QFile saved(path);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const auto document = QJsonDocument::fromJson(saved.readAll()).object();
        saved.close();
        QCOMPARE(document, mcp.call("export_openocd_configuration", {{"view_id", service->viewId}}).toObject());
        service->configure({{"adapter_serial", "CHANGED"}}, service->revision());
        panel->loadConfiguration(path, service->revision());
        QCOMPARE(service->exportConfiguration(), document);
        QCOMPARE(panel->findChild<QLineEdit *>("openocdAdapterSerial")->text(), QString("PROBE-A"));
        auto imported = document;
        imported["title"] = "Imported board";
        mcp.call("import_openocd_configuration", {{"view_id", service->viewId}, {"revision", service->revision()}, {"document", imported}});
        QCOMPARE(pageFor(window, service->viewId)->pageTitle, QString("Imported board"));
        QCOMPARE(other->configuration(), otherConfiguration);
        QVERIFY(!service->busy() && !other->busy());
        QVERIFY(service->state()["job_id"].toString().isEmpty());
        const auto unchanged = service->exportConfiguration();
        const auto revision = service->revision();
        write(path, "{not valid json}");
        QVERIFY_EXCEPTION_THROWN(panel->loadConfiguration(path, revision), std::runtime_error);
        write(path, QByteArray(1024 * 1024 + 1, ' '));
        QVERIFY_EXCEPTION_THROWN(panel->loadConfiguration(path, revision), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(panel->loadConfiguration(path + ".missing", revision), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(panel->saveConfiguration(directory.path()), std::runtime_error);
        QCOMPARE(service->exportConfiguration(), unchanged);
        QCOMPARE(service->revision(), revision);
    }

    void catalogIsReadOnlyAndDiscoversNestedConfigs()
    {
        sw::OpenOcdService service;
        service.restore(fixture());
        const auto catalog = service.catalog();
        QVERIFY(catalog["error"].toString().isEmpty());
        QVERIFY(catalog["targets"].toArray().contains("target/vendor/nested.cfg"));
        QVERIFY(catalog["interfaces"].toArray().contains("interface/stlink.cfg"));
        QVERIFY(!service.busy());
        QVERIFY(!service.catalog("missing-openocd-executable")["error"].toString().isEmpty());
        QCOMPARE(service.state()["status"].toString(), QString("idle"));
    }

    void defaultFlowAndPathQuoting()
    {
        sw::OpenOcdService service;
        service.restore(fixture());
        const auto preview = service.preview("flash");
        const auto script = preview["script"].toString();
        const auto steps = preview["steps"].toArray();
        QCOMPARE(steps.size(), 1);
        QVERIFY(steps[0].toString().startsWith("program \""));
        QVERIFY(steps[0].toString().endsWith(" verify reset 0x8000000"));
        QVERIFY(!script.contains("\n    init\n"));
        QVERIFY(script.contains("\\} \\{ \\$name \\[shutdown\\];.bin"));
        QVERIFY(script.contains("gdb_port disabled"));
        QVERIFY(script.contains("shutdown error"));
        QVERIFY(preview["writes_flash"].toBool());
        QCOMPARE(service.state()["status"].toString(), QString("idle"));
        service.configure({{"adapter_serial", "id\"\\$[x]{\nend"}}, service.revision());
        QVERIFY(service.preview("flash")["script"].toString().contains("id\\\"\\\\\\$\\[x\\]\\{\\n"));
    }

    void explicitInitialization_data()
    {
        QTest::addColumn<QString>("startup");
        QTest::addColumn<QString>("command");
        QTest::newRow("board hooks") << "reset_init" << "reset init";
        QTest::newRow("reset and halt") << "reset_halt" << "reset halt";
        QTest::newRow("attach and halt") << "halt" << "halt";
    }

    void explicitInitialization()
    {
        QFETCH(QString, startup);
        QFETCH(QString, command);
        sw::OpenOcdService service;
        service.restore(fixture());
        service.configure({{"startup", startup}, {"verify", false}, {"reset_run", false}}, service.revision());
        const auto steps = service.preview("flash")["steps"].toArray();
        QCOMPARE(steps.size(), 3);
        QCOMPARE(steps[0].toString(), QString("init"));
        QCOMPARE(steps[1].toString(), command);
        QVERIFY(steps[2].toString().startsWith("flash write_image erase "));
        QVERIFY(steps[2].toString().endsWith(" 0x8000000 bin"));
        const auto check = service.preview("connect");
        QVERIFY(!check["writes_flash"].toBool());
        QCOMPARE(check["steps"].toArray().size(), 2);
    }

    void embeddedAddressesAndInvalidFirmware()
    {
        sw::OpenOcdService service;
        service.restore(fixture());
        const QString elf = directory.filePath("firmware.elf");
        write(elf, "fixture elf");
        service.configure({{"images", QJsonArray {QJsonObject {{"path", elf}, {"address", ""}}}}}, service.revision());
        QVERIFY(service.preview("flash")["steps"].toArray()[0].toString().endsWith(" verify reset"));
        service.configure({{"startup", "reset_init"}}, service.revision());
        const auto script = service.preview("flash")["script"].toString();
        QVERIFY(script.contains(" 0 elf\n"));
        service.configure({{"images", QJsonArray {QJsonObject {{"path", elf}, {"address", "0x1000"}}}}}, service.revision());
        QVERIFY_EXCEPTION_THROWN(service.preview("flash"), std::runtime_error);
        const auto bin = fixture()["images"].toArray()[0].toObject()["path"].toString();
        for (const auto &address : {QString(), QString("0x1000;shutdown"), QString("-1"), QString("0x10000000000000000")})
        {
            service.configure({{"images", QJsonArray {QJsonObject {{"path", bin}, {"address", address}}}}}, service.revision());
            QVERIFY_EXCEPTION_THROWN(service.preview("flash"), std::runtime_error);
        }
        service.configure({{"images", QJsonArray {}}}, service.revision());
        QVERIFY_EXCEPTION_THROWN(service.preview("flash"), std::runtime_error);
        QVERIFY(!service.preview("connect")["writes_flash"].toBool());
    }

    void espSegmentsAndCompatibilityChecks()
    {
        sw::OpenOcdService service;
        auto config = fixture();
        auto images = config["images"].toArray();
        auto second = images.first().toObject();
        second["address"] = "0x10000";
        images.append(second);
        config["images"] = images;
        config["strategy"] = "esp";
        service.restore(config);
        const auto script = service.preview("flash")["script"].toString();
        QVERIFY(script.contains("info commands program_esp"));
        QCOMPARE(script.count("\n    program_esp "), 2);
        QVERIFY(script.contains(" verify\n"));
        QVERIFY(script.contains(" verify reset\n"));
        QVERIFY(!script.contains("\n    init\n"));
        service.configure({{"startup", "reset_halt"}}, service.revision());
        QVERIFY_EXCEPTION_THROWN(service.preview("flash"), std::runtime_error);
        service.configure({{"startup", "default"}, {"mode", "target"}, {"interface", "interface/stlink.cfg"},
            {"target", "target/stm32f4x.cfg"}, {"transport", "swd"}}, service.revision());
        QVERIFY_EXCEPTION_THROWN(service.preview("flash"), std::runtime_error);
        service.configure({{"strategy", "program"}, {"transport", "default"}}, service.revision());
        QVERIFY_EXCEPTION_THROWN(service.preview("flash"), std::runtime_error);
    }

    void targetConfigOrderAndResetWiring()
    {
        sw::OpenOcdService service;
        service.restore(fixture());
        service.configure({{"mode", "target"}, {"interface", "interface/cmsis-dap.cfg"}, {"target", "target/stm32f4x.cfg"},
            {"transport", "swd"}, {"speed_khz", 1000}, {"reset_config", "connect_assert_srst"}}, service.revision());
        const auto script = service.preview("flash")["script"].toString();
        QVERIFY(script.indexOf("interface/cmsis-dap.cfg") < script.indexOf("transport select swd"));
        QVERIFY(script.indexOf("transport select swd") < script.indexOf("target/stm32f4x.cfg"));
        QVERIFY(script.contains("adapter speed 1000"));
        QVERIFY(script.contains("reset_config srst_only srst_nogate connect_assert_srst"));
    }

    void asynchronousResult_data()
    {
        QTest::addColumn<QByteArray>("mode");
        QTest::addColumn<QString>("result");
        QTest::newRow("completed") << QByteArray("success") << "succeeded";
        QTest::newRow("nonzero despite marker") << QByteArray("failure") << "failed";
        QTest::newRow("premature zero exit") << QByteArray("missing-marker") << "failed";
    }

    void asynchronousResult()
    {
        QFETCH(QByteArray, mode);
        QFETCH(QString, result);
        qputenv("SW_FAKE_OPENOCD_MODE", mode);
        sw::OpenOcdService service;
        service.restore(fixture());
        service.start("flash", service.revision());
        QVERIFY(service.busy());
        QVERIFY_EXCEPTION_THROWN(service.start("version", service.revision()), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(service.configure({{"verify", false}}, service.revision()), std::runtime_error);
        QTRY_VERIFY_WITH_TIMEOUT(!service.busy(), 5000);
        QCOMPARE(service.state()["status"].toString(), result);
        QVERIFY(!output(service).isEmpty());
    }

    void failedStart()
    {
        sw::OpenOcdService service;
        auto config = fixture();
        const auto invalid = directory.filePath("not-a-program.exe");
        write(invalid, "not an executable");
#ifndef Q_OS_WIN
        QVERIFY(QFile::setPermissions(invalid, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
#endif
        config["executable"] = invalid;
        service.restore(config);
        service.start("version", service.revision());
        QTRY_VERIFY_WITH_TIMEOUT(!service.busy(), 5000);
        QCOMPARE(service.state()["status"].toString(), QString("failed"));
        QVERIFY(!service.state()["error"].toString().isEmpty());
    }

    void cancellationAndRestart()
    {
        qputenv("SW_FAKE_OPENOCD_MODE", "hang");
        sw::OpenOcdService service;
        service.restore(fixture());
        service.start("connect", service.revision());
        QTRY_VERIFY_WITH_TIMEOUT(!service.state()["records"].toArray().isEmpty(), 2000);
        QVERIFY_EXCEPTION_THROWN(service.cancel("old job"), std::runtime_error);
        const auto cancelledJob = service.state()["job_id"].toString();
        service.cancel(cancelledJob);
        // An Agent may retry cancellation while polling. Repeated requests
        // must not keep postponing the forced-stop deadline.
        QTimer repeatCancel;
        connect(&repeatCancel, &QTimer::timeout, &service, [&service, cancelledJob]
        {
            if (service.busy()) { service.cancel(cancelledJob); }
        });
        repeatCancel.start(100);
        QTRY_VERIFY_WITH_TIMEOUT(!service.busy(), 2500);
        repeatCancel.stop();
        QCOMPARE(service.state()["status"].toString(), QString("cancelled"));
        qputenv("SW_FAKE_OPENOCD_MODE", "success");
        service.start("version", service.revision());
        // Clearing a job with prior output must not make readers spin waiting
        // for non-existent records from that previous job.
        QVERIFY(!service.state()["has_more"].toBool());
        QTRY_VERIFY_WITH_TIMEOUT(!service.busy(), 2500);
        QCOMPARE(service.state()["status"].toString(), QString("succeeded"));
        QTest::qWait(900);
        QCOMPARE(service.state()["status"].toString(), QString("succeeded"));
    }

    void timeout()
    {
        qputenv("SW_FAKE_OPENOCD_MODE", "hang");
        sw::OpenOcdService service;
        service.restore(fixture());
        service.configure({{"timeout_ms", 5000}}, service.revision());
        service.start("connect", service.revision());
        QTRY_VERIFY_WITH_TIMEOUT(!service.busy(), 7000);
        QCOMPARE(service.state()["status"].toString(), QString("timed_out"));
    }

    void boundedIndependentLogs()
    {
        qputenv("SW_FAKE_OPENOCD_MODE", "flood");
        sw::OpenOcdService service;
        service.restore(fixture());
        service.start("flash", service.revision());
        QTRY_VERIFY_WITH_TIMEOUT(!service.busy(), 5000);
        QCOMPARE(service.state()["status"].toString(), QString("succeeded"));
        QVERIFY(service.state()["gap"].toBool());
        QCOMPARE(service.state(0, 2), service.state(0, 2));
        const auto first = service.state(0, 2);
        const auto next = service.state(first["next_cursor"].toInteger(), 2);
        QVERIFY(next["next_cursor"].toInteger() > first["next_cursor"].toInteger());
        QVERIFY(output(service).toUtf8().size() <= 256 * 1024);
    }

    void workspaceProfilesPersistAtomically()
    {
        sw::Engine engine;
        auto first = engine.openocd;
        first->restore(fixture());
        first->title = "Board A";
        first->hasPage = true;
        const auto firstId = first->viewId;
        auto second = engine.createOpenOcdView("Board B", {{"adapter_serial", "PROBE-B"}});
        second->hasPage = true;
        const auto secondId = second->viewId;
        QCOMPARE(second->configuration()["executable"], first->configuration()["executable"]);
        QVERIFY(second->configuration()["images"].toArray().isEmpty());
        QVERIFY(second->configuration()["board"].toString().isEmpty());
        QVERIFY_EXCEPTION_THROWN(engine.openOcdView(), std::runtime_error);
        const auto saved = engine.openOcdSettings();

        sw::Engine restored;
        restored.restoreOpenOcdViews(saved);
        QCOMPARE(restored.openOcdSettings(), saved);
        QVERIFY(!restored.openOcdView(firstId)->busy());
        QCOMPARE(restored.openOcdView(secondId)->state()["status"].toString(), QString("idle"));
        auto duplicate = saved;
        duplicate.append(saved.first());
        auto unchanged = restored.openOcdView(firstId);
        QVERIFY_EXCEPTION_THROWN(restored.restoreOpenOcdViews(duplicate), std::runtime_error);
        QCOMPARE(restored.openOcdView(firstId), unchanged);
        QCOMPARE(restored.openOcdSettings(), saved);
        restored.removeOpenOcdView(firstId);
        QCOMPARE(restored.openOcdView(), restored.openOcdView(secondId));
        restored.removeOpenOcdView(secondId);
        QVERIFY(restored.openocdViews.isEmpty());
        QVERIFY(!restored.openocd);
        restored.restoreOpenOcdViews({});
        QVERIFY_EXCEPTION_THROWN(restored.openOcdView(), std::runtime_error);
        QVERIFY(restored.createOpenOcdView());
    }

    void parallelProbesAndIndependentCancellation()
    {
        qputenv("SW_FAKE_OPENOCD_MODE", "hang");
        sw::Engine engine;
        auto first = engine.openocd;
        first->restore(fixture());
        auto second = engine.createOpenOcdView("Board B", fixture());
        second->configure({{"adapter_serial", "PROBE-B"}}, second->revision());
        first->start("connect", first->revision());
        QVERIFY_EXCEPTION_THROWN(second->start("connect", second->revision()), std::runtime_error);
        first->cancel(first->state()["job_id"].toString());
        QTRY_VERIFY_WITH_TIMEOUT(!first->busy(), 2500);
        first->configure({{"adapter_serial", "PROBE-A"}}, first->revision());
        qputenv("SW_FAKE_OPENOCD_LABEL", "FIRST_PROBE");
        first->start("connect", first->revision());
        second->configure({{"adapter_serial", "probe-a"}}, second->revision());
        QVERIFY_EXCEPTION_THROWN(second->start("connect", second->revision()), std::runtime_error);
        second->configure({{"adapter_serial", "PROBE-B"}}, second->revision());
        qputenv("SW_FAKE_OPENOCD_LABEL", "SECOND_PROBE");
        second->start("flash", second->revision());
        QTRY_VERIFY_WITH_TIMEOUT(output(*first).contains("FIRST_PROBE"), 2500);
        QTRY_VERIFY_WITH_TIMEOUT(output(*second).contains("SECOND_PROBE"), 2500);
        QVERIFY(!output(*first).contains("SECOND_PROBE"));
        QVERIFY(!output(*second).contains("FIRST_PROBE"));
        QVERIFY(first->busy() && second->busy());
        const auto secondJob = second->state()["job_id"].toString();
        QVERIFY(secondJob != first->state()["job_id"].toString());
        QVERIFY_EXCEPTION_THROWN(engine.removeOpenOcdView(second->viewId), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(engine.restoreOpenOcdViews(engine.openOcdSettings()), std::runtime_error);
        first->cancel(first->state()["job_id"].toString());
        QTRY_VERIFY_WITH_TIMEOUT(!first->busy(), 2500);
        QCOMPARE(first->state()["status"].toString(), QString("cancelled"));
        QVERIFY(second->busy());
        engine.removeOpenOcdView(first->viewId);
        QCOMPARE(engine.openOcdView()->state()["job_id"].toString(), secondJob);
        second->cancel(secondJob);
        QTRY_VERIFY_WITH_TIMEOUT(!second->busy(), 2500);
        QCOMPARE(second->state()["status"].toString(), QString("cancelled"));
    }

    void tabbedJobsAndMcpIsolation()
    {
        const auto path = directory.filePath("tabs-settings.json");
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, path);
        window.show();
        const auto handle = window.winId();
        mcp.setProtocolEnabled("openocd", true);
        engine.allowControl = true;
        const auto firstId = engine.openocd->viewId;
        mcp.call("configure_openocd", {{"view_id", firstId}, {"revision", engine.openocd->revision()}, {"title", "Board A"}, {"patch", fixture()}});
        auto first = engine.openOcdView(firstId);
        first->configure({{"adapter_serial", "PROBE-A"}}, first->revision());
        auto config = fixture();
        config["adapter_serial"] = "PROBE-B";
        const auto created = mcp.call("create_openocd_view", {{"title", "Board B"}, {"configuration", config}}).toObject();
        const auto secondId = created["view_id"].toString();
        auto second = engine.openOcdView(secondId);
        auto firstPage = pageFor(window, firstId);
        auto secondPage = pageFor(window, secondId);
        QVERIFY(firstPage && secondPage && firstPage != secondPage);
        QCOMPARE(window.winId(), handle);
        QCOMPARE(mcp.call("list_openocd_views", {}).toObject()["views"].toArray().size(), 2);
        QVERIFY_EXCEPTION_THROWN(mcp.call("get_openocd_state", {}), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(mcp.call("configure_openocd", {{"revision", first->revision()}, {"patch", QJsonObject {}}}), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(mcp.call("close_openocd_view", {}), std::runtime_error);
        auto receivePage = window.active;
        auto parameters = window.findChild<QScrollArea *>("parameterScroll");
        QVERIFY(parameters->isVisible());
        const auto parameterGeometry = parameters->geometry();
        const auto tabPosition = window.tabs->mapToGlobal(QPoint());
        window.showPane(firstPage);
        QVERIFY(parameters->isVisible());
        QVERIFY(window.parameterToggle->isEnabled());
        QCOMPARE(parameters->geometry(), parameterGeometry);
        QCOMPARE(window.tabs->mapToGlobal(QPoint()), tabPosition);
        window.showPane(receivePage);
        QCOMPARE(parameters->geometry(), parameterGeometry);
        QCOMPARE(window.tabs->mapToGlobal(QPoint()), tabPosition);
        window.parameterToggle->setChecked(false);
        window.showPane(firstPage);
        QVERIFY(!parameters->isVisible());
        window.parameterToggle->setChecked(true);
        QVERIFY(parameters->isVisible());
        auto serial = firstPage->findChild<QLineEdit *>("openocdAdapterSerial");
        QVERIFY(serial);
        QCOMPARE(serial->text(), QString("PROBE-A"));
        qputenv("SW_FAKE_OPENOCD_MODE", "hang");
        mcp.call("start_openocd", {{"view_id", firstId}, {"revision", first->revision()}, {"operation", "connect"}});
        mcp.call("start_openocd", {{"view_id", secondId}, {"revision", second->revision()}, {"operation", "flash"}});
        QVERIFY(first->busy() && second->busy());
        window.closeTab(pageIndex(window, firstPage));
        QVERIFY(pageFor(window, firstId));
        QVERIFY_EXCEPTION_THROWN(mcp.call("close_openocd_view", {{"view_id", secondId}}), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(mcp.call("cancel_openocd", {{"view_id", firstId}, {"job_id", second->state()["job_id"]}}), std::runtime_error);
        mcp.call("cancel_openocd", {{"view_id", firstId}, {"job_id", first->state()["job_id"]}});
        QTRY_VERIFY_WITH_TIMEOUT(!first->busy(), 2500);
        QVERIFY(second->busy());
        window.tabs->moveTab(pageIndex(window, firstPage), 0);
        mcp.call("close_openocd_view", {{"view_id", firstId}});
        QVERIFY(!pageFor(window, firstId));
        QCOMPARE(pageFor(window, secondId), secondPage);
        QVERIFY(second->busy());
        mcp.call("cancel_openocd", {{"view_id", secondId}, {"job_id", second->state()["job_id"]}});
        QTRY_VERIFY_WITH_TIMEOUT(!second->busy(), 2500);
        window.saveSettings();
        {
            sw::Engine restored;
            sw::McpServer restoredMcp(&restored);
            sw::Window restoredWindow(&restored, &restoredMcp, path);
            QCOMPARE(restored.openocdViews.size(), 1);
            auto page = pageFor(restoredWindow, secondId);
            QVERIFY(page);
            QCOMPARE(page->pageTitle, QString("Board B"));
            QCOMPARE(restored.openOcdView(secondId)->configuration(), second->configuration());
            QVERIFY(!restored.openOcdView(secondId)->busy());
            restoredMcp.call("close_openocd_view", {{"view_id", secondId}});
            QVERIFY(restored.openocdViews.isEmpty());
            restoredWindow.saveSettings();
        }
        {
            QFile savedFile(path);
            QVERIFY(savedFile.open(QIODevice::ReadOnly));
            auto saved = QJsonDocument::fromJson(savedFile.readAll()).object();
            savedFile.close();
            QVERIFY(saved["openocd_views"].toArray().isEmpty());
            saved["widget_pages"] = QJsonArray {QJsonObject {{"kind", "openocd"}, {"openocd_view_id", "missing-profile"}}};
            write(path, QJsonDocument(saved).toJson());
        }
        {
            sw::Engine empty;
            sw::McpServer emptyMcp(&empty);
            sw::Window emptyWindow(&empty, &emptyMcp, path);
            QVERIFY(empty.openocdViews.isEmpty());
            emptyWindow.openOpenOcd();
            QCOMPARE(empty.openocdViews.size(), 1);
        }
        window.close();
    }

    void searchableChoicesDoNotEditConfiguration()
    {
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("search-settings.json"));
        auto config = fixture();
        write(directory.filePath("scripts/interface/stlink-v2-1.cfg"));
        write(directory.filePath("scripts/interface/dp_busblaster_kt-link.cfg"));
        config["mode"] = "target";
        config["interface"] = "interface/stlink.cfg";
        config["target"] = "target/stm32f4x.cfg";
        engine.openocd->configure(config, engine.openocd->revision());
        window.show();
        window.openOpenOcd();
        auto dialog = window.openOcdDialog.data();
        auto adapter = dialog->findChild<QComboBox *>("openocdInterface");
        QVERIFY(adapter);
        const auto revision = engine.openocd->revision();
        adapter->setFocus();
        QTest::keyClick(adapter->lineEdit(), Qt::Key_Space, Qt::ControlModifier);
        auto popup = QApplication::activePopupWidget();
        QVERIFY(popup);
        auto query = popup->findChild<QLineEdit *>("comboSearchInput");
        auto matches = popup->findChild<QListView *>("comboSearchResults");
        QVERIFY(query);
        QVERIFY(matches);
        query->setText("st link");
        QCOMPARE(matches->model()->rowCount(), 2);
        QCOMPARE(matches->currentIndex().data().toString(), adapter->currentText());
        query->setText("ST Link V2");
        QCOMPARE(matches->model()->rowCount(), 1);
        QCOMPARE(engine.openocd->revision(), revision);
        QCOMPARE(adapter->currentText(), QString("interface/stlink.cfg"));
        QTest::keyClick(query, Qt::Key_Return);
        QCOMPARE(adapter->currentText(), QString("interface/stlink-v2-1.cfg"));
        QCOMPARE(engine.openocd->configuration()["interface"].toString(), adapter->currentText());
        QVERIFY(!popup->isVisible());
        QVERIFY(dialog->isVisible());

        const auto selected = engine.openocd->configuration();
        adapter->showPopup();
        query->setText("no-such-probe-9837");
        QCOMPARE(matches->model()->rowCount(), 0);
        QTest::keyClick(query, Qt::Key_Return);
        QVERIFY(popup->isVisible());
        QTest::keyClick(query, Qt::Key_Escape);
        QVERIFY(!popup->isVisible());
        QCOMPARE(engine.openocd->configuration(), selected);
        QVERIFY(dialog->isVisible());

        QVERIFY(!engine.openocd->busy());
        window.close();
    }

    void logLayoutAndResponsiveTabs()
    {
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("layout-settings.json"));
        engine.openocd->configure(fixture(), engine.openocd->revision());
        window.resize(1320, 880);
        window.show();
        window.openOpenOcd();
        auto dialog = window.openOcdDialog.data();
        auto tabs = dialog->findChild<QTabWidget *>("openocdConfigurationTabs");
        auto logs = dialog->findChild<QPlainTextEdit *>("openocdLogs");
        auto split = dialog->findChild<QSplitter *>("openocdSplit");
        QVERIFY(tabs);
        QVERIFY(logs);
        QVERIFY(split);
        QTRY_VERIFY(split->isVisible() && logs->isVisible());
        QTest::qWait(80);
        QTRY_VERIFY(logs->height() >= 220);
        auto viewport = dialog->findChild<QScrollArea *>("openocdScroll")->viewport();
        auto mode = dialog->findChild<QComboBox *>("openocdMode");
        QTRY_VERIFY(viewport->rect().contains(QRect(mode->mapTo(viewport, QPoint()), mode->size())));
        QVERIFY(split->handleWidth() >= 8);
        const auto config = engine.openocd->configuration();
        logs->setPlainText("Log selection remains intact\nsecond line\n");
        QCOMPARE(tabs->count(), 3);
        QTest::mouseClick(dialog->findChild<QPushButton *>("openocdAdvanced"), Qt::LeftButton);
        QCOMPARE(tabs->currentIndex(), 1);
        tabs->setCurrentIndex(2);
        QVERIFY(dialog->findChild<QLineEdit *>("openocdExecutable")->isVisible());
        window.resize(900, 640);
        auto compact = dialog->findChild<QTabWidget *>("openocdCompactTabs");
        QTRY_VERIFY(compact->isVisible());
        QCOMPARE(compact->count(), 2);
        QCOMPARE(compact->currentIndex(), 0);
        QVERIFY(tabs->height() >= 230);
        QTest::keyClick(dialog, Qt::Key_Escape);
        QVERIFY(dialog->isVisible());
        compact->setCurrentIndex(1);
        QTRY_VERIFY(logs->height() >= 170);
        const auto flash = dialog->findChild<QPushButton *>("openocdFlash");
        QVERIFY(flash->isVisible());
        QVERIFY(dialog->rect().contains(flash->mapTo(dialog, flash->rect().center())));
        window.resize(1320, 880);
        QTRY_VERIFY(split->isVisible() && tabs->isVisible() && logs->isVisible());
        QVERIFY(logs->toPlainText().contains("Log selection remains intact"));
        QCOMPARE(engine.openocd->configuration(), config);
        window.close();
    }

    void mcpPermissionsAndSharedGui()
    {
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        engine.openocd->configure(fixture(), engine.openocd->revision());
        QVERIFY(!mcp.protocolEnabled("openocd"));
        for (const auto &name : {"get_openocd_state", "list_openocd_configs", "configure_openocd", "start_openocd",
            "export_openocd_configuration", "import_openocd_configuration"})
        {
            QVERIFY_EXCEPTION_THROWN(mcp.call(name, {}), std::runtime_error);
        }
        mcp.setProtocolEnabled("openocd", true);
        QVERIFY(!engine.allowControl);
        const auto document = mcp.call("export_openocd_configuration", {}).toObject();
        QVERIFY(document["configuration"].isObject());
        QVERIFY_EXCEPTION_THROWN(mcp.call("import_openocd_configuration", {{"view_id", engine.openocd->viewId},
            {"revision", engine.openocd->revision()}, {"document", document}}), std::runtime_error);
        QVERIFY(mcp.call("preview_openocd", {}).toObject()["writes_flash"].toBool());
        QVERIFY_EXCEPTION_THROWN(mcp.call("start_openocd", {{"operation", "version"}, {"revision", engine.openocd->revision()}}), std::runtime_error);
        window.openOpenOcd();
        auto dialog = window.openOcdDialog.data();
        QVERIFY(dialog);
        auto startup = dialog->findChild<QComboBox *>("openocdStartup");
        QVERIFY(startup);
        engine.allowControl = true;
        const auto revision = engine.openocd->revision();
        const auto viewId = engine.openocd->viewId;
        mcp.call("configure_openocd", {{"view_id", viewId}, {"revision", revision}, {"patch", QJsonObject {{"startup", "reset_halt"}}}});
        QCOMPARE(startup->currentData().toString(), QString("reset_halt"));
        QVERIFY_EXCEPTION_THROWN(mcp.call("configure_openocd", {{"view_id", viewId}, {"revision", revision}, {"patch", QJsonObject {{"verify", false}}}}), std::runtime_error);
        startup->setCurrentIndex(startup->findData("halt"));
        QCOMPARE(mcp.call("get_openocd_state", {}).toObject()["configuration"].toObject()["startup"].toString(), QString("halt"));
        auto active = mcp.call("start_openocd", {{"view_id", viewId}, {"operation", "version"}, {"revision", engine.openocd->revision()}}).toObject();
        QVERIFY(active["running"].toBool());
        QVERIFY(!dialog->findChild<QPushButton *>("openocdFlash")->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(!engine.openocd->busy(), 5000);
        QCOMPARE(mcp.call("get_openocd_state", {}).toObject()["status"].toString(), QString("succeeded"));
        QVERIFY(dialog->findChild<QPlainTextEdit *>("openocdLogs")->toPlainText().contains("process fixture"));
        QVERIFY(dialog->findChild<QPushButton *>("openocdFlash")->isEnabled());
        window.saveSettings();
        QFile saved(directory.filePath("settings.json"));
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const auto settings = QJsonDocument::fromJson(saved.readAll()).object();
        QCOMPARE(settings["openocd"].toObject(), engine.openocd->configuration());
        QVERIFY(settings["mcp_protocols"].toObject()["openocd"].toBool());
        mcp.setProtocolEnabled("openocd", false);
        QVERIFY_EXCEPTION_THROWN(mcp.call("get_openocd_state", {}), std::runtime_error);
        window.close();
    }

    void realOpenOcdTclAndVersion()
    {
        const auto real = qEnvironmentVariable("SW_REAL_OPENOCD");
        if (real.isEmpty()) { QSKIP("Set SW_REAL_OPENOCD for a no-hardware check of the installed Tcl engine and version"); }
        sw::OpenOcdService service;
        auto config = fixture();
        config["executable"] = real;
        const QString stub = directory.filePath("scripts/board/fixture.cfg");
        write(stub, "proc program {args} {\n if {![file exists [lindex $args 0]]} {error {filename was not preserved}}\n echo TCL_PATH_OK\n}\n");
        service.restore(config);
        service.start("version", service.revision());
        QTRY_VERIFY_WITH_TIMEOUT(!service.busy(), 10000);
        QCOMPARE(service.state()["status"].toString(), QString("succeeded"));
        QVERIFY(output(service).contains("Open On-Chip Debugger"));
        service.start("flash", service.revision());
        QTRY_VERIFY_WITH_TIMEOUT(!service.busy(), 10000);
        QVERIFY2(service.state()["status"] == "succeeded", qPrintable(output(service)));
        QVERIFY(output(service).contains("TCL_PATH_OK"));
        const auto unicode = directory.filePath("中文固件 } { $name [shutdown];.bin");
        write(unicode, "unicode fixture");
        service.configure({{"images", QJsonArray {QJsonObject {{"path", unicode}, {"address", "0x08000000"}}}}}, service.revision());
#ifdef Q_OS_WIN
        QCOMPARE(service.preview("flash")["staged_images"].toArray().size(), 1);
#endif
        service.start("flash", service.revision());
        QTRY_VERIFY_WITH_TIMEOUT(!service.busy(), 10000);
        QVERIFY2(service.state()["status"] == "succeeded", qPrintable(output(service)));
        QVERIFY(output(service).contains("TCL_PATH_OK"));
    }

    void visualCapture()
    {
        const auto captures = qEnvironmentVariable("SW_OPENOCD_CAPTURES");
        if (captures.isEmpty()) { QSKIP("Optional actual window capture"); }
        QVERIFY(QDir().mkpath(captures));
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("capture-settings.json"));
        auto settings = fixture();
        settings["executable"] = qEnvironmentVariable("SW_REAL_OPENOCD", settings["executable"].toString());
        settings["scripts_dir"] = qEnvironmentVariable("SW_REAL_OPENOCD_SCRIPTS", settings["scripts_dir"].toString());
        settings["mode"] = "target";
        settings["interface"] = "interface/stlink.cfg";
        settings["target"] = "target/stm32f4x.cfg";
        settings["speed_khz"] = 1000;
        engine.openocd->configure(settings, engine.openocd->revision());
        window.resize(1400, 880);
        window.show();
        window.openOpenOcd();
        auto dialog = window.openOcdDialog.data();
        const auto firstId = engine.openocd->viewId;
        engine.openocd->configure({{"adapter_serial", "PROBE-A"}}, engine.openocd->revision());
        auto second = window.newInstrument("openocd");
        second->instrument->flasherService->configure({{"adapter_serial", "PROBE-B"}, {"mode", "target"},
            {"interface", "interface/cmsis-dap.cfg"}, {"target", "target/stm32f1x.cfg"}}, second->instrument->flasherService->revision());
        window.showPane(pageFor(window, firstId));
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        engine.openocd->start("version", engine.openocd->revision());
        QTRY_VERIFY_WITH_TIMEOUT(!engine.openocd->busy(), 10000);
        QTest::qWait(180);
        QVERIFY(window.grab().save(captures + "/openocd-light.png"));
        window.applyTheme(true);
        QTest::qWait(180);
        QVERIFY(window.grab().save(captures + "/openocd-dark.png"));
        auto adapter = dialog->findChild<QComboBox *>("openocdInterface");
        adapter->showPopup();
        auto popup = QApplication::activePopupWidget();
        QVERIFY(popup);
        popup->findChild<QLineEdit *>("comboSearchInput")->setText("st link");
        QTest::qWait(220);
        auto screenshot = window.grab();
        {
            QPainter painter(&screenshot);
            painter.drawPixmap(window.mapFromGlobal(popup->mapToGlobal(QPoint())), popup->grab());
        }
        QVERIFY(screenshot.save(captures + "/openocd-search.png"));
        adapter->hidePopup();
        window.resize(900, 640);
        QTest::qWait(180);
        QVERIFY(window.grab().save(captures + "/openocd-compact.png"));
        dialog->findChild<QTabWidget *>("openocdCompactTabs")->setCurrentIndex(1);
        QTest::qWait(100);
        QVERIFY(window.grab().save(captures + "/openocd-compact-output.png"));
        window.close();
    }
};

QTEST_MAIN(OpenOcdTests)
#include "openocd_tests.moc"
