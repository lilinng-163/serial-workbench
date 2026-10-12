#include "controls.h"
#include <cmath>
#include <numeric>

namespace sw
{
Theme Theme::make(bool dark)
{
    Theme t;
    t.dark = dark;
    if (dark)
    {
        t.background = "#1c2431";
        t.panel = "#202b3a";
        t.sidebar = "#232e3e";
        t.text = "#dce5ef";
        t.muted = "#8d9caf";
        t.line = "#344256";
        t.accent = "#74acff";
        t.hover = "#30425a";
        t.green = "#55c6ac";
        t.red = "#ed8894";
        t.amber = "#dfba7b";
    }
    return t;
}

QString Theme::stylesheet() const
{
    QString s = R"(
        QWidget { color: $text; font-family: "Microsoft YaHei UI"; font-size: 13px; }
        QMainWindow, QDialog, QWidget#root { background: $bg; }
        QFrame#sidebar { background: $side; border-right: 1px solid $line; }
        QFrame#rail { background: $side; border-right: 1px solid $line; }
        QWidget#workspace, QWidget#page, QWidget#sender { background: $panel; }
        QLabel { background: transparent; border: none; }
        QLabel#muted { color: $muted; font-size: 12px; }
        QLabel#section { font-weight: 600; font-size: 17px; }
        QLabel#brand { font-weight: 600; font-size: 13px; }
        QLineEdit, QComboBox, QSpinBox { background: $panel; border: 1px solid $line; border-radius: 6px; padding: 6px 10px; min-height: 22px; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: $accent; }
        QLineEdit#searchBox { border-radius: 16px; padding: 5px 8px; background: $side; }
        QComboBox { padding-right: 28px; }
        QComboBox::drop-down { border: none; width: 28px; }
        QComboBox::down-arrow { image: none; }
        QComboBox QLineEdit { border: none; padding: 0; min-height: 0; background: transparent; }
        QComboBox QAbstractItemView { background: $panel; color: $text; border: 1px solid $line; selection-background-color: $hover; selection-color: $text; outline: none; padding: 4px; }
        QComboBox QAbstractItemView::item { min-height: 28px; padding: 4px 8px; }
        QSpinBox { padding-right: 22px; }
        QSpinBox::up-button, QSpinBox::down-button { width: 20px; border: none; }
        QSpinBox::up-arrow { image: url(:/native/chevron-up.svg); width: 8px; height: 5px; }
        QSpinBox::down-arrow { image: url(:/native/chevron-down.svg); width: 8px; height: 5px; }
        QPushButton, QToolButton { background: transparent; border: 1px solid transparent; border-radius: 6px; padding: 6px 12px; min-height: 22px; }
        QPushButton:hover, QToolButton:hover { background: $hover; }
        QPushButton:checked { background: $hover; color: $accent; font-weight: 600; }
        QPushButton:focus, QToolButton:focus { border-color: $accent; }
        QPushButton#primary { background: $accent; color: $primarytext; font-weight: 600; padding: 7px 18px; }
        QPushButton#primary:hover { background: $primaryhover; }
        QPushButton#outline { border: 1px solid $line; background: $panel; }
        QPushButton:disabled, QToolButton:disabled { color: $muted; }
        QPlainTextEdit, QTextEdit { border: 1px solid $line; border-radius: 6px; background: $panel; padding: 8px; selection-background-color: $accent; }
        QTextEdit#monitor { border: none; border-radius: 0; padding: 16px 18px; }
        QTreeWidget { border: none; background: $panel; alternate-background-color: $side; outline: none; }
        QTreeWidget::item { padding: 5px; min-height: 22px; }
        QTreeWidget::item:selected { background: $hover; color: $text; }
        QHeaderView::section { background: $side; border: none; border-bottom: 1px solid $line; padding: 9px 8px; color: $muted; }
        QTabWidget::pane { border: none; border-top: 1px solid $line; background: $panel; }
        QTabBar { background: transparent; }
        QTabBar QToolButton { background: $side; border: none; padding: 0; min-height: 0; border-radius: 4px; }
        QTabBar::scroller { width: 48px; }
        QTabBar::tab { background: transparent; padding: 10px 12px; border-bottom: 2px solid transparent; color: $muted; }
        QTabBar::tab:selected { color: $accent; border-bottom-color: $accent; background: $panel; }
        QTabBar::tab:hover { background: $hover; }
        QTabBar#connections::tab { padding: 9px 10px; margin: 4px 2px 0 0; border-radius: 7px 7px 0 0; }
        QMenu { background: $panel; border: 1px solid $line; padding: 6px; }
        QMenu::item { padding: 8px 28px 8px 12px; border-radius: 4px; }
        QMenu::item:selected { background: $hover; }
        QMenu::separator { height: 1px; background: $line; margin: 5px 8px; }
        QCheckBox { spacing: 7px; background: transparent; }
        QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid $muted; border-radius: 3px; background: $panel; }
        QCheckBox::indicator:hover { border-color: $accent; }
        QCheckBox::indicator:checked { border-color: $accent; background: $accent; image: url(:/native/check.svg); }
        QScrollArea { border: none; background: transparent; }
        QScrollBar:vertical { background: transparent; width: 9px; margin: 0; }
        QScrollBar:horizontal { background: transparent; height: 9px; margin: 0; }
        QScrollBar::handle { background: $line; border-radius: 4px; min-height: 28px; min-width: 28px; }
        QScrollBar::handle:hover { background: $muted; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        QStatusBar { background: $panel; color: $muted; font-size: 11px; border-top: 1px solid $line; }
        QToolTip { background: $panel; color: $text; border: 1px solid $line; padding: 5px; }
    )";
    const QMap<QString, QString> colors {{"$text", text.name()}, {"$muted", muted.name()}, {"$side", sidebar.name()},
        {"$bg", background.name()}, {"$panel", panel.name()}, {"$line", line.name()}, {"$accent", accent.name()},
        {"$hover", hover.name()}, {"$primarytext", dark ? "#172337" : "#ffffff"}, {"$primaryhover", dark ? "#92bdff" : "#286ccc"}};
    for (auto it = colors.begin(); it != colors.end(); ++it)
    {
        s.replace(it.key(), it.value());
    }
    return s;
}

