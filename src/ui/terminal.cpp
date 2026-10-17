// SPDX-License-Identifier: Apache-2.0

#include "terminal.h"
#include <QInputMethod>

namespace sw
{
static QColor ansiColor(int index)
{
    static const QList<QColor> basic {QColor("#263342"), QColor("#cf5266"), QColor("#26997c"), QColor("#ad812b"),
        QColor("#397fca"), QColor("#9d68b7"), QColor("#218f9a"), QColor("#c4cbd4"),
        QColor("#7a899a"), QColor("#ed8894"), QColor("#62c8a3"), QColor("#ddbd72"),
        QColor("#80b5fa"), QColor("#c29cdd"), QColor("#69c8d1"), QColor("#f0f4fa")};
    if (index < 16)
    {
        return basic[qBound(0, index, 15)];
    }
    if (index >= 232)
    {
        const int level = qBound(0, 8 + (index - 232) * 10, 255);
        return QColor(level, level, level);
    }
    index -= 16;
    auto component = [](int value) { return value == 0 ? 0 : 55 + value * 40; };
    return QColor(component(index / 36), component(index / 6 % 6), component(index % 6));
}

void AnsiState::sgr(const QList<int> &input, const Theme &theme)
{
    const auto codes = input.isEmpty() ? QList<int> {0} : input;
    for (int i = 0; i < codes.size(); ++i)
    {
        const int code = codes[i];
        if (code == 0)
        {
            foreground = QColor();
            background = QColor();
            bold = false;
        }
        else if (code == 1 || code == 22)
        {
            bold = code == 1;
        }
        else if (code >= 30 && code <= 37)
        {
            foreground = ansiColor(code - 30);
        }
        else if (code >= 90 && code <= 97)
        {
            foreground = ansiColor(code - 90 + 8);
        }
        else if (code >= 40 && code <= 47)
        {
            background = ansiColor(code - 40);
        }
        else if (code >= 100 && code <= 107)
        {
            background = ansiColor(code - 100 + 8);
        }
        else if (code == 39)
        {
            foreground = QColor();
        }
        else if (code == 49)
        {
            background = QColor();
        }
        else if ((code == 38 || code == 48) && i + 1 < codes.size())
        {
            QColor color;
            if (codes[i + 1] == 5 && i + 2 < codes.size())
            {
                color = ansiColor(qBound(0, codes[i + 2], 255));
                i += 2;
            }
            else if (codes[i + 1] == 2 && i + 4 < codes.size())
            {
                color = QColor(qBound(0, codes[i + 2], 255), qBound(0, codes[i + 3], 255), qBound(0, codes[i + 4], 255));
                i += 4;
            }
            if (code == 38)
            {
                foreground = color;
            }
            else
            {
                background = color;
            }
        }
    }
}

LogView::LogView(QWidget *parent) : QTextEdit(parent)
{
    setObjectName("monitor");
    setReadOnly(true);
    document()->setMaximumBlockCount(4000);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    setWrapping(true);
}

void LogView::setWrapping(bool enabled)
{
    setLineWrapMode(enabled ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
    setHorizontalScrollBarPolicy(enabled ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    if (follow)
    {
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    }
}

void LogView::paintEvent(QPaintEvent *event)
{
    QTextEdit::paintEvent(event);
    if (!document()->isEmpty())
    {
        return;
    }

    QPainter painter(viewport());
    if (viewport()->height() < 180)
    {
        painter.setFont(Theme::interfaceFont());
        painter.setPen(theme.muted);
        painter.drawText(viewport()->rect(), Qt::AlignCenter, "等待设备数据");
        return;
    }
    const int center = viewport()->width() / 2;
    const int top = qMax(20, viewport()->height() / 2 - 90);
    const QRect iconRect(center - 36, top - 4, 72, 72);
    drawIcon(painter, "lightning", iconRect, theme.green);

    QFont title = Theme::interfaceFont();
    title.setPixelSize(22);
    painter.setFont(title);
    painter.setPen(theme.text);
    painter.drawText(QRect(0, top + 82, viewport()->width(), 30), Qt::AlignCenter, "等待设备数据");
    painter.setFont(Theme::interfaceFont());
    painter.setPen(theme.muted);
    painter.drawText(QRect(8, top + 124, viewport()->width() - 16, 24), Qt::AlignCenter, "建立连接后，收发内容会显示在这里");
}

void LogView::scrollContentsBy(int dx, int dy)
{
    QTextEdit::scrollContentsBy(dx, dy);
    // The wallpaper stays attached to the viewport while the text scrolls.
    if (transparentBackground) { viewport()->update(); }
}

void LogView::reset()
{
    clear();
    streams.clear();
}

void LogView::setFilter(const SearchFilter &criteria)
{
    matcher = criteria;
    filter = criteria.text;
    const int previous = verticalScrollBar()->value();
    for (auto block = document()->begin(); block.isValid(); block = block.next())
    {
        const bool visible = matcher.matches(block.text());
        block.setVisible(visible);
        block.setLineCount(visible ? 1 : 0);
    }
    document()->markContentsDirty(0, document()->characterCount());
    viewport()->update();
    verticalScrollBar()->setValue(follow ? verticalScrollBar()->maximum() : previous);
}

void LogView::feed(const QJsonObject &event)
{
    const QString direction = event["direction"].toString();
    const QString key = direction + ":" + event["peer"].toString();
    if (event["stream_reset"].toBool())
    {
        streams.clear();
    }
    if (!streams.contains(key))
    {
        auto stream = std::make_shared<Stream>();
        auto codec = QTextCodec::codecForName(encoding.toLatin1());
        stream->decoder.reset((codec ? codec : QTextCodec::codecForName("UTF-8"))->makeDecoder());
        streams[key] = stream;
    }
    auto &stream = *streams[key];
    const QByteArray data = QByteArray::fromHex(event["hex"].toString().toLatin1());
    QString text = hex && !data.isEmpty() ? event["hex"].toString().toUpper() + "\n" : stream.decoder->toUnicode(data);
    if (direction == "status")
    {
        text = event["message"].toString() + "\n";
    }
    const int previous = verticalScrollBar()->value();
    const auto ensureLine = [&]
    {
        if (!stream.block.isValid())
        {
            QTextCursor cursor(document());
            cursor.movePosition(QTextCursor::End);
            if (!document()->isEmpty())
            {
                cursor.insertBlock();
            }
            QTextCharFormat format;
            format.setForeground(theme.muted);
            QString prefix = timestamps ? event["time"].toString().mid(11, 12) + "  " : QString();
            prefix += direction.toUpper().leftJustified(6, ' ');
            if (!event["peer"].toString().isEmpty())
            {
                prefix += event["peer"].toString() + "  ";
            }
            cursor.insertText(prefix, format);
            stream.block = cursor.block();
            const bool visible = matcher.matches(stream.block.text());
            stream.block.setVisible(visible);
            stream.block.setLineCount(visible ? 1 : 0);
            document()->markContentsDirty(stream.block.position(), stream.block.length());
        }
    };
    for (int at = 0; at < text.size(); ++at)
    {
        const QChar c = text[at];
        auto &state = stream.ansi;
        if (state.osc)
        {
            state.escape += c;
            if (c == '\a' || state.escape.endsWith("\x1b\\"))
            {
                state.escape.clear();
                state.osc = false;
            }
            if (state.escape.size() > 4096)
            {
                state.escape.clear();
                state.osc = false;
            }
            continue;
        }
        if (!state.escape.isEmpty())
        {
            state.escape += c;
            if (state.escape == "\x1b]")
            {
                state.osc = true;
            }
            else if (state.escape.startsWith("\x1b[") && c.unicode() >= 0x40 && c.unicode() <= 0x7e && state.escape.size() > 2)
            {
                if (c == 'm')
                {
                    QList<int> codes;
                    for (const auto &part : state.escape.mid(2, state.escape.size() - 3).split(';'))
                    {
                        codes.append(part.toInt());
                    }
                    state.sgr(codes, theme);
                }
                state.escape.clear();
            }
            else if (state.escape.size() > 128 || (state.escape.size() == 2 && c != '[' && c != ']'))
            {
                state.escape.clear();
            }
            continue;
        }
        if (c == QChar(0x1b))
        {
            state.escape = c;
            continue;
        }
        if (c == '\r' || c == '\n')
        {
            if (c == '\n' && stream.cr)
            {
                stream.cr = false;
                continue;
            }
            ensureLine();
            stream.block = {};
            stream.cr = c == '\r';
            continue;
        }
        stream.cr = false;
        if (c.unicode() < 32 && c != '\t')
        {
            continue;
        }
        ensureLine();
        QTextCursor cursor(stream.block);
        cursor.movePosition(QTextCursor::EndOfBlock);
        QTextCharFormat format;
        format.setForeground(state.foreground.isValid() ? state.foreground : direction == "tx" ? theme.accent : direction == "status" ? theme.amber : theme.text);
        if (state.background.isValid())
        {
            format.setBackground(state.background);
        }
        format.setFontWeight(state.bold ? QFont::Bold : QFont::Normal);
        QString run(c);
        while (at + 1 < text.size() && (text[at + 1].unicode() >= 32 || text[at + 1] == '\t'))
        {
            run += text[++at];
        }
        cursor.insertText(run, format);
        if (!filter.isEmpty())
        {
            const bool visible = matcher.matches(stream.block.text());
            stream.block.setVisible(visible);
            stream.block.setLineCount(visible ? 1 : 0);
            document()->markContentsDirty(stream.block.position(), stream.block.length());
        }
    }
    verticalScrollBar()->setValue(follow ? verticalScrollBar()->maximum() : previous);
}

void LogView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        emit zoomRequested(event->angleDelta().y() / 120);
        event->accept();
    }
    else
    {
        QTextEdit::wheelEvent(event);
    }
}

TerminalView::TerminalView(QWidget *parent) : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled);
    setMinimumHeight(90);
    scrollBar = new QScrollBar(Qt::Vertical, this);
    scrollBar->setObjectName("terminalHistoryScroll");
    scrollBar->setFocusPolicy(Qt::NoFocus);
    connect(scrollBar, &QScrollBar::valueChanged, this, [this] { update(); });
    setToolTip("滚轮查看历史 · Ctrl + 滚轮调整字号");
    blink.setInterval(500);
    connect(&blink, &QTimer::timeout, this, [this]
    {
        caret = !caret;
        if (isVisible() && hasFocus())
        {
            update();
        }
    });
    blink.start();
    reset();
}

