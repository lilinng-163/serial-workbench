// SPDX-License-Identifier: Apache-2.0

#include "controls.h"
#include "button_motion.h"
#include <QSvgRenderer>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace sw
{
namespace
{
qreal relativeLuminance(const QColor &color)
{
    const auto linear = [](qreal value)
    {
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF())
        + 0.0722 * linear(color.blueF());
}

qreal contrastRatio(const QColor &first, const QColor &second)
{
    const qreal a = relativeLuminance(first);
    const qreal b = relativeLuminance(second);
    return (qMax(a, b) + 0.05) / (qMin(a, b) + 0.05);
}

QColor blendColor(const QColor &from, const QColor &to, qreal amount)
{
    return QColor(qRound(from.red() + (to.red() - from.red()) * amount),
        qRound(from.green() + (to.green() - from.green()) * amount),
        qRound(from.blue() + (to.blue() - from.blue()) * amount));
}

QColor textOn(const QColor &background)
{
    return relativeLuminance(background) > 0.179 ? QColor(Qt::black) : QColor(Qt::white);
}

QColor readableColor(const QColor &color, const QList<QColor> &surfaces, qreal minimum, bool dark)
{
    const auto readable = [&](const QColor &candidate)
    {
        return std::all_of(surfaces.cbegin(), surfaces.cend(), [&](const QColor &surface)
        {
            return contrastRatio(candidate, surface) >= minimum;
        });
    };
    if (readable(color))
    {
        return color;
    }
    const QColor target = dark ? QColor(Qt::white) : QColor(Qt::black);
    int low = 0;
    int high = 255;
    while (low < high)
    {
        const int middle = (low + high) / 2;
        if (readable(blendColor(color, target, middle / 255.0)))
        {
            high = middle;
        }
        else
        {
            low = middle + 1;
        }
    }
    return blendColor(color, target, high / 255.0);
}

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

QColor Theme::parseColor(const QString &text)
{
    const auto value = text.trimmed();
    static const QRegularExpression format("^#(?:[0-9a-fA-F]{3}|[0-9a-fA-F]{6})$");
    return format.match(value).hasMatch() ? QColor(value) : QColor();
}

Theme Theme::make(bool dark, const QColor &color, const QColor &contrast)
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
    if (color.isValid())
    {
        // The main color tints surfaces only. Default accent components keep
        // their own role and are set from the contrast color below.
        const qreal hue = qMax(0.0, color.hslHueF());
        const qreal saturation = color.hslSaturationF() * 0.32;
        const auto tint = [hue, saturation](qreal lightness)
        {
            return QColor::fromHslF(hue, saturation, lightness);
        };
        t.background = tint(dark ? 0.11 : 0.88);
        t.panel = tint(dark ? 0.15 : 0.97);
        t.sidebar = tint(dark ? 0.19 : 0.93);
        t.text = tint(dark ? 0.91 : 0.16);
        t.muted = tint(dark ? 0.70 : 0.39);
        t.line = tint(dark ? 0.33 : 0.73);
        t.edge = tint(dark ? 0.49 : 0.51);
        t.hover = tint(dark ? 0.25 : 0.86);
        t.rail = tint(0.10);
    }
    if (color.isValid() || contrast.isValid())
    {
        // Default theme roles: the contrast color is the accent used by
        // buttons, links, selection, focus, checkboxes, sliders, progress and
        // the active rail icon. The warm default accent is the fallback.
        const QColor fallbackAccent = dark ? QColor("#edaa50") : QColor("#dc8c38");
        t.accent = contrast.isValid() ? QColor(contrast.toRgb()) : fallbackAccent;
        t.accent.setAlpha(255);

        t.railAccent = readableColor(t.accent, {t.rail}, 3.0, true);
        t.hover = blendColor(t.panel, t.accent, dark ? 0.22 : 0.16);
    }
    const QList<QColor> surfaces {t.background, t.panel, t.sidebar, t.hover};
    // Preserve the selected fill, while giving text and small marks enough
    // contrast on every workspace surface (including hovered menu items).
    t.onAccent = textOn(t.accent);
    t.accentText = readableColor(t.accent, surfaces, 4.5, dark);
    t.accentStroke = readableColor(t.accent, surfaces, 3.0, dark);
    t.muted = readableColor(t.muted, surfaces, 4.5, dark);
    t.red = readableColor(t.red, surfaces, 4.5, dark);
    t.amber = readableColor(t.amber, surfaces, 4.5, dark);
    return t;
}

QPalette Theme::palette() const
{
    const QList<QColor> surfaces {background, panel, sidebar, hover};
    const bool distinct = std::all_of(surfaces.cbegin(), surfaces.cend(), [&](const QColor &surface)
    {
        return contrastRatio(accent, surface) >= 1.5;
    });
    const QColor highlight = distinct ? accent : accentStroke;
    QPalette result;
    result.setColor(QPalette::Window, background);
    result.setColor(QPalette::Base, panel);
    result.setColor(QPalette::AlternateBase, sidebar);
    result.setColor(QPalette::Button, panel);
    result.setColor(QPalette::Text, text);
    result.setColor(QPalette::WindowText, text);
    result.setColor(QPalette::ButtonText, text);
    result.setColor(QPalette::Highlight, highlight);
    result.setColor(QPalette::HighlightedText, textOn(highlight));
    result.setColor(QPalette::Link, accentText);
    result.setColor(QPalette::LinkVisited, accentText);
    result.setColor(QPalette::PlaceholderText, muted);
    result.setColor(QPalette::ToolTipBase, panel);
    result.setColor(QPalette::ToolTipText, text);
    result.setColor(QPalette::Light, line);
    result.setColor(QPalette::Midlight, line);
    result.setColor(QPalette::Mid, muted);
    result.setColor(QPalette::Dark, edge);
    result.setColor(QPalette::Shadow, rail);
    for (auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText, QPalette::PlaceholderText})
    {
        result.setColor(QPalette::Disabled, role, muted);
    }
    return result;
}

