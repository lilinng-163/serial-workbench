// SPDX-License-Identifier: Apache-2.0
#include "button_motion.h"
#include "controls.h"

namespace sw
{
namespace
{
Theme motionTheme;

class ButtonHalo : public QWidget
{
public:
    ButtonHalo(QAbstractButton *source, ButtonMotion *motion)
        : QWidget(source->window()), source(source), motion(motion)
    {
        setObjectName("buttonHalo");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (!source || !source->isEnabled() || !source->isVisible()) { return; }
        const bool keyboard = source->hasFocus() && source->property("keyboardFocus").toBool();
        if (!keyboard) { return; }
        const QRectF outline = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
        auto color = motionTheme.text;
        if (source->parentWidget() && source->parentWidget()->objectName() == "rail")
        { color = QColor("#d7dfc4"); }
        color.setAlpha(200);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(color, 1.1));
        painter.drawRoundedRect(outline, 5, 5);
    }

private:
    QPointer<QAbstractButton> source;
    ButtonMotion *motion;
};

bool supported(QAbstractButton *button)
{
    return qobject_cast<QPushButton *>(button) || qobject_cast<QToolButton *>(button)
        || qobject_cast<IconButton *>(button);
}

void startPopupFade(QWidget *popup)
{
    auto animation = popup->findChild<QVariantAnimation *>("popupFade", Qt::FindDirectChildrenOnly);
    if (!animation)
    {
        animation = new QVariantAnimation(popup);
        animation->setObjectName("popupFade");
        animation->setDuration(120);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);
        QObject::connect(animation, &QVariantAnimation::valueChanged, popup, [popup](const QVariant &value)
        {
            popup->setWindowOpacity(value.toReal());
        });
        QObject::connect(animation, &QVariantAnimation::finished, popup, [popup]
        {
            popup->setWindowOpacity(1);
        });
    }
    animation->stop();
    popup->setWindowOpacity(0);
    animation->start();
}

void finishPopupFade(QWidget *popup)
{
    if (auto animation = popup->findChild<QVariantAnimation *>("popupFade", Qt::FindDirectChildrenOnly))
    {
        animation->stop();
        popup->setWindowOpacity(1);
    }
}

class MotionInstaller : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        const auto type = event->type();
        if (type != QEvent::Polish && type != QEvent::Show && type != QEvent::Hide
            && type != QEvent::Resize && type != QEvent::Paint) { return false; }

        auto widget = qobject_cast<QWidget *>(watched);
        const bool menu = qobject_cast<QMenu *>(widget);
        auto combo = widget ? qobject_cast<QComboBox *>(widget->parentWidget()) : nullptr;
        const bool list = combo && qobject_cast<QFrame *>(widget) && widget->windowType() == Qt::Popup;
        if (menu || list)
        {
            if (type == QEvent::Hide)
            {
                finishPopupFade(widget);
                return false;
            }
            roundPopupWindow(widget);
            if (type == QEvent::Show) { startPopupFade(widget); }
            if (list && type == QEvent::Paint)
            {
                // The private combo frame does not draw a stylesheet panel.
                // Paint the whole surface, including the inset around its view.
                QPainter painter(widget);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(QPen(motionTheme.line, 1));
                painter.setBrush(motionTheme.panel);
                painter.drawRoundedRect(QRectF(widget->rect()).adjusted(0.5, 0.5, -0.5, -0.5), 9.5, 9.5);
                return true;
            }
            if (type == QEvent::Show || type == QEvent::Resize)
            {
                QPointer<QWidget> guarded = widget;
                QTimer::singleShot(0, widget, [guarded]
                {
                    if (guarded && guarded->isVisible()) { roundPopupWindow(guarded); }
                });
            }
            return false;
        }
        if (type != QEvent::Polish && type != QEvent::Show) { return false; }
        if (auto check = qobject_cast<QCheckBox *>(widget))
        {
            if (!check->findChild<CheckBoxMotion *>(QString(), Qt::FindDirectChildrenOnly))
            {
                new CheckBoxMotion(check);
            }
            return false;
        }
        auto button = qobject_cast<QAbstractButton *>(watched);
        if (button && supported(button) && !button->findChild<ButtonMotion *>(QString(), Qt::FindDirectChildrenOnly))
        { new ButtonMotion(button); }
        return false;
    }
};
}