void TerminalView::reset(const QString &encoding)
{
    auto codec = QTextCodec::codecForName(encoding.toLatin1());
    decoder.reset((codec ? codec : QTextCodec::codecForName("UTF-8"))->makeDecoder());
    ansi = {};
    history.clear();
    primaryGrid.clear();
    alternateScreen = false;
    savedX = savedY = 0;
    x = y = 0;
    wrapPending = false;
    autoWrap = true;
    cursorVisible = true;
    preeditText.clear();
    selecting = false;
    selectionActive = false;
    keyboardSelection = false;
    selectionCanEdit = false;
    grid = QVector<QVector<Cell>>(rows, QVector<Cell>(columns));
    updateScrollBar(true);
    update();
}

void TerminalView::newline()
{
    ++y;
    if (y >= rows)
    {
        if (!alternateScreen) { appendHistory(grid.first()); }
        grid.removeFirst();
        grid.append(QVector<Cell>(columns));
        y = rows - 1;
    }
}

void TerminalView::appendHistory(QVector<Cell> line)
{
    history.push_back(std::move(line));
    if (history.size() > historyLimit)
    {
        history.pop_front();
        scrollBar->setValue(qMax(0, scrollBar->value() - 1));
        if (hasSelection())
        {
            if (selectionAnchor.y() == 0 || selectionCursor.y() == 0) { clearSelection(); }
            else
            {
                selectionAnchor.ry() -= 1;
                selectionCursor.ry() -= 1;
            }
        }
    }
}

