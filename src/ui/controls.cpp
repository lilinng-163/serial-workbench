#include "controls.h"
#include <cmath>
#include <numeric>

namespace sw
{
QString Theme::fontFamily(int pixels)
{
    static const QMap<int, QString> families = []
    {
        QMap<int, QString> loaded;
        for (int size : {8, 12})
        {
            const auto path = QString(":/fonts/fusion-pixel-%1px-monospaced-zh_hans.otf").arg(size);
            const int id = QFontDatabase::addApplicationFont(path);
            const auto names = QFontDatabase::applicationFontFamilies(id);
            loaded[size] = names.isEmpty() ? QString("Consolas") : names.first();
        }
        return loaded;
    }();
    return families.value(pixels);
}

QFont Theme::interfaceFont()
{
    QFont font("Microsoft YaHei UI");
    font.setPixelSize(14);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

QFont Theme::defaultDataFont()
{
    const auto families = QFontDatabase::families();
    QFont font(families.contains("Cascadia Mono") ? "Cascadia Mono" : "Consolas", 12);
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

Theme Theme::make(bool dark)
{
    Theme t;
    t.dark = dark;
    if (dark)
    {
        t.background = "#1e2b25";
        t.panel = "#24332b";
        t.sidebar = "#2c3c32";
        t.text = "#e1e5ce";
        t.muted = "#a1b29a";
        t.line = "#465b48";
        t.accent = "#edaa50";
        t.hover = "#3b5140";
        t.green = "#9abf80";
        t.red = "#e78b6d";
        t.amber = "#edaa50";
        t.edge = "#71826a";
        t.rail = "#17231d";
    }
    return t;
}

QString Theme::stylesheet() const
{
    QString s = R"(
        QWidget { color: $text; font-family: "$font"; font-size: 14px; font-weight: normal; }
        QMainWindow, QDialog, QWidget#root { background: $bg; }
        QFrame#sidebar { background: $side; border-right: 2px solid $edge; }
        QFrame#rail { background: $rail; border-right: 2px solid $rail; }
        QWidget#workspace, QWidget#page { background: $panel; }
        QWidget#tabHeader { background: $bg; border-bottom: 2px solid $edge; }
        QWidget#monitorToolbar { background: $side; border-bottom: 1px solid $line; }
        QWidget#sender { background: $side; border-top: 1px solid $line; }
        QLabel { color: $text; background: transparent; border: none; }
        QGroupBox { color: $text; }
        QLabel#muted { color: $muted; font-size: 13px; }
        QLabel#section { font-size: 14px; font-weight: 600; color: $text; padding: 0 0 8px 0; border-bottom: 2px solid $edge; }
        QLabel#brand { font-size: 14px; }
        QLabel#themeTitle { font-size: 24px; }
        QWidget#pixelBrand { background: $rail; border: 2px solid $rail; border-bottom: 4px solid $edge; }
        QLabel#brandTitle { color: #f6f4e8; font-family: "$pixelFont"; font-size: 24px; }
        QLabel#brandSubtitle { color: #d7dfc4; font-size: 13px; }
        QLabel#brandMark { color: #edaa50; font-family: "$pixelFont"; font-size: 24px; }
        QLabel#senderLabel { color: $muted; font-size: 13px; }
        QLabel#connectionHint { color: $muted; padding: 10px 0 0 0; border-top: 1px solid $line; }
        QLineEdit, QComboBox, QSpinBox { background: $panel; border: 1px solid $edge; border-bottom-width: 2px; border-radius: 0; padding: 6px 10px; min-height: 22px; selection-background-color: $accent; selection-color: $onaccent; }
        QLineEdit:hover, QComboBox:hover, QSpinBox:hover { border-color: $text; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 2px solid $accent; padding: 5px 9px 6px 9px; }
        QComboBox#protocolSelector { background: $hover; border: 2px solid $edge; padding: 8px 28px 8px 10px; }
        QLineEdit#searchBox { padding: 5px 8px; background: $panel; }
        QLineEdit#searchBox:focus { padding: 4px 7px 5px 7px; }
        QComboBox { padding-right: 28px; }
        QComboBox:focus { padding-right: 27px; }
        QComboBox::drop-down { border: none; width: 28px; }
        QComboBox::down-arrow { image: none; }
        QComboBox QLineEdit, QComboBox QLineEdit:focus, QComboBox QLineEdit:hover { border: none; padding: 0; min-height: 0; background: transparent; }
        QComboBox QAbstractItemView { background: $panel; color: $text; border: 2px solid $edge; selection-background-color: $hover; selection-color: $text; outline: none; padding: 4px; }
        QComboBox QAbstractItemView::item { min-height: 28px; padding: 4px 8px; }
        QSpinBox { padding-right: 22px; }
        QSpinBox::up-button, QSpinBox::down-button { width: 20px; border: none; }
        QSpinBox::up-arrow { image: url(:/icons/chevron-up.svg); width: 8px; height: 5px; }
        QSpinBox::down-arrow { image: url(:/icons/chevron-down.svg); width: 8px; height: 5px; }
        QPushButton, QToolButton { background: transparent; border: 1px solid transparent; border-radius: 0; padding: 6px 12px; min-height: 22px; }
        QPushButton:hover, QToolButton:hover { color: $accent; }
        QPushButton:checked { color: $accent; font-weight: 600; }
        QPushButton:focus, QToolButton:focus { color: $accent; }
        QPushButton:pressed, QToolButton:pressed { color: $text; padding-top: 7px; padding-bottom: 5px; }
        QPushButton#primary { background: $accent; color: $onaccent; border: 2px solid $text; border-bottom-width: 4px; padding: 6px 16px; }
        QPushButton#primary:hover { background: $primaryhover; color: $onaccent; }
        QPushButton#primary:pressed { color: $onaccent; border-bottom-width: 2px; padding-top: 8px; }
        QPushButton#primary:disabled { background: $line; color: $muted; border-color: $edge; }
        QPushButton#outline { color: $text; border: 1px solid $edge; border-bottom-width: 2px; background: $panel; }
        QPushButton#outline:focus { border-color: $text; }
        QPushButton#outline:hover { color: $accent; }
        QPushButton:disabled, QToolButton:disabled { color: $muted; }
        QPlainTextEdit, QTextEdit { border: 1px solid $edge; border-radius: 0; background: $panel; padding: 8px; selection-background-color: $accent; selection-color: $onaccent; }
        QPlainTextEdit:focus { border-color: $accent; }
        QTextEdit#monitor { border: none; padding: 16px 18px; }
        QFontDialog QListView { color: $text; background: $panel; border: 1px solid $edge; selection-background-color: $hover; selection-color: $text; outline: none; }
        QFontDialog QListView::item { padding: 3px 6px; }
        QFontDialog QGroupBox { color: $text; border: 1px solid $edge; margin-top: 12px; padding: 10px; }
        QFontDialog QGroupBox::title { color: $text; subcontrol-origin: margin; left: 8px; padding: 0 4px; }
        QTreeWidget { border: 1px solid $line; background: $panel; alternate-background-color: $side; outline: none; }
        QTreeWidget::item { padding: 6px; min-height: 22px; }
        QTreeWidget::item:selected { background: $hover; color: $text; }
        QHeaderView::section { background: $side; border: none; border-bottom: 2px solid $edge; padding: 10px 8px; color: $muted; }
        QTabWidget { background: $side; }
        QTabWidget::pane { border: none; border-top: 1px solid $edge; background: $panel; }
        QTabBar { background: transparent; }
        QTabBar QToolButton { background: $side; border: 1px solid $edge; padding: 0; min-height: 0; }
        QTabBar::scroller { width: 48px; }
        QTabBar::tab { background: $side; padding: 11px 12px; border-right: 1px solid $line; border-bottom: 3px solid transparent; color: $muted; }
        QTabBar::tab:selected { color: $text; border-bottom-color: $accent; background: $panel; }
        QTabBar::tab:hover { background: $hover; }
        QTabBar#connections::tab { padding: 9px 10px; margin: 4px 0 0 0; border: 1px solid $edge; border-bottom: 3px solid $edge; background: $side; }
        QTabBar#connections::tab:selected { background: $panel; border-bottom-color: $accent; color: $text; }
        QMenu { background: $panel; border: 2px solid $edge; padding: 4px; }
        QMenu::item { padding: 10px 28px 10px 16px; border: 1px solid transparent; }
        QMenu::item:selected { background: $hover; border-color: $edge; }
        QMenu::separator { height: 1px; background: $line; margin: 5px 8px; }
        QMenu::indicator:checked { image: url(:/icons/check.svg); background: $accent; width: 12px; height: 12px; }
        QCheckBox { spacing: 7px; background: transparent; }
        QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid $edge; border-bottom-width: 2px; background: $panel; }
        QCheckBox::indicator:hover { border-color: $accent; }
        QCheckBox::indicator:checked { border-color: $text; background: $accent; image: url(:/icons/check.svg); }
        QScrollArea { border: none; background: transparent; }
        QScrollBar:vertical { background: $side; width: 10px; margin: 0; }
        QScrollBar:horizontal { background: $side; height: 10px; margin: 0; }
        QScrollBar::handle { background: $line; border: 1px solid $edge; min-height: 28px; min-width: 28px; }
        QScrollBar::handle:hover { background: $muted; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        QStatusBar { background: $side; color: $muted; border-top: 2px solid $edge; }
        QStatusBar::item { border: none; }
        QToolTip { background: $panel; color: $text; border: 2px solid $edge; padding: 6px; }
    )";
    const QMap<QString, QString> colors{{"$font", interfaceFont().family()},
                                      {"$pixelFont", fontFamily()},
                                        {"$text", text.name()},
                                        {"$muted", muted.name()},
                                        {"$side", sidebar.name()},
                                        {"$bg", background.name()},
                                        {"$rail", rail.name()},
                                        {"$panel", panel.name()},
                                        {"$line", line.name()},
                                        {"$edge", edge.name()},
                                        {"$accent", accent.name()},
                                        {"$hover", hover.name()},
                                        {"$onaccent", onAccent.name()},
                                        {"$primaryhover", dark ? "#f4be71" : "#eba04b"}};
    for (auto it = colors.begin(); it != colors.end(); ++it)
    {
        s.replace(it.key(), it.value());
    }
    return s;
}

// Every icon uses the same 16 x 16 grid. Integer cells stay sharp at any DPI.
void drawIcon(QPainter &p, const QString &name, QRectF rect, const QColor &color, qreal)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    const qreal unit = qMax(1.0, std::floor(rect.width() / 16));
    p.translate(std::floor(rect.center().x() - 8 * unit), std::floor(rect.center().y() - 8 * unit));
    p.scale(unit, unit);
    p.setPen(QPen(color, 2, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    p.setBrush(Qt::NoBrush);
    const auto line = [&](int x1, int y1, int x2, int y2)
    {
        // Bresenham rasterization avoids smoothed diagonals and rounded joins.
        const int dx = std::abs(x2 - x1);
        const int dy = -std::abs(y2 - y1);
        const int sx = x1 < x2 ? 1 : -1;
        const int sy = y1 < y2 ? 1 : -1;
        int error = dx + dy;
        while (true)
        {
            p.fillRect(QRect(x1, y1, 2, 2), color);
            if (x1 == x2 && y1 == y2)
            {
                break;
            }
            const int twice = 2 * error;
            if (twice >= dy)
            {
                error += dy;
                x1 += sx;
            }
            if (twice <= dx)
            {
                error += dx;
                y1 += sy;
            }
        }
    };
    if (name == "gear")
    {
        p.drawRect(4, 4, 8, 8);
        p.fillRect(6, 0, 4, 4, color);
        p.fillRect(6, 12, 4, 4, color);
        p.fillRect(0, 6, 4, 4, color);
        p.fillRect(12, 6, 4, 4, color);
        p.drawRect(7, 7, 2, 2);
    }
    else if (name == "plus")
    {
        line(7, 2, 7, 12);
        line(2, 7, 12, 7);
    }
    else if (name == "clock")
    {
        line(4, 0, 10, 0);
        line(2, 2, 2, 2);
        line(12, 2, 12, 2);
        line(0, 4, 0, 10);
        line(14, 4, 14, 10);
        line(2, 12, 2, 12);
        line(12, 12, 12, 12);
        line(4, 14, 10, 14);
        line(7, 4, 7, 8);
        line(7, 8, 10, 8);
    }
    else if (name == "hex")
    {
        p.drawRect(1, 4, 4, 8);
        line(8, 5, 13, 10);
        line(13, 5, 8, 10);
    }
    else if (name == "terminal")
    {
        p.drawRect(1, 2, 14, 12);
        line(4, 5, 6, 7);
        line(6, 7, 4, 9);
        line(9, 10, 12, 10);
    }
    else if (name == "cursor")
    {
        line(5, 1, 10, 1);
        line(7, 1, 7, 13);
        line(5, 13, 10, 13);
    }
    else if (name == "dock")
    {
        p.drawRect(1, 2, 14, 12);
        line(1, 5, 13, 5);
        line(5, 9, 11, 9);
        line(5, 9, 7, 7);
        line(5, 9, 7, 11);
    }
    else if (name == "widgets")
    {
        p.drawRect(1, 1, 5, 5);
        p.drawRect(10, 1, 5, 5);
        p.drawRect(1, 10, 5, 5);
        line(12, 9, 12, 14);
        line(9, 12, 14, 12);
    }
    else if (name == "close")
    {
        line(3, 3, 11, 11);
        line(11, 3, 3, 11);
    }
    else if (name == "refresh")
    {
        line(3, 4, 11, 4);
        line(3, 4, 3, 10);
        line(3, 10, 11, 10);
        line(11, 10, 11, 8);
        line(11, 1, 11, 5);
        line(8, 5, 11, 5);
    }
    else if (name == "search")
    {
        p.drawRect(2, 2, 8, 8);
        line(10, 10, 13, 13);
    }
    else if (name == "pause")
    {
        p.fillRect(3, 3, 3, 10, color);
        p.fillRect(10, 3, 3, 10, color);
    }
    else if (name == "play")
    {
        for (int i = 0; i < 6; ++i)
        {
            p.fillRect(4 + i, 2 + i, 1, 12 - i * 2, color);
        }
    }
    else if (name == "trash")
    {
        line(1, 3, 13, 3);
        line(5, 1, 9, 1);
        p.drawRect(3, 5, 10, 9);
        line(6, 6, 6, 11);
        line(9, 6, 9, 11);
    }
    else if (name == "wave")
    {
        line(0, 8, 3, 8);
        line(3, 8, 3, 3);
        line(3, 3, 6, 3);
        line(6, 3, 6, 12);
        line(6, 12, 10, 12);
        line(10, 12, 10, 6);
        line(10, 6, 14, 6);
    }
    else if (name == "port")
    {
        p.drawRect(3, 5, 10, 7);
        line(5, 1, 5, 4);
        line(10, 1, 10, 4);
        line(7, 12, 7, 14);
        p.fillRect(5, 7, 2, 2, color);
        p.fillRect(9, 7, 2, 2, color);
    }
    else if (name == "send")
    {
        line(1, 7, 13, 7);
        line(8, 2, 13, 7);
        line(13, 7, 8, 12);
    }
    else if (name == "more")
    {
        for (int x : {1, 7, 13})
        {
            p.fillRect(x, 7, 2, 2, color);
        }
    }
    else
    {
        p.drawRect(2, 1, 12, 14);
        line(5, 5, 10, 5);
        line(5, 9, 8, 9);
    }
    p.restore();
}

QIcon icon(const QString &name, const QColor &color, int size)
{
    QPixmap pixmap(size * 2, size * 2);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    drawIcon(p, name, QRectF(0, 0, size, size), color);
    return QIcon(pixmap);
}

IconButton::IconButton(const QString &name, const QString &title, QWidget *parent)
    : QAbstractButton(parent), glyph(name)
{
    setFixedSize(34, 34);
    setToolTip(title);
    setAccessibleName(title);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    animation.setDuration(140);
    animation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value)
            {
                progress = value.toReal();
                update();
            });
    connect(this, &QAbstractButton::toggled, this, qOverload<>(&QWidget::update));
}

QSize IconButton::sizeHint() const
{
    return QSize(34, 34);
}

void IconButton::enterEvent(QEnterEvent *event)
{
    animation.stop();
    animation.setStartValue(progress);
    animation.setEndValue(1.0);
    animation.start();
    QAbstractButton::enterEvent(event);
}

void IconButton::leaveEvent(QEvent *event)
{
    animation.stop();
    animation.setStartValue(progress);
    animation.setEndValue(0.0);
    animation.start();
    QAbstractButton::leaveEvent(event);
}

void IconButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const bool onRail = parentWidget() && parentWidget()->objectName() == "rail";
    const QColor foreground = onRail ? QColor("#d7dfc4") : theme.muted;
    const QColor active = onRail ? QColor("#edaa50") : theme.accent;
    const QColor color = !isEnabled() ? theme.line
        : isChecked() || hasFocus() || progress > 0.3 ? active : foreground;
    const qreal shift = isDown() ? 1.0 : -progress;
    const qreal extent = qMin(20.0, qreal(qMin(width(), height()) - 6)) + progress;
    drawIcon(p, glyph == "pause" && isChecked() ? "play" : glyph,
             QRectF((width() - extent) / 2, (height() - extent) / 2 + shift, extent, extent), color);
}