void roundPopupWindow(QWidget *popup)
{
    if (!popup->property("roundedPopup").toBool())
    {
        popup->setProperty("roundedPopup", true);
        popup->setWindowFlags(popup->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        popup->setAttribute(Qt::WA_TranslucentBackground);
        popup->setAttribute(Qt::WA_StyledBackground);
        popup->setAutoFillBackground(false);
        if (qobject_cast<QComboBox *>(popup->parentWidget()))
        {
            // Qt treats its private combo container as unstyleable until the
            // container has a stylesheet of its own. Colors and padding then
            // inherit normally from the application theme.
            popup->setStyleSheet("QFrame[roundedPopup=\"true\"] { border-radius: 10px; }");
        }
        popup->style()->unpolish(popup);
        popup->style()->polish(popup);
    }
    QPainterPath outline;
    outline.addRoundedRect(QRectF(popup->rect()), 10, 10);
    const QRegion mask(outline.toFillPolygon().toPolygon());
    if (popup->mask() != mask) { popup->setMask(mask); }
}

void installButtonMotion(const Theme &theme)
{
    motionTheme = theme;
    static auto installer = []
    {
        // Use the same short fade for both menus and combo lists. Disable
        // Qt's separate scroll/fade effects so they cannot stack or disagree.
        QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
        QApplication::setEffectEnabled(Qt::UI_AnimateMenu, false);
        QApplication::setEffectEnabled(Qt::UI_FadeMenu, false);
        auto result = new MotionInstaller(qApp);
        qApp->installEventFilter(result);
        return result;
    }();
    Q_UNUSED(installer);
    // Theme changes repaint visible feedback without restarting its animations.
    for (auto widget : QApplication::allWidgets())
    {
        if (auto motion = qobject_cast<ButtonMotion *>(widget)) { motion->update(); }
        if (auto check = qobject_cast<CheckBoxMotion *>(widget)) { check->update(); }
    }
}

ButtonMotion::ButtonMotion(QAbstractButton *parent) : QWidget(parent), button(parent)
{
    setObjectName("buttonMotion");
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    setGeometry(button->rect());
    button->installEventFilter(this);
    const auto bind = [this](QVariantAnimation &animation, qreal *progress)
    {
        animation.setEasingCurve(QEasingCurve::OutCubic);
        connect(&animation, &QVariantAnimation::valueChanged, this, [this, progress](const QVariant &value)
        {
            *progress = value.toReal();
            update();
            syncHalo();
        });
    };
    bind(hoverAnimation, &hoverProgress);
    bind(pressAnimation, &pressProgress);
    bind(checkedAnimation, &checkedProgress);
    checkedProgress = button->isChecked() ? 1 : 0;
    connect(button, &QAbstractButton::pressed, this, [this]
    { if (button->isEnabled()) { animate(pressAnimation, pressProgress, 1, 85); } });
    connect(button, &QAbstractButton::released, this, [this]
    {
        if (!button->isEnabled()) { reset(); return; }
        // Even a short mouse click or keyboard activation gets a visible release.
        pressProgress = qMax(pressProgress, 0.35);
        animate(pressAnimation, pressProgress, 0, 140);
    });
    connect(button, &QAbstractButton::toggled, this, [this](bool checked)
    { animate(checkedAnimation, checkedProgress, checked ? 1 : 0, 150); });
    trackParents();
    show();
    raise();
}

ButtonMotion::~ButtonMotion()
{
    delete halo.data();
}

void ButtonMotion::trackParents()
{
    for (const auto &parent : ancestors) { if (parent) { parent->removeEventFilter(this); } }
    ancestors.clear();
    for (auto parent = button->parentWidget(); parent; parent = parent->parentWidget())
    {
        ancestors.append(parent);
        parent->installEventFilter(this);
        if (parent->isWindow()) { break; }
    }
    if (!halo) { halo = new ButtonHalo(button, this); }
    if (halo->parentWidget() != button->window()) { halo->setParent(button->window()); }
    syncHalo();
}

void ButtonMotion::syncHalo()
{
    if (!halo || halo->parentWidget() != button->window()) { return; }
    const auto position = button->mapTo(halo->parentWidget(), QPoint());
    halo->setGeometry(QRect(position, button->size()));
    const bool keyboard = button->hasFocus() && button->property("keyboardFocus").toBool();
    const bool visible = button->isVisible() && button->isEnabled() && keyboard;
    halo->setVisible(visible);
    if (visible) { halo->raise(); halo->update(); }
}

bool ButtonMotion::running() const
{
    return hoverAnimation.state() == QAbstractAnimation::Running
        || pressAnimation.state() == QAbstractAnimation::Running
        || checkedAnimation.state() == QAbstractAnimation::Running;
}

void ButtonMotion::animate(QVariantAnimation &animation, qreal from, qreal to, int duration)
{
    animation.stop();
    animation.setDuration(duration);
    animation.setStartValue(from);
    animation.setEndValue(to);
    animation.start();
}

void ButtonMotion::reset()
{
    hoverAnimation.stop();
    pressAnimation.stop();
    checkedAnimation.stop();
    hoverProgress = 0;
    pressProgress = 0;
    checkedProgress = button->isChecked() ? 1 : 0;
    update();
    syncHalo();
}

bool ButtonMotion::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != button)
    {
        switch (event->type())
        {
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::LayoutRequest:
            syncHalo();
            break;
        default:
            break;
        }
        return false;
    }
    switch (event->type())
    {
    case QEvent::Enter:
        if (button->isEnabled()) { animate(hoverAnimation, hoverProgress, 1, 155); }
        break;
    case QEvent::Leave:
        animate(hoverAnimation, hoverProgress, 0, 155);
        // A dragged-out press must not leave a pressed highlight behind.
        animate(pressAnimation, pressProgress, 0, 140);
        break;
    case QEvent::Move:
        syncHalo();
        break;
    case QEvent::ParentChange:
        trackParents();
        break;
    case QEvent::Resize:
        setGeometry(button->rect());
        syncHalo();
        break;
    case QEvent::Show:
        setGeometry(button->rect());
        show();
        raise();
        break;
    case QEvent::Hide:
    case QEvent::EnabledChange:
        reset();
        break;
    case QEvent::FocusIn:
    case QEvent::FocusOut:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        update();
        syncHalo();
        break;
    default:
        break;
    }
    return false;
}

