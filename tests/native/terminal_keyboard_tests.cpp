// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include <QtTest>

class TerminalKeyboardTests : public QObject
{
    Q_OBJECT
private:
    static constexpr auto SelectWord = Qt::ControlModifier | Qt::ShiftModifier;

    void prepare(sw::TerminalView &terminal, const QByteArray &text)
    {
        terminal.setFont(sw::Theme::defaultDataFont());
        terminal.resize(840, 360);
        terminal.show();
        terminal.setFocus();
        QApplication::processEvents();
        terminal.reset();
        terminal.enabledInput = true;
        terminal.keyboardSelectionEnabled = true;
        terminal.feed(text);
    }

    static QPoint terminalCell(const sw::TerminalView &terminal, int column, int row)
    {
        const QFontMetricsF metrics(terminal.font());
        return QPoint(int(16 + (column + 0.5) * metrics.horizontalAdvance('M')),
            int(12 + (row + 0.5) * metrics.height() * 1.15));
    }

    static void dragSelectionTo(sw::TerminalView &terminal, const QPoint &position)
    {
        QMouseEvent move(QEvent::MouseMove, position, terminal.mapToGlobal(position),
            Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&terminal, &move);
    }

    void prepareHistory(sw::TerminalView &terminal)
    {
        prepare(terminal, {});
        QByteArray output;
        for (int line = 0; line < 300; ++line)
        {
            if (line > 0) { output += "\r\n"; }
            output += "row-" + QByteArray::number(line).rightJustified(4, '0');
        }
        terminal.feed(output);
    }

private slots:
    void initTestCase()
    {
        QApplication::setStyle("Fusion");
        QApplication::setFont(sw::Theme::interfaceFont());
    }

    void cellBackgroundsHaveNoGaps_data()
    {
        QTest::addColumn<qreal>("scale");
        QTest::addColumn<int>("pointSize");
        QTest::addColumn<bool>("selection");
        for (const qreal scale : {1.0, 1.25, 1.5, 2.0})
        {
            for (const int pointSize : {11, 12, 13})
            {
                for (const bool selection : {false, true})
                {
                    const auto name = QString("%1x-%2pt-%3")
                        .arg(scale).arg(pointSize).arg(selection ? "selection" : "ansi");
                    QTest::newRow(qPrintable(name)) << scale << pointSize << selection;
                }
            }
        }
    }

    void cellBackgroundsHaveNoGaps()
    {
        QFETCH(qreal, scale);
        QFETCH(int, pointSize);
        QFETCH(bool, selection);
#ifdef Q_OS_WIN
        // The offscreen plugin does not discover Windows system fonts itself.
        if (QGuiApplication::platformName() == "offscreen")
        {
            const auto fonts = QStandardPaths::standardLocations(QStandardPaths::FontsLocation);
            for (const auto &directory : fonts)
            {
                if (QFontDatabase::addApplicationFont(directory + "/consola.ttf") >= 0) { break; }
            }
        }
#endif
        sw::TerminalView terminal;
        prepare(terminal, {});
        QFont font = terminal.font();
        font.setPointSize(pointSize);
        terminal.setFont(font);
        terminal.resize(1120, 800);
        terminal.cursorVisible = false;
        terminal.transparentBackground = selection;
        QApplication::processEvents();

        const QColor background = selection
            ? terminal.theme.palette().color(QPalette::Highlight) : QColor("#438b59");
        if (selection)
        {
            terminal.selectionAnchor = QPoint(0, 0);
            terminal.selectionCursor = QPoint(terminal.columns - 1, terminal.rows - 1);
            terminal.selectionActive = true;
        }
        else
        {
            for (auto &row : terminal.grid)
            {
                for (auto &cell : row)
                {
                    cell.background = background;
                }
            }
        }

        QImage capture(QSize(qCeil(terminal.width() * scale), qCeil(terminal.height() * scale)),
            QImage::Format_ARGB32_Premultiplied);
        capture.setDevicePixelRatio(scale);
        capture.fill(Qt::magenta);
        terminal.render(&capture);
        const auto captureDirectory = qEnvironmentVariable("SW_TERMINAL_CAPTURE_DIR");
        if (!captureDirectory.isEmpty())
        {
            QDir().mkpath(captureDirectory);
            QVERIFY(capture.save(captureDirectory + "/background-"
                + QTest::currentDataTag() + ".png"));
        }

        const QFontMetricsF metrics(terminal.font());
        // Check every interior pixel, including fractional cell boundaries. Exclude
        // the selection's partial last row and the outer edge of the cell grid.
        const int left = qCeil(17 * scale);
        const int top = qCeil(13 * scale);
        const int right = qFloor((16 + terminal.columns * metrics.horizontalAdvance('M') - 1) * scale);
        const int bottom = qFloor((12 + (terminal.rows - 1) * metrics.height() * 1.15 - 1) * scale);
        for (int y = top; y < bottom; ++y)
        {
            for (int x = left; x < right; ++x)
            {
                if (capture.pixelColor(x, y) != background)
                {
                    QFAIL(qPrintable(QString("Background gap at physical pixel (%1, %2): %3 instead of %4")
                        .arg(x).arg(y).arg(capture.pixelColor(x, y).name()).arg(background.name())));
                }
            }
        }
    }