Combo::Combo(QWidget *parent) : QComboBox(parent)
{
    setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    setMinimumContentsLength(4);
    setMinimumWidth(0);
    setMaxVisibleItems(12);
}

void Combo::wheelEvent(QWheelEvent *event)
{
    if (view()->isVisible())
    {
        QComboBox::wheelEvent(event);
    }
    else
    {
        event->ignore();
    }
}

void Combo::paintEvent(QPaintEvent *event)
{
    QComboBox::paintEvent(event);
    QPainter p(this);
    const QColor color = palette().color(QPalette::Text);
    const int x = width() - 18;
    const int y = height() / 2 - 2;
    p.fillRect(x, y, 2, 2, color);
    p.fillRect(x + 6, y, 2, 2, color);
    p.fillRect(x + 2, y + 2, 4, 2, color);
}

ProtocolCombo::ProtocolCombo(QWidget *parent) : Combo(parent)
{
    addItems({"串口", "TCP 客户端", "TCP 服务端", "UDP"});
    setObjectName("protocolSelector");
}

void Combo::showPopup()
{
    if (!menuPopup)
    {
        QComboBox::showPopup();
        return;
    }
    auto menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setMinimumWidth(width());
    for (int i = 0; i < count(); ++i)
    {
        const auto description = itemData(i, Qt::ToolTipRole).toString();
        auto action = menu->addAction(description.isEmpty() ? itemText(i) : description);
        action->setCheckable(true);
        action->setChecked(i == currentIndex());
        connect(action, &QAction::triggered, this,
                [this, i]
                {
                    setCurrentIndex(i);
                    emit activated(i);
                });
    }
    menu->popup(mapToGlobal(QPoint(0, height() + 5)));
    auto fade = new QPropertyAnimation(menu, "windowOpacity", menu);
    fade->setDuration(110);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}

