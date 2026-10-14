// SPDX-License-Identifier: Apache-2.0

#include "controls.h"
#include <QSvgRenderer>
#include <cmath>
#include <numeric>

namespace sw
{
namespace
{
// Mouse focus must not look like a selected action. Keep a separate cue for
// keyboard navigation, including buttons supplied by Qt dialogs.
class ButtonFocusFeedback : public QObject
{
  public:
    using QObject::QObject;

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        auto button = qobject_cast<QAbstractButton *>(watched);
        if (!button)
        {
            return false;
        }

        if (event->type() == QEvent::MouseButtonPress)
        {
            setKeyboardFocus(button, false);
        }
        else if (event->type() == QEvent::KeyPress)
        {
            setKeyboardFocus(button, true);
        }
        else if (event->type() == QEvent::FocusIn)
        {
            const auto reason = static_cast<QFocusEvent *>(event)->reason();
            if (reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason || reason == Qt::ShortcutFocusReason)
            {
                setKeyboardFocus(button, true);
            }
            else if (reason != Qt::ActiveWindowFocusReason && reason != Qt::PopupFocusReason)
            {
                setKeyboardFocus(button, false);
            }
            button->update();
        }
        else if (event->type() == QEvent::FocusOut)
        {
            button->update();
        }
        return false;
    }

