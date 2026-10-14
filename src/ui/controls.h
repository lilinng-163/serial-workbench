// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "core.h"
#include "docking.h"
#include <QtWidgets>

namespace sw
{
struct Theme
{
    bool dark = false;
    QColor background = "#dce2d3";
    QColor panel = "#f6f4e8";
    QColor sidebar = "#e9ecdd";
    QColor text = "#28392f";
    QColor muted = "#647360";
    QColor line = "#b7c2ac";
    QColor accent = "#dc8c38";
    QColor hover = "#dce4cd";
    QColor green = "#477255";
    QColor red = "#b85845";
    QColor amber = "#996022";
    QColor edge = "#7c8c74";
    QColor rail = "#293e35";
    QColor onAccent = "#28392f";
    static Theme make(bool dark);
    static QString fontFamily(int pixels = 8);
    static QFont interfaceFont();
    static QFont defaultDataFont();
    QString stylesheet() const;
};

QIcon icon(const QString &name, const QColor &color, int size = 20);
void drawIcon(QPainter &painter, const QString &name, QRectF rect, const QColor &color, qreal rotation = 0);

class StatusLed : public QWidget
{
  public:
    enum class State { Offline, Online, Pending, Error };

    explicit StatusLed(QWidget *parent = nullptr);
    Theme theme;
    State state() const { return currentState; }
    void setState(State state);
    void paintEvent(QPaintEvent *event) override;

  private:
    State currentState = State::Offline;
};

class IconButton : public QAbstractButton
{
    Q_OBJECT
  public:
    IconButton(const QString &name, const QString &title, QWidget *parent = nullptr);
    QString glyph;
    Theme theme;
    qreal progress = 0;
    QVariantAnimation animation;
    qreal rotation = 0;
    QVariantAnimation rotationAnimation;
    QSize sizeHint() const override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

  private:
    void turnGear();
    bool turnPending = false;
};

class Combo : public QComboBox
{
  public:
    explicit Combo(QWidget *parent = nullptr);
    bool menuPopup = false;
    void showPopup() override;
    void wheelEvent(QWheelEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
};

class ProtocolCombo : public Combo
{
  public:
    explicit ProtocolCombo(QWidget *parent = nullptr);
    void showPopup() override;
};

class SearchBox : public QLineEdit
{
    Q_OBJECT
  public:
    explicit SearchBox(QWidget *parent = nullptr);
    IconButton *clearButton;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
};

class ConnectionBar : public TearTabBar
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
    Q_OBJECT
  public:
    explicit WavePlot(QWidget *parent = nullptr);
    Theme theme;
    QMap<QString, QVector<QPointF>> series;
    void setSamples(const QJsonObject &data);
    bool updateChannel(Channel *channel, int limit);
    QRectF plotArea() const;
    QPair<double, double> horizontalRange() const;
    QPair<double, double> verticalRange() const;
    bool xAxisVisible() const { return showX; }
    bool yAxisVisible() const { return showY; }
    void setXAxisVisible(bool visible);
    void setYAxisVisible(bool visible);
    bool followsLatest() const { return following; }
    void setFollowLatest(bool enabled);
    void resetView();
    QJsonObject inspect(const QPointF &position) const;
    quint64 dataRevision() const { return revision; }
    int renderedPointCount() const { return renderedPoints; }
    bool isCurveVisible(const QString &name) const;
    void setCurveVisible(const QString &name, bool visible);
    void invalidatePlot();
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

  signals:
    void followLatestChanged(bool enabled);

  private:
    QPair<double, double> sampleBounds() const;
    QPair<double, double> valueRange() const;
    QColor curveColor(const QString &name) const;
    void dataChanged();
    void updateLegend();
    void rebuildImage();
    void drawInspection(QPainter &painter);
    QScrollArea *legendScroll;
    QWidget *legendBody;
    QHBoxLayout *legendLayout;
    QMap<QString, QToolButton *> legendButtons;
    QMap<QString, int> colorSlots;
    QSet<QString> hiddenCurves;
    QPointer<Channel> boundChannel;
    int boundLimit = 0;
    QMap<QString, qint64> sourceCursors;
    QMap<QString, QStringList> sourceCurves;
    quint64 revision = 0;
    mutable bool rangeDirty = true;
    mutable QPair<double, double> cachedRange;
    QPixmap image;
    bool imageDirty = true;
    QFont imageFont;
    QList<QColor> imageColors;
    bool showX = true;
    bool showY = true;
    bool following = true;
    double followSpan = 0;
    bool manualVertical = false;
    QPair<double, double> horizontalView;
    QPair<double, double> verticalView;
    QPointF dragStart;
    QRectF dragArea;
    QPair<double, double> dragHorizontal;
    QPair<double, double> dragVertical;
    int renderedPoints = 0;
    QPointF hoverPosition;
    bool hovering = false;
    bool dragging = false;
};
} // namespace sw