void TerminalView::updateScrollBar(bool followLatest)
{
    scrollBar->setGeometry(width() - 14, 0, 14, height());
    scrollBar->setRange(0, alternateScreen ? 0 : historyLineCount());
    scrollBar->setPageStep(rows);
    scrollBar->setSingleStep(1);
    scrollBar->setVisible(scrollBar->maximum() > 0);
    if (followLatest) { scrollBar->setValue(scrollBar->maximum()); }
}

const TerminalView::Cell &TerminalView::documentCell(int row, int column) const
{
    static const Cell blank;
    const int offset = alternateScreen ? 0 : historyLineCount();
    const auto &line = row < offset ? history[row] : grid[row - offset];
    return column < line.size() ? line[column] : blank;
}

void TerminalView::setAlternateScreen(bool enabled)
{
    if (alternateScreen == enabled) { return; }
    clearSelection();
    if (enabled)
    {
        primaryGrid = std::move(grid);
        primaryCursor = QPoint(x, y);
        primarySavedCursor = QPoint(savedX, savedY);
        primaryWrapPending = wrapPending;
        primaryScroll = scrollBar->value();
        grid = QVector<QVector<Cell>>(rows, QVector<Cell>(columns));
        x = y = 0;
        wrapPending = false;
        alternateScreen = true;
    }
    else
    {
        alternateScreen = false;
        grid = std::move(primaryGrid);
        x = primaryCursor.x();
        y = primaryCursor.y();
        savedX = primarySavedCursor.x();
        savedY = primarySavedCursor.y();
        wrapPending = primaryWrapPending;
        resizeGrid();
    }
    updateScrollBar(false);
    scrollBar->setValue(enabled ? 0 : primaryScroll);
}

void TerminalView::csi(QChar command, const QString &parameters)
{
    if ((command == 'h' || command == 'l') && parameters.startsWith('?'))
    {
        for (const auto &value : parameters.mid(1).split(';'))
        {
            if (value.toInt() == 1049 || value.toInt() == 1047 || value.toInt() == 47)
            {
                setAlternateScreen(command == 'h');
            }
            if (value.toInt() == 7) { autoWrap = command == 'h'; }
            if (value.toInt() == 25) { cursorVisible = command == 'h'; }
        }
        return;
    }
    if (command != 'm') { wrapPending = false; }
    QList<int> codes;
    for (const auto &value : parameters.split(';'))
    {
        codes.append(value.toInt());
    }
    const int n = codes.isEmpty() || codes[0] == 0 ? 1 : codes[0];
    if (command == 'm')
    {
        ansi.sgr(codes, theme);
    }
    else if (command == 'H' || command == 'f')
    {
        y = qBound(0, n - 1, rows - 1);
        x = qBound(0, codes.size() > 1 ? std::max(1, codes[1]) - 1 : 0, columns - 1);
    }
    else if (command == 'A')
    {
        y = std::max(0, y - n);
    }
    else if (command == 'B')
    {
        y = std::min(rows - 1, y + n);
    }
    else if (command == 'C')
    {
        x = std::min(columns - 1, x + n);
    }
    else if (command == 'D')
    {
        x = std::max(0, x - n);
    }
    else if (command == 'G')
    {
        x = qBound(0, n - 1, columns - 1);
    }
    else if (command == 'd')
    {
        y = qBound(0, n - 1, rows - 1);
    }
    else if (command == 'E' || command == 'F')
    {
        y = qBound(0, y + (command == 'E' ? n : -n), rows - 1);
        x = 0;
    }
    else if (command == 'X' || command == '@' || command == 'P')
    {
        const int count = std::min(n, columns - x);
        if (command == '@')
        {
            for (int col = columns - 1; col >= x + count; --col) { grid[y][col] = grid[y][col - count]; }
        }
        else if (command == 'P')
        {
            for (int col = x; col < columns - count; ++col) { grid[y][col] = grid[y][col + count]; }
        }
        const int begin = command == 'P' ? columns - count : x;
        for (int col = begin; col < begin + count; ++col) { grid[y][col] = {}; }
    }
    else if (command == 'S' || command == 'T' || command == 'L' || command == 'M')
    {
        const int begin = command == 'L' || command == 'M' ? y : 0;
        for (int i = 0; i < std::min(n, rows - begin); ++i)
        {
            if (command == 'T' || command == 'L')
            {
                grid.removeLast();
                grid.insert(begin, QVector<Cell>(columns));
            }
            else
            {
                if (command == 'S' && !alternateScreen) { appendHistory(grid.first()); }
                grid.removeAt(begin);
                grid.append(QVector<Cell>(columns));
            }
        }
    }
    else if (command == 'J')
    {
        const int mode = codes.value(0);
        if (mode == 3)
        {
            history.clear();
            clearSelection();
            updateScrollBar(true);
            return;
        }
        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < columns; ++col)
            {
                if (mode == 2 || (mode == 0 && (row > y || (row == y && col >= x))) || (mode == 1 && (row < y || (row == y && col <= x))))
                {
                    grid[row][col] = {};
                }
            }
        }
    }
    else if (command == 'K')
    {
        const int mode = codes.value(0);
        for (int col = 0; col < columns; ++col)
        {
            if (mode == 2 || (mode == 0 && col >= x) || (mode == 1 && col <= x))
            {
                grid[y][col] = {};
            }
        }
    }
    else if (command == 's')
    {
        savedX = x;
        savedY = y;
    }
    else if (command == 'u')
    {
        x = qBound(0, savedX, columns - 1);
        y = qBound(0, savedY, rows - 1);
    }
}