  private:
    static void setKeyboardFocus(QAbstractButton *button, bool visible)
    {
        if (button->property("keyboardFocus").toBool() == visible)
        {
            return;
        }
        button->setProperty("keyboardFocus", visible);
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
};

void installButtonFocusFeedback()
{
    static auto feedback = []
    {
        auto result = new ButtonFocusFeedback(qApp);
        qApp->installEventFilter(result);
        return result;
    }();
    Q_UNUSED(feedback);
}
} // namespace

StatusLed::StatusLed(QWidget *parent) : QWidget(parent)
{
    setFixedSize(22, 22);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void StatusLed::setState(State state)
{
    if (currentState == state)
    {
        return;
    }
    currentState = state;
    update();
}

void StatusLed::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPointF center(width() / 2.0, height() / 2.0);
    QColor color = theme.muted;
    if (currentState == State::Online)
    {
        color = theme.dark ? QColor("#6edf8d") : QColor("#2b9e53");
    }
    else if (currentState == State::Pending)
    {
        color = theme.dark ? QColor("#f4c46b") : QColor("#c58220");
    }
    else if (currentState == State::Error)
    {
        color = theme.dark ? QColor("#ff927c") : QColor("#ca513f");
    }

    const bool lit = currentState != State::Offline;
    if (lit)
    {
        QRadialGradient glow(center, 11);
        auto halo = color;
        halo.setAlpha(theme.dark ? 100 : 65);
        glow.setColorAt(0.35, halo);
        halo.setAlpha(0);
        glow.setColorAt(1, halo);
        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawEllipse(center, 11, 11);
    }

    painter.setPen(QPen(lit ? color.darker(130) : theme.edge, 1));
    painter.setBrush(lit ? color : theme.line);
    painter.drawEllipse(center, 5.5, 5.5);
    if (lit)
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 150));
        painter.drawEllipse(center + QPointF(-1.5, -1.8), 1.7, 1.3);
    }
}

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
    font.setFamilies({font.family(), "Microsoft YaHei UI"});
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
    installButtonFocusFeedback();
    QString s = R"(
        QWidget { color: $text; font-family: "$font"; font-size: 14px; font-weight: normal; }
        QMainWindow, QDialog, QWidget#root { background: $bg; }
        QFrame#agentCard { background: $panel; border: 1px solid $line; }
        QLabel#agentTitle { font-size: 14px; font-weight: 600; }
        QLabel#agentLogo { background: #ffffff; border: none; }
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
        QLabel#connectionState { font-weight: 600; }
        QLabel#themeTitle { font-size: 24px; }
        QLabel#aboutTitle { font-family: "Segoe UI"; font-size: 24px; font-weight: 600; }
        QLabel#aboutVersion { color: $muted; font-size: 14px; }
        QLabel#aboutSection { font-weight: 600; }
        QWidget#pixelBrand { background: $rail; border: 2px solid $rail; border-bottom: 4px solid $edge; }
        QLabel#brandTitle { color: #f6f4e8; font-family: "Segoe UI"; font-size: 23px; font-weight: 600; }
        QLabel#brandSubtitle { color: #d7dfc4; font-size: 13px; }
        QLabel#brandMark { color: #edaa50; font-family: "Segoe UI"; font-size: 16px; font-weight: 600; }
        QLabel#senderLabel { color: $muted; font-size: 13px; }
        QLabel#connectionHint { color: $muted; padding: 10px 0 0 0; border-top: 1px solid $line; }
        QLineEdit, QComboBox, QSpinBox { background: $panel; border: 1px solid $line; border-bottom: 2px solid $edge; border-radius: 0; padding: 6px 10px; min-height: 24px; selection-background-color: $accent; selection-color: $onaccent; }
        QLineEdit:hover, QComboBox:hover, QSpinBox:hover { border-color: $edge; border-bottom-color: $accent; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: $accent; border-bottom-width: 2px; }
        QComboBox#protocolSelector { background: $hover; border-color: $edge; padding: 8px 28px 8px 10px; }
        QComboBox#protocolSelector:focus { border-color: $accent; }
        QLineEdit#searchBox { padding: 5px 8px; background: $panel; }
        QLineEdit#tabTitleEditor { background: $panel; color: $text; border: none; border-bottom: 2px solid $accent; padding: 0 3px; min-height: 0; }
        QComboBox { padding-right: 28px; combobox-popup: 0; }
        QComboBox::drop-down { border: none; width: 28px; }
        QComboBox::down-arrow { image: none; }
        QComboBox QLineEdit, QComboBox QLineEdit:focus, QComboBox QLineEdit:hover { border: none; padding: 0; min-height: 0; background: transparent; }
        QComboBox QAbstractItemView { background: $panel; color: $text; border: none; selection-background-color: $hover; selection-color: $text; outline: none; padding: 4px; }
        QComboBox QAbstractItemView::item { min-height: 30px; padding: 4px 8px; }
        QSpinBox { padding-right: 26px; }
        QSpinBox::up-button, QSpinBox::down-button { width: 24px; border: none; border-left: 1px solid $line; }
        QSpinBox::up-arrow { image: url(:/icons/chevron-up.svg); width: 10px; height: 6px; }
        QSpinBox::down-arrow { image: url(:/icons/chevron-down.svg); width: 10px; height: 6px; }
        QPushButton, QToolButton { outline: none; background: transparent; border: 1px solid transparent; border-radius: 0; padding: 6px 12px; min-height: 24px; }
        QPushButton:hover, QToolButton:hover { color: $accent; }
        QPushButton:checked { color: $accent; font-weight: 600; }
        QPushButton[keyboardFocus="true"]:focus, QToolButton[keyboardFocus="true"]:focus { color: $accent; }
        QPushButton:pressed, QToolButton:pressed { color: $text; padding-top: 7px; padding-bottom: 5px; }
        QPushButton#primary { background: $accent; color: $onaccent; border: 1px solid $edge; border-top-color: $primarytop; border-bottom: 2px solid $primaryshadow; padding: 6px 16px; }
        QPushButton#primary:hover, QPushButton#primary[keyboardFocus="true"]:focus { background: $primaryhover; color: $onaccent; }
        QPushButton#primary:pressed { color: $onaccent; padding-top: 7px; padding-bottom: 5px; }
        QPushButton#primary:disabled { background: $line; color: $muted; border-color: $edge; }
        QPushButton#outline { color: $text; border: 1px solid $line; border-bottom: 2px solid $edge; background: $panel; }
        QPushButton#outline[keyboardFocus="true"]:focus { border-color: $accent; color: $accent; }
        QPushButton#outline:hover { color: $accent; border-bottom-color: $accent; }
        QPushButton:disabled, QToolButton:disabled { color: $muted; }
        QPlainTextEdit, QTextEdit { border: 1px solid $edge; border-radius: 0; background: $panel; padding: 8px; selection-background-color: $accent; selection-color: $onaccent; }
        QPlainTextEdit:focus { border-color: $accent; }
        QTextEdit#monitor { border: none; padding: 16px 18px; }
        QFontDialog QListView { color: $text; background: $panel; border: 1px solid $edge; selection-background-color: $hover; selection-color: $text; outline: none; }
        QFontDialog QListView::item { padding: 3px 6px; }
        QFontDialog QGroupBox { color: $text; border: 1px solid $edge; margin-top: 12px; padding: 10px; }
        QFontDialog QGroupBox::title { color: $text; subcontrol-origin: margin; left: 8px; padding: 0 4px; }
        QTreeWidget { border: 1px solid $line; background: $panel; alternate-background-color: $side; outline: none; }
        QTreeWidget::item { padding: 6px; min-height: 24px; }
        QTreeWidget::item:selected { background: $hover; color: $text; }
        QHeaderView::section { background: $side; border: none; border-bottom: 2px solid $edge; padding: 10px 8px; color: $muted; }
        QTabWidget { background: $side; }
        QTabWidget::pane { border: none; border-top: 1px solid $edge; background: $panel; }
        QTabBar { background: transparent; }
        QTabBar QToolButton { background: $side; border: 1px solid $line; padding: 0; min-height: 0; }
        QTabBar::scroller { width: 48px; }
        QTabBar::tab { background: $side; padding: 11px 13px; border-right: 1px solid $line; border-bottom: 2px solid transparent; color: $muted; }
        QTabBar::tab:selected { color: $text; border-bottom-color: $accent; background: $panel; }
        QTabBar::tab:hover { background: $hover; color: $text; }
        QTabBar#connections::tab { padding: 10px 11px; margin: 4px 0 0 0; border: 1px solid $edge; border-bottom: 2px solid $edge; background: $side; }
        QTabBar#connections::tab:selected { background: $panel; border-bottom-color: $accent; color: $text; }
        QMenu { background: $panel; border: 1px solid $edge; padding: 4px; }
        QMenu::item { padding: 10px 28px 10px 16px; border-left: 2px solid transparent; }
        QMenu::item:selected { background: $hover; border-left-color: $accent; color: $text; }
        QMenu::separator { height: 1px; background: $line; margin: 5px 8px; }
        QMenu::indicator { width: 18px; height: 18px; margin-left: 8px; subcontrol-origin: padding; subcontrol-position: center left; image: none; background: transparent; }
        QMenu::indicator:checked { image: url(:/icons/check.svg); background: $accent; }
        QCheckBox { spacing: 7px; background: transparent; }
        QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid $edge; border-bottom-width: 2px; background: $panel; }
        QCheckBox::indicator:hover { border-bottom-color: $accent; }
        QCheckBox::indicator:checked { border-color: $text; background: $accent; image: url(:/icons/check.svg); }
        QScrollArea { border: none; background: transparent; }
        QScrollBar:vertical { background: $side; width: 11px; margin: 0; }
        QScrollBar:horizontal { background: $side; height: 11px; margin: 0; }
        QScrollBar::handle { background: $line; border: 1px solid $edge; min-height: 30px; min-width: 30px; }
        QScrollBar::handle:hover { background: $muted; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        QStatusBar { background: $side; color: $muted; border-top: 2px solid $edge; }
        QStatusBar::item { border: none; }
        QToolTip { background: $panel; color: $text; border: 2px solid $edge; padding: 6px; }
    )";
    const QMap<QString, QString> colors{{"$font", interfaceFont().family()},
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
                                        {"$primarytop", dark ? "#ffd48d" : "#f4b765"},
                                        {"$primaryshadow", dark ? "#9b6128" : "#9b5d23"},
                                        {"$primaryhover", dark ? "#f4be71" : "#eba04b"}};
    for (auto it = colors.begin(); it != colors.end(); ++it)
    {
        s.replace(it.key(), it.value());
    }
    return s;
}