QScrollArea *scrollPanel(QWidget *content, const QString &name)
{
    auto scroll = new QScrollArea();
    scroll->setObjectName(name);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    scroll->setMinimumSize(0, 0);
    scroll->setWidget(content);
    content->setAutoFillBackground(false);
    return scroll;
}

void makeDialogScrollable(QDialog *dialog, const QSize &minimum)
{
    auto body = qobject_cast<QVBoxLayout *>(dialog->layout());
    Q_ASSERT(body && body->count() > 1);
    const auto initialSize = dialog->testAttribute(Qt::WA_Resized) ? dialog->size() : body->totalSizeHint();
    auto content = new QWidget();
    content->setObjectName("dialogContent");
    auto layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(body->spacing());
    // Keep the final row outside the scroll area and preserve editor stretch.
    while (body->count() > 1)
    {
        const int stretch = body->stretch(0);
        auto item = body->takeAt(0);
        if (auto widget = item->widget())
        {
            layout->addWidget(widget, stretch, item->alignment());
            delete item;
        }
        else if (auto child = item->layout())
        {
            child->setParent(nullptr);
            layout->addLayout(child, stretch);
        }
        else
        {
            layout->addItem(item);
            layout->setStretch(layout->count() - 1, stretch);
        }
    }
    body->insertWidget(0, scrollPanel(content, "dialogContentScroll"), 1);
    dialog->setMinimumSize(minimum);
    dialog->setSizeGripEnabled(true);
    dialog->setWindowFlag(Qt::WindowMaximizeButtonHint);
    dialog->resize(initialSize.expandedTo(minimum));
}

