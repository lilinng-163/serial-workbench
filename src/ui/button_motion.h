// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QtWidgets>

namespace sw
{
struct Theme;

// Shared feedback for text, menu and icon buttons. Never handles user input.
class ButtonMotion : public QWidget
{
    Q_OBJECT
public:
    explicit ButtonMotion(QAbstractButton *button);
    ~ButtonMotion() override;
    QWidget *outerRing() const { return halo.data(); }
    qreal hoverAmount() const { return hoverProgress; }
    qreal pressAmount() const { return pressProgress; }
    qreal checkedAmount() const { return checkedProgress; }
    bool running() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QAbstractButton *button;
    QPointer<QWidget> halo;
    QList<QPointer<QWidget>> ancestors;
    void trackParents();
    void syncHalo();
    QVariantAnimation hoverAnimation;
    QVariantAnimation pressAnimation;
    QVariantAnimation checkedAnimation;
    qreal hoverProgress = 0;
    qreal pressProgress = 0;
    qreal checkedProgress = 0;
    void animate(QVariantAnimation &animation, qreal from, qreal to, int duration);
    void reset();
};

// Paint only the indicator; Qt retains the label, hit area and keyboard input.
class CheckBoxMotion : public QWidget
{
    Q_OBJECT
public:
    explicit CheckBoxMotion(QCheckBox *source);
    qreal checkedAmount() const { return progress; }
    bool running() const { return animation.state() == QAbstractAnimation::Running; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QCheckBox *source;
    QVariantAnimation animation;
    qreal progress = 0;
    void settle();
};

// Native combo layout can clear its mask after Show; finish it again afterward.
void roundPopupWindow(QWidget *popup);
void installButtonMotion(const Theme &theme);
}