void TerminalView::feed(const QByteArray &data)
{
    const bool followLatest = scrollBar->value() == scrollBar->maximum();
    const bool wasAlternate = alternateScreen;
    // Output can redraw the command line without moving the cursor. Keep it copyable,
    // but do not apply an old selection to the new remote input.
    if (!data.isEmpty()) { selectionCanEdit = false; }
    for (const QChar c : decoder->toUnicode(data))
    {
        if (ansi.osc)
        {
            ansi.escape += c;
            if (c == '\a' || ansi.escape.endsWith("\x1b\\") || ansi.escape.size() > 4096)
            {
                ansi.osc = false;
                ansi.escape.clear();
            }
            continue;
        }
        if (!ansi.escape.isEmpty())
        {
            ansi.escape += c;
            if (ansi.escape == "\x1b]")
            {
                ansi.osc = true;
            }
            else if (ansi.escape.startsWith("\x1b[") && ansi.escape.size() > 2 && c.unicode() >= 0x40 && c.unicode() <= 0x7e)
            {
                csi(c, ansi.escape.mid(2, ansi.escape.size() - 3));
                ansi.escape.clear();
            }
            else if (ansi.escape.size() == 3 && QString("()*+").contains(ansi.escape[1]))
            {
                ansi.escape.clear();
            }
            else if (ansi.escape.size() == 2 && c != '[' && c != ']' && !QString("()*+").contains(c))
            {
                if (c == '7')
                {
                    savedX = x;
                    savedY = y;
                }
                else if (c == 'D' || c == 'E')
                {
                    wrapPending = false;
                    if (c == 'E') { x = 0; }
                    newline();
                }
                else if (c == 'M')
                {
                    wrapPending = false;
                    if (y > 0) { --y; }
                    else
                    {
                        grid.removeLast();
                        grid.prepend(QVector<Cell>(columns));
                    }
                }
                else if (c == '8')
                {
                    x = qBound(0, savedX, columns - 1);
                    y = qBound(0, savedY, rows - 1);
                }
                ansi.escape.clear();
            }
            else if (ansi.escape.size() > 128)
            {
                ansi.escape.clear();
            }
            continue;
        }
        if (c == QChar(0x1b))
        {
            ansi.escape = c;
        }
        else if (c == '\r')
        {
            wrapPending = false;
            x = 0;
        }
        else if (c == '\n')
        {
            wrapPending = false;
            newline();
        }
        else if (c == '\b')
        {
            wrapPending = false;
            x = std::max(0, x - 1);
        }
        else if (c == '\t')
        {
            wrapPending = false;
            x = std::min(columns - 1, (x / 8 + 1) * 8);
        }
        else if (c.unicode() >= 32)
        {
            const int cellWidth = c.unicode() >= 0x2e80 && c.unicode() <= 0xffef ? 2 : 1;
            if (autoWrap && (wrapPending || (cellWidth == 2 && x == columns - 1)))
            {
                x = 0;
                newline();
            }
            wrapPending = false;
            grid[y][x] = {c, ansi.foreground, ansi.background, ansi.bold};
            if (cellWidth == 2 && x + 1 < columns)
            {
                grid[y][x + 1] = {};
                grid[y][x + 1].continuation = true;
            }
            if (x + cellWidth >= columns)
            {
                x = columns - 1;
                wrapPending = autoWrap;
            }
            else { x += cellWidth; }
        }
    }
    updateScrollBar(alternateScreen == wasAlternate && followLatest);
    update();
    if (hasFocus())
    {
        QApplication::inputMethod()->update(Qt::ImCursorRectangle);
    }
}

QString TerminalView::text() const
{
    QStringList lines;
    for (const auto &row : grid)
    {
        QString line;
        for (const auto &cell : row)
        {
            line += cell.character;
        }
        lines.append(line.trimmed());
    }
    return lines.join('\n');
}

