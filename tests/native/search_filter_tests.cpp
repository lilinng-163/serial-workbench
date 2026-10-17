// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "serial_monitor_dialog.h"
#include <QtTest>

class SearchFilterTests : public QObject
{
    Q_OBJECT
private:
    static QStringList visibleLines(sw::LogView *log)
    {
        QStringList lines;
        for (auto block = log->document()->begin(); block.isValid(); block = block.next())
        {
            if (block.isVisible()) { lines.append(block.text()); }
        }
        return lines;
    }

    static QToolButton *option(sw::FilterBox *box, const char *name)
    {
        return box->findChild<QToolButton *>(name);
    }

private slots:
    void initTestCase()
    {
        QApplication::setStyle("Fusion");
        QApplication::setFont(sw::Theme::interfaceFont());
    }

    void matching_data()
    {
        QTest::addColumn<QString>("query");
        QTest::addColumn<QString>("line");
        QTest::addColumn<bool>("logical");
        QTest::addColumn<bool>("regex");
        QTest::addColumn<bool>("word");
        QTest::addColumn<bool>("sensitive");
        QTest::addColumn<bool>("expected");
        QTest::newRow("empty") << QStringLiteral("") << QStringLiteral("anything") << false << false << false << false << true;
        QTest::newRow("plain-insensitive") << QStringLiteral("error") << QStringLiteral("ERROR 5") << false << false << false << false << true;
        QTest::newRow("case-sensitive") << QStringLiteral("error") << QStringLiteral("ERROR 5") << false << false << false << true << false;
        QTest::newRow("literal-regex") << QStringLiteral("a.b") << QStringLiteral("axb") << false << false << false << false << false;
        QTest::newRow("regex") << QStringLiteral("a.b") << QStringLiteral("axb") << false << true << false << false << true;
        QTest::newRow("regex-backreference") << QStringLiteral("(ab)\\1") << QStringLiteral("abab") << false << true << true << false << true;
        QTest::newRow("regex-case") << QStringLiteral("^ERR[0-9]+$") << QStringLiteral("err12") << false << true << false << true << false;
        QTest::newRow("whole-positive") << QStringLiteral("error") << QStringLiteral("[ERROR]") << false << false << true << false << true;
        QTest::newRow("whole-negative") << QStringLiteral("error") << QStringLiteral("errors") << false << false << true << false << false;
        QTest::newRow("whole-underscore") << QStringLiteral("error") << QStringLiteral("error_code") << false << false << true << false << false;
        QTest::newRow("whole-chinese") << QStringLiteral("温度") << QStringLiteral("温度: 25") << false << false << true << false << true;
        QTest::newRow("unicode-boundary") << QStringLiteral("温度") << QStringLiteral("当前温度") << false << false << true << false << false;
        QTest::newRow("or") << QStringLiteral("温度 || 报警") << QStringLiteral("电机报警") << true << false << false << false << true;
        QTest::newRow("and") << QStringLiteral("温度 && 报警") << QStringLiteral("温度正常") << true << false << false << false << false;
        QTest::newRow("precedence-left") << QStringLiteral("a || b && c") << QStringLiteral("a") << true << false << false << false << true;
        QTest::newRow("precedence-right") << QStringLiteral("a || b && c") << QStringLiteral("b") << true << false << false << false << false;
        QTest::newRow("quoted-operator") << QStringLiteral("\"a||b\" && ready") << QStringLiteral("a||b ready") << true << false << false << false << true;
        QTest::newRow("logic-disabled") << QStringLiteral("a || b") << QStringLiteral("a") << false << false << false << false << false;
        QTest::newRow("regex-and-logic") << QStringLiteral("^ERR[0-9]+ && motor || offline$") << QStringLiteral("ERR12 motor") << true << true << false << false << true;
        QTest::newRow("regex-quoted-alternative") << QStringLiteral("\"^(ERR||WARN)$\"") << QStringLiteral("WARN") << true << true << false << false << true;
        QTest::newRow("regex-escape") << QStringLiteral("\\d+ && V") << QStringLiteral("12 V") << true << true << false << false << true;
    }

