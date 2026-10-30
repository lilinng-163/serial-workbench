// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "controls.h"

namespace sw
{
class ModelTabBar : public QTabBar
{
    Q_OBJECT
public:
    explicit ModelTabBar(QWidget *parent = nullptr);
    void setTheme(const Theme &value);
    QRectF selectionRect() const { return selection; }
    bool animating() const { return movement.state() == QAbstractAnimation::Running; }

protected:
    QSize tabSizeHint(int index) const override;
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void tabLayoutChange() override;

private:
    void selectTab(bool animate);
    void hoverTab(int index);
    Theme theme;
    QRectF selection;
    QVariantAnimation movement;
    QVariantAnimation hover;
    QVector<qreal> hoverFrom;
    QVector<qreal> hoverValues;
    int hovered = -1;
};

class ModelTabs : public QTabWidget
{
public:
    explicit ModelTabs(QWidget *parent = nullptr);
    ~ModelTabs() override;
    void setTheme(const Theme &theme);

private:
    void finishFade();
    QVariantAnimation fade;
    QPointer<QWidget> fadingPage;
    QPointer<QGraphicsOpacityEffect> opacity;
};
}