QString TerminalView::visibleText() const
{
    QStringList lines;
    const int top = scrollBar->value();
    for (int row = 0; row < rows; ++row)
    {
        QString line;
        for (int col = 0; col < columns; ++col)
        {
            const auto &cell = documentCell(top + row, col);
            if (!cell.continuation) { line += cell.character; }
        }
        while (line.endsWith(' ')) { line.chop(1); }
        lines.append(line);
    }
    return lines.join('\n');
}

void TerminalView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (!transparentBackground) { p.fillRect(rect(), theme.panel); }
    const QFontMetricsF metrics(font());
    const qreal width = metrics.horizontalAdvance('M');
    const qreal height = metrics.height() * 1.15;
    const int top = scrollBar->value();
    const bool atLiveScreen = alternateScreen || top == historyLineCount();
    const auto [selectionStart, selectionEnd] = selectionRange();
    const bool highlightSelection = hasSelection();
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < columns; ++col)
        {
            const auto &cell = documentCell(top + row, col);
            const QRectF box(16 + col * width, 12 + row * height, width, height);
            const int position = (top + row) * columns + col;
            const bool selected = highlightSelection && selectionStart <= position && position < selectionEnd;
            if (cell.background.isValid())
            {
                p.fillRect(box, cell.background);
            }
            if (selected)
            {
                p.fillRect(box, theme.accent);
            }
        }
        // Paint all cell backgrounds first so a wide glyph's second cell cannot erase it.
        for (int col = 0; col < columns; ++col)
        {
            const auto &cell = documentCell(top + row, col);
            if (cell.continuation) { continue; }
            const int position = (top + row) * columns + col;
            const bool selected = highlightSelection && selectionStart <= position && position < selectionEnd;
            const QRectF box(16 + col * width, 12 + row * height, width, height);
            p.setPen(selected ? theme.onAccent : cell.foreground.isValid() ? cell.foreground : theme.text);
            QFont current = font();
            current.setBold(cell.bold || font().bold());
            p.setFont(current);
            p.drawText(box.topLeft() + QPointF(0, metrics.ascent()), QString(cell.character));
        }
    }
    if (atLiveScreen && caret && cursorVisible && hasFocus() && enabledInput)
    {
        QColor color = theme.accent;
        color.setAlpha(150);
        p.fillRect(QRectF(16 + std::min(columns - 1, x) * width, 12 + y * height,
            blockCursor ? width : 2, height), color);
    }
    if (atLiveScreen && !preeditText.isEmpty())
    {
        const qreal left = 16 + std::min(columns - 1, x) * width;
        const qreal top = 12 + y * height;
        p.setPen(theme.accent);
        p.setFont(font());
        p.drawText(QPointF(left, top + metrics.ascent()), preeditText);
        p.fillRect(QRectF(left, top + metrics.height(), metrics.horizontalAdvance(preeditText), 1), theme.accent);
    }
}

void TerminalView::resizeEvent(QResizeEvent *event)
{
    resizeGrid();
    QWidget::resizeEvent(event);
}

void TerminalView::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange)
    {
        resizeGrid();
        update();
    }
}

void TerminalView::resizeGrid()
{
    const bool followLatest = scrollBar->value() == scrollBar->maximum();
    const QFontMetricsF metrics(font());
    // Reserve the scrollbar gutter even before history exists, avoiding PTY resize loops.
    const int newColumns = qBound(2, int((width() - 46) / metrics.horizontalAdvance('M')), 500);
    const int newRows = qBound(1, int((height() - 24) / (metrics.height() * 1.15)), 200);
    const bool changed = columns != newColumns || rows != newRows;
    const int remove = qMin(qMax(0, int(grid.size()) - newRows), qMax(0, y - newRows + 1));
    for (int line = 0; line < remove; ++line)
    {
        if (!alternateScreen) { appendHistory(grid.first()); }
        grid.removeFirst();
        --y;
        savedY = qMax(0, savedY - 1);
    }
    grid.resize(newRows);
    for (auto &row : grid)
    {
        row.resize(newColumns);
    }
    columns = newColumns;
    rows = newRows;
    if (changed)
    {
        wrapPending = false;
        selectionCanEdit = false;
    }
    const int selectionColumns = keyboardSelection ? columns : columns - 1;
    for (auto point : {&selectionAnchor, &selectionCursor})
    {
        point->setX(qBound(0, point->x(), selectionColumns));
        point->setY(qBound(0, point->y(), documentRows() - 1));
    }
    x = qBound(0, x, columns - 1);
    y = qBound(0, y, rows - 1);
    updateScrollBar(followLatest);
    if (hasFocus())
    {
        QApplication::inputMethod()->update(Qt::ImCursorRectangle);
    }
    if (changed) { emit gridSizeChanged(QSize(columns, rows)); }
}

bool TerminalView::event(QEvent *event)
{
    if (enabledInput && event->type() == QEvent::KeyPress)
    {
        auto key = static_cast<QKeyEvent *>(event);
        const bool tab = key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab;
        const bool shortcut = key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
        if (tab && !shortcut)
        {
            // QWidget otherwise consumes Tab for focus traversal before keyPressEvent.
            keyPressEvent(key);
            return true;
        }
    }
    return QWidget::event(event);
}