// Scale continuous vector paths so small icons remain readable at every DPI.
void drawIcon(QPainter &p, const QString &name, QRectF rect, const QColor &color, qreal rotation)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(rect.center());
    p.rotate(rotation);
    const qreal scale = qMin(rect.width(), rect.height()) / 24.0;
    p.scale(scale, scale);
    p.translate(-12, -12);
    p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    const auto stroke = [&](std::initializer_list<QPointF> points)
    {
        QPainterPath path;
        auto point = points.begin();
        path.moveTo(*point++);
        while (point != points.end())
        {
            path.lineTo(*point++);
        }
        p.drawPath(path);
    };

    if (name == "gear")
    {
        QPainterPath outline;
        const qreal pi = std::acos(-1.0);
        const QList<qreal> angles {-25, -14, -10, 10, 14, 25};
        const QList<qreal> radii {7, 7, 9.5, 9.5, 7, 7};
        for (int tooth = 0; tooth < 6; ++tooth)
        {
            for (int corner = 0; corner < angles.size(); ++corner)
            {
                const qreal angle = (tooth * 60 + angles[corner] - 90) * pi / 180;
                const QPointF point(12 + radii[corner] * std::cos(angle),
                                    12 + radii[corner] * std::sin(angle));
                if (tooth == 0 && corner == 0)
                {
                    outline.moveTo(point);
                }
                else
                {
                    outline.lineTo(point);
                }
            }
        }
        outline.closeSubpath();
        p.drawPath(outline);
        p.drawEllipse(QPointF(12, 12), 3, 3);
    }
    else if (name == "plus")
    {
        stroke({{12, 5}, {12, 19}});
        stroke({{5, 12}, {19, 12}});
    }
    else if (name == "clock")
    {
        p.drawEllipse(QRectF(3, 3, 18, 18));
        stroke({{12, 7}, {12, 12}, {16, 14}});
    }
    else if (name == "hex")
    {
        p.drawRoundedRect(QRectF(4, 6, 7, 12), 3, 3);
        stroke({{14, 10}, {20, 18}});
        stroke({{20, 10}, {14, 18}});
    }
    else if (name == "wrap")
    {
        stroke({{3, 5}, {21, 5}});
        stroke({{3, 10}, {16, 10}});
        stroke({{3, 17}, {7, 17}});
        QPainterPath turn;
        turn.moveTo(16, 10);
        turn.cubicTo(23, 10, 23, 17, 16, 17);
        turn.lineTo(12, 17);
        p.drawPath(turn);
        stroke({{15, 14}, {12, 17}, {15, 20}});
    }
    else if (name == "terminal")
    {
        p.drawRoundedRect(QRectF(3, 4, 18, 16), 1.5, 1.5);
        stroke({{7, 9}, {10, 12}, {7, 15}});
        stroke({{13, 15}, {17, 15}});
    }
    else if (name == "cursor")
    {
        stroke({{9, 4}, {15, 4}});
        stroke({{12, 4}, {12, 20}});
        stroke({{9, 20}, {15, 20}});
    }
    else if (name == "code")
    {
        stroke({{8, 6}, {2, 12}, {8, 18}});
        stroke({{16, 6}, {22, 12}, {16, 18}});
        stroke({{14, 4}, {10, 20}});
    }
    else if (name == "scriptCollapse" || name == "scriptExpand")
    {
        if (name == "scriptCollapse") { stroke({{6, 15}, {12, 9}, {18, 15}}); }
        else { stroke({{6, 9}, {12, 15}, {18, 9}}); }
    }
    else if (name == "clipboard")
    {
        QPainterPath body;
        body.moveTo(8, 5);
        body.lineTo(6, 5);
        body.quadTo(5, 5, 5, 7);
        body.lineTo(5, 20);
        body.quadTo(5, 21, 7, 21);
        body.lineTo(17, 21);
        body.quadTo(19, 21, 19, 19);
        body.lineTo(19, 7);
        body.quadTo(19, 5, 17, 5);
        body.lineTo(16, 5);
        p.drawPath(body);
        p.drawRoundedRect(QRectF(8, 3, 8, 4), 1, 1);
        stroke({{9, 12}, {15, 12}});
        stroke({{9, 16}, {13, 16}});
    }
    else if (name == "dock")
    {
        p.drawRoundedRect(QRectF(3, 4, 18, 16), 1.5, 1.5);
        stroke({{3, 8}, {21, 8}});
        stroke({{17, 14}, {8, 14}});
        stroke({{11, 11}, {8, 14}, {11, 17}});
    }
    else if (name == "widgets")
    {
        p.drawRoundedRect(QRectF(3, 3, 7, 7), 1, 1);
        p.drawRoundedRect(QRectF(14, 3, 7, 7), 1, 1);
        p.drawRoundedRect(QRectF(3, 14, 7, 7), 1, 1);
        stroke({{17.5, 14}, {17.5, 21}});
        stroke({{14, 17.5}, {21, 17.5}});
    }
    else if (name == "close")
    {
        stroke({{6, 6}, {18, 18}});
        stroke({{18, 6}, {6, 18}});
    }
    else if (name == "refresh")
    {
        QPainterPath arc;
        arc.moveTo(20, 10);
        arc.cubicTo(19, 6, 15, 3, 11, 4);
        arc.cubicTo(6, 4, 3, 8, 4, 13);
        arc.cubicTo(5, 18, 9, 21, 14, 20);
        arc.cubicTo(17, 20, 19, 18, 20, 16);
        p.drawPath(arc);
        stroke({{20, 4}, {20, 10}, {14, 10}});
    }
    else if (name == "search")
    {
        p.drawEllipse(QRectF(3, 3, 13, 13));
        stroke({{14, 14}, {21, 21}});
    }
    else if (name == "pause")
    {
        p.setPen(QPen(color, 2.6, Qt::SolidLine, Qt::RoundCap));
        stroke({{8, 6}, {8, 18}});
        stroke({{16, 6}, {16, 18}});
    }
    else if (name == "play")
    {
        p.setBrush(color);
        p.drawPolygon(QPolygonF {{7, 4}, {20, 12}, {7, 20}});
    }
    else if (name == "trash")
    {
        stroke({{4, 7}, {20, 7}});
        stroke({{8, 7}, {8, 4}, {16, 4}, {16, 7}});
        stroke({{6, 7}, {7, 21}, {17, 21}, {18, 7}});
        stroke({{10, 11}, {10.5, 17}});
        stroke({{14, 11}, {13.5, 17}});
    }
    else if (name == "follow")
    {
        stroke({{12, 3}, {12, 15}});
        stroke({{7, 10}, {12, 15}, {17, 10}});
        stroke({{5, 20}, {19, 20}});
    }
    else if (name == "export")
    {
        stroke({{12, 15}, {12, 3}});
        stroke({{7, 8}, {12, 3}, {17, 8}});
        stroke({{4, 14}, {4, 20}, {20, 20}, {20, 14}});
    }
    else if (name == "repeat")
    {
        stroke({{4, 10}, {4, 7}, {20, 7}});
        stroke({{17, 4}, {20, 7}, {17, 10}});
        stroke({{20, 14}, {20, 17}, {4, 17}});
        stroke({{7, 14}, {4, 17}, {7, 20}});
    }
    else if (name == "wave")
    {
        stroke({{2, 12}, {6, 12}, {9, 5}, {14, 20}, {18, 9}, {22, 9}});
    }
    else if (name == "port" || name == "lightning")
    {
        static QSvgRenderer lightning(QStringLiteral(":/brand/lightning.svg"));
        lightning.render(&p, QRectF(0, 0, 24, 24));
    }
    else if (name == "file-transfer")
    {
        stroke({{5, 20}, {5, 3}, {14, 3}, {19, 8}, {19, 12}});
        stroke({{14, 3}, {14, 8}, {19, 8}});
        stroke({{9, 16}, {21, 16}, {18, 13}});
        stroke({{21, 16}, {18, 19}});
        stroke({{5, 20}, {11, 20}});
    }
    else if (name == "send")
    {
        stroke({{3, 12}, {21, 12}});
        stroke({{14, 5}, {21, 12}, {14, 19}});
    }
    else if (name == "more")
    {
        p.setBrush(color);
        for (int x : {5, 12, 19})
        {
            p.drawEllipse(QPointF(x, 12), 0.8, 0.8);
        }
    }
    else
    {
        stroke({{14, 3}, {5, 3}, {5, 21}, {19, 21}, {19, 8}, {14, 3},
                {14, 8}, {19, 8}});
        stroke({{9, 12}, {15, 12}});
        stroke({{9, 16}, {13, 16}});
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
    installButtonFocusFeedback();
    setFixedSize(36, 36);
    setToolTip(title);
    setAccessibleName(title);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    animation.setDuration(165);
    animation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value)
            {
                progress = value.toReal();
                update();
            });
    rotationAnimation.setDuration(480);
    rotationAnimation.setEasingCurve(QEasingCurve::InOutCubic);
    rotationAnimation.setStartValue(0.0);
    rotationAnimation.setEndValue(360.0);
    connect(&rotationAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value)
            {
                rotation = value.toReal();
                update();
            });
    connect(&rotationAnimation, &QVariantAnimation::finished, this,
            [this]
            {
                rotation = 0;
                update();
                if (turnPending)
                {
                    turnPending = false;
                    rotationAnimation.start();
                }
            });
    connect(this, &QAbstractButton::toggled, this, qOverload<>(&QWidget::update));
}

