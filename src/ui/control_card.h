// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QtWidgets>

namespace sw
{
class ControlCard : public QFrame
{
    Q_OBJECT
public:
    explicit ControlCard(QWidget *parent = nullptr);
    void setFreeLayout(bool enabled);
    void setDragHandle(QWidget *handle, QWidget *title);
    void fitContents();

signals:
    void geometryEdited();
    void interactionStarted();

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QWidget *handle = nullptr;
    QWidget *grip = nullptr;
    bool freeLayout = false;
    bool moving = false;
    Qt::Edges resizing;
    QPoint pressPoint;
    QRect pressRect;
    Qt::Edges edges(const QPoint &point) const;
    void begin(const QPoint &global, Qt::Edges edges, bool move);
    void updateGeometryFromMouse(const QPoint &global);
    void finish();
    void updateCursor(Qt::Edges edges);
};
}