void TerminalView::keyPressEvent(QKeyEvent *event)
{
    const auto modifiers = event->modifiers();
    const bool control = modifiers & Qt::ControlModifier;
    const bool shift = modifiers & Qt::ShiftModifier;
    const bool localControl = control && !(modifiers & (Qt::AltModifier | Qt::MetaModifier));
    const int key = event->key();
    if (localControl && !shift && key == Qt::Key_C)
    {
        if (!event->isAutoRepeat()) { copyHeld = false; }
        else if (copyHeld)
        {
            event->accept();
            return;
        }
    }
    if (key == Qt::Key_Insert && modifiers == Qt::NoModifier)
    {
        blockCursor = !blockCursor;
        caret = true;
        update();
        emit cursorShapeChanged(blockCursor);
        event->accept();
        return;
    }
    if (copyShortcutEnabled && ((localControl && key == Qt::Key_C && (shift || hasSelection()))
        || (localControl && !shift && key == Qt::Key_Insert)))
    {
        copyHeld = localControl && !shift && key == Qt::Key_C && hasSelection();
        copySelection();
        event->accept();
        return;
    }
    if (pasteShortcutEnabled && ((localControl && shift && key == Qt::Key_V)
        || (shift && !control && !(modifiers & (Qt::AltModifier | Qt::MetaModifier)) && key == Qt::Key_Insert)))
    {
        pasteClipboard();
        event->accept();
        return;
    }
    if (keyboardSelectionEnabled && key == Qt::Key_Escape && hasSelection())
    {
        clearSelection();
        event->accept();
        return;
    }
    if (selectWithKeyboard(event))
    {
        event->accept();
        return;
    }
    if (!enabledInput)
    {
        event->ignore();
        return;
    }

    if (keyboardSelectionEnabled && hasSelection()
        && (key == Qt::Key_Backspace || key == Qt::Key_Delete)
        && !(modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)))
    {
        const QByteArray deletion = selectionDeletion();
        if (!deletion.isEmpty())
        {
            clearSelection();
            emit transmit(deletion);
        }
        else
        {
            QToolTip::showText(mapToGlobal(rect().center()),
                "只能删除从当前光标选中的同一行输入；历史输出选区可复制。", this);
        }
        event->accept();
        return;
    }

    QByteArray data;
    const auto modifiedKey = [modifiers](QChar final)
    {
        const auto active = modifiers & (Qt::ShiftModifier | Qt::AltModifier | Qt::ControlModifier | Qt::MetaModifier);
        if (active == Qt::NoModifier) { return QByteArray("\x1b[") + final.toLatin1(); }
        int modifier = 1;
        if (active & Qt::ShiftModifier) { modifier += 1; }
        if (active & Qt::AltModifier) { modifier += 2; }
        if (active & Qt::ControlModifier) { modifier += 4; }
        if (active & Qt::MetaModifier) { modifier += 8; }
        return QByteArray("\x1b[1;") + QByteArray::number(modifier) + final.toLatin1();
    };
    if (key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up || key == Qt::Key_Down)
    {
        const QMap<int, QChar> finals {{Qt::Key_Left, 'D'}, {Qt::Key_Right, 'C'}, {Qt::Key_Up, 'A'}, {Qt::Key_Down, 'B'}};
        data = modifiedKey(finals.value(key));
    }
    else if (key == Qt::Key_Home || key == Qt::Key_End)
    {
        const auto active = modifiers & (Qt::ShiftModifier | Qt::AltModifier | Qt::ControlModifier | Qt::MetaModifier);
        data = active == Qt::NoModifier ? (key == Qt::Key_Home ? QByteArray("\x1b[7~") : QByteArray("\x1b[4~"))
            : modifiedKey(key == Qt::Key_Home ? 'H' : 'F');
    }
    else if (key == Qt::Key_PageUp) { data = "\x1b[5~"; }
    else if (key == Qt::Key_PageDown) { data = "\x1b[6~"; }
    else if (key == Qt::Key_Insert) { data = "\x1b[2~"; }
    const QMap<int, QByteArray> keys {{Qt::Key_Return, "\r"}, {Qt::Key_Enter, "\r"}, {Qt::Key_Tab, "\t"},
        {Qt::Key_Delete, "\x1b[3~"}, {Qt::Key_Backspace, "\x7f"}, {Qt::Key_Escape, "\x1b"}};
    if (data.isEmpty())
    {
        if (key == Qt::Key_Backtab || (key == Qt::Key_Tab && shift)) { data = "\x1b[Z"; }
        else if (control && key >= Qt::Key_A && key <= Qt::Key_Z) { data.append(char(key - Qt::Key_A + 1)); }
        else { data = keys.value(key, event->text().toUtf8()); }
    }
    if (!data.isEmpty())
    {
        clearSelection();
        scrollBar->setValue(scrollBar->maximum());
        emit transmit(data);
    }
    event->accept();
}

void TerminalView::inputMethodEvent(QInputMethodEvent *event)
{
    if (!enabledInput)
    {
        event->ignore();
        return;
    }
    preeditText = event->preeditString();
    if (!event->commitString().isEmpty())
    {
        clearSelection();
        scrollBar->setValue(scrollBar->maximum());
        emit transmit(event->commitString().toUtf8());
    }
    update();
    QApplication::inputMethod()->update(Qt::ImCursorRectangle);
    event->accept();
}

