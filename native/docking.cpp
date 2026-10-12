#include "docking.h"
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace sw
{
bool cursorOver(QWidget *widget, const QPoint &globalPosition)
{
    return widget && widget->isVisible() &&
           QRect(widget->mapToGlobal(QPoint()), widget->size()).adjusted(-8, -10, 8, 10).contains(globalPosition);
}

void beginNativeMove(QWidget *window)
{
    QPointer<QWidget> guarded(window);
    QTimer::singleShot(0, window, [guarded] {
        if (guarded && QApplication::mouseButtons().testFlag(Qt::LeftButton) && guarded->windowHandle())
        {
            // The OS move loop provides Snap Bar, edge snapping and native animation.
            guarded->windowHandle()->startSystemMove();
        }
    });
}

TearTabBar::TearTabBar(QWidget *parent) : QTabBar(parent)
{
    setMovable(true);
    setDrawBase(false);
    setExpanding(false);
    setElideMode(Qt::ElideRight);
}

QObject *TearTabBar::identity(int index) const
{
    if (index < 0)
    {
        return nullptr;
    }
    auto stored = tabData(index).value<QObject *>();
    if (stored)
    {
        return stored;
    }
    auto tabs = qobject_cast<QTabWidget *>(parentWidget());
    return tabs ? tabs->widget(index) : nullptr;
}

void TearTabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        pressPosition = event->position().toPoint();
        pressedPage = identity(tabAt(pressPosition));
    }
    QTabBar::mousePressEvent(event);
}

void TearTabBar::mouseMoveEvent(QMouseEvent *event)
{
    if (pressedPage && event->buttons().testFlag(Qt::LeftButton) &&
        (event->position().toPoint() - pressPosition).manhattanLength() >= QApplication::startDragDistance() &&
        !rect().adjusted(-36, -26, 36, 26).contains(event->position().toPoint()))
    {
        int index = -1;
        for (int i = 0; i < count(); ++i)
        {
            if (identity(i) == pressedPage)
            {
                index = i;
                break;
            }
        }
        pressedPage.clear();
        QMouseEvent release(QEvent::MouseButtonRelease, event->position(), event->globalPosition(), Qt::LeftButton,
                            Qt::NoButton, event->modifiers());
        QTabBar::mouseReleaseEvent(&release);
        releaseMouse();
        if (index >= 0)
        {
            emit tearRequested(index, event->globalPosition().toPoint());
        }
        return;
    }
    QTabBar::mouseMoveEvent(event);
}

void TearTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    pressedPage.clear();
    QTabBar::mouseReleaseEvent(event);
}

DetachableTabs::DetachableTabs(QWidget *parent) : QTabWidget(parent)
{
    auto bar = new TearTabBar();
    setTabBar(bar);
    connect(bar, &TearTabBar::tearRequested, this,
            [this](int index, const QPoint &position) { detachPage(index, position, true); });
    bar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(bar, &QWidget::customContextMenuRequested, this, [this, bar](const QPoint &position) {
        const int index = bar->tabAt(position);
        if (index < 0)
        {
            return;
        }
        QMenu menu(this);
        menu.addAction("移至独立窗口", this, [this, index] { detachPage(index, QCursor::pos()); });
        menu.exec(bar->mapToGlobal(position));
    });
}

FloatingPage *DetachableTabs::detachPage(int index, const QPoint &position, bool systemMove)
{
    auto content = widget(index);
    if (!content)
    {
        return nullptr;
    }
    const auto title = tabText(index);
    const auto tabIcon = this->tabIcon(index);
    removeTab(index);
    auto detached = new FloatingPage(this, content, title, tabIcon, index);
    floating[content] = detached;
    detached->move(position - QPoint(100, 14));
    detached->show();
    if (systemMove)
    {
        beginNativeMove(detached);
    }
    return detached;
}

void DetachableTabs::restoreAll()
{
    for (auto detached : floating.values())
    {
        if (detached)
        {
            detached->returnToTabs();
        }
    }
    for (auto child : findChildren<DetachableTabs *>(QString(), Qt::FindDirectChildrenOnly))
    {
        child->restoreAll();
    }
}

void DetachableTabs::paintEvent(QPaintEvent *event)
{
    QTabWidget::paintEvent(event);
    if (count() == 0 && !floating.isEmpty())
    {
        QPainter painter(this);
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawText(rect().adjusted(24, 60, -24, -24), Qt::AlignCenter | Qt::TextWordWrap,
                         "页面已移至独立窗口\n关闭独立窗口，或点击“返回标签栏”重新停靠");
    }
}

void DetachableTabs::activatePage(QWidget *page)
{
    auto detached = floating.value(page);
    if (detached)
    {
        detached->showNormal();
        detached->raise();
        detached->activateWindow();
    }
    else
    {
        setCurrentWidget(page);
    }
}

FloatingPage::FloatingPage(DetachableTabs *tabs, QWidget *content, const QString &text, const QIcon &icon, int index)
    : QMainWindow(nullptr, Qt::Window), origin(tabs), page(content), title(text), tabIcon(icon), originalIndex(index)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(text + " · Serial Workbench");
    setWindowIcon(tabs->window()->windowIcon());
    setFont(tabs->window()->font());
    setStyleSheet(tabs->window()->styleSheet());
    setPalette(tabs->palette());
    connect(tabs, &QObject::destroyed, this, [this] {
        origin.clear();
        close();
    });
    setMinimumSize(320, 240);
    resize(900, 640);
    setCentralWidget(content);
    content->show();
    auto back = new QPushButton("返回标签栏");
    back->setToolTip("也可将窗口标题栏拖回原标签栏；关闭窗口会返回工作台");
    statusBar()->addPermanentWidget(back);
    statusBar()->showMessage("拖动系统标题栏分屏  ·  Win + Z 选择布局");
    connect(back, &QPushButton::clicked, this, &FloatingPage::returnToTabs);
}

void FloatingPage::returnToTabs()
{
    close();
}

void FloatingPage::closeEvent(QCloseEvent *event)
{
    if (!returned && origin && page)
    {
        returned = true;
        auto content = takeCentralWidget();
        origin->floating.remove(content);
        const int index = origin->insertTab(qMin(originalIndex, origin->count()), content, tabIcon, title);
        origin->setCurrentIndex(index);
        emit origin->pageAttached(content);
    }
    event->accept();
}

bool FloatingPage::nativeEvent(const QByteArray &type, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    const auto native = static_cast<MSG *>(message);
    if (native->message == WM_EXITSIZEMOVE && origin && cursorOver(origin->tabBar(), QCursor::pos()))
    {
        QTimer::singleShot(0, this, &FloatingPage::returnToTabs);
    }
#endif
    return QMainWindow::nativeEvent(type, message, result);
}
} // namespace sw