QSize IconButton::sizeHint() const
{
    return QSize(36, 36);
}

void IconButton::turnGear()
{
    if (glyph != "gear" || !isEnabled())
    {
        return;
    }
    if (rotationAnimation.state() == QAbstractAnimation::Running)
    {
        // Finish the current revolution before the exit feedback; coalesce
        // rapid pointer crossings rather than building a long animation queue.
        turnPending = true;
        return;
    }
    rotationAnimation.start();
}

void IconButton::enterEvent(QEnterEvent *event)
{
    animation.stop();
    animation.setStartValue(progress);
    animation.setEndValue(1.0);
    animation.start();
    turnGear();
    QAbstractButton::enterEvent(event);
}

void IconButton::leaveEvent(QEvent *event)
{
    animation.stop();
    animation.setStartValue(progress);
    animation.setEndValue(0.0);
    animation.start();
    turnGear();
    QAbstractButton::leaveEvent(event);
}

void IconButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const bool onRail = parentWidget() && parentWidget()->objectName() == "rail";
    const QColor foreground = onRail ? QColor("#d7dfc4") : theme.text;
    const QColor active = onRail ? QColor("#edaa50") : theme.accent;
    const bool selected = isChecked() || (hasFocus() && property("keyboardFocus").toBool());
    const qreal amount = selected ? 1.0 : progress;
    const qreal colorAmount = qBound(0.0, amount, 1.0);
    const QColor color = !isEnabled() ? theme.line : QColor(
        qRound(foreground.red() + (active.red() - foreground.red()) * colorAmount),
        qRound(foreground.green() + (active.green() - foreground.green()) * colorAmount),
        qRound(foreground.blue() + (active.blue() - foreground.blue()) * colorAmount));
    const qreal scale = 1.0 + progress * 0.05;
    const qreal shift = isDown() ? 1.5 : -progress;
    const qreal iconSize = glyph == "lightning" || glyph == "port" ? 28.0 : 22.0;
    const qreal extent = qMin(iconSize * scale, qreal(qMin(width(), height()) - 5));
    drawIcon(p, glyph == "pause" && isChecked() ? "play" : glyph,
             QRectF((width() - extent) / 2, (height() - extent) / 2 + shift, extent, extent), color, rotation);
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
    const auto group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const bool emphasized = isEnabled() && (underMouse() || view()->isVisible());
    const QColor color = emphasized ? palette().color(QPalette::Highlight)
                                    : palette().color(group, QPalette::Text);
    const int dividerX = width() - 28;
    const int dividerY = height() / 2;
    p.setPen(palette().color(group, QPalette::Mid));
    p.drawLine(dividerX, dividerY - 5, dividerX, dividerY + 5);

    const qreal x = width() - 14;
    const qreal y = height() / 2.0;
    p.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath chevron;
    chevron.moveTo(x - 4, y - 2);
    chevron.lineTo(x, y + 2);
    chevron.lineTo(x + 4, y - 2);
    p.drawPath(chevron);
}