    void wordSelectionAndSmartInterrupt()
    {
        sw::TerminalView terminal;
        prepare(terminal, "PS> echo alpha beta");
        const auto text = terminal.text();
        const QPoint remoteCursor(terminal.x, terminal.y);
        QSignalSpy sent(&terminal, &sw::TerminalView::transmit);
        QTest::keyClick(&terminal, Qt::Key_Left, SelectWord);
        QCOMPARE(terminal.selectedText(), QString("beta"));
        QTest::keyClick(&terminal, Qt::Key_Left, SelectWord);
        QCOMPARE(terminal.selectedText(), QString("alpha beta"));
        QCOMPARE(sent.size(), 0);
        QCOMPARE(QPoint(terminal.x, terminal.y), remoteCursor);
        QCOMPARE(terminal.text(), text);
        const auto capture = qEnvironmentVariable("SW_TERMINAL_CAPTURE_DIR");
        if (!capture.isEmpty())
        {
            QDir().mkpath(capture);
            QVERIFY(terminal.grab().save(capture + "/ssh-word-selection.png"));
            terminal.theme = sw::Theme::make(true);
            QVERIFY(terminal.grab().save(capture + "/ssh-word-selection-dark.png"));
            terminal.theme = sw::Theme::make(false);
        }
        QTest::keyClick(&terminal, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QString("alpha beta"));
        QVERIFY(!terminal.hasSelection());
        QCOMPARE(sent.size(), 0);
        QTest::keyClick(&terminal, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(sent.size(), 1);
        QCOMPARE(sent.takeFirst().first().toByteArray(), QByteArray(1, char(3)));
        QTest::keyClick(&terminal, Qt::Key_C, SelectWord);
        QCOMPARE(sent.size(), 0);
    }

    void heldCopyDoesNotInterrupt()
    {
        sw::TerminalView terminal;
        prepare(terminal, "copy this");
        QSignalSpy sent(&terminal, &sw::TerminalView::transmit);
        QTest::keyClick(&terminal, Qt::Key_Left, SelectWord);
        QKeyEvent first(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier);
        QApplication::sendEvent(&terminal, &first);
        QKeyEvent repeated(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier, {}, true);
        QApplication::sendEvent(&terminal, &repeated);
        QApplication::sendEvent(&terminal, &repeated);
        QCOMPARE(QApplication::clipboard()->text(), QString("this"));
        QCOMPARE(sent.size(), 0);
        QTest::keyClick(&terminal, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(sent.takeFirst().first().toByteArray(), QByteArray(1, char(3)));
    }

    void eraseSelectedInput_data()
    {
        QTest::addColumn<int>("key");
        QTest::addColumn<bool>("forward");
        QTest::newRow("backspace-before-caret") << int(Qt::Key_Backspace) << false;
        QTest::newRow("delete-before-caret") << int(Qt::Key_Delete) << false;
        QTest::newRow("backspace-after-caret") << int(Qt::Key_Backspace) << true;
        QTest::newRow("delete-after-caret") << int(Qt::Key_Delete) << true;
    }

    void eraseSelectedInput()
    {
        QFETCH(int, key);
        QFETCH(bool, forward);
        sw::TerminalView terminal;
        prepare(terminal, "PS> echo alpha beta");
        if (forward) { terminal.feed("\x1b[1;10H"); }

        QSignalSpy sent(&terminal, &sw::TerminalView::transmit);
        QTest::keyClick(&terminal, forward ? Qt::Key_Right : Qt::Key_Left, SelectWord);
        QCOMPARE(terminal.selectedText(), forward ? QString("alpha ") : QString("beta"));
        const QString screen = terminal.text();
        const QPoint caret(terminal.x, terminal.y);
        QTest::keyClick(&terminal, Qt::Key(key));

        QCOMPARE(sent.size(), 1);
        const QByteArray expected = forward ? QByteArray("\x1b[3~").repeated(6) : QByteArray(4, char(0x7f));
        QCOMPARE(sent.takeFirst().first().toByteArray(), expected);
        QVERIFY(!terminal.hasSelection());
        // The remote shell owns the input buffer and its echo. Do not erase the display locally.
        QCOMPARE(terminal.text(), screen);
        QCOMPARE(QPoint(terminal.x, terminal.y), caret);
        QTest::keyClick(&terminal, Qt::Key_Backspace);
        QCOMPARE(sent.takeFirst().first().toByteArray(), QByteArray(1, char(0x7f)));
    }

    void eraseUnicodeAndFullLine()
    {
        sw::TerminalView terminal;
        prepare(terminal, QString("echo 中文").toUtf8());
        QSignalSpy sent(&terminal, &sw::TerminalView::transmit);
        QTest::keyClick(&terminal, Qt::Key_Left, SelectWord);
        QTest::keyClick(&terminal, Qt::Key_Backspace);
        QCOMPARE(sent.takeFirst().first().toByteArray(), QByteArray(2, char(0x7f)));

        terminal.reset();
        terminal.feed(QString::fromUtf8("A😀").toUtf8());
        QTest::keyClick(&terminal, Qt::Key_Left, Qt::ShiftModifier);
        QTest::keyClick(&terminal, Qt::Key_Delete);
        QCOMPARE(sent.takeFirst().first().toByteArray(), QByteArray(1, char(0x7f)));

        terminal.reset();
        terminal.feed("alpha beta");
        QTest::keyClick(&terminal, Qt::Key_Home, Qt::ShiftModifier);
        QTest::keyClick(&terminal, Qt::Key_Backspace);
        QCOMPARE(sent.takeFirst().first().toByteArray(), QByteArray(10, char(0x7f)));

        terminal.reset();
        terminal.feed(QByteArray(terminal.columns, 'x'));
        QVERIFY(terminal.wrapPending);
        QTest::keyClick(&terminal, Qt::Key_Left, SelectWord);
        QTest::keyClick(&terminal, Qt::Key_Backspace);
        QCOMPARE(sent.takeFirst().first().toByteArray(), QByteArray(terminal.columns, char(0x7f)));
    }

    void outputSelectionCannotEraseRemoteInput()
    {
        sw::TerminalView terminal;
        prepare(terminal, "old output\r\necho ready");
        QSignalSpy sent(&terminal, &sw::TerminalView::transmit);
        QTest::keyClick(&terminal, Qt::Key_Home, SelectWord);
        QTest::keyClick(&terminal, Qt::Key_Backspace);
        QCOMPARE(sent.size(), 0);
        QVERIFY(terminal.hasSelection());

        terminal.clearSelection();
        terminal.selectionAnchor = QPoint(5, 1);
        terminal.selectionCursor = QPoint(9, 1);
        terminal.selectionActive = true;
        QTest::keyClick(&terminal, Qt::Key_Backspace);
        QCOMPARE(sent.size(), 0);
        // Extending a mouse selection does not turn it into an editable input selection.
        QTest::keyClick(&terminal, Qt::Key_Left, Qt::ShiftModifier);
        QTest::keyClick(&terminal, Qt::Key_Delete);
        QCOMPARE(sent.size(), 0);

        terminal.clearSelection();
        QTest::keyClick(&terminal, Qt::Key_Left, SelectWord);
        // A remote redraw at the same cursor position invalidates deletion, but not copying.
        terminal.feed("\rnew prompt");
        QTest::keyClick(&terminal, Qt::Key_Backspace);
        QCOMPARE(sent.size(), 0);
        QVERIFY(terminal.hasSelection());
        terminal.clearSelection();
        QTest::keyClick(&terminal, Qt::Key_Left, SelectWord);
        terminal.resize(700, 300);
        QApplication::processEvents();
        QTest::keyClick(&terminal, Qt::Key_Delete);
        QCOMPARE(sent.size(), 0);

        terminal.clearSelection();
        QTest::keyClick(&terminal, Qt::Key_Left, SelectWord);
        terminal.enabledInput = false;
        QTest::keyClick(&terminal, Qt::Key_Backspace);
        QCOMPARE(sent.size(), 0);
        QToolTip::hideText();
    }

    void charactersShrinkReverseAndEscape()
    {
        sw::TerminalView terminal;
        prepare(terminal, "abc");
        QSignalSpy sent(&terminal, &sw::TerminalView::transmit);
        QTest::keyClick(&terminal, Qt::Key_Left, Qt::ShiftModifier);
        QCOMPARE(terminal.selectedText(), QString("c"));
        QTest::keyClick(&terminal, Qt::Key_Left, Qt::ShiftModifier);
        QCOMPARE(terminal.selectedText(), QString("bc"));
        QTest::keyClick(&terminal, Qt::Key_Right, Qt::ShiftModifier);
        QCOMPARE(terminal.selectedText(), QString("c"));
        QTest::keyClick(&terminal, Qt::Key_Right, Qt::ShiftModifier);
        QVERIFY(!terminal.hasSelection());
        QTest::keyClick(&terminal, Qt::Key_Right, Qt::ShiftModifier);
        QCOMPARE(terminal.selectedText(), QString(" "));
        QTest::keyClick(&terminal, Qt::Key_Escape);
        QVERIFY(!terminal.hasSelection());
        QCOMPARE(sent.size(), 0);
        QTest::keyClick(&terminal, Qt::Key_Escape);
        QCOMPARE(sent.takeFirst().first().toByteArray(), QByteArray(1, char(27)));
    }

    void lineAndScreenSelection()
    {
        sw::TerminalView terminal;
        prepare(terminal, "alpha beta\r\none two");
        QSignalSpy sent(&terminal, &sw::TerminalView::transmit);
        QTest::keyClick(&terminal, Qt::Key_Home, Qt::ShiftModifier);
        QCOMPARE(terminal.selectedText(), QString("one two"));
        QTest::keyClick(&terminal, Qt::Key_End, Qt::ShiftModifier);
        QVERIFY(!terminal.hasSelection());
        QTest::keyClick(&terminal, Qt::Key_Home, SelectWord);
        QCOMPARE(terminal.selectedText(), QString("alpha beta\none two"));
        QTest::keyClick(&terminal, Qt::Key_End, SelectWord);
        QVERIFY(!terminal.hasSelection());
        QTest::keyClick(&terminal, Qt::Key_Up, Qt::ShiftModifier);
        QCOMPARE(terminal.selectedText(), QString("eta\none two"));
        QTest::keyClick(&terminal, Qt::Key_Down, Qt::ShiftModifier);
        QVERIFY(!terminal.hasSelection());
        QCOMPARE(sent.size(), 0);
    }

    void wordsForwardAndPunctuation()
    {
        sw::TerminalView terminal;
        prepare(terminal, "alpha beta-gamma");
        terminal.feed("\x1b[1;7H");
        QTest::keyClick(&terminal, Qt::Key_Right, SelectWord);
        QCOMPARE(terminal.selectedText(), QString("beta"));
        QTest::keyClick(&terminal, Qt::Key_Right, SelectWord);
        QCOMPARE(terminal.selectedText(), QString("beta-"));
        QTest::keyClick(&terminal, Qt::Key_Right, SelectWord);
        QCOMPARE(terminal.selectedText(), QString("beta-gamma"));
        QTest::keyClick(&terminal, Qt::Key_Right, SelectWord);
        QCOMPARE(terminal.selectedText(), QString("beta-gamma"));
    }

    void unicodeAndMouseSelection()
    {
        sw::TerminalView terminal;
        prepare(terminal, QString("温度 中文").toUtf8());
        QTest::keyClick(&terminal, Qt::Key_Left, SelectWord);
        QCOMPARE(terminal.selectedText(), QString("中文"));
        QTest::keyClick(&terminal, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QString("中文"));
        QTest::keyClick(&terminal, Qt::Key_Left, Qt::ShiftModifier);
        QCOMPARE(terminal.selectedText(), QString("文"));
        terminal.reset();
        terminal.feed(QString::fromUtf8("A😀").toUtf8());
        QTest::keyClick(&terminal, Qt::Key_Left, Qt::ShiftModifier);
        QCOMPARE(terminal.selectedText(), QString::fromUtf8("😀"));
        terminal.reset();
        terminal.feed("abcdef");
        const QFontMetricsF metrics(terminal.font());
        const auto cell = [&](int column)
        {
            return QPoint(int(16 + (column + 0.5) * metrics.horizontalAdvance('M')),
                int(12 + metrics.height() * 1.15 / 2));
        };
        QTest::mousePress(&terminal, Qt::LeftButton, {}, cell(1));
        QTest::mouseRelease(&terminal, Qt::LeftButton, {}, cell(3));
        QCOMPARE(terminal.selectedText(), QString("bcd"));
        QTest::keyClick(&terminal, Qt::Key_Left, Qt::ShiftModifier);
        QCOMPARE(terminal.selectedText(), QString("bc"));
    }

    void clipboardEditingAndReadOnly()
    {
        sw::TerminalView terminal;
        prepare(terminal, "echo ready");
        QSignalSpy sent(&terminal, &sw::TerminalView::transmit);
        QTest::keyClick(&terminal, Qt::Key_Left, SelectWord);
        QTest::keyClick(&terminal, Qt::Key_Insert, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QString("ready"));
        QVERIFY(!terminal.hasSelection());
        QCOMPARE(sent.size(), 0);
        QTest::keyClick(&terminal, Qt::Key_Left, SelectWord);
        QApplication::clipboard()->setText("pasted");
        QTest::keyClick(&terminal, Qt::Key_Insert, Qt::ShiftModifier);
        QCOMPARE(sent.takeFirst().first().toByteArray(), QByteArray("pasted"));
        QVERIFY(!terminal.hasSelection());
        QTest::keyClick(&terminal, Qt::Key_Left, SelectWord);
        QInputMethodEvent commit;
        commit.setCommitString("你好");
        QApplication::sendEvent(&terminal, &commit);
        QCOMPARE(sent.takeFirst().first().toByteArray(), QString("你好").toUtf8());
        QVERIFY(!terminal.hasSelection());
        terminal.enabledInput = false;
        QTest::keyClick(&terminal, Qt::Key_Left, SelectWord);
        QCOMPARE(terminal.selectedText(), QString("ready"));
        QTest::keyClick(&terminal, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QString("ready"));
        QTest::keyClick(&terminal, Qt::Key_C, Qt::ControlModifier);
        QTest::keyClick(&terminal, Qt::Key_V, SelectWord);
        QCOMPARE(sent.size(), 0);
    }

    void remoteKeysAndPreferences()
    {
        sw::TerminalView terminal;
        prepare(terminal, "remote");
        QSignalSpy sent(&terminal, &sw::TerminalView::transmit);
        QTest::keyClick(&terminal, Qt::Key_Left, SelectWord);
        terminal.copyShortcutEnabled = false;
        QTest::keyClick(&terminal, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(sent.takeFirst().first().toByteArray(), QByteArray(1, char(3)));
        QVERIFY(!terminal.hasSelection());
        QTest::keyClick(&terminal, Qt::Key_Left, Qt::ControlModifier);
        QCOMPARE(sent.takeFirst().first().toByteArray(), QByteArray("\x1b[1;5D"));
        QTest::keyClick(&terminal, Qt::Key_Up, Qt::ControlModifier);
        QCOMPARE(sent.takeFirst().first().toByteArray(), QByteArray("\x1b[1;5A"));
        QTest::keyClick(&terminal, Qt::Key_Tab, Qt::ShiftModifier);
        QCOMPARE(sent.takeFirst().first().toByteArray(), QByteArray("\x1b[Z"));
        terminal.keyboardSelectionEnabled = false;
        QTest::keyClick(&terminal, Qt::Key_Left, SelectWord);
        QCOMPARE(sent.takeFirst().first().toByteArray(), QByteArray("\x1b[1;6D"));
        QVERIFY(!terminal.hasSelection());
    }

    void screenBoundsAndResize()
    {
        sw::TerminalView terminal;
        prepare(terminal, {});
        terminal.feed(QByteArray(terminal.columns * terminal.rows, 'x'));
        QTest::keyClick(&terminal, Qt::Key_Left, Qt::ShiftModifier);
        QCOMPARE(terminal.selectedText(), QString("x"));
        QTest::keyClick(&terminal, Qt::Key_Home, Qt::ShiftModifier);
        QCOMPARE(terminal.selectedText(), QString(terminal.columns, 'x'));
        terminal.resize(400, 180);
        QApplication::processEvents();
        QVERIFY(terminal.selectedText().size() <= terminal.rows * terminal.columns + terminal.rows);
        terminal.reset();
        QVERIFY(!terminal.hasSelection());
    }

    void draggingAboveScrollsAndCopiesManyHistoryLines()
    {
        sw::TerminalView terminal;
        prepareHistory(terminal);
        auto scroll = terminal.verticalScrollBar();
        scroll->setValue(100);
        QSignalSpy sent(&terminal, &sw::TerminalView::transmit);
        QTest::mousePress(&terminal, Qt::LeftButton, {}, terminalCell(terminal, 7, 8));
        const QPoint anchor = terminal.selectionAnchor;
        const QPoint outside(terminalCell(terminal, 0, 0).x(), -80);
        dragSelectionTo(terminal, outside);
        // No more mouse moves: holding beyond the edge must keep scrolling.
        QTRY_COMPARE_WITH_TIMEOUT(scroll->value(), 0, 3000);
        QCOMPARE(terminal.selectionAnchor, anchor);
        const auto lines = terminal.selectedText().split('\n');
        QCOMPARE(lines.size(), 109);
        QCOMPARE(lines.first(), QString("row-0000"));
        QCOMPARE(lines.last(), QString("row-0108"));
        const auto capture = qEnvironmentVariable("SW_TERMINAL_CAPTURE_DIR");
        if (!capture.isEmpty())
        {
            QDir().mkpath(capture);
            QVERIFY(terminal.grab().save(capture + "/ssh-drag-history-selection.png"));
        }
        QTest::mouseRelease(&terminal, Qt::LeftButton, {}, outside);
        const auto selected = terminal.selectedText();
        QTest::keyClick(&terminal, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), selected);
        QCOMPARE(sent.size(), 0);
    }

    void draggingBelowStopsOnRelease()
    {
        sw::TerminalView terminal;
        prepareHistory(terminal);
        auto scroll = terminal.verticalScrollBar();
        scroll->setValue(20);
        QSignalSpy sent(&terminal, &sw::TerminalView::transmit);
        QTest::mousePress(&terminal, Qt::LeftButton, {}, terminalCell(terminal, 0, 2));
        const QPoint outside(terminalCell(terminal, 7, 0).x(), terminal.height() + 80);
        dragSelectionTo(terminal, outside);
        QTRY_VERIFY_WITH_TIMEOUT(scroll->value() > 20, 1000);
        QTest::mouseRelease(&terminal, Qt::LeftButton, {}, outside);
        const int stopped = scroll->value();
        const auto selected = terminal.selectedText();
        QTest::qWait(160);
        QCOMPARE(scroll->value(), stopped);
        QCOMPARE(terminal.selectedText(), selected);
        QCOMPARE(sent.size(), 0);
    }

    void draggingCanReturnInsideAndReverseScrollDirection()
    {
        sw::TerminalView terminal;
        prepareHistory(terminal);
        auto scroll = terminal.verticalScrollBar();
        scroll->setValue(100);
        QTest::mousePress(&terminal, Qt::LeftButton, {}, terminalCell(terminal, 7, 8));
        const QPoint anchor = terminal.selectionAnchor;
        dragSelectionTo(terminal, QPoint(30, -60));
        QTRY_VERIFY_WITH_TIMEOUT(scroll->value() < 100, 1000);
        dragSelectionTo(terminal, terminalCell(terminal, 0, 4));
        const int stopped = scroll->value();
        QTest::qWait(160);
        QCOMPARE(scroll->value(), stopped);
        const QPoint below(terminalCell(terminal, 7, 0).x(), terminal.height() + 60);
        dragSelectionTo(terminal, below);
        QTRY_COMPARE_WITH_TIMEOUT(scroll->value(), scroll->maximum(), 3000);
        QCOMPARE(terminal.selectionAnchor, anchor);
        QTest::mouseRelease(&terminal, Qt::LeftButton, {}, below);
        QVERIFY(terminal.selectedText().contains("row-0299"));
    }

    void wheelWhileDraggingExtendsSelectionAtTheSamePointer()
    {
        sw::TerminalView terminal;
        prepareHistory(terminal);
        auto scroll = terminal.verticalScrollBar();
        scroll->setValue(50);
        QSignalSpy sent(&terminal, &sw::TerminalView::transmit);
        QTest::mousePress(&terminal, Qt::LeftButton, {}, terminalCell(terminal, 7, 5));
        const auto position = terminalCell(terminal, 0, 1);
        dragSelectionTo(terminal, position);
        QWheelEvent wheel(position, terminal.mapToGlobal(position), {}, QPoint(0, 120),
            Qt::LeftButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&terminal, &wheel);
        QCOMPARE(scroll->value(), 47);
        QCOMPARE(terminal.selectionCursor.y(), 48);
        QCOMPARE(terminal.selectedText().split('\n').first(), QString("row-0048"));
        QTest::mouseRelease(&terminal, Qt::LeftButton, {}, position);
        QCOMPARE(sent.size(), 0);
    }

    void incomingOutputDoesNotPullAnActiveDragToTheBottom()
    {
        sw::TerminalView terminal;
        prepareHistory(terminal);
        auto scroll = terminal.verticalScrollBar();
        QTest::mousePress(&terminal, Qt::LeftButton, {}, terminalCell(terminal, 7, 5));
        dragSelectionTo(terminal, terminalCell(terminal, 0, 1));
        const int reading = scroll->value();
        const auto visible = terminal.visibleText();
        terminal.feed("\r\nnew output\r\nmore output");
        QCOMPARE(scroll->value(), reading);
        QCOMPARE(terminal.visibleText(), visible);
        QTest::mouseRelease(&terminal, Qt::LeftButton, {}, terminalCell(terminal, 0, 1));
    }

    void endingDragStopsAutomaticScrolling_data()
    {
        QTest::addColumn<int>("action");
        QTest::newRow("clear-selection") << 0;
        QTest::newRow("reset") << 1;
        QTest::newRow("hidden") << 2;
        QTest::newRow("lost-mouse-grab") << 3;
        QTest::newRow("window-deactivated") << 4;
        QTest::newRow("alternate-screen") << 5;
    }

    void endingDragStopsAutomaticScrolling()
    {
        QFETCH(int, action);
        sw::TerminalView terminal;
        prepareHistory(terminal);
        auto scroll = terminal.verticalScrollBar();
        scroll->setValue(100);
        QTest::mousePress(&terminal, Qt::LeftButton, {}, terminalCell(terminal, 7, 5));
        dragSelectionTo(terminal, QPoint(30, -60));
        QTRY_VERIFY_WITH_TIMEOUT(scroll->value() < 100, 1000);
        if (action == 0) { terminal.clearSelection(); }
        else if (action == 1) { terminal.reset(); }
        else if (action == 2) { terminal.hide(); }
        else if (action == 5) { terminal.feed("\x1b[?1049h"); }
        else
        {
            QEvent cancel(action == 3 ? QEvent::UngrabMouse : QEvent::WindowDeactivate);
            QApplication::sendEvent(&terminal, &cancel);
        }
        QVERIFY(!terminal.selecting);
        const int stopped = scroll->value();
        QTest::qWait(160);
        QCOMPARE(scroll->value(), stopped);
        QTest::mouseRelease(&terminal, Qt::LeftButton, {}, QPoint(30, -60));
    }

    void scrollbackRetainsOutputAndKeepsReadingPosition()
    {
        sw::TerminalView terminal;
        prepare(terminal, {});
        QByteArray output;
        for (int i = 0; i < 300; ++i)
        {
            output += "row-" + QByteArray::number(i).rightJustified(4, '0') + "\r\n";
        }
        terminal.feed(output);
        QVERIFY(terminal.historyLineCount() > 250);
        auto scroll = terminal.verticalScrollBar();
        QCOMPARE(scroll->value(), scroll->maximum());
        QVERIFY(terminal.visibleText().contains("row-0299"));
        QSignalSpy sent(&terminal, &sw::TerminalView::transmit);
        const QPoint wheelPosition(80, 80);
        QWheelEvent wheel(wheelPosition, terminal.mapToGlobal(wheelPosition), {}, QPoint(0, 120),
            Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&terminal, &wheel);
        QCOMPARE(scroll->value(), scroll->maximum() - 3);
        scroll->setValue(0);
        QVERIFY(terminal.visibleText().startsWith("row-0000\n"));
        const auto previous = terminal.visibleText();
        terminal.feed("more output\r\n");
        QCOMPARE(scroll->value(), 0);
        QCOMPARE(terminal.visibleText(), previous);
        const QFontMetricsF metrics(terminal.font());
        const auto cell = [&](int column)
        {
            return QPoint(int(16 + (column + 0.5) * metrics.horizontalAdvance('M')),
                int(12 + metrics.height() * 1.15 / 2));
        };
        QTest::mousePress(&terminal, Qt::LeftButton, {}, cell(0));
        QTest::mouseRelease(&terminal, Qt::LeftButton, {}, cell(7));
        QCOMPARE(terminal.selectedText(), QString("row-0000"));
        QTest::keyClick(&terminal, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QString("row-0000"));
        QCOMPARE(sent.size(), 0);
        QTest::keyClicks(&terminal, "x");
        QCOMPARE(scroll->value(), scroll->maximum());
        QCOMPARE(sent.takeFirst().first().toByteArray(), QByteArray("x"));
        terminal.reset();
        QCOMPARE(terminal.historyLineCount(), 0);
        QCOMPARE(scroll->maximum(), 0);
    }

    void scrollbackIsBoundedAndResizingKeepsThePrompt()
    {
        sw::TerminalView terminal;
        prepare(terminal, {});
        QByteArray output;
        for (int i = 0; i < 10100; ++i)
        {
            output += "line-" + QByteArray::number(i) + "\r\n";
        }
        terminal.feed(output + "prompt>");
        QCOMPARE(terminal.historyLineCount(), 10000);
        terminal.verticalScrollBar()->setValue(0);
        const int first = 10101 - terminal.rows - 10000;
        QVERIFY(terminal.visibleText().startsWith(QString("line-%1\n").arg(first)));
        terminal.verticalScrollBar()->setValue(100);
        const auto pinned = terminal.visibleText();
        terminal.feed("\r\nnext prompt>");
        QCOMPARE(terminal.visibleText(), pinned);
        terminal.verticalScrollBar()->setValue(terminal.verticalScrollBar()->maximum());
        terminal.resize(840, 180);
        QApplication::processEvents();
        QVERIFY(terminal.text().contains("prompt>"));
        QVERIFY(terminal.visibleText().contains("prompt>"));
        QCOMPARE(terminal.historyLineCount(), 10000);
    }

    void alternateScreenAndExplicitHistoryClear()
    {
        sw::TerminalView terminal;
        prepare(terminal, {});
        for (int i = 0; i < 100; ++i) { terminal.feed("shell output\r\n"); }
        terminal.feed("prompt>");
        const int history = terminal.historyLineCount();
        const auto normal = terminal.text();
        terminal.verticalScrollBar()->setValue(3);
        const auto reading = terminal.visibleText();
        terminal.feed("\x1b[?1049h");
        for (int i = 0; i < 100; ++i) { terminal.feed("full screen output\r\n"); }
        QCOMPARE(terminal.historyLineCount(), history);
        QCOMPARE(terminal.verticalScrollBar()->maximum(), 0);
        QTest::keyClick(&terminal, Qt::Key_Home, SelectWord);
        QVERIFY(terminal.selectedText().contains("full screen output"));
        terminal.feed("\x1b[?1049l");
        QCOMPARE(terminal.text(), normal);
        QCOMPARE(terminal.historyLineCount(), history);
        QCOMPARE(terminal.verticalScrollBar()->value(), 3);
        QCOMPARE(terminal.visibleText(), reading);
        terminal.feed("\x1b[3J");
        QCOMPARE(terminal.historyLineCount(), 0);
        QCOMPARE(terminal.text(), normal);
        terminal.feed("\x1b[2J\x1b[H");
        QVERIFY(terminal.text().trimmed().isEmpty());
    }

    void sshSessionKeepsCommandsInTheTerminal()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        engine.sshExecutable = QCoreApplication::applicationDirPath() + "/fake_ssh.exe";
        sw::McpServer server(&engine);
        sw::Window window(&engine, &server, directory.filePath("settings.json"));
        window.resize(1100, 740);
        window.show();
        window.protocol->setCurrentIndex(4);
        window.sshTarget->setCurrentText("history-test");
        window.connectionAction();
        auto pane = window.active;
        QVERIFY(pane->channel);
        QTRY_COMPARE(pane->channel->state, QString("connected"));
        QByteArray output;
        for (int line = 1; line <= 300; ++line)
        {
            output += "output " + QByteArray::number(line) + ": SSH terminal history\r\n";
        }
        pane->channel->receive(output + "\x1b[32muser@host:~$ \x1b[0m");
        pane->tick();
        QVERIFY(pane->terminalMode);
        QVERIFY(pane->modeButton->isHidden());
        QVERIFY(pane->sender->isHidden());
        QVERIFY(pane->log->toPlainText().isEmpty());
        QVERIFY(pane->terminal->historyLineCount() > 250);
        QVERIFY(pane->terminal->visibleText().contains("user@host:~$"));
        const auto capture = qEnvironmentVariable("SW_TERMINAL_CAPTURE_DIR");
        if (!capture.isEmpty())
        {
            QDir().mkpath(capture);
            QVERIFY(window.grab().save(capture + "/ssh-terminal-live.png"));
        }
        pane->terminal->verticalScrollBar()->setValue(0);
        QVERIFY(pane->terminal->visibleText().contains("output 1:"));
        if (!capture.isEmpty())
        {
            QVERIFY(window.grab().save(capture + "/ssh-terminal-history.png"));
        }
        const auto firstHistoryLine = pane->terminal->visibleText().section('\n', 0, 0);
        pane->terminal->verticalScrollBar()->setValue(200);
        QSignalSpy sent(pane->terminal, &sw::TerminalView::transmit);
        QTest::mousePress(pane->terminal, Qt::LeftButton, {}, terminalCell(*pane->terminal, 7, 7));
        const QPoint outside(terminalCell(*pane->terminal, 0, 0).x(), -80);
        dragSelectionTo(*pane->terminal, outside);
        QTRY_COMPARE_WITH_TIMEOUT(pane->terminal->verticalScrollBar()->value(), 0, 5000);
        const auto selectedLines = pane->terminal->selectedText().split('\n');
        QCOMPARE(selectedLines.first(), firstHistoryLine);
        QCOMPARE(selectedLines.size(), 208);
        QVERIFY(pane->terminal->selectedText().contains("output 1:"));
        if (!capture.isEmpty())
        {
            QVERIFY(window.grab().save(capture + "/ssh-workbench-drag-selection.png"));
        }
        QTest::mouseRelease(pane->terminal, Qt::LeftButton, {}, outside);
        const auto selected = pane->terminal->selectedText();
        QTest::keyClick(pane->terminal, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), selected);
        QCOMPARE(sent.size(), 0);
        pane->setTerminal(false);
        QVERIFY(pane->terminalMode);
        QVERIFY(pane->sender->isHidden());
        window.close();
    }