QVariant TerminalView::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (query == Qt::ImEnabled) { return enabledInput; }
    if (query == Qt::ImCursorRectangle || query == Qt::ImAnchorRectangle)
    {
        const QFontMetricsF metrics(font());
        const qreal width = metrics.horizontalAdvance('M');
        const qreal height = metrics.height() * 1.15;
        return QRectF(16 + std::min(columns - 1, x) * width, 12 + y * height,
            std::max<qreal>(2, width), height);
    }
    if (query == Qt::ImCursorPosition || query == Qt::ImAnchorPosition)
    {
        return y * columns + x;
    }
    if (query == Qt::ImFont) { return font(); }
    return QWidget::inputMethodQuery(query);
}

void TerminalView::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    QApplication::inputMethod()->update(Qt::ImEnabled | Qt::ImCursorRectangle);
}

QPair<int, int> TerminalView::selectionRange() const
{
    int first = selectionAnchor.y() * columns + selectionAnchor.x();
    int last = selectionCursor.y() * columns + selectionCursor.x();
    if (first > last) { std::swap(first, last); }
    if (!keyboardSelection) { ++last; }
    return {qBound(0, first, documentRows() * columns), qBound(0, last, documentRows() * columns)};
}

void TerminalView::clearSelection()
{
    selecting = false;
    selectionActive = false;
    keyboardSelection = false;
    selectionCanEdit = false;
    update();
}

QByteArray TerminalView::selectionDeletion() const
{
    if (!keyboardSelection || !selectionCanEdit) { return {}; }

    const int row = (alternateScreen ? 0 : historyLineCount()) + y;
    const int caretPosition = row * columns + x + (wrapPending ? 1 : 0);
    const int anchor = selectionAnchor.y() * columns + selectionAnchor.x();
    const auto [start, end] = selectionRange();
    if (anchor != caretPosition || start < row * columns || end > (row + 1) * columns)
    {
        return {};
    }

    // A wide glyph takes multiple cells, and a surrogate pair takes two UTF-16
    // units. The remote line editor needs one erase key per Unicode character.
    const qsizetype characters = selectedText().toUcs4().size();
    const QByteArray erase = end == caretPosition ? QByteArray(1, char(0x7f)) : QByteArray("\x1b[3~");
    return erase.repeated(characters);
}

bool TerminalView::selectWithKeyboard(QKeyEvent *event)
{
    const auto modifiers = event->modifiers();
    if (!keyboardSelectionEnabled || !(modifiers & Qt::ShiftModifier)
        || (modifiers & (Qt::AltModifier | Qt::MetaModifier)))
    {
        return false;
    }
    const int key = event->key();
    if (key != Qt::Key_Left && key != Qt::Key_Right && key != Qt::Key_Up && key != Qt::Key_Down
        && key != Qt::Key_Home && key != Qt::Key_End && key != Qt::Key_PageUp && key != Qt::Key_PageDown)
    {
        return false;
    }

    const int totalRows = documentRows();
    const int limit = totalRows * columns;
    const auto offset = [this](const QPoint &point) { return point.y() * columns + point.x(); };
    const auto point = [this, limit, totalRows](int position)
    {
        position = qBound(0, position, limit);
        return position == limit ? QPoint(columns, totalRows - 1) : QPoint(position % columns, position / columns);
    };
    if (!keyboardSelection)
    {
        selectionCanEdit = !hasSelection() && enabledInput
            && scrollBar->value() == scrollBar->maximum();
        if (hasSelection())
        {
            // Mouse endpoints include the cell under the pointer; keyboard endpoints are gaps.
            if (offset(selectionAnchor) <= offset(selectionCursor))
            {
                selectionCursor = point(offset(selectionCursor) + 1);
            }
            else
            {
                selectionAnchor = point(offset(selectionAnchor) + 1);
            }
        }
        else
        {
            selectionAnchor = point(((alternateScreen ? 0 : historyLineCount()) + y) * columns + x + (wrapPending ? 1 : 0));
            selectionCursor = selectionAnchor;
        }
        keyboardSelection = true;
    }
    int position = offset(selectionCursor);
    const auto cell = [this](int index) -> const Cell & { return documentCell(index / columns, index % columns); };
    const auto previous = [&](int index)
    {
        if (index <= 0) { return 0; }
        --index;
        while (index > 0 && cell(index).continuation) { --index; }
        if (index > 0 && cell(index).character.isLowSurrogate())
        {
            int lead = index - 1;
            while (lead > 0 && cell(lead).continuation) { --lead; }
            if (cell(lead).character.isHighSurrogate()) { index = lead; }
        }
        return index;
    };
    const auto next = [&](int index)
    {
        if (index >= limit) { return limit; }
        const auto character = cell(index).character;
        ++index;
        while (index < limit && cell(index).continuation) { ++index; }
        if (character.isHighSurrogate() && index < limit && cell(index).character.isLowSurrogate())
        {
            ++index;
            while (index < limit && cell(index).continuation) { ++index; }
        }
        return index;
    };
    const auto category = [&](int index)
    {
        const auto character = cell(index).character;
        if (character.isSpace() || character.isNull() || cell(index).continuation) { return 0; }
        return character.isLetterOrNumber() || character == '_' || character.isSurrogate() ? 1 : 2;
    };
    const auto lineEnd = [this](int row)
    {
        int end = columns;
        while (end > 0 && documentCell(row, end - 1).character.isSpace() && !documentCell(row, end - 1).continuation) { --end; }
        return row * columns + end;
    };
    const auto documentEnd = [&]
    {
        int row = totalRows - 1;
        while (row > 0 && lineEnd(row) == row * columns) { --row; }
        return lineEnd(row);
    };
    const bool control = modifiers & Qt::ControlModifier;
    if (key == Qt::Key_Left)
    {
        if (!control) { position = previous(position); }
        else
        {
            while (position > 0 && category(previous(position)) == 0) { position = previous(position); }
            if (position > 0)
            {
                const int kind = category(previous(position));
                while (position > 0 && category(previous(position)) == kind) { position = previous(position); }
            }
        }
    }
    else if (key == Qt::Key_Right)
    {
        if (!control) { position = next(position); }
        else
        {
            const int end = qMax(position, documentEnd());
            const int kind = position < end ? category(position) : 0;
            while (position < end && category(position) == kind) { position = next(position); }
            while (position < end && category(position) == 0) { position = next(position); }
        }
    }
    else if (key == Qt::Key_Home)
    {
        position = control ? 0 : qMin(position / columns, totalRows - 1) * columns;
    }
    else if (key == Qt::Key_End)
    {
        position = control ? documentEnd() : lineEnd(qMin(position / columns, totalRows - 1));
    }
    else
    {
        const int direction = key == Qt::Key_Up || key == Qt::Key_PageUp ? -1 : 1;
        const int distance = key == Qt::Key_PageUp || key == Qt::Key_PageDown ? rows : 1;
        const int row = qBound(0, qMin(position / columns, totalRows - 1) + direction * distance, totalRows - 1);
        position = row * columns + qMin(position % columns, columns - 1);
        if (cell(position).continuation) { position = previous(position); }
    }
    selectionCursor = point(position);
    selecting = false;
    selectionActive = offset(selectionAnchor) != position;
    const int visibleRow = qMin(position / columns, totalRows - 1);
    if (visibleRow < scrollBar->value()) { scrollBar->setValue(visibleRow); }
    else if (visibleRow >= scrollBar->value() + rows) { scrollBar->setValue(visibleRow - rows + 1); }
    update();
    return true;
}