ProtocolCombo::ProtocolCombo(QWidget *parent) : Combo(parent)
{
    addItems({"串口", "TCP 客户端", "TCP 服务端", "UDP", "SSH"});
    setObjectName("protocolSelector");
}

void Combo::showPopup()
{
    if (!menuPopup)
    {
        QComboBox::showPopup();
        if (auto frame = qobject_cast<QFrame *>(view()->window()))
        {
            frame->setFrameShape(QFrame::NoFrame);
        }
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
    menu->popup(mapToGlobal(QPoint(0, height() + 1)));
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
    setTextMargins(25, 0, 28, 0);
    setMinimumWidth(70);
    setMaximumWidth(260);
    clearButton = new IconButton("close", "清空搜索", this);
    clearButton->setObjectName("clearSearch");
    clearButton->setFixedSize(26, 26);
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
    drawIcon(painter, "search", QRectF(11, (height() - 17) / 2.0, 17, 17), clearButton->theme.muted);
}

ConnectionBar::ConnectionBar(QWidget *parent) : TearTabBar(parent)
{
    middleClickCloses = true;
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
        qBound(128, fontMetrics().horizontalAdvance(tabText(index)) + (tabIcon(index).isNull() ? 80 : 100), 230), 45);
}

QSize ConnectionBar::sizeHint() const
{
    int width = 0;
    for (int i = 0; i < count(); ++i)
    {
        width += tabSizeHint(i).width();
    }
    return QSize(width, 45);
}

QSize ConnectionBar::minimumSizeHint() const
{
    return QSize(60, 45);
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

} // namespace sw
