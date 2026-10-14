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

void LogView::reset()
{
    clear();
    streams.clear();
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
            stream.block.setVisible(stream.block.text().contains(filter, Qt::CaseInsensitive));
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
    x = y = 0;
    preeditText.clear();
    selecting = false;
    selectionActive = false;
    grid = QVector<QVector<Cell>>(rows, QVector<Cell>(columns));
    update();
}

void TerminalView::newline()
{
    ++y;
    if (y >= rows)
    {
        grid.removeFirst();
        grid.append(QVector<Cell>(columns));
        y = rows - 1;
    }
}

void TerminalView::csi(QChar command, const QString &parameters)
{
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
    else if (command == 'J')
    {
        const int mode = codes.value(0);
        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < columns; ++col)
            {
                if (mode == 2 || mode == 3 || (mode == 0 && (row > y || (row == y && col >= x))) || (mode == 1 && (row < y || (row == y && col <= x))))
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
            else if (ansi.escape.size() == 2 && c != '[' && c != ']')
            {
                if (c == '7')
                {
                    savedX = x;
                    savedY = y;
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
            x = 0;
        }
        else if (c == '\n')
        {
            newline();
        }
        else if (c == '\b')
        {
            x = std::max(0, x - 1);
        }
        else if (c == '\t')
        {
            x = std::min(columns - 1, (x / 8 + 1) * 8);
        }
        else if (c.unicode() >= 32)
        {
            if (x >= columns)
            {
                x = 0;
                newline();
            }
            grid[y][x] = {c, ansi.foreground, ansi.background, ansi.bold};
            ++x;
            if (c.unicode() >= 0x2e80 && c.unicode() <= 0xffef && x < columns)
            {
                grid[y][x++] = {};
            }
        }
    }
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

void TerminalView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), theme.panel);
    const QFontMetricsF metrics(font());
    const qreal width = metrics.horizontalAdvance('M');
    const qreal height = metrics.height() * 1.15;
    int selectionStart = selectionAnchor.y() * columns + selectionAnchor.x();
    int selectionEnd = selectionCursor.y() * columns + selectionCursor.x();
    if (selectionStart > selectionEnd)
    {
        std::swap(selectionStart, selectionEnd);
    }
    const bool highlightSelection = selecting || selectionActive;
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < columns; ++col)
        {
            const auto &cell = grid[row][col];
            const QRectF box(16 + col * width, 12 + row * height, width, height);
            const int position = row * columns + col;
            const bool selected = highlightSelection && selectionStart <= position && position <= selectionEnd;
            if (cell.background.isValid())
            {
                p.fillRect(box, cell.background);
            }
            if (selected)
            {
                QColor selection = theme.accent;
                selection.setAlpha(170);
                p.fillRect(box, selection);
            }
            p.setPen(selected ? theme.panel : cell.foreground.isValid() ? cell.foreground : theme.text);
            QFont current = font();
            current.setBold(cell.bold);
            p.setFont(current);
            p.drawText(box.topLeft() + QPointF(0, metrics.ascent()), QString(cell.character));
        }
    }
    if (caret && hasFocus() && enabledInput)
    {
        QColor color = theme.accent;
        color.setAlpha(150);
        p.fillRect(QRectF(16 + std::min(columns - 1, x) * width, 12 + y * height,
            blockCursor ? width : 2, height), color);
    }
    if (!preeditText.isEmpty())
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
    const QFontMetricsF metrics(font());
    const int newColumns = qBound(20, int((width() - 32) / metrics.horizontalAdvance('M')), 240);
    const int newRows = qBound(4, int((height() - 24) / (metrics.height() * 1.15)), 100);
    for (auto &row : grid)
    {
        row.resize(newColumns);
    }
    grid.resize(newRows);
    for (auto &row : grid)
    {
        row.resize(newColumns);
    }
    columns = newColumns;
    rows = newRows;
    x = qBound(0, x, columns - 1);
    y = qBound(0, y, rows - 1);
    selectionAnchor.setX(qBound(0, selectionAnchor.x(), columns - 1));
    selectionAnchor.setY(qBound(0, selectionAnchor.y(), rows - 1));
    selectionCursor.setX(qBound(0, selectionCursor.x(), columns - 1));
    selectionCursor.setY(qBound(0, selectionCursor.y(), rows - 1));
    if (hasFocus())
    {
        QApplication::inputMethod()->update(Qt::ImCursorRectangle);
    }
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
    if (event->key() == Qt::Key_Insert && event->modifiers() == Qt::NoModifier)
    {
        blockCursor = !blockCursor;
        caret = true;
        update();
        emit cursorShapeChanged(blockCursor);
        event->accept();
        return;
    }
    if (!enabledInput)
    {
        event->ignore();
        return;
    }
    QByteArray data;
    const auto modifiedKey = [event](QChar final)
    {
        const Qt::KeyboardModifiers modifiers = event->modifiers()
            & (Qt::ShiftModifier | Qt::AltModifier | Qt::ControlModifier | Qt::MetaModifier);
        if (modifiers == Qt::NoModifier)
        {
            return QByteArray("\x1b[") + final.toLatin1();
        }
        int modifier = 1;
        if (modifiers & Qt::ShiftModifier) { modifier += 1; }
        if (modifiers & Qt::AltModifier) { modifier += 2; }
        if (modifiers & Qt::ControlModifier) { modifier += 4; }
        if (modifiers & Qt::MetaModifier) { modifier += 8; }
        return QByteArray("\x1b[1;") + QByteArray::number(modifier) + final.toLatin1();
    };
    const bool control = event->modifiers() & Qt::ControlModifier;
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    if (control && shift && event->key() == Qt::Key_C && copyShortcutEnabled)
    {
        copySelection();
        event->accept();
        return;
    }
    if (control && shift && event->key() == Qt::Key_V && pasteShortcutEnabled)
    {
        pasteClipboard();
        event->accept();
        return;
    }
    if (control && event->key() == Qt::Key_C && copyShortcutEnabled && hasSelection())
    {
        copySelection();
        event->accept();
        return;
    }
    if ((control && event->key() == Qt::Key_Left) || (event->modifiers() & Qt::AltModifier && event->key() == Qt::Key_Left))
    {
        data = modifiedKey('D');
    }
    else if ((control && event->key() == Qt::Key_Right) || (event->modifiers() & Qt::AltModifier && event->key() == Qt::Key_Right))
    {
        data = modifiedKey('C');
    }
    else if (event->key() == Qt::Key_Home)
    {
        const auto modifiers = event->modifiers()
            & (Qt::ShiftModifier | Qt::AltModifier | Qt::ControlModifier | Qt::MetaModifier);
        data = modifiers == Qt::NoModifier ? QByteArray("\x1b[7~") : modifiedKey('H');
    }
    else if (event->key() == Qt::Key_End)
    {
        const auto modifiers = event->modifiers()
            & (Qt::ShiftModifier | Qt::AltModifier | Qt::ControlModifier | Qt::MetaModifier);
        data = modifiers == Qt::NoModifier ? QByteArray("\x1b[4~") : modifiedKey('F');
    }
    else if (event->key() == Qt::Key_PageUp)
    {
        data = "\x1b[5~";
    }
    else if (event->key() == Qt::Key_PageDown)
    {
        data = "\x1b[6~";
    }
    else if (event->key() == Qt::Key_Insert)
    {
        data = "\x1b[2~";
    }
    const QMap<int, QByteArray> keys {{Qt::Key_Return, "\r"}, {Qt::Key_Enter, "\r"}, {Qt::Key_Tab, "\t"},
        {Qt::Key_Up, "\x1b[A"}, {Qt::Key_Down, "\x1b[B"}, {Qt::Key_Right, "\x1b[C"}, {Qt::Key_Left, "\x1b[D"},
        {Qt::Key_Delete, "\x1b[3~"}, {Qt::Key_Backspace, "\x7f"}, {Qt::Key_Escape, "\x1b"}};
    if (data.isEmpty())
    {
        if (event->key() == Qt::Key_Backtab
            || (event->key() == Qt::Key_Tab && (event->modifiers() & Qt::ShiftModifier)))
        {
            data = "\x1b[Z";
        }
        else if ((event->modifiers() & Qt::ControlModifier) && event->key() >= Qt::Key_A && event->key() <= Qt::Key_Z)
        {
            data.append(char(event->key() - Qt::Key_A + 1));
        }
        else
        {
            data = keys.value(event->key(), event->text().toUtf8());
        }
    }
    if (!data.isEmpty())
    {
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
    if (!event->commitString().isEmpty()) { emit transmit(event->commitString().toUtf8()); }
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
        qBound(0, int((position.y() - 12) / height), rows - 1));
}

QString TerminalView::selectedText() const
{
    if (!hasSelection()) { return {}; }
    int start = selectionAnchor.y() * columns + selectionAnchor.x();
    int end = selectionCursor.y() * columns + selectionCursor.x();
    if (start > end) { std::swap(start, end); }
    QStringList lines;
    const int firstRow = start / columns;
    const int lastRow = end / columns;
    for (int row = firstRow; row <= lastRow; ++row)
    {
        const int firstColumn = row == firstRow ? start % columns : 0;
        const int lastColumn = row == lastRow ? end % columns : columns - 1;
        QString line;
        for (int col = firstColumn; col <= lastColumn; ++col)
        {
            line += grid[row][col].character;
        }
        while (line.endsWith(' ')) { line.chop(1); }
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
}

void TerminalView::pasteClipboard()
{
    if (enabledInput)
    {
        const QByteArray value = QApplication::clipboard()->text().toUtf8();
        if (!value.isEmpty()) { emit transmit(value); }
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
    event->accept();
}
}
