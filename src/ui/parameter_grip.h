// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QWidget>

namespace sw
{
class Window;

class ParameterGrip : public QWidget
{
public:
    explicit ParameterGrip(Window *owner, QWidget *parent);
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    Window *window;
    bool dragging = false;
    qreal startX = 0;
    int startWidth = 246;
};
}