void ButtonMotion::paintEvent(QPaintEvent *)
{
    if (!button->isEnabled()) { return; }
    const bool keyboardFocus = button->hasFocus() && button->property("keyboardFocus").toBool();
    const qreal emphasis = qMax(hoverProgress, qMax(checkedProgress * 0.8, keyboardFocus ? 1.0 : 0.0));
    if (emphasis < 0.001 && pressProgress < 0.001) { return; }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    // Feedback stays inside the control and never changes its outline or size.
    const QRectF bounds = QRectF(rect()).adjusted(1, 1, -1, -1);
    const qreal radius = qMin(6.0, bounds.height() / 3.0);
    const bool filled = button->objectName() == "primary" || button->objectName() == "addSerialControl"
        || button->objectName() == "serialMonitorSend";
    auto fill = filled ? QColor(Qt::white) : motionTheme.accent;
    fill.setAlpha(qRound(emphasis * (filled ? 16 : 18)));
    auto edge = motionTheme.accent;
    edge.setAlpha(qRound(checkedProgress * 55));
    painter.setBrush(fill);
    painter.setPen(QPen(edge, keyboardFocus ? 1.5 : 1.0));
    painter.drawRoundedRect(bounds, radius, radius);
    if (pressProgress > 0.001)
    {
        auto pressure = filled ? QColor(Qt::black) : motionTheme.accent;
        pressure.setAlpha(qRound(pressProgress * (filled ? 20 : 24)));
        painter.setPen(Qt::NoPen);
        painter.setBrush(pressure);
        painter.drawRoundedRect(bounds, radius, radius);
    }

}