    void matching()
    {
        QFETCH(QString, query);
        QFETCH(QString, line);
        QFETCH(bool, logical);
        QFETCH(bool, regex);
        QFETCH(bool, word);
        QFETCH(bool, sensitive);
        QFETCH(bool, expected);
        sw::SearchFilter filter;
        filter.text = query;
        filter.logical = logical;
        filter.regularExpression = regex;
        filter.wholeWord = word;
        filter.caseSensitive = sensitive;
        filter.compile();
        QVERIFY2(filter.error.isEmpty(), qPrintable(filter.error));
        QCOMPARE(filter.matches(line), expected);
    }

    void invalidExpressionsRemainVisible()
    {
        for (const auto &text : {"[", "a &&", "|| b", "a || || b", "\"unclosed"})
        {
            sw::SearchFilter filter;
            filter.text = text;
            filter.logical = true;
            filter.regularExpression = true;
            filter.compile();
            QVERIFY(!filter.error.isEmpty());
            QVERIFY(filter.matches("keep this visible"));
        }
        sw::FilterBox box;
        option(&box, "filterRegex")->setChecked(true);
        box.setText("[");
        QVERIFY(box.property("invalidFilter").toBool());
        QVERIFY(box.toolTip().contains(box.criteria().error));
        box.clear();
        QVERIFY(!box.property("invalidFilter").toBool());
    }

    void splitFramesAndExistingBlocks()
    {
        sw::LogView log;
        log.timestamps = false;
        sw::SearchFilter filter;
        filter.text = "temperature && alarm";
        filter.logical = true;
        filter.compile();
        log.setFilter(filter);
        const auto feed = [&](const QByteArray &data)
        {
            log.feed({{"direction", "rx"}, {"hex", QString::fromLatin1(data.toHex())}});
        };
        feed("temperature");
        QVERIFY(visibleLines(&log).isEmpty());
        feed(" alarm\r\nnormal\r\n");
        QCOMPARE(visibleLines(&log).size(), 1);
        QVERIFY(visibleLines(&log).first().contains("temperature alarm"));
        const auto original = log.toPlainText();
        filter.text.clear();
        filter.compile();
        log.setFilter(filter);
        QCOMPARE(visibleLines(&log).size(), 2);
        QCOMPARE(log.toPlainText(), original);
        filter.text = "RX";
        filter.compile();
        log.setFilter(filter);
        QCOMPARE(visibleLines(&log).size(), 2);
    }

    void controlsPausePersistenceAndCapture()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer server(&engine);
        sw::Window window(&engine, &server, directory.filePath("settings.json"));
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        auto pane = window.findPane(channel);
        QVERIFY(pane);
        const int index = window.panes.indexOf(pane);
        window.tabs->setCurrentIndex(index);
        window.activate(index);
        window.show();
        channel->receive("temperature alarm\r\nTEMPERATURE normal\r\nmotor fault\r\n");
        pane->tick();
        pane->pause->setChecked(true);
        const auto history = engine.journal.read(0, 1000, channel->id);
        const auto cursor = pane->cursor;
        const auto text = pane->log->toPlainText();
        pane->filter->setText("temperature && alarm || fault");
        option(pane->filter, "filterLogic")->setChecked(true);
        QCOMPARE(visibleLines(pane->log).size(), 2);
        QCOMPARE(pane->cursor, cursor);
        QCOMPARE(pane->log->toPlainText(), text);
        QCOMPARE(engine.journal.read(0, 1000, channel->id), history);
        QCOMPARE(channel->tx, 0);
        const auto saved = pane->settings();
        sw::FilterBox restored;
        restored.restore(saved["search_filter"].toObject());
        QCOMPARE(restored.settings(), pane->filter->settings());

