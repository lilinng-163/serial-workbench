// SPDX-License-Identifier: Apache-2.0
#include "model3d_tabs.h"

namespace sw
{
ModelTabBar::ModelTabBar(QWidget *parent) : QTabBar(parent)
{
    setObjectName("model3dParameterTabBar");
    setDrawBase(false);
    setExpanding(false);
    setElideMode(Qt::ElideRight);
    setMouseTracking(true);
    movement.setDuration(180);
    movement.setEasingCurve(QEasingCurve::OutCubic);
    connect(&movement, &QVariantAnimation::valueChanged, this, [this](const QVariant &value)
    {
        selection = value.toRectF();
        update();
    });
    hover.setDuration(110);
    hover.setEasingCurve(QEasingCurve::OutCubic);
    connect(&hover, &QVariantAnimation::valueChanged, this, [this](const QVariant &value)
    {
        const qreal amount = value.toReal();
        hoverValues.resize(count());
        for (int i = 0; i < count(); ++i)
        {
            const qreal from = hoverFrom.value(i);
            hoverValues[i] = from + ((i == hovered ? 1.0 : 0.0) - from) * amount;
        }
        update();
    });
    connect(this, &QTabBar::currentChanged, this, [this] { selectTab(true); });
}

void ModelTabBar::setTheme(const Theme &value)
{
    theme = value;
    update();
}

QSize ModelTabBar::tabSizeHint(int index) const
{
    return {qMax(54, fontMetrics().horizontalAdvance(tabText(index)) + 28), 38};
}

void ModelTabBar::selectTab(bool animate)
{
    movement.stop();
    const QRectF destination = currentIndex() >= 0 ? tabRect(currentIndex()) : QRect();
    if (!animate || !isVisible() || selection.isEmpty()
        || !style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this))
    {
        selection = destination;
        update();
        return;
    }
    // Restart from the visible position, so rapid clicks never jump backwards.
    movement.setStartValue(selection);
    movement.setEndValue(destination);
    movement.start();
}

void ModelTabBar::hoverTab(int index)
{
    if (hovered == index) { return; }
    hover.stop();
    hovered = index;
    hoverFrom = hoverValues;
    hover.setStartValue(0.0);
    hover.setEndValue(1.0);
    hover.start();
}

void ModelTabBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < count(); ++i)
    {
        auto color = theme.hover;
        color.setAlphaF(color.alphaF() * hoverValues.value(i) * .55);
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(tabRect(i)).adjusted(2, 3, -2, -3), 7, 7);
    }
    if (!selection.isEmpty())
    {
        painter.setBrush(theme.hover);
        painter.drawRoundedRect(selection.adjusted(2, 3, -2, -3), 7, 7);
        painter.setBrush(theme.accent);
        painter.drawRoundedRect(QRectF(selection.left() + 12, height() - 4,
            qMax(1.0, selection.width() - 24), 2.5), 1.25, 1.25);
    }
    for (int i = 0; i < count(); ++i)
    {
        painter.setPen(isTabEnabled(i) ? (i == currentIndex() ? theme.text : theme.muted) : theme.line);
        const auto text = fontMetrics().elidedText(tabText(i), elideMode(), tabRect(i).width() - 16);
        painter.drawText(tabRect(i), Qt::AlignCenter | Qt::TextShowMnemonic, text);
    }
    if (hasFocus() && currentIndex() >= 0)
    {
        QStyleOptionFocusRect option;
        option.initFrom(this);
        option.rect = tabRect(currentIndex()).adjusted(5, 5, -5, -6);
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter, this);
    }
}

void ModelTabBar::mouseMoveEvent(QMouseEvent *event)
{
    const int index = tabAt(event->position().toPoint());
    hoverTab(index >= 0 && isTabEnabled(index) ? index : -1);
    QTabBar::mouseMoveEvent(event);
}

void ModelTabBar::leaveEvent(QEvent *event)
{
    hoverTab(-1);
    QTabBar::leaveEvent(event);
}

void ModelTabBar::resizeEvent(QResizeEvent *event)
{
    QTabBar::resizeEvent(event);
    selectTab(false);
}

void ModelTabBar::tabLayoutChange()
{
    QTabBar::tabLayoutChange();
    selectTab(false);
}

void ModelTabBar::showEvent(QShowEvent *event)
{
    QTabBar::showEvent(event);
    selectTab(false);
}

void ModelTabBar::hideEvent(QHideEvent *event)
{
    movement.stop();
    hover.stop();
    hovered = -1;
    hoverValues.fill(0);
    QTabBar::hideEvent(event);
}

ModelTabs::ModelTabs(QWidget *parent) : QTabWidget(parent)
{
    setTabBar(new ModelTabBar(this));
    fade.setDuration(130);
    fade.setEasingCurve(QEasingCurve::OutCubic);
    connect(&fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &value)
    {
        if (opacity) { opacity->setOpacity(value.toReal()); }
    });
    connect(&fade, &QVariantAnimation::finished, this, &ModelTabs::finishFade);
    connect(this, &QTabWidget::currentChanged, this, [this](int index)
    {
        finishFade();
        if (!isVisible() || index < 0 || !style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this)) { return; }
        fadingPage = widget(index);
        opacity = new QGraphicsOpacityEffect(fadingPage);
        opacity->setOpacity(.82);
        fadingPage->setGraphicsEffect(opacity);
        fade.setStartValue(.82);
        fade.setEndValue(1.0);
        fade.start();
    });
}

ModelTabs::~ModelTabs()
{
    finishFade();
}

void ModelTabs::finishFade()
{
    fade.stop();
    if (fadingPage && opacity) { fadingPage->setGraphicsEffect(nullptr); }
    opacity.clear();
    fadingPage.clear();
}

void ModelTabs::setTheme(const Theme &theme)
{
    static_cast<ModelTabBar *>(tabBar())->setTheme(theme);
}
}
