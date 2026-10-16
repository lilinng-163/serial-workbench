// SPDX-License-Identifier: Apache-2.0
#include "control_card.h"

namespace sw
{
namespace
{
class CardGrip : public QWidget
{
public:
    explicit CardGrip(QWidget *parent) : QWidget(parent)
    {
        setObjectName("controlResizeGrip");
        setFixedSize(16, 16);
        setCursor(Qt::SizeFDiagCursor);
        setToolTip("拖动调整控件大小");
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setPen(QPen(palette().color(QPalette::WindowText), 1));
        for (int distance : {4, 8, 12})
        {
            painter.drawLine(width() - distance, height() - 3, width() - 3, height() - distance);
        }
    }
};
}

ControlCard::ControlCard(QWidget *parent) : QFrame(parent)
{
    setObjectName("serialControlCard");
    setAttribute(Qt::WA_StyledBackground);
    setMouseTracking(true);
    setMinimumSize(320, 132);
    setMaximumSize(4000, 4000);
    grip = new CardGrip(this);
    grip->installEventFilter(this);
    grip->hide();
}

void ControlCard::setFreeLayout(bool enabled)
{
    freeLayout = enabled;
    grip->setVisible(enabled);
    if (handle) { handle->setCursor(enabled ? Qt::SizeAllCursor : Qt::ArrowCursor); }
    if (!enabled) { moving = false; resizing = {}; unsetCursor(); }
}

void ControlCard::setDragHandle(QWidget *widget, QWidget *title)
{
    handle = widget;
    handle->setObjectName("controlDragHandle");
    handle->setAttribute(Qt::WA_StyledBackground);
    handle->setToolTip("拖动标题栏移动控件；拖动边缘或右下角调整大小");
    handle->installEventFilter(this);
    title->installEventFilter(this);
}

void ControlCard::fitContents()
{
    if (!layout()) { return; }
    layout()->activate();
    const QSize required = layout()->minimumSize().expandedTo(QSize(320, 132));
    setMinimumSize(required);
    if (freeLayout && (width() < required.width() || height() < required.height()))
    {
        resize(size().expandedTo(required));
    }
}

Qt::Edges ControlCard::edges(const QPoint &point) const
{
    Qt::Edges result;
    if (point.x() < 6) { result |= Qt::LeftEdge; }
    if (point.x() >= width() - 6) { result |= Qt::RightEdge; }
    if (point.y() < 6) { result |= Qt::TopEdge; }
    if (point.y() >= height() - 6) { result |= Qt::BottomEdge; }
    return result;
}

void ControlCard::updateCursor(Qt::Edges edge)
{
    if ((edge & Qt::LeftEdge && edge & Qt::TopEdge) || (edge & Qt::RightEdge && edge & Qt::BottomEdge))
    { setCursor(Qt::SizeFDiagCursor); }
    else if ((edge & Qt::RightEdge && edge & Qt::TopEdge) || (edge & Qt::LeftEdge && edge & Qt::BottomEdge))
    { setCursor(Qt::SizeBDiagCursor); }
    else if (edge & (Qt::LeftEdge | Qt::RightEdge)) { setCursor(Qt::SizeHorCursor); }
    else if (edge & (Qt::TopEdge | Qt::BottomEdge)) { setCursor(Qt::SizeVerCursor); }
    else { unsetCursor(); }
}

void ControlCard::begin(const QPoint &global, Qt::Edges edge, bool move)
{
    if (!freeLayout) { return; }
    emit interactionStarted();
    moving = move;
    resizing = edge;
    pressPoint = global;
    pressRect = geometry();
    raise();
    grabMouse();
}

void ControlCard::updateGeometryFromMouse(const QPoint &global)
{
    if (!freeLayout || (!moving && !resizing)) { return; }
    const QPoint delta = global - pressPoint;
    QRect next = pressRect;
    constexpr int maximumExtent = 16000;
    if (moving)
    {
        next.moveTopLeft(QPoint(qBound(0, pressRect.x() + delta.x(), maximumExtent - width()),
            qBound(0, pressRect.y() + delta.y(), maximumExtent - height())));
    }
    else
    {
        if (resizing & Qt::LeftEdge) { next.setLeft(qBound(qMax(0, pressRect.right() - maximumWidth() + 1), pressRect.left() + delta.x(), pressRect.right() - minimumWidth() + 1)); }
        if (resizing & Qt::TopEdge) { next.setTop(qBound(qMax(0, pressRect.bottom() - maximumHeight() + 1), pressRect.top() + delta.y(), pressRect.bottom() - minimumHeight() + 1)); }
        if (resizing & Qt::RightEdge) { next.setRight(qBound(next.left() + minimumWidth() - 1, pressRect.right() + delta.x(), qMin(maximumExtent - 1, next.left() + maximumWidth() - 1))); }
        if (resizing & Qt::BottomEdge) { next.setBottom(qBound(next.top() + minimumHeight() - 1, pressRect.bottom() + delta.y(), qMin(maximumExtent - 1, next.top() + maximumHeight() - 1))); }
    }
    if (next == geometry()) { return; }
    setGeometry(next);
    emit geometryEdited();
}

void ControlCard::finish()
{
    if (!moving && !resizing) { return; }
    moving = false;
    resizing = {};
    releaseMouse();
    emit geometryEdited();
}

bool ControlCard::eventFilter(QObject *object, QEvent *event)
{
    if (!freeLayout) { return QFrame::eventFilter(object, event); }
    if (event->type() == QEvent::MouseButtonPress)
    {
        auto mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton) { return false; }
        begin(mouse->globalPosition().toPoint(), object == grip ? Qt::RightEdge | Qt::BottomEdge : Qt::Edges(), object != grip);
        return true;
    }
    if (event->type() == QEvent::MouseMove && (moving || resizing))
    {
        updateGeometryFromMouse(static_cast<QMouseEvent *>(event)->globalPosition().toPoint());
        return true;
    }
    if (event->type() == QEvent::MouseButtonRelease && (moving || resizing))
    {
        updateGeometryFromMouse(static_cast<QMouseEvent *>(event)->globalPosition().toPoint());
        finish();
        return true;
    }
    return QFrame::eventFilter(object, event);
}

void ControlCard::mousePressEvent(QMouseEvent *event)
{
    const auto edge = edges(event->position().toPoint());
    if (freeLayout && event->button() == Qt::LeftButton && edge)
    {
        begin(event->globalPosition().toPoint(), edge, false);
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void ControlCard::mouseMoveEvent(QMouseEvent *event)
{
    if (moving || resizing) { updateGeometryFromMouse(event->globalPosition().toPoint()); }
    else if (freeLayout) { updateCursor(edges(event->position().toPoint())); }
    QFrame::mouseMoveEvent(event);
}

void ControlCard::mouseReleaseEvent(QMouseEvent *event)
{
    if (moving || resizing)
    {
        updateGeometryFromMouse(event->globalPosition().toPoint());
        finish();
    }
    QFrame::mouseReleaseEvent(event);
}

void ControlCard::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    grip->move(width() - grip->width() - 2, height() - grip->height() - 2);
    grip->raise();
}
}