void drawIcon(QPainter &p, const QString &name, QRectF rect, const QColor &color, qreal rotation)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(rect.center());
    p.scale(rect.width() / 24, rect.height() / 24);
    p.rotate(rotation);
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    if (name == "gear")
    {
        QPainterPath path;
        for (int i = 0; i < 32; ++i)
        {
            const qreal radius = (i % 4 == 1 || i % 4 == 2) ? 9.8 : 7.8;
            const qreal angle = i * M_PI / 16;
            const QPointF point(radius * std::cos(angle), radius * std::sin(angle));
            i == 0 ? path.moveTo(point) : path.lineTo(point);
        }
        path.closeSubpath();
        p.drawPath(path);
        p.drawEllipse(QPointF(), 3, 3);
    }
    else if (name == "plus")
    {
        p.drawLine(-7, 0, 7, 0);
        p.drawLine(0, -7, 0, 7);
    }
    else if (name == "clock")
    {
        p.drawEllipse(QPointF(), 8, 8);
        p.drawLine(0, -5, 0, 0);
        p.drawLine(0, 0, 4, 2);
    }
    else if (name == "hex")
    {
        p.setFont(QFont("Cascadia Mono", 11, QFont::DemiBold));
        p.drawText(QRectF(-12, -12, 24, 24), Qt::AlignCenter, "0x");
    }
    else if (name == "terminal")
    {
        p.drawRoundedRect(QRectF(-9, -8, 18, 16), 3, 3);
        p.drawPolyline(QPolygonF {QPointF(-5, -3), QPointF(-2, 0), QPointF(-5, 3)});
        p.drawLine(1, 3, 5, 3);
    }
    else if (name == "cursor")
    {
        p.drawLine(-3, -8, 3, -8);
        p.drawLine(0, -8, 0, 8);
        p.drawLine(-3, 8, 3, 8);
    }
    else if (name == "dock")
    {
        p.drawRoundedRect(QRectF(-9, -8, 18, 16), 2, 2);
        p.drawLine(-9, -3, 9, -3);
        p.drawLine(5, 2, -4, 2);
        p.drawPolyline(QPolygonF {QPointF(-1, -1), QPointF(-4, 2), QPointF(-1, 5)});
    }
    else if (name == "widgets")
    {
        p.drawRoundedRect(QRectF(-8, -8, 6, 6), 1, 1);
        p.drawRoundedRect(QRectF(2, -8, 6, 6), 1, 1);
        p.drawRoundedRect(QRectF(-8, 2, 6, 6), 1, 1);
        p.drawLine(5, 2, 5, 8);
        p.drawLine(2, 5, 8, 5);
    }
    else if (name == "close")
    {
        p.drawLine(-5, -5, 5, 5);
        p.drawLine(5, -5, -5, 5);
    }
    else if (name == "refresh")
    {
        p.drawArc(QRectF(-7, -7, 14, 14), 30 * 16, 300 * 16);
        p.drawPolyline(QPolygonF {QPointF(3, -8), QPointF(7, -6), QPointF(7, -10)});
    }
    else if (name == "search")
    {
        p.drawEllipse(QPointF(-2, -2), 6, 6);
        p.drawLine(3, 3, 8, 8);
    }
    else if (name == "pause")
    {
        p.drawLine(-3, -6, -3, 6);
        p.drawLine(3, -6, 3, 6);
    }
    else if (name == "play")
    {
        p.drawPolygon(QPolygonF {QPointF(-4, -7), QPointF(7, 0), QPointF(-4, 7)});
    }
    else if (name == "trash")
    {
        p.drawLine(-8, -6, 8, -6);
        p.drawLine(-3, -9, 3, -9);
        p.drawRoundedRect(QRectF(-6, -6, 12, 14), 2, 2);
        p.drawLine(-2, -3, -2, 5);
        p.drawLine(2, -3, 2, 5);
    }
    else if (name == "wave")
    {
        p.drawPolyline(QPolygonF {QPointF(-10, 2), QPointF(-6, 2), QPointF(-3, -7), QPointF(2, 8), QPointF(6, -2), QPointF(10, -2)});
    }
    else if (name == "port")
    {
        p.drawRoundedRect(QRectF(-8, -5, 16, 11), 3, 3);
        p.drawLine(-4, -9, -4, -5);
        p.drawLine(4, -9, 4, -5);
        p.drawLine(0, 6, 0, 10);
        p.drawPoint(-3, 0);
        p.drawPoint(3, 0);
    }
    else if (name == "send")
    {
        p.drawPolygon(QPolygonF {QPointF(-9, -8), QPointF(10, 0), QPointF(-9, 8), QPointF(-5, 0)});
        p.drawLine(-5, 0, 8, 0);
    }
    else if (name == "more")
    {
        p.drawEllipse(QPointF(-6, 0), 0.8, 0.8);
        p.drawEllipse(QPointF(), 0.8, 0.8);
        p.drawEllipse(QPointF(6, 0), 0.8, 0.8);
    }
    else
    {
        p.drawRoundedRect(QRectF(-8, -8, 16, 16), 2, 2);
        p.drawLine(-4, -2, 4, -2);
        p.drawLine(-4, 3, 2, 3);
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
    connect(&animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value)
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
    p.setRenderHint(QPainter::Antialiasing);
    QColor fill = theme.accent;
    fill.setAlphaF((isChecked() ? 0.13 : 0.09 * progress) + (isDown() ? 0.06 : 0));
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 7, 7);
    const qreal extent = qMin(20, qMin(width(), height()) - 8);
    drawIcon(p, glyph == "pause" && isChecked() ? "play" : glyph, QRectF((width() - extent) / 2, (height() - extent) / 2, extent, extent),
        isChecked() || progress > 0.3 ? theme.accent : theme.muted, glyph == "gear" ? progress * 24 : 0);
    if (hasFocus())
    {
        p.setPen(QPen(theme.accent, 1, Qt::DotLine));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 7, 7);
    }
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
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(palette().color(QPalette::Text), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPointF center(width() - 15, height() / 2.0);
    p.drawPolyline(QPolygonF {center + QPointF(-3, -1.5), center + QPointF(0, 1.5), center + QPointF(3, -1.5)});
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
        connect(action, &QAction::triggered, this, [this, i]
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
    connect(clearButton, &QAbstractButton::clicked, this, [this] { clear(); setFocus(); });
    connect(this, &QLineEdit::textChanged, clearButton, [this](const QString &text)
    {
        clearButton->setVisible(!text.isEmpty());
    });
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
    drawIcon(painter, "search", QRectF(11, (height() - 16) / 2.0, 16, 16), palette().color(QPalette::Mid));
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
    return QSize(qBound(116, fontMetrics().horizontalAdvance(tabText(index)) + (tabIcon(index).isNull() ? 68 : 88), 218), 43);
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
    p.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap));
    const auto center = rect().center();
    if (orientation() == Qt::Vertical)
    {
        p.drawLine(center + QPoint(-16, 0), center + QPoint(16, 0));
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
        p.drawText(QRectF(0, y - 10, 54, 20), Qt::AlignRight | Qt::AlignVCenter, QString::number(ymin + (ymax - ymin) * i / 8, 'g', 4));
        const qreal x = area.left() + area.width() * i / 8;
        p.drawText(QRectF(x - 35, area.bottom() + 8, 70, 20), Qt::AlignCenter, QString::number(xmin + span * i / 8, 'f', 0));
    }
    p.drawText(QRectF(area.left(), height() - 23, area.width(), 20), Qt::AlignCenter, "采样序号");
    const QList<QColor> colors {theme.accent, theme.green, QColor("#c48a39"), QColor("#9c78c4"), QColor("#d57491"), QColor("#708d50")};
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
            const QPointF mapped(area.left() + (point.x() - xmin) / span * area.width(), area.bottom() - (point.y() - ymin) / (ymax - ymin) * area.height());
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
}
