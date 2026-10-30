// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "model3d_panel.h"
#include <QtTest>

class StreamUiTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QApplication::setStyle("Fusion");
        QApplication::setFont(sw::Theme::interfaceFont());
    }

    void journalPagingKeepsChannelPermissionsAndGap()
    {
        sw::Journal journal;
        journal.append("a", "rx", "first", {}, {}, false, "serial");
        journal.append("b", "rx", "hidden", {}, {}, false, "ssh");
        journal.append("a", "tx", "second", {}, {}, false, "serial");
        journal.append("b", "rx", "last", {}, {}, false, "ssh");
        const auto serial = [](const QJsonObject &event) { return event["protocol"] == "serial"; };
        auto page = journal.read(0, 1, {}, serial);
        QCOMPARE(page["next_cursor"].toInteger(), 1);
        QVERIFY(page["has_more"].toBool());
        page = journal.read(1, 1, {}, serial);
        QCOMPARE(page["events"].toArray().first().toObject()["text"].toString(), QString("second"));
        QCOMPARE(page["next_cursor"].toInteger(), 4);
        QVERIFY(!page["has_more"].toBool());
        QCOMPARE(journal.read(1, 10, "b")["events"].toArray().size(), 2);
        QVERIFY(journal.read(4)["events"].toArray().isEmpty());
        QVERIFY(journal.read(9)["cursor_reset_required"].toBool());

        for (int i = 0; i < 20010; ++i) { journal.append("a", "rx", "line\n"); }
        page = journal.read(0, 2, "a");
        QVERIFY(page["gap"].toBool());
        const auto oldest = page["oldest_cursor"].toInteger();
        QCOMPARE(page["events"].toArray().first().toObject()["seq"].toInteger(), oldest);
        QVERIFY(!journal.read(oldest - 1, 2, "a")["gap"].toBool());
        QCOMPARE(journal.read(journal.sequence - 1)["events"].toArray().size(), 1);
    }

    void logBatchPreservesStreamsAnsiFilteringAndSelection()
    {
        sw::Journal journal;
        sw::LogView log;
        log.resize(520, 180);
        log.show();
        const auto bytes = QString("中文").toUtf8();
        log.feedBatch({journal.append("a", "rx", "\x1b[3", "one"),
            journal.append("a", "rx", "2m" + bytes.left(2), "one"),
            journal.append("a", "rx", "peer\n", "two"),
            journal.append("a", "tx", "command\n")});
        log.feedBatch({journal.append("a", "rx", bytes.mid(2) + "\x1b[0m done\r", "one"),
            journal.append("a", "rx", "\npartial", "one"),
            journal.append("a", "status", {}, {}, "reconnected", true),
            journal.append("a", "rx", "fresh\n", "one")});
        const auto text = log.toPlainText();
        QVERIFY(text.contains("中文 done"));
        QVERIFY(text.contains("peer"));
        QVERIFY(text.contains("command"));
        QVERIFY(text.contains("partial"));
        QCOMPARE(log.document()->blockCount(), 6);
        QTextCursor color(log.document());
        color.setPosition(text.indexOf("done") + 2);
        QCOMPARE(color.charFormat().foreground().color(), log.theme.text);
        color.setPosition(text.indexOf("中文") + 1);
        QVERIFY(color.charFormat().foreground().color() != log.theme.text);
        sw::SearchFilter filter;
        filter.text = "done || fresh";
        filter.logical = true;
        filter.compile();
        log.setFilter(filter);
        int visible = 0;
        for (auto block = log.document()->begin(); block.isValid(); block = block.next())
        {
            if (block.isVisible()) { ++visible; }
        }
        QCOMPARE(visible, 2);
        filter.text.clear();
        filter.compile();
        log.setFilter(filter);
        QJsonArray lines;
        for (int i = 0; i < 100; ++i) { lines.append(journal.append("a", "rx", "line\n")); }
        log.feedBatch(lines);
        log.follow = false;
        log.verticalScrollBar()->setValue(10);
        const auto scroll = log.verticalScrollBar()->value();
        QTextCursor selection(log.document());
        selection.setPosition(log.toPlainText().indexOf("command"));
        selection.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 7);
        log.setTextCursor(selection);
        log.feedBatch(lines);
        QCOMPARE(log.textCursor().selectedText(), QString("command"));
        QCOMPARE(log.verticalScrollBar()->value(), scroll);
        log.document()->setMaximumBlockCount(100);
        log.feedBatch({journal.append("a", "rx", "last-line\n")});
        QCOMPARE(log.document()->blockCount(), 100);
        QVERIFY(log.toPlainText().endsWith("last-line"));
    }

    void hiddenReceivePageStaysCurrentAndPauseStillWorks()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        auto receive = window.findPane(channel);
        window.newInstrument("waveform", channel);
        window.show();
        window.timer.stop();
        window.portTimer.stop();
        QVERIFY(!receive->isVisible());
        receive->clearView();
        const auto start = engine.journal.sequence;
        for (int i = 0; i < 180; ++i) { channel->receive("line-" + QByteArray::number(i) + "\r\n"); }
        receive->tick();
        QVERIFY(receive->cursor > start);
        QVERIFY(receive->cursor < engine.journal.sequence); // bounded work, not one large replay
        QTRY_COMPARE(receive->cursor, engine.journal.sequence);
        QVERIFY(!receive->catchupTimer.isActive());
        QVERIFY(receive->log->toPlainText().contains("line-179"));
        QVERIFY(receive->terminal->text().contains("line-179"));
        QCOMPARE(engine.journal.read(start, 1000, channel->id)["events"].toArray().size(), 180);

        receive->pause->setChecked(true);
        channel->receive("paused-line\n");
        const auto cursor = receive->cursor;
        receive->tick();
        QCOMPARE(receive->cursor, cursor);
        QVERIFY(!receive->log->toPlainText().contains("paused-line"));
        receive->pause->setChecked(false);
        receive->tick();
        window.showPane(receive);
        QCOMPARE(receive->cursor, engine.journal.sequence);
        QVERIFY(receive->log->toPlainText().contains("paused-line"));

        receive->clearView();
        engine.journal.append(channel->id, "rx", "discard-before-reset");
        engine.journal.append(channel->id, "status", {}, {}, "reset", true);
        engine.journal.append(channel->id, "rx", "keep-after-reset");
        engine.journal.append(channel->id, "rx", "transfer-data", {}, "file-transfer");
        receive->tick();
        QVERIFY(!receive->terminal->text().contains("discard-before-reset"));
        QVERIFY(!receive->terminal->text().contains("transfer-data"));
        QVERIFY(receive->terminal->text().contains("keep-after-reset"));
        QVERIFY(receive->log->toPlainText().contains("discard-before-reset"));
    }

    void cameraFieldsStayLiveAndFlushOnRelease()
    {
        sw::ModelPanel panel;
        auto view = panel.view;
        auto field = panel.findChild<QDoubleSpinBox *>("modelCamera0");
        const auto initial = field->value();
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(100, 100), QPointF(100, 100),
            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view, &press);
        for (int i = 1; i <= 80; ++i)
        {
            const QPointF point(100 + i, 100);
            QMouseEvent move(QEvent::MouseMove, point, point, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(view, &move);
        }
        QVERIFY(view->cameraSettings()["yaw"].toDouble() != initial);
        QCOMPARE(field->value(), initial);
        QTRY_VERIFY(qAbs(field->value() - view->cameraSettings()["yaw"].toDouble()) < 1e-5);
        QMouseEvent move(QEvent::MouseMove, QPointF(190, 100), QPointF(190, 100),
            Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view, &move);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(190, 100), QPointF(190, 100),
            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(view, &release);
        QVERIFY(!view->isNavigating());
        QVERIFY(qAbs(field->value() - view->cameraSettings()["yaw"].toDouble()) < 1e-5);
        view->restoreCamera({{"yaw", 35}});
        QCOMPARE(field->value(), 35.);
        QTest::qWait(45);
        QCOMPARE(field->value(), 35.); // no stale timer can overwrite an API update
    }
};

QTEST_MAIN(StreamUiTests)
#include "stream_ui_tests.moc"
