// SPDX-License-Identifier: Apache-2.0

#include "parameter_grip.h"
#include "window.h"

namespace sw
{
ParameterGrip::ParameterGrip(Window *owner, QWidget *parent)
    : QWidget(parent), window(owner)
{
    setObjectName("parameterGrip");
    setFixedWidth(8);
    setCursor(Qt::SplitHCursor);
    setFocusPolicy(Qt::TabFocus);
    setAccessibleName("连接参数栏宽度");
    setToolTip("拖动调整连接参数栏宽度，双击恢复默认\n方向键微调，Shift 加速，Home 恢复默认");
}

void ParameterGrip::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    auto color = window->theme.muted;
    const bool active = dragging || underMouse() || hasFocus();
    color.setAlpha(active ? 170 : 65);
    if (active) { color = window->theme.accent; }
    painter.setPen(QPen(color, 1));
    const int center = width() / 2;
    painter.drawLine(center, 0, center, height());
    for (int offset : {-6, 0, 6})
    {
        painter.fillRect(center - 1, height() / 2 + offset, 2, 2, color);
    }
}

void ParameterGrip::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    update();
}

void ParameterGrip::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    update();
}

void ParameterGrip::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) { QWidget::mousePressEvent(event); return; }
    dragging = true;
    startX = event->globalPosition().x();
    startWidth = window->findChild<QScrollArea *>("parameterScroll")->width();
    update();
    event->accept();
}

void ParameterGrip::mouseMoveEvent(QMouseEvent *event)
{
    if (!dragging) { QWidget::mouseMoveEvent(event); return; }
    window->resizeParameters(startWidth + qRound(event->globalPosition().x() - startX));
    event->accept();
}

void ParameterGrip::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !dragging) { QWidget::mouseReleaseEvent(event); return; }
    dragging = false;
    window->saveSettings();
    update();
    event->accept();
}

void ParameterGrip::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) { QWidget::mouseDoubleClickEvent(event); return; }
    dragging = false;
    window->resizeParameters(246);
    window->saveSettings();
    update();
    event->accept();
}

void ParameterGrip::keyPressEvent(QKeyEvent *event)
{
    const int step = event->modifiers().testFlag(Qt::ShiftModifier) ? 40 : 10;
    if (event->key() == Qt::Key_Home) { window->resizeParameters(246); }
    else if (event->key() == Qt::Key_Left) { window->resizeParameters(window->parameterWidth - step); }
    else if (event->key() == Qt::Key_Right) { window->resizeParameters(window->parameterWidth + step); }
    else { QWidget::keyPressEvent(event); return; }
    window->saveSettings();
    event->accept();
}
}