bool TerminalView::hasSelection() const
{
    return selecting || selectionActive;
}

QPoint TerminalView::cellAt(const QPointF &position) const
{
    const QFontMetricsF metrics(font());
    const qreal width = qMax<qreal>(1, metrics.horizontalAdvance('M'));
    const qreal height = qMax<qreal>(1, metrics.height() * 1.15);
    return QPoint(qBound(0, int((position.x() - 16) / width), columns - 1),
        scrollBar->value() + qBound(0, int((position.y() - 12) / height), rows - 1));
}

QString TerminalView::selectedText() const
{
    if (!hasSelection()) { return {}; }
    const auto [start, end] = selectionRange();
    if (start >= end) { return {}; }
    QStringList lines;
    const int firstRow = start / columns;
    const int lastRow = (end - 1) / columns;
    for (int row = firstRow; row <= lastRow; ++row)
    {
        const int firstColumn = row == firstRow ? start % columns : 0;
        const int lastColumn = row == lastRow ? (end - 1) % columns : columns - 1;
        QString line;
        for (int col = firstColumn; col <= lastColumn; ++col)
        {
            if (!documentCell(row, col).continuation) { line += documentCell(row, col).character; }
        }
        if (!keyboardSelection || row != lastRow)
        {
            while (line.endsWith(' ')) { line.chop(1); }
        }
        lines.append(line);
    }
    return lines.join('\n');
}

void TerminalView::copySelection()
{
    const QString value = selectedText();
    if (!value.isEmpty())
    {
        QApplication::clipboard()->setText(value);
    }
    clearSelection();
}

void TerminalView::pasteClipboard()
{
    if (enabledInput)
    {
        const QByteArray value = QApplication::clipboard()->text().toUtf8();
        if (!value.isEmpty())
        {
            clearSelection();
            scrollBar->setValue(scrollBar->maximum());
            emit transmit(value);
        }
    }
}

void TerminalView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
    {
        if (enabledInput && rightClickPasteEnabled)
        {
            setFocus(Qt::MouseFocusReason);
            pasteClipboard();
            event->accept();
            return;
        }
        event->ignore();
        return;
    }
    if (event->button() == Qt::LeftButton)
    {
        setFocus(Qt::MouseFocusReason);
        keyboardSelection = false;
        selectionAnchor = cellAt(event->position());
        selectionCursor = selectionAnchor;
        selecting = true;
        selectionActive = false;
        update();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TerminalView::mouseMoveEvent(QMouseEvent *event)
{
    if (selecting && (event->buttons() & Qt::LeftButton))
    {
        selectionCursor = cellAt(event->position());
        update();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void TerminalView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && selecting)
    {
        selectionCursor = cellAt(event->position());
        selecting = false;
        selectionActive = selectionAnchor != selectionCursor;
        update();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void TerminalView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        emit zoomRequested(event->angleDelta().y() / 120);
    }
    else if (!alternateScreen)
    {
        const int pixels = event->pixelDelta().y();
        const int steps = pixels ? (pixels > 0 ? 1 : -1) * qMax(1, qAbs(pixels) / qMax(1, fontMetrics().height()))
                                 : event->angleDelta().y() / 120 * 3;
        scrollBar->setValue(scrollBar->value() - steps);
    }
    event->accept();
}
}
