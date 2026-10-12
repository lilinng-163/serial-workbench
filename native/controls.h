#pragma once
#include <QtWidgets>
#include "core.h"

namespace sw
{
struct Theme
{
    bool dark = false;
    QColor background = "#f2f5f9";
    QColor panel = "#ffffff";
    QColor sidebar = "#f8fafc";
    QColor text = "#27374d";
    QColor muted = "#7d8ba0";
    QColor line = "#e3e9f0";
    QColor accent = "#337de7";
    QColor hover = "#e9f0fa";
    QColor green = "#239b83";
    QColor red = "#d65666";
    QColor amber = "#b87a24";
    static Theme make(bool dark);
    QString stylesheet() const;
};

QIcon icon(const QString &name, const QColor &color, int size = 20);
void drawIcon(QPainter &painter, const QString &name, QRectF rect, const QColor &color, qreal rotation = 0);

class IconButton : public QAbstractButton
{
    Q_OBJECT
public:
    IconButton(const QString &name, const QString &title, QWidget *parent = nullptr);
    QString glyph;
    Theme theme;
    qreal progress = 0;
    QVariantAnimation animation;
    QSize sizeHint() const override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
};

class Combo : public QComboBox
{
public:
    explicit Combo(QWidget *parent = nullptr);
    void wheelEvent(QWheelEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
};

class ConnectionBar : public QTabBar
{
public:
    explicit ConnectionBar(QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    QSize tabSizeHint(int index) const override;
};

class Grip : public QSplitterHandle
{
public:
    Grip(Qt::Orientation orientation, QSplitter *parent);
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
};

class Splitter : public QSplitter
{
public:
    explicit Splitter(Qt::Orientation orientation = Qt::Horizontal, QWidget *parent = nullptr);
    QSplitterHandle *createHandle() override;
};

class WavePlot : public QWidget
{
public:
    explicit WavePlot(QWidget *parent = nullptr);
    Theme theme;
    QMap<QString, QVector<QPointF>> series;
    qreal zoom = 1;
    qreal offset = 0;
    QPoint lastMouse;
    void setSamples(const QJsonObject &data);
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
};
}