    void sshPaneEnablesLocalSelection()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer server(&engine);
        sw::Window window(&engine, &server, directory.filePath("settings.json"));
        auto pane = window.newConnection();
        pane->draft.kind = "ssh";
        pane->setTerminal(true);
        pane->tick();
        QVERIFY(pane->terminal->keyboardSelectionEnabled);
        QVERIFY(pane->modeButton->isHidden());
        QVERIFY(pane->sender->isHidden());
        pane->setTerminal(false);
        QVERIFY(pane->terminalMode);
        QCOMPARE(pane->display->currentWidget(), pane->terminal);
        QVERIFY(pane->sender->isHidden());
        QVERIFY(pane->timestamps->isHidden());
        pane->restore({{"terminal_mode", false}});
        QVERIFY(pane->terminalMode);
        pane->draft.kind = "serial";
        pane->tick();
        QVERIFY(!pane->terminal->keyboardSelectionEnabled);
        QVERIFY(!pane->modeButton->isHidden());
        pane->setTerminal(false);
        QVERIFY(!pane->terminalMode);
        QVERIFY(!pane->sender->isHidden());
        window.close();
    }

    void savedSshTerminalModeDoesNotLeakIntoSerialDraft()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.json");
        QFile settings(path);
        QVERIFY(settings.open(QIODevice::WriteOnly));
        settings.write(QJsonDocument(QJsonObject {
            {"connection", QJsonObject {{"kind", "ssh"}, {"host", "example.invalid"}}},
            {"appearance", QJsonObject {{"terminal_mode", true}}}}).toJson());
        settings.close();

        sw::Engine engine;
        sw::McpServer server(&engine);
        sw::Window window(&engine, &server, path);
        auto pane = window.active;
        QVERIFY(!pane->channel);
        QVERIFY(engine.channels.isEmpty());
        QVERIFY(pane->terminalMode);
        QVERIFY(pane->sender->isHidden());

        // Exercise the actual protocol selector rather than mutating the draft.
        window.protocol->setCurrentIndex(0);
        pane->tick();
        QCOMPARE(pane->draft.kind, QString("serial"));
        QVERIFY(!pane->terminalMode);
        QVERIFY(!pane->sender->isHidden());
        QVERIFY(!pane->modeButton->isHidden());
        QVERIFY(engine.channels.isEmpty());
        window.close();
    }

    void connectedSerialRestoresExplicitTerminalPreference()
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
        channel->receive("MODE_RESTORE_SAMPLE\r\n");
        const auto history = engine.journal.read(0, 1000, channel->id);

        pane->restore({{"terminal_mode", true}});
        pane->tick();
        QVERIFY(pane->userTerminalMode);
        QVERIFY(pane->terminalMode);
        QCOMPARE(pane->display->currentWidget(), static_cast<QWidget *>(pane->terminal));
        QVERIFY(pane->sender->isHidden());

        pane->restore({{"terminal_mode", false}});
        pane->tick();
        QVERIFY(!pane->userTerminalMode);
        QVERIFY(!pane->terminalMode);
        QCOMPARE(pane->display->currentWidget(), static_cast<QWidget *>(pane->log));
        QVERIFY(!pane->sender->isHidden());
        QCOMPARE(engine.journal.read(0, 1000, channel->id), history);
        QCOMPARE(channel->tx, 0);
        window.close();
    }

    void switchingFromSshBackToSerialRestoresLogView()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer server(&engine);
        sw::Window window(&engine, &server, directory.filePath("settings.json"));
        auto pane = window.newConnection();
        pane->draft.kind = "ssh";
        pane->tick();
        QVERIFY(pane->terminalMode);
        QVERIFY(pane->sender->isHidden());
        pane->draft.kind = "serial";
        pane->tick();
        QVERIFY(!pane->terminalMode);
        QCOMPARE(pane->display->currentWidget(), static_cast<QWidget *>(pane->log));
        QVERIFY(!pane->sender->isHidden());
        QVERIFY(!pane->modeButton->isHidden());
        window.close();
    }

    void manualTerminalPreferenceSurvivesSshExcursion()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer server(&engine);
        sw::Window window(&engine, &server, directory.filePath("settings.json"));
        auto pane = window.newConnection();
        pane->draft.kind = "serial";
        pane->tick();
        QTest::mouseClick(pane->modeButton, Qt::LeftButton);
        QVERIFY(pane->terminalMode);
        pane->draft.kind = "ssh";
        pane->tick();
        QVERIFY(pane->terminalMode);
        pane->draft.kind = "serial";
        pane->tick();
        QVERIFY(pane->terminalMode);
        QVERIFY(pane->sender->isHidden());
        QCOMPARE(pane->display->currentWidget(), static_cast<QWidget *>(pane->terminal));
        window.close();
    }
};
QTEST_MAIN(TerminalKeyboardTests)
#include "terminal_keyboard_tests.moc"