        const auto captures = qEnvironmentVariable("FILTER_CAPTURE_DIR");
        if (!captures.isEmpty())
        {
            QDir().mkpath(captures);
            QApplication::processEvents();
            QVERIFY(window.grab().save(captures + "/filter-light.png"));
            // Theme changes rebuild the view from its journal, so resume for
            // that existing behavior before capturing a paused filtered view.
            pane->pause->setChecked(false);
            window.applyTheme(true);
            pane->tick();
            pane->pause->setChecked(true);
            window.resize(1000, 700);
            QApplication::processEvents();
            QVERIFY(window.grab().save(captures + "/filter-dark-narrow.png"));
            option(pane->filter, "filterRegex")->setChecked(true);
            pane->filter->setText("[");
            QApplication::processEvents();
            QVERIFY(window.grab().save(captures + "/filter-error.png"));
        }
        QTest::mouseClick(pane->filter->clearButton, Qt::LeftButton);
        QVERIFY(pane->filter->text().isEmpty());
        QVERIFY(visibleLines(pane->log).size() >= 3);
        QVERIFY(!pane->log->filter.size());
        window.close();
    }

    void savedOptionsSurviveWindowReopen()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer server(&engine);
        const auto path = directory.filePath("settings.json");
        QJsonObject expected;
        {
            sw::Window window(&engine, &server, path);
            auto search = window.active->filter;
            search->setText("READY || ERROR");
            for (const auto name : {"filterCase", "filterWord", "filterRegex", "filterLogic"})
            {
                QTest::mouseClick(option(search, name), Qt::LeftButton);
            }
            expected = search->settings();
            QVERIFY(expected["case_sensitive"].toBool());
            QVERIFY(expected["whole_word"].toBool());
            QVERIFY(expected["regex"].toBool());
            QVERIFY(expected["logical"].toBool());
            window.saveSettings();
            window.close();
        }
        sw::Window reopened(&engine, &server, path);
        QCOMPARE(reopened.active->filter->settings(), expected);
        QVERIFY(!reopened.active->filter->clearButton->isHidden());
        reopened.close();
    }

    void structuredAndMonitorUseSameRules()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->configureParser("auto");
        channel->receive("{\"state\":\"ALARM\"}\n{\"state\":\"ok\"}\n");
        sw::Instrument instrument("json");
        instrument.filter->setText("ALARM || failure");
        option(instrument.filter, "filterLogic")->setChecked(true);
        instrument.tick(channel);
        QVERIFY(instrument.table->topLevelItemCount() >= 2);
        int visible = 0;
        for (int at = 0; at < instrument.table->topLevelItemCount(); ++at)
        {
            if (!instrument.table->topLevelItem(at)->isHidden()) { ++visible; }
        }
        QCOMPARE(visible, 1);
        QVERIFY(instrument.detail->toPlainText().contains("ALARM"));
        instrument.filter->setText("not present");
        QVERIFY(instrument.detail->toPlainText().isEmpty());
        instrument.filter->setText("ok");
        QVERIFY(instrument.detail->toPlainText().contains("ok"));
        instrument.filter->setText("ALARM || failure");
        QVERIFY(instrument.detail->toPlainText().contains("ALARM"));
        QCOMPARE(instrument.settings()["search_filter"].toObject(), instrument.filter->settings());

        sw::SerialMonitorDialog monitor;
        auto search = monitor.findChild<sw::FilterBox *>();
        QVERIFY(search);
        search->setText("READY");
        option(search, "filterCase")->setChecked(true);
        auto log = monitor.findChild<sw::LogView *>();
        log->feed({{"direction", "rx"}, {"hex", "72656164790a52454144590a"}});
        QCOMPARE(visibleLines(log).size(), 1);
        const auto saved = monitor.settings();
        sw::SerialMonitorDialog restored;
        restored.restore(saved);
        QCOMPARE(restored.settings()["search_filter"], saved["search_filter"]);

        const auto captures = qEnvironmentVariable("FILTER_CAPTURE_DIR");
        if (!captures.isEmpty())
        {
            instrument.setStyleSheet(sw::Theme::make(false).stylesheet());
            instrument.resize(960, 540);
            instrument.show();
            monitor.resize(430, 360);
            monitor.show();
            QApplication::processEvents();
            QVERIFY(instrument.grab().save(captures + "/filter-json.png"));
            QVERIFY(monitor.grab().save(captures + "/filter-monitor.png"));
        }
    }
};

QTEST_MAIN(SearchFilterTests)
#include "search_filter_tests.moc"