CheckBoxMotion::CheckBoxMotion(QCheckBox *source) : QWidget(source), source(source)
{
    setObjectName("checkBoxMotion");
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    source->setProperty("animatedCheck", true);
    setGeometry(source->rect());
    progress = source->checkState() == Qt::Unchecked ? 0 : 1;
    animation.setDuration(160);
    animation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value)
    {
        progress = value.toReal();
        update();
    });
    connect(source, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state)
    {
        animation.stop();
        animation.setStartValue(progress);
        animation.setEndValue(state == Qt::Unchecked ? 0.0 : 1.0);
        animation.start();
    });
    source->installEventFilter(this);
    show();
    raise();
}

void CheckBoxMotion::settle()
{
    animation.stop();
    progress = source->checkState() == Qt::Unchecked ? 0 : 1;
    update();
}

bool CheckBoxMotion::eventFilter(QObject *, QEvent *event)
{
    switch (event->type())
    {
    case QEvent::Resize:
        setGeometry(source->rect());
        break;
    case QEvent::Show:
        setGeometry(source->rect());
        settle();
        show();
        raise();
        break;
    case QEvent::Hide:
    case QEvent::EnabledChange:
        settle();
        break;
    case QEvent::Enter:
    case QEvent::Leave:
    case QEvent::FocusIn:
    case QEvent::FocusOut:
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
        update();
        break;
    default:
        break;
    }
    return false;
}

void CheckBoxMotion::paintEvent(QPaintEvent *)
{
    QStyleOptionButton option;
    option.initFrom(source);
    option.text = source->text();
    const QRectF indicator = source->style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, source);
    const QRectF bounds = indicator.adjusted(0.75, 0.75, -0.75, -0.75);
    const auto blend = [](const QColor &from, const QColor &to, qreal amount)
    {
        return QColor(qRound(from.red() + (to.red() - from.red()) * amount),
            qRound(from.green() + (to.green() - from.green()) * amount),
            qRound(from.blue() + (to.blue() - from.blue()) * amount));
    };
    const bool keyboard = source->hasFocus() && source->property("keyboardFocus").toBool();
    const auto edge = keyboard || source->underMouse() ? motionTheme.accent : motionTheme.edge;
    // Programmatic state changes can arrive with signals blocked (for example
    // when AI-synchronized scripts update the toggle), so fall back to the
    // actual check state whenever no animation is running.
    const qreal amount = animation.state() == QAbstractAnimation::Running ? progress
        : source->checkState() == Qt::Unchecked ? 0.0 : 1.0;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(source->isEnabled() ? 1 : 0.45);
    painter.setPen(QPen(edge, keyboard ? 1.4 : 1.0));
    painter.setBrush(blend(motionTheme.panel, motionTheme.accent, amount));
    painter.drawRoundedRect(bounds, 4.5, 4.5);
    if (amount < 0.001) { return; }

    painter.setPen(QPen(motionTheme.onAccent, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (source->checkState() == Qt::PartiallyChecked)
    {
        const qreal half = bounds.width() * 0.24 * amount;
        painter.drawLine(bounds.center() - QPointF(half, 0), bounds.center() + QPointF(half, 0));
        return;
    }
    const QPointF start(bounds.left() + bounds.width() * 0.23, bounds.top() + bounds.height() * 0.52);
    const QPointF middle(bounds.left() + bounds.width() * 0.43, bounds.top() + bounds.height() * 0.71);
    const QPointF end(bounds.left() + bounds.width() * 0.78, bounds.top() + bounds.height() * 0.31);
    const qreal first = QLineF(start, middle).length();
    const qreal second = QLineF(middle, end).length();
    const qreal distance = (first + second) * amount;
    QPainterPath tick(start);
    if (distance <= first)
    {
        tick.lineTo(start + (middle - start) * (distance / first));
    }
    else
    {
        tick.lineTo(middle);
        tick.lineTo(middle + (end - middle) * ((distance - first) / second));
    }
    painter.drawPath(tick);
}
}