void ProtocolCombo::showPopup()
{
    menuPopup = true;
    Combo::showPopup();
}

SearchBox::SearchBox(QWidget *parent) : QLineEdit(parent)
{
    setObjectName("searchBox");
    setClearButtonEnabled(false);
    setTextMargins(23, 0, 24, 0);
    setMinimumWidth(70);
    setMaximumWidth(250);
    clearButton = new IconButton("close", "清空搜索", this);
    clearButton->setObjectName("clearSearch");
    clearButton->setFixedSize(24, 24);
    clearButton->hide();
    clearButton->setFocusPolicy(Qt::NoFocus);
    connect(clearButton, &QAbstractButton::clicked, this,
            [this]
            {
                clear();
                setFocus();
            });
    connect(this, &QLineEdit::textChanged, clearButton,
            [this](const QString &text) { clearButton->setVisible(!text.isEmpty()); });
}

void SearchBox::resizeEvent(QResizeEvent *event)
{
    QLineEdit::resizeEvent(event);
    clearButton->move(width() - clearButton->width() - 5, (height() - clearButton->height()) / 2);
}

void SearchBox::paintEvent(QPaintEvent *event)
{
    QLineEdit::paintEvent(event);
    QPainter painter(this);
    drawIcon(painter, "search", QRectF(11, (height() - 16) / 2.0, 16, 16), clearButton->theme.muted);
}

