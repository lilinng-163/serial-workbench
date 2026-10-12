#pragma once
#include "controls.h"
#include <QTextCodec>

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
    bool timestamps = true;
    bool hex = false;
    bool follow = true;
    QString filter;
    QString encoding = "utf-8";
    struct Stream
    {
        std::unique_ptr<QTextDecoder> decoder;
        AnsiState ansi;
        QTextBlock block;
        bool cr = false;
    };
    QMap<QString, std::shared_ptr<Stream>> streams;
    void feed(const QJsonObject &event);
    void reset();
    void wheelEvent(QWheelEvent *event) override;
signals:
    void zoomRequested(int steps);
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
    };
    Theme theme;
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
    QTimer blink;
    void feed(const QByteArray &data);
    void reset(const QString &encoding = "utf-8");
    QString text() const;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeGrid();
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
signals:
    void transmit(const QByteArray &data);
    void zoomRequested(int steps);
    void cursorShapeChanged(bool block);
private:
    void csi(QChar command, const QString &parameters);
    void newline();
};
}