QString Theme::stylesheet() const
{
    installButtonFocusFeedback();
    installButtonMotion(*this);
    const auto colorsPalette = palette();
    QString s = R"(
        QWidget { color: $text; font-family: "$font"; font-size: 14px; font-weight: normal; }
        QMainWindow, QDialog, QWidget#root { background: $bg; }
        QFrame#agentCard { background: $panel; border: 1px solid $line; border-radius: 12px; }
        QDialog#serialControlEditor, QScrollArea#controlEditorScroll, QWidget#controlEditorContent, QWidget#controlEditorViewport { background: $bg; border: none; }
        QWidget#serialControlPanel { background: $bg; border: none; }
        QScrollArea#controlScroll, QWidget#controlViewport, QWidget#controlCanvas { background: $side; border: none; }
        QFrame#controlCanvasFrame { background: $side; border: 1px solid $line; border-radius: 12px; }
        QFrame#controlToolbar { background: $panel; border: 1px solid $line; border-radius: 12px; }
        QFrame#controlToolbar QPushButton { min-height: 24px; padding: 2px 9px; }
        QFrame#controlToolbar QComboBox { min-height: 24px; padding-top: 2px; padding-bottom: 2px; }
        QLabel#controlGroupLabel { color: $muted; font-size: 12px; font-weight: 600; }
        QPushButton#addSerialControl { background: $accent; color: $onaccent; border: 1px solid $edge; font-weight: 600; }
        QPushButton#controlMonitorToggle:checked { background: $hover; border: 1px solid $stroke; }
        QFrame#serialControlCard { background: $panel; border: 1px solid $line; border-radius: 12px; }
        QWidget#controlDragHandle { background: $side; border: none; border-radius: 6px; }
        QLabel#controlCardTitle { font-weight: 600; }
        QLabel#controlKindBadge { color: $muted; background: $panel; border: 1px solid $line; border-radius: 3px; font-size: 11px; padding: 1px 4px; }
        QPushButton#sendControl { background: $hover; border: 1px solid $edge; }
        QPushButton#sendControl:disabled { background: $side; color: $muted; border-color: $line; }
        QToolButton#controlPreviewToggle { color: $muted; }
        QToolButton#controlLayerMenu::menu-indicator { image: none; width: 0px; height: 0px; }
        QDialog#serialMonitorDialog { background: $panel; }
        QLabel#serialMonitorSource { font-weight: 600; }
        QLabel#serialMonitorConnection { background: $side; border: 1px solid $line; border-radius: 4px; padding: 3px 8px; }
        QLabel#serialMonitorLegend, QLabel#serialMonitorStatistics { color: $muted; font-size: 12px; }
        QLabel#serialMonitorMessage { color: $red; }
        QTextEdit#serialMonitorLog { background: $panel; border: 1px solid $line; border-radius: 4px; padding: 4px; }
        QPushButton#serialMonitorSend { background: $accent; color: $onaccent; border: 1px solid $edge; padding: 6px 10px; }
        QPushButton#serialMonitorSend:disabled { background: $side; color: $muted; border-color: $line; }
        QFrame#serialControlCard QPushButton, QFrame#serialControlCard QToolButton { min-height: 22px; padding: 1px 6px; }
        QFrame#serialControlCard QAbstractSpinBox, QFrame#serialControlCard QComboBox { min-height: 22px; padding-top: 2px; padding-bottom: 2px; }
        QSlider { min-height: 24px; background: transparent; }
        QSlider::groove:horizontal { height: 6px; background: $line; border: none; border-radius: 3px; }
        QSlider::sub-page:horizontal { background: $stroke; border: none; border-radius: 3px; }
        QSlider::add-page:horizontal { background: $line; border: none; border-radius: 3px; }
        QSlider::handle:horizontal { width: 12px; margin: -5px 0; background: $panel; border: 2px solid $edge; border-radius: 8px; }
        QSlider::handle:horizontal:hover, QSlider::handle:horizontal:focus { background: $hover; border-color: $stroke; }
        QSlider::handle:horizontal:pressed { background: $accent; border-color: $stroke; }
        QSlider::sub-page:horizontal:disabled { background: $muted; }
        QSlider::handle:horizontal:disabled { background: $side; border-color: $line; }
        QLabel#controlValidation { color: $red; }

        QLabel#agentTitle { font-size: 14px; font-weight: 600; }
        QLabel#agentLogo { background: transparent; border: none; }
        QFrame#sidebar { background: $side; border-right: 1px solid $line; }
        QFrame#rail { background: $rail; border-right: 2px solid $rail; }
        QWidget#workspace, QWidget#page { background: $panel; }
        QWidget#tabHeader { background: $bg; border: none; }
        QWidget#monitorToolbar { background: $side; border-bottom: 1px solid $line; }
        QWidget#sender { background: $side; border-top: 1px solid $line; }
        QLabel { color: $text; background: transparent; border: none; }
        QGroupBox { color: $text; }
        QLabel#muted { color: $muted; font-size: 13px; }
        QLabel#section { font-size: 14px; font-weight: 600; color: $text; padding: 0 0 8px 0; border-bottom: 1px solid $line; }
        QLabel#brand { font-size: 14px; }
        QLabel#connectionState { font-weight: 600; }
        QLabel#themeTitle { font-size: 24px; }
        QLabel#aboutTitle { font-family: "Segoe UI"; font-size: 24px; font-weight: 600; }
        QLabel#aboutVersion { color: $muted; font-size: 14px; }
        QLabel#aboutSection { font-weight: 600; }
        QWidget#pixelBrand { background: $rail; border: 1px solid $edge; border-radius: 12px; }
        QLabel#brandTitle { color: #f6f4e8; font-family: "Segoe UI"; font-size: 23px; font-weight: 600; }
        QLabel#brandSubtitle { color: #d7dfc4; font-size: 13px; }
        QLabel#senderLabel { color: $muted; font-size: 13px; }
        QLabel#connectionHint { color: $muted; padding: 10px 0 0 0; border-top: 1px solid $line; }
        QLineEdit, QComboBox, QAbstractSpinBox { background: $panel; border: 1px solid $line; border-radius: 8px; padding: 6px 10px; min-height: 24px; selection-background-color: $selectbg; selection-color: $selectfg; }
        QLineEdit QToolButton { background: transparent; border: none; padding: 0; margin: 0 4px 3px 0; }
        QLineEdit:hover, QComboBox:hover, QAbstractSpinBox:hover { border-color: $edge; }
        QLineEdit:focus, QComboBox:focus, QAbstractSpinBox:focus { border-color: $stroke; }
        QComboBox#protocolSelector { padding: 8px 28px 8px 10px; }
        QComboBox#protocolSelector:focus { border-color: $stroke; }
        QLineEdit#searchBox { padding: 5px 8px; background: $panel; }
        QLineEdit#searchBox[invalidFilter="true"] { border-color: $red; }
        QToolButton[searchOption="true"] { padding: 0; min-height: 0; border-radius: 4px; font-family: 'Segoe UI'; font-size: 12px; color: $muted; }
        QToolButton[searchOption="true"]:hover { background: $hover; color: $text; }
        QToolButton[searchOption="true"]:checked { background: $hover; color: $ink; border-color: $stroke; }
        QLineEdit#tabTitleEditor, QLineEdit#tabTitleEditor:focus, QLineEdit#tabTitleEditor:hover { background: $panel; color: $text; border: none; border-bottom: 1px solid $edge; border-radius: 0; padding: 0 3px; min-height: 0; }
        QComboBox { padding-right: 28px; combobox-popup: 0; }
        QComboBox::drop-down { border: none; width: 28px; }
        QComboBox::down-arrow { image: none; }
        QComboBox QLineEdit, QComboBox QLineEdit:focus, QComboBox QLineEdit:hover { border: none; padding: 0; min-height: 0; background: transparent; }
        QComboBox QAbstractItemView { background: $panel; color: $text; border: none; selection-background-color: $hover; selection-color: $text; outline: none; padding: 4px; }
        QComboBox QAbstractItemView::item { min-height: 30px; padding: 4px 8px; margin: 2px; border: none; border-radius: 5px; }
        QComboBox QAbstractItemView::item:selected { background: $hover; color: $text; }
        QAbstractSpinBox { padding-right: 26px; }
        QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { width: 24px; border: none; }
        QAbstractSpinBox::up-arrow { image: url($uparrow); width: 10px; height: 6px; }
        QAbstractSpinBox::down-arrow { image: url($downarrow); width: 10px; height: 6px; }
        QPushButton, QToolButton { outline: none; background: transparent; border: 1px solid transparent; border-radius: 8px; padding: 6px 12px; min-height: 24px; }
        QPushButton:hover, QToolButton:hover { color: $ink; }
        QPushButton:checked { color: $ink; font-weight: 600; }
        QPushButton[keyboardFocus="true"]:focus, QToolButton[keyboardFocus="true"]:focus { color: $ink; }
        QPushButton:pressed, QToolButton:pressed { color: $text; }
        QPushButton#primary, QPushButton#releaseHistoryDownload { background: $accent; color: $onaccent; border: 1px solid $edge; padding: 6px 16px; }
        QPushButton#primary:hover, QPushButton#primary[keyboardFocus="true"]:focus, QPushButton#releaseHistoryDownload:hover, QPushButton#releaseHistoryDownload[keyboardFocus="true"]:focus { background: $accent; color: $onaccent; }
        QPushButton#primary:pressed, QPushButton#releaseHistoryDownload:pressed { color: $onaccent; }
        QPushButton#primary:disabled, QPushButton#releaseHistoryDownload:disabled { background: $line; color: $muted; border-color: $edge; }
        QPushButton#outline, QPushButton[variant="outline"] { color: $text; border: 1px solid $line; background: $panel; }
        QPushButton#outline[keyboardFocus="true"]:focus, QPushButton[variant="outline"][keyboardFocus="true"]:focus { border-color: $stroke; color: $ink; }
        QPushButton#outline:hover, QPushButton[variant="outline"]:hover { color: $ink; border-color: $stroke; }
        QPushButton#updateButton { color: $ink; border: none; padding: 1px 5px; min-height: 18px; font-weight: 600; }
        QPushButton#updateButton:hover { color: $text; }
        QPushButton#updateButton:pressed { color: $muted; }
        QProgressBar#updateProgress { border-radius: 6px; border: 1px solid $edge; background: $panel; padding: 0; }
        QProgressBar#updateProgress::chunk { background: $stroke; border-radius: 5px; }
        QProgressBar#updateDialogProgress { border-radius: 6px; border: 1px solid $edge; background: $panel; min-height: 20px; text-align: center; }
        QProgressBar#updateDialogProgress::chunk { background: $stroke; border-radius: 5px; }
        QPushButton:disabled, QToolButton:disabled { color: $muted; }
        QToolButton[unitPreset="true"] { background: $panel; border: 1px solid $line; padding: 2px 8px; min-height: 22px; }
        QToolButton[unitPreset="true"]:hover { border-color: $stroke; background: $hover; }
        QPlainTextEdit, QTextEdit { border: 1px solid $edge; border-radius: 8px; background: $panel; padding: 8px; selection-background-color: $selectbg; selection-color: $selectfg; }
        QPlainTextEdit:focus { border-color: $stroke; }
        QTextEdit#monitor { border: none; padding: 16px 18px; }
        QFontDialog QListView { border-radius: 8px; padding: 4px; color: $text; background: $panel; border: 1px solid $line; selection-background-color: $hover; selection-color: $text; outline: none; }
        QFontDialog QListView::item { padding: 3px 6px; }
        QFontDialog QGroupBox { border-radius: 8px; color: $text; border: 1px solid $line; margin-top: 12px; padding: 10px; }
        QFontDialog QGroupBox::title { color: $text; subcontrol-origin: margin; left: 8px; padding: 0 4px; }
        QTreeWidget { border-radius: 8px; padding: 4px; border: 1px solid $line; background: $panel; alternate-background-color: $side; outline: none; }
        QTreeWidget::item { padding: 6px; min-height: 24px; }
        QTreeWidget::item:selected { background: $hover; color: $text; }
        QHeaderView::section { background: $side; border: none; border-bottom: 1px solid $line; padding: 10px 8px; color: $muted; }
        QTabWidget { background: $bg; }
        QTabWidget::pane { border: none; border-radius: 8px; background: $panel; padding: 6px; }
        QTabWidget[leadingTabSelected="true"]::pane { border-top-left-radius: 0; }
        QTabWidget::tab-bar { left: 0; }
        QTabBar { background: transparent; }
        sw--TearTabBar { qproperty-pageColor: $panel; }
        QTabBar QToolButton { background: $side; border: none; border-radius: 5px; padding: 0; min-height: 0; }
        QTabBar::scroller { width: 48px; }
        QTabBar::tab { background: transparent; padding: 10px 13px; margin: 2px 6px 0; border: none; border-top-left-radius: 6px; border-top-right-radius: 6px; border-bottom-left-radius: 0; border-bottom-right-radius: 0; color: $muted; }
        QTabBar::tab:hover { background: $hover; color: $text; }
        QTabBar::tab:selected { color: $text; background: $panel; }
        QTabBar#connections::tab { padding: 10px 11px; }
        QTabWidget QTabBar::tab { margin-left: 0; margin-right: 12px; }
        QTabWidget#model3dMappingTabs { background: transparent; }
        QTabWidget#model3dMappingTabs::pane { padding: 0; border: none; border-top-left-radius: 0; top: -1px; }
        QTabWidget#model3dMappingTabs QTabBar::tab { margin: 0 3px 0 0; padding: 8px 10px; }
        QMenu, QFrame[roundedPopup="true"] { background: $panel; border: 1px solid $line; border-radius: 10px; padding: 5px; }
        QFrame[roundedPopup="true"] QAbstractItemView { border: none; background: $panel; padding: 2px; }
        QMenu::item { padding: 10px 28px 10px 16px; border: none; border-radius: 6px; }
        QMenu::item:selected { background: $hover; color: $text; }
        QMenu::icon { subcontrol-origin: padding; subcontrol-position: center left; left: 10px; }
        QMenu::separator { height: 1px; background: $line; margin: 5px 8px; }
        QMenu::indicator { width: 18px; height: 18px; margin-left: 8px; subcontrol-origin: padding; subcontrol-position: center left; image: none; background: transparent; }
        QMenu::indicator:checked { image: url($checkmark); background: $accent; border: 1px solid $edge; border-radius: 4px; }
        QCheckBox { spacing: 7px; background: transparent; }
        QCheckBox::indicator { border-radius: 4px; width: 16px; height: 16px; border: 1px solid $edge; background: $panel; }
        QCheckBox::indicator:hover { border-color: $stroke; }
        QCheckBox::indicator:checked { border-color: $text; background: $accent; image: url($checkmark); }
        QCheckBox[animatedCheck="true"]::indicator,
        QCheckBox[animatedCheck="true"]::indicator:checked,
        QCheckBox[animatedCheck="true"]::indicator:indeterminate { border: 1px solid transparent; background: transparent; image: none; }
        QScrollArea { border: none; background: transparent; }
        QScrollBar:vertical { background: $side; width: 11px; margin: 0; }
        QScrollBar:horizontal { background: $side; height: 11px; margin: 0; }
        QScrollBar::handle { background: $line; border: 1px solid $line; border-radius: 5px; min-height: 30px; min-width: 30px; }
        QScrollBar::handle:hover { background: $muted; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        QStatusBar { background: $side; color: $muted; border-top: 1px solid $line; }
        QStatusBar::item { border: none; }
        QToolTip { background: $panel; color: $text; border: 1px solid $line; border-radius: 6px; padding: 6px; }
    )";
    const QMap<QString, QString> colors{{"$font", interfaceFont().family()},
                                        {"$text", text.name()},
                                        {"$muted", muted.name()},
                                        {"$red", red.name()},
                                        {"$side", sidebar.name()},
                                        {"$bg", background.name()},
                                        {"$rail", rail.name()},
                                        {"$panel", panel.name()},
                                        {"$line", line.name()},
                                        {"$edge", edge.name()},
                                        {"$accent", accent.name()},
                                        {"$ink", accentText.name()},
                                        {"$stroke", accentStroke.name()},
                                        {"$selectbg", colorsPalette.color(QPalette::Highlight).name()},
                                        {"$selectfg", colorsPalette.color(QPalette::HighlightedText).name()},
                                        {"$uparrow", dark ? ":/icons/chevron-up-dark.svg" : ":/icons/chevron-up.svg"},
                                        {"$downarrow", dark ? ":/icons/chevron-down-dark.svg" : ":/icons/chevron-down.svg"},
                                        {"$checkmark", onAccent == QColor(Qt::white) ? ":/icons/check-white.svg" : ":/icons/check.svg"},
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
    else if (name == "json")
    {
        QPainterPath braces;
        braces.moveTo(8, 3);
        braces.cubicTo(3, 3, 7, 10, 3, 12);
        braces.cubicTo(7, 14, 3, 21, 8, 21);
        braces.moveTo(16, 3);
        braces.cubicTo(21, 3, 17, 10, 21, 12);
        braces.cubicTo(17, 14, 21, 21, 16, 21);
        p.drawPath(braces);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        for (int x : {9, 12, 15})
        {
            p.drawEllipse(QPointF(x, 12), 0.8, 0.8);
        }
    }
    else if (name == "code" || name == "xml")
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
    else if (name == "control")
    {
        stroke({{3, 5}, {21, 5}});
        stroke({{3, 12}, {21, 12}});
        stroke({{3, 19}, {21, 19}});
        p.setBrush(color);
        p.drawRoundedRect(QRectF(7, 2, 3, 6), 1, 1);
        p.drawRoundedRect(QRectF(15, 9, 3, 6), 1, 1);
        p.drawRoundedRect(QRectF(5, 16, 3, 6), 1, 1);
    }
    else if (name == "cube")
    {
        QPolygonF outline {{12, 2}, {21, 7}, {21, 17}, {12, 22}, {3, 17}, {3, 7}, {12, 2}};
        p.drawPolyline(outline);
        p.drawLine(QPointF(3, 7), QPointF(12, 12));
        p.drawLine(QPointF(21, 7), QPointF(12, 12));
        p.drawLine(QPointF(12, 12), QPointF(12, 22));
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
        QPainterPath wave;
        wave.moveTo(2, 12);
        wave.cubicTo(5, 12, 4, 4, 7, 4);
        wave.cubicTo(11, 4, 10, 20, 14, 20);
        wave.cubicTo(18, 20, 17, 12, 22, 12);
        p.drawPath(wave);
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
    else if (name == "receive")
    {
        // Log page with a bidirectional send/receive arrow.
        stroke({{14, 3}, {5, 3}, {5, 21}, {19, 21}, {19, 8}, {14, 3}, {14, 8}, {19, 8}});
        stroke({{9, 10}, {15, 10}});
        stroke({{9, 13}, {13, 13}});
        stroke({{8, 18}, {16, 18}});
        stroke({{11, 15.5}, {8, 18}, {11, 20.5}});
        stroke({{13, 15.5}, {16, 18}, {13, 20.5}});
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
    const QColor active = onRail ? theme.railAccent : theme.accentText;
    const bool selected = isChecked() || (hasFocus() && property("keyboardFocus").toBool());
    const qreal amount = selected ? 1.0 : progress;
    const qreal colorAmount = qBound(0.0, amount, 1.0);
    const QColor color = !isEnabled() ? theme.line : QColor(
        qRound(foreground.red() + (active.red() - foreground.red()) * colorAmount),
        qRound(foreground.green() + (active.green() - foreground.green()) * colorAmount),
        qRound(foreground.blue() + (active.blue() - foreground.blue()) * colorAmount));
    const qreal scale = 1.0;
    const qreal shift = 0.0;
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
        auto popup = qobject_cast<QFrame *>(view()->window());
        if (popup && popup->windowType() == Qt::Popup)
        {
            roundPopupWindow(popup);
        }
        QComboBox::showPopup();
        if (popup && popup->isVisible())
        {
            // QComboBox's native layout may overwrite the mask during Show.
            roundPopupWindow(popup);
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