ConnectionBar::ConnectionBar(QWidget *parent) : TearTabBar(parent)
{
    setObjectName("connections");
    setDrawBase(false);
    setExpanding(false);
    setMovable(true);
    setElideMode(Qt::ElideRight);
    setUsesScrollButtons(true);
    setMinimumWidth(0);
}

QSize ConnectionBar::tabSizeHint(int index) const
{
    return QSize(
        qBound(124, fontMetrics().horizontalAdvance(tabText(index)) + (tabIcon(index).isNull() ? 76 : 96), 226), 43);
}

QSize ConnectionBar::sizeHint() const
{
    int width = 0;
    for (int i = 0; i < count(); ++i)
    {
        width += tabSizeHint(i).width();
    }
    return QSize(width, 43);
}

QSize ConnectionBar::minimumSizeHint() const
{
    return QSize(60, 43);
}

Grip::Grip(Qt::Orientation orientation, QSplitter *parent) : QSplitterHandle(orientation, parent)
{
    setToolTip("拖动调整大小，双击均分");
}

void Grip::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QColor color = palette().color(QPalette::Mid);
    color.setAlpha(underMouse() ? 150 : 60);
    p.setPen(QPen(color, 2, Qt::SolidLine, Qt::FlatCap));
    const auto center = rect().center();
    if (orientation() == Qt::Vertical)
    {
        for (int x : {-8, -2, 4})
        {
            p.fillRect(QRect(center + QPoint(x, -1), QSize(4, 2)), color);
        }
    }
    else if (underMouse())
    {
        p.drawLine(center + QPoint(0, -16), center + QPoint(0, 16));
    }
}

