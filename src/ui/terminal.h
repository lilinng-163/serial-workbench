// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "controls.h"
#include "search_filter.h"
#include "background_image.h"
#include <QTextCodec>
#include <deque>

namespace sw
{
struct AnsiState
{
    QColor foreground;
    QColor background;
    bool bold = false;
    QString escape;
    bool osc = false;
    void sgr(const QList<int> &codes, const Theme &theme);
};

class LogView : public QTextEdit
{
    Q_OBJECT
public:
    explicit LogView(QWidget *parent = nullptr);
    Theme theme;
    bool transparentBackground = false;
    bool timestamps = true;
    bool hex = false;
    bool follow = true;
    QString filter;
    void setFilter(const SearchFilter &criteria);
    QString encoding = "utf-8";
    struct Stream
    {
        std::unique_ptr<QTextDecoder> decoder;
        AnsiState ansi;
        QTextBlock block;
        bool cr = false;
    };
    QMap<QString, std::shared_ptr<Stream>> streams;
    void setWrapping(bool enabled);
    void feed(const QJsonObject &event);
    void reset();
    void paintEvent(QPaintEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void wheelEvent(QWheelEvent *event) override;
signals:
    void zoomRequested(int steps);
private:
    SearchFilter matcher;
};

class TerminalView : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalView(QWidget *parent = nullptr);
    struct Cell
    {
        QChar character = ' ';
        QColor foreground;
        QColor background;
        bool bold = false;
        bool continuation = false;
    };
    Theme theme;
    bool transparentBackground = false;
    QVector<QVector<Cell>> grid;
    AnsiState ansi;
    std::unique_ptr<QTextDecoder> decoder;
    int x = 0;
    int y = 0;
    int savedX = 0;
    int savedY = 0;
    int columns = 120;
    int rows = 32;
    bool enabledInput = false;
    bool caret = true;
    bool blockCursor = true;
    bool wrapPending = false;
    bool autoWrap = true;
    bool cursorVisible = true;
    bool copyShortcutEnabled = true;
    bool pasteShortcutEnabled = true;
    bool rightClickPasteEnabled = true;
    QPoint selectionAnchor;
    QPoint selectionCursor;
    bool selecting = false;
    bool selectionActive = false;
    bool keyboardSelectionEnabled = false;
    bool keyboardSelection = false;
    QString preeditText;
    QTimer blink;
    void feed(const QByteArray &data);
    void reset(const QString &encoding = "utf-8");
    QString text() const;
    QString visibleText() const;
    int historyLineCount() const { return int(history.size()); }
    QScrollBar *verticalScrollBar() const { return scrollBar; }
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeGrid();
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void focusInEvent(QFocusEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool hasSelection() const;
    QString selectedText() const;
    void copySelection();
    void pasteClipboard();
    void clearSelection();
signals:
    void transmit(const QByteArray &data);
    void zoomRequested(int steps);
    void cursorShapeChanged(bool block);
    void gridSizeChanged(const QSize &size);
private:
    static constexpr int historyLimit = 10000;
    std::deque<QVector<Cell>> history;
    QScrollBar *scrollBar;
    bool alternateScreen = false;
    QVector<QVector<Cell>> primaryGrid;
    QPoint primaryCursor;
    QPoint primarySavedCursor;
    bool primaryWrapPending = false;
    int primaryScroll = 0;
    void appendHistory(QVector<Cell> line);
    void updateScrollBar(bool followLatest);
    void setAlternateScreen(bool enabled);
    int documentRows() const { return (alternateScreen ? 0 : historyLineCount()) + rows; }
    const Cell &documentCell(int row, int column) const;
    bool copyHeld = false;
    bool selectionCanEdit = false;
    void csi(QChar command, const QString &parameters);
    void newline();
    QPoint cellAt(const QPointF &position) const;
    QPair<int, int> selectionRange() const;
    bool selectWithKeyboard(QKeyEvent *event);
    QByteArray selectionDeletion() const;
};
}
