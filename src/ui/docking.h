// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <QtWidgets>

namespace sw
{
class TearTabBar : public QTabBar
{
    Q_OBJECT
  public:
    explicit TearTabBar(QWidget *parent = nullptr);
    QPoint pressPosition;
    QPointer<QObject> pressedPage;
    bool middleClickCloses = false;
    QPointer<QObject> middlePressedPage;
    QObject *identity(int index) const;
    void initStyleOption(QStyleOptionTab *option, int index) const override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
  signals:
    void tearRequested(int index, const QPoint &globalPosition);
};

class FloatingPage;

class DetachableTabs : public QTabWidget
{
    Q_OBJECT
  public:
    explicit DetachableTabs(QWidget *parent = nullptr);
    QMap<QWidget *, QPointer<FloatingPage>> floating;
    FloatingPage *detachPage(int index, const QPoint &position, bool systemMove = false);
    void restoreAll();
    void activatePage(QWidget *page);
    void paintEvent(QPaintEvent *event) override;
  signals:
    void pageAttached(QWidget *page);
};

class FloatingPage : public QMainWindow
{
    Q_OBJECT
  public:
    FloatingPage(DetachableTabs *origin, QWidget *page, const QString &title, const QIcon &icon, int index);
    QPointer<DetachableTabs> origin;
    QPointer<QWidget> page;
    QString title;
    QIcon tabIcon;
    int originalIndex;
    bool returned = false;
    void returnToTabs();
    void closeEvent(QCloseEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
};

void beginNativeMove(QWidget *window);
bool cursorOver(QWidget *widget, const QPoint &globalPosition);
} // namespace sw