void Grip::mouseDoubleClickEvent(QMouseEvent *event)
{
    const auto sizes = splitter()->sizes();
    const int total = std::accumulate(sizes.cbegin(), sizes.cend(), 0);
    splitter()->setSizes({total / 2, total / 2});
    event->accept();
}

Splitter::Splitter(Qt::Orientation orientation, QWidget *parent) : QSplitter(orientation, parent)
{
    setHandleWidth(8);
    setChildrenCollapsible(false);
}

QSplitterHandle *Splitter::createHandle()
{
    return new Grip(orientation(), this);
}

WavePlot::WavePlot(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(200, 150);
    setMouseTracking(true);
}

void WavePlot::setSamples(const QJsonObject &data)
{
    series.clear();
    const auto sources = data["sources"].toObject();
    for (auto it = sources.begin(); it != sources.end(); ++it)
    {
        for (const auto &value : it.value().toObject()["samples"].toArray())
        {
            const auto sample = value.toObject();
            const auto values = sample["values"].toObject();
            for (auto variable = values.begin(); variable != values.end(); ++variable)
            {
                const QString name = it.key() == "default" ? variable.key() : it.key() + "/" + variable.key();
                if (!series.contains(name) && series.size() >= 16)
                {
                    continue;
                }
                series[name].append(QPointF(sample["index"].toDouble(), variable.value().toDouble()));
            }
        }
    }
    update();
}

void WavePlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), theme.panel);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF area = QRectF(rect()).adjusted(64, 34, -24, -48);
    p.setPen(QPen(theme.line, 1));
    for (int i = 0; i <= 8; ++i)
    {
        const qreal x = area.left() + area.width() * i / 8;
        const qreal y = area.top() + area.height() * i / 8;
        p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }
    if (series.isEmpty())
    {
        p.setPen(theme.muted);
        p.drawText(area, Qt::AlignCenter, "等待数值报文\n支持 CSV、命名数值和 JSON");
        return;
    }
    double xmin = std::numeric_limits<double>::max();
    double xmax = std::numeric_limits<double>::lowest();
    double ymin = xmin;
    double ymax = xmax;
    for (const auto &points : series)
    {
        for (const auto &point : points)
        {
            xmin = std::min(xmin, point.x());
            xmax = std::max(xmax, point.x());
            ymin = std::min(ymin, point.y());
            ymax = std::max(ymax, point.y());
        }
    }
    const double span = std::max(1.0, xmax - xmin) / zoom;
    xmax += offset;
    xmin = xmax - span;
    const double yspan = std::max(0.01, ymax - ymin);
    ymax += yspan * 0.1;
    ymin -= yspan * 0.1;
    p.setPen(theme.muted);
    for (int i = 0; i <= 8; ++i)
    {
        const qreal y = area.bottom() - area.height() * i / 8;
        p.drawText(QRectF(0, y - 10, 54, 20), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(ymin + (ymax - ymin) * i / 8, 'g', 4));
        const qreal x = area.left() + area.width() * i / 8;
        p.drawText(QRectF(x - 35, area.bottom() + 8, 70, 20), Qt::AlignCenter,
                   QString::number(xmin + span * i / 8, 'f', 0));
    }
    p.drawText(QRectF(area.left(), height() - 23, area.width(), 20), Qt::AlignCenter, "采样序号");
    const QList<QColor> colors{theme.accent,      theme.green, QColor("#6c92a0"),
                               QColor("#b0839a"), theme.red,   QColor("#9e9a52")};
    int index = 0;
    int legendX = 76;
    for (auto it = series.begin(); it != series.end(); ++it)
    {
        const QColor color = colors[index++ % colors.size()];
        p.setPen(QPen(color, 2));
        p.drawLine(legendX, 17, legendX + 18, 17);
        p.setPen(theme.text);
        p.drawText(legendX + 24, 22, it.key());
        legendX += 38 + fontMetrics().horizontalAdvance(it.key());
        p.save();
        p.setClipRect(area);
        QPainterPath path;
        bool first = true;
        for (const auto &point : it.value())
        {
            const QPointF mapped(area.left() + (point.x() - xmin) / span * area.width(),
                                 area.bottom() - (point.y() - ymin) / (ymax - ymin) * area.height());
            first ? path.moveTo(mapped) : path.lineTo(mapped);
            first = false;
        }
        p.setPen(QPen(color, 1.8));
        p.drawPath(path);
        p.restore();
    }
}

void WavePlot::wheelEvent(QWheelEvent *event)
{
    zoom = qBound(1.0, zoom * (event->angleDelta().y() > 0 ? 1.2 : 1 / 1.2), 100.0);
    update();
    event->accept();
}

void WavePlot::mousePressEvent(QMouseEvent *event)
{
    lastMouse = event->pos();
}

void WavePlot::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        offset -= (event->pos().x() - lastMouse.x()) * 2 / zoom;
        lastMouse = event->pos();
        update();
    }
}

void WavePlot::mouseDoubleClickEvent(QMouseEvent *)
{
    zoom = 1;
    offset = 0;
    update();
}
} // namespace sw
