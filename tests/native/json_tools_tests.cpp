// SPDX-License-Identifier: Apache-2.0
#include "json_tools.h"
#include "instrument_config.h"
#include "window.h"
#include <QtTest>

class JsonToolsTests : public QObject
{
    Q_OBJECT
private:
    void capture(QWidget *widget, const QString &name)
    {
        const auto directory = qEnvironmentVariable("SW_JSON_CAPTURE_DIR");
        if (directory.isEmpty()) { return; }
        QDir().mkpath(directory);
        QVERIFY(widget->grab().save(directory + "/" + name + ".png"));
    }

private slots:
    void initTestCase()
    {
        QApplication::setStyle("Fusion");
        QApplication::setFont(sw::Theme::interfaceFont());
    }

    void validJson_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("nested") << QString("{\"id\":42,\"temperature\":25.5,\"enabled\":true,\"extra\":null,\"nested\":{\"items\":[1,\"two\",false,{},[]]}}");
        QTest::newRow("unicode") << QString::fromUtf8("{\"\u6e29\u5ea6\":25.5,\"\u8bf4\u660e\":\"\u4f60\u597d\\n\\\"test\\\"\\\\path\",\"emoji\":\"\u26a1\",\"quoted\\\"key\":\"ok\"}");
        QTest::newRow("object") << QString("{}");
        QTest::newRow("array") << QString("[]");
        QTest::newRow("array-root") << QString("[1,2,{\"v\":3}]");
        QTest::newRow("string-root") << QString("\"hello\"");
        QTest::newRow("number-root") << QString("-0.00125");
        QTest::newRow("boolean-root") << QString("true");
        QTest::newRow("null-root") << QString("null");
        QTest::newRow("identifiers") << QString("{\"humi\":65.5,\"humi_value\":1,\"sensor\":{\"humi\":2},\"a-b\":3,\"a_b\":4,\"class\":5,\"root\":6,\"text\":7,\"end\":8,\"1st\":9,\"__x\":10,\"cJSON_Delete\":11,\"CJSON_VERSION_MAJOR\":12,\"true\":13,\"double\":14}");
    }

    void validJson()
    {
        QFETCH(QString, text);
        const auto result = sw::analyzeJson(text);
        QVERIFY2(result.valid, qPrintable(result.message));
        QVERIFY(!result.formatted.isEmpty());
        QVERIFY(result.parseCode.contains("cJSON_Delete(root)"));
        QVERIFY(result.buildCode.contains("cJSON_PrintUnformatted"));
        QVERIFY(result.buildCode.contains("cJSON_free"));
        QVERIFY(sw::analyzeJson(result.formatted).valid);
        QCOMPARE(sw::analyzeJson(result.compact).compact, result.compact);
        const auto directory = qEnvironmentVariable("SW_CJSON_FIXTURE_DIR");
        if (!directory.isEmpty())
        {
            const auto fixture = QDir(directory).filePath(QTest::currentDataTag());
            QDir().mkpath(fixture);
            const QList<QPair<QString, QString>> files {
                {"parse.c", result.parseCode}, {"build.c", result.buildCode}, {"expected.json", result.compact}
            };
            for (const auto &entry : files)
            {
                QFile file(QDir(fixture).filePath(entry.first));
                QVERIFY(file.open(QIODevice::WriteOnly));
                file.write(entry.second.toUtf8());
            }
        }
    }

    void namesFollowFields()
    {
        const auto result = sw::analyzeJson("{\"humi\":65.5,\"sensor\":{\"humi\":45}}");
        QVERIFY(result.valid);
        QVERIFY(result.parseCode.contains("const cJSON *humi = cJSON_GetObjectItemCaseSensitive(root, \"humi\")"));
        QVERIFY(result.parseCode.contains("double humi_value = humi->valuedouble"));
        QVERIFY(result.parseCode.contains("const cJSON *sensor_humi"));
        QVERIFY(result.buildCode.contains("cJSON *humi = cJSON_CreateNumber"));
        QVERIFY(!result.parseCode.contains("node_"));
    }

    void syntaxErrorsAndLimits()
    {
        for (const auto &text : QStringList {"{bad:1}", "{\"a\":1,}", "[1,]", "true false", "1,2", "{\"a\":\"\\z\"}"})
        {
            const auto result = sw::analyzeJson(text);
            QVERIFY2(!result.valid, qPrintable(text));
            QVERIFY(result.parseCode.isEmpty());
            QVERIFY(result.buildCode.isEmpty());
            QVERIFY(result.errorPosition >= 0);
            QVERIFY(result.message.contains("行"));
        }
        const auto unicode = sw::analyzeJson("{\n\"温度\": 1,\n\"x\": } ");
        QVERIFY(!unicode.valid);
        QVERIFY(unicode.message.contains("第 3 行"));
        const auto zero = sw::analyzeJson("{\"v\":\"\\u0000\"}");
        QVERIFY(zero.valid);
        QVERIFY(zero.parseCode.isEmpty());
        const auto big = sw::analyzeJson(QString(1024 * 1024 + 1, 'x'));
        QVERIFY(big.message.contains("1 MB"));
    }

    void pasteFormatAndCodeWindow()
    {
        QWidget parent;
        parent.setStyleSheet(sw::Theme::make(false).stylesheet());
        auto dialog = sw::showJsonAssistant(&parent);
        auto input = dialog->findChild<QPlainTextEdit *>("jsonInput");
        auto output = dialog->findChild<QPlainTextEdit *>("jsonFormatted");
        auto parse = dialog->findChild<QPlainTextEdit *>("cjsonParseCode");
        auto build = dialog->findChild<QPlainTextEdit *>("cjsonBuildCode");
        const QString text = "{\"device\":\"sensor\",\"temperature\":25.5,\"online\":true,\"config\":{\"period_ms\":20},\"samples\":[1,2,3]}";
        QApplication::clipboard()->setText(text);
        dialog->findChild<QPushButton *>("pasteJson")->click();
        QTRY_VERIFY(output->toPlainText().contains('\n'));
        QCOMPARE(input->toPlainText(), text);
        QVERIFY(parse->toPlainText().contains("parse_message"));
        QVERIFY(build->toPlainText().contains("build_message"));
        auto tabs = dialog->findChild<QTabWidget *>("jsonAssistantTabs");
        capture(dialog, "json-assistant-format");
        tabs->setCurrentIndex(1);
        capture(dialog, "json-assistant-parse");
        dialog->findChild<QPushButton *>("copyJsonResult")->click();
        QCOMPARE(QApplication::clipboard()->text(), parse->toPlainText());
        tabs->setCurrentIndex(0);
        dialog->findChild<QPushButton *>("expandJson")->click();
        QVERIFY(input->toPlainText().contains('\n'));
        input->setPlainText("{\"x\": }");
        QTRY_VERIFY(parse->toPlainText().isEmpty());
        QVERIFY(build->toPlainText().isEmpty());
        QVERIFY(!input->extraSelections().isEmpty());
        dialog->close();
        QCOMPARE(sw::showJsonAssistant(&parent), dialog);
        QCOMPARE(input->toPlainText(), QString("{\"x\": }"));
        dialog->close();
    }

    void functionReferenceIsReadable()
    {
        QWidget parent;
        parent.setStyleSheet(sw::Theme::make(false).stylesheet());
        auto dialog = sw::showJsonAssistant(&parent);
        auto tabs = dialog->findChild<QTabWidget *>("jsonAssistantTabs");
        tabs->setCurrentIndex(3);
        auto reference = dialog->findChild<QTextBrowser *>("cjsonReference");
        const auto text = reference->toPlainText();
        QVERIFY(text.contains("开源项目 cJSON"));
        QVERIFY(text.contains("cJSON_AddNumberToObject"));
        QVERIFY(!text.contains("ESP-IDF"));
        const auto linkColor = [reference]
        {
            return reference->document()->find("查看 cJSON 开源项目").charFormat().foreground().color();
        };
        QTRY_COMPARE(linkColor(), QColor("#477255"));
        capture(dialog, "cjson-reference-light");
        parent.setStyleSheet(sw::Theme::make(true).stylesheet());
        QTRY_COMPARE(linkColor(), QColor("#edaa50"));
        capture(dialog, "cjson-reference-dark");
        dialog->close();
    }

    void integratedValuesAndSeparateFormats()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        auto channel = engine.open(sw::Config::fromJson(QJsonObject {{"port", "loop://"}}));
        auto wave = window.newInstrument("waveform", channel);
        QVERIFY(wave->instrument->valuesPanel->isHidden());
        wave->instrument->valuesToggle->click();
        QVERIFY(!wave->instrument->valuesPanel->isHidden());
        channel->receive("@wave temperature=25.5,count=42\n");
        wave->instrument->tick(channel);
        QCOMPARE(wave->instrument->table->topLevelItemCount(), 2);
        wave->instrument->freeze->setChecked(true);
        channel->receive("@wave temperature=30\n");
        wave->instrument->tick(channel);
        QCOMPARE(wave->instrument->table->topLevelItemCount(), 2);
        wave->instrument->freeze->setChecked(false);
        wave->instrument->tick(channel);
        QCOMPARE(wave->instrument->table->topLevelItemCount(), 1);
        QCOMPARE(wave->instrument->table->topLevelItem(0)->text(1), QString("30"));
        capture(&window, "wave-values-panel");
        auto json = window.newInstrument("json", channel);
        auto xml = window.newInstrument("xml", channel);
        QVERIFY(json->instrument->findChild<QPushButton *>("jsonAssistant"));
        QVERIFY(!xml->instrument->findChild<QPushButton *>("jsonAssistant"));
        channel->receive("{\"temperature\":25.5}<device><temperature>25.5</temperature></device>\n");
        json->instrument->tick(channel);
        xml->instrument->tick(channel);
        QCOMPARE(json->instrument->table->topLevelItemCount(), 1);
        QCOMPARE(xml->instrument->table->topLevelItemCount(), 1);
        QCOMPARE(json->instrument->table->topLevelItem(0)->text(1), QString("JSON"));
        QCOMPARE(xml->instrument->table->topLevelItem(0)->text(1), QString("XML"));
        auto menu = window.newPageMenu();
        QVERIFY(menu->findChild<QAction *>("new_json"));
        QVERIFY(menu->findChild<QAction *>("new_xml"));
        QVERIFY(!menu->findChild<QAction *>("new_values"));
        QVERIFY(!menu->findChild<QAction *>("new_structured"));
        delete menu;
        const auto state = wave->settings();
        QVERIFY(state["show_values"].toBool());
        wave->instrument->valuesToggle->click();
        wave->restore(state);
        QVERIFY(wave->instrument->valuesToggle->isChecked());
        window.close();
    }

    void legacyMigration()
    {
        const QString key = "serial:com8";
        const auto migrated = sw::migrateInstrumentPages(QJsonArray {
            QJsonObject {{"kind", "values"}, {"source_key", key}},
            QJsonObject {{"kind", "waveform"}, {"source_key", key}, {"points", 2500}},
            QJsonObject {{"kind", "structured"}, {"source_key", key}, {"parser", 0}}
        });
        QCOMPARE(migrated.size(), 3);
        QCOMPARE(migrated[0].toObject()["kind"].toString(), QString("waveform"));
        QVERIFY(migrated[0].toObject()["show_values"].toBool());
        QCOMPARE(migrated[0].toObject()["points"].toInt(), 2500);
        QCOMPARE(migrated[1].toObject()["kind"].toString(), QString("json"));
        QCOMPARE(migrated[2].toObject()["kind"].toString(), QString("xml"));
        QCOMPARE(sw::migrateInstrumentPages(migrated), migrated);
        const auto xmlOnly = sw::migrateInstrumentPages(QJsonArray {QJsonObject {{"kind", "structured"}, {"parser", 2}}});
        QCOMPARE(xmlOnly.size(), 1);
        QCOMPARE(xmlOnly.first().toObject()["kind"].toString(), QString("xml"));
    }
};

QTEST_MAIN(JsonToolsTests)
#include "json_tools_tests.moc"
