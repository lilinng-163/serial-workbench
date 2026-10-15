// SPDX-License-Identifier: Apache-2.0

#include "controls.h"
#include <cmath>
#include <functional>

namespace sw
{
namespace
{
WaveCurveStyle cleanStyle(WaveCurveStyle style)
{
    style.width = std::isfinite(style.width) ? qBound(0.5, style.width, 8.0) : 1;
    if (style.line < Qt::SolidLine || style.line > Qt::DashDotLine)
    {
        style.line = Qt::SolidLine;
    }
    return style;
}

QJsonObject styleJson(const WaveCurveStyle &style)
{
    return {{"color", style.color.isValid() ? style.color.name() : QString()},
        {"width", style.width}, {"line", int(style.line)}};
}

WaveCurveStyle readStyle(const QJsonObject &value)
{
    WaveCurveStyle style;
    style.color = QColor(value["color"].toString());
    style.width = value["width"].toDouble(1);
    style.line = static_cast<Qt::PenStyle>(value["line"].toInt(Qt::SolidLine));
    return cleanStyle(style);
}

QString shortText(QString text, int limit)
{
    text.remove(QRegularExpression("[\\x00-\\x1f\\x7f]"));
    return text.trimmed().left(limit);
}

QString axisLabel(const QString &title, const QString &unit)
{
    if (unit.isEmpty()) { return title; }
    return title.isEmpty() ? unit : title + " (" + unit + ")";
}

class StyleEditor : public QWidget
{
public:
    QCheckBox *automatic;
    QPushButton *color;
    QDoubleSpinBox *width;
    QComboBox *line;
    QColor selected;
    bool loading = false;
    std::function<void()> edited;

    StyleEditor(const QColor &fallback, QWidget *parent = nullptr) : QWidget(parent), selected(fallback)
    {
        auto form = new QFormLayout(this);
        form->setContentsMargins(0, 0, 0, 0);
        form->setVerticalSpacing(12);
        form->setFormAlignment(Qt::AlignTop);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto row = new QHBoxLayout();
        automatic = new QCheckBox("自动配色");
        automatic->setObjectName("waveAutomaticColor");
        color = new QPushButton();
        color->setObjectName("waveCurveColor");
        color->setProperty("variant", "outline");
        row->addWidget(automatic);
        row->addWidget(color, 1);
        form->addRow("颜色", row);
        width = new QDoubleSpinBox();
        width->setObjectName("waveCurveWidth");
        width->setRange(0.5, 8);
        width->setSingleStep(0.5);
        width->setDecimals(1);
        width->setSuffix(" 像素");
        form->addRow("线宽", width);
        line = new Combo();
        line->setObjectName("waveCurveLine");
        line->addItem("实线", int(Qt::SolidLine));
        line->addItem("虚线", int(Qt::DashLine));
        line->addItem("点线", int(Qt::DotLine));
        line->addItem("点划线", int(Qt::DashDotLine));
        form->addRow("线型", line);
        connect(automatic, &QCheckBox::toggled, color, &QPushButton::setDisabled);
        connect(automatic, &QCheckBox::toggled, this, [this] { notifyEdit(); });
        connect(width, &QDoubleSpinBox::valueChanged, this, [this] { notifyEdit(); });
        connect(line, &QComboBox::currentIndexChanged, this, [this] { notifyEdit(); });
        connect(color, &QPushButton::clicked, this, [this]
        {
            const auto chosen = QColorDialog::getColor(selected, this, "选择曲线颜色", QColorDialog::DontUseNativeDialog);
            if (!chosen.isValid()) { return; }
            selected = chosen;
            refreshColor();
            notifyEdit();
        });
    }

    void notifyEdit()
    {
        if (!loading && edited) { edited(); }
    }

    void refreshColor()
    {
        QPixmap sample(32, 16);
        sample.fill(selected);
        color->setIcon(QIcon(sample));
        color->setText(selected.name().toUpper());
    }

    void setValue(const WaveCurveStyle &style)
    {
        loading = true;
        if (style.color.isValid()) { selected = style.color; }
        automatic->setChecked(!style.color.isValid());
        color->setDisabled(automatic->isChecked());
        width->setValue(style.width);
        line->setCurrentIndex(line->findData(int(style.line)));
        refreshColor();
        loading = false;
    }

    WaveCurveStyle value() const
    {
        return {automatic->isChecked() ? QColor() : selected, width->value(),
            static_cast<Qt::PenStyle>(line->currentData().toInt())};
    }
};

QWidget *unitEditor(QLineEdit **field, const QString &name)
{
    auto widget = new QWidget();
    auto body = new QVBoxLayout(widget);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(6);
    auto row = new QHBoxLayout();
    auto edit = new QLineEdit();
    edit->setObjectName(name);
    edit->setMaxLength(24);
    edit->setPlaceholderText("可留空，也可输入自定义单位");
    *field = edit;
    row->addWidget(edit, 1);
    auto picker = new QToolButton();
    picker->setText("常用单位");
    picker->setPopupMode(QToolButton::InstantPopup);
    auto menu = new QMenu(picker);
    const QList<QPair<QString, QStringList>> groups {
        {"时间 / 频率", {"ns", "µs", "ms", "s", "min", "Hz", "kHz", "MHz"}},
        {"电学", {"µV", "mV", "V", "µA", "mA", "A", "Ω", "kΩ", "MΩ", "pF", "nF", "µF", "mH", "W"}},
        {"温度 / 角度", {"°C", "°F", "K", "°", "rad", "rpm"}},
        {"其他", {"%", "m", "mm", "m/s", "m/s²", "g", "N", "Pa", "kPa"}}
    };
    for (const auto &group : groups)
    {
        auto section = menu->addMenu(group.first);
        for (const auto &unit : group.second)
        {
            auto action = section->addAction(unit);
            QObject::connect(action, &QAction::triggered, edit, [edit, unit] { edit->setText(unit); });
        }
    }
    auto empty = menu->addAction("无单位");
    QObject::connect(empty, &QAction::triggered, edit, &QLineEdit::clear);
    picker->setMenu(menu);
    row->addWidget(picker);
    body->addLayout(row);
    auto quick = new QHBoxLayout();
    quick->setSpacing(4);
    for (const auto &unit : QStringList {"µs", "ms", "°C", "Ω", "µA", "°", "m/s²"})
    {
        auto button = new QToolButton();
        button->setText(unit);
        button->setObjectName(name + "Preset");
        button->setProperty("unitPreset", true);
        button->setToolTip("使用单位 " + unit);
        QObject::connect(button, &QToolButton::clicked, edit, [edit, unit] { edit->setText(unit); });
        quick->addWidget(button);
    }
    quick->addStretch();
    body->addLayout(quick);
    return widget;
}

class SampleIntervalBox : public QDoubleSpinBox
{
public:
    QString textFromValue(double value) const override
    {
        return locale().toString(value, 'g', 10);
    }
};
}

WaveCurveStyle WavePlot::curveStyle(const QString &name) const
{
    return curveStyles.value(name, defaultStyle);
}

void WavePlot::setCurveStyle(const QString &name, const WaveCurveStyle &style)
{
    curveStyles[name] = cleanStyle(style);
    updateLegend();
    invalidatePlot();
    emit presentationChanged();
}

void WavePlot::resetCurveStyle(const QString &name)
{
    curveStyles.remove(name);
    updateLegend();
    invalidatePlot();
    emit presentationChanged();
}

QString WavePlot::xAxisLabel() const { return axisLabel(xTitle, xUnit); }
QString WavePlot::yAxisLabel() const { return axisLabel(yTitle, yUnit); }

QString WavePlot::curveLabel(const QString &name) const
{
    if (boundChannel && boundChannel->config.isWaveformDemo())
    {
        const QMap<QString, QString> names {
            {"sine", "正弦波"}, {"square", "方波"}, {"triangle", "三角波"}, {"sawtooth", "锯齿波"}
        };
        return names.value(name, name);
    }
    return name;
}

QJsonObject WavePlot::presentationSettings() const
{
    QJsonObject curves;
    for (auto style = curveStyles.cbegin(); style != curveStyles.cend(); ++style)
    {
        curves[style.key()] = styleJson(style.value());
    }
    QStringList hidden = hiddenCurves.values();
    hidden.sort();
    return {{"defaults", styleJson(defaultStyle)}, {"curves", curves},
        {"hidden", QJsonArray::fromStringList(hidden)}, {"x_title", xTitle}, {"y_title", yTitle},
        {"x_unit", xUnit}, {"y_unit", yUnit}, {"x_increment", xIncrement}};
}

void WavePlot::restorePresentation(const QJsonObject &settings)
{
    defaultStyle = readStyle(settings["defaults"].toObject());
    curveStyles.clear();
    const auto curves = settings["curves"].toObject();
    for (auto style = curves.begin(); style != curves.end(); ++style)
    {
        curveStyles[style.key()] = readStyle(style.value().toObject());
    }
    hiddenCurves.clear();
    for (const auto &name : settings["hidden"].toArray()) { hiddenCurves.insert(name.toString()); }
    xTitle = shortText(settings["x_title"].toString("采样序号"), 64);
    yTitle = shortText(settings["y_title"].toString(), 64);
    xUnit = shortText(settings["x_unit"].toString(), 24);
    yUnit = shortText(settings["y_unit"].toString(), 24);
    xIncrement = settings["x_increment"].toDouble(1);
    if (!std::isfinite(xIncrement) || xIncrement < 1e-9 || xIncrement > 1e9) { xIncrement = 1; }
    rangeDirty = true;
    dragging = false;
    updateLegend();
    invalidatePlot();
}

void WavePlot::curveMenu(const QString &name, const QPoint &position)
{
    QMenu menu(this);
    menu.setObjectName("waveCurveMenu");
    auto settings = menu.addAction("曲线设置…");
    settings->setObjectName("editWaveCurve");
    connect(settings, &QAction::triggered, this, [this, name] { openCurveSettings(name); });
    auto visible = menu.addAction("显示曲线");
    visible->setCheckable(true);
    visible->setChecked(isCurveVisible(name));
    connect(visible, &QAction::toggled, this, [this, name](bool enabled) { setCurveVisible(name, enabled); });
    auto reset = menu.addAction("恢复默认样式");
    reset->setObjectName("resetWaveCurve");
    connect(reset, &QAction::triggered, this, [this, name] { resetCurveStyle(name); });
    menu.exec(position);
}

void WavePlot::openCurveSettings(const QString &name)
{
    QDialog dialog(this);
    dialog.setObjectName("waveCurveDialog");
    dialog.setWindowTitle("曲线设置 · " + curveLabel(name));
    dialog.setMinimumWidth(400);
    auto body = new QVBoxLayout(&dialog);
    body->setContentsMargins(20, 18, 20, 18);
    body->setSpacing(14);
    auto title = new QLabel("当前通道：" + curveLabel(name));
    title->setObjectName("waveCurveTitle");
    title->setWordWrap(true);
    body->addWidget(title);
    auto inherit = new QCheckBox("使用默认样式");
    inherit->setObjectName("waveInheritStyle");
    inherit->setChecked(!curveStyles.contains(name));
    body->addWidget(inherit);
    auto editor = new StyleEditor(curveColor(name));
    editor->setValue(curveStyle(name));
    body->addWidget(editor);
    auto hint = new QLabel("直接修改任一选项，即可为当前通道单独保存样式。");
    hint->setWordWrap(true);
    body->addWidget(hint);
    editor->edited = [inherit] { inherit->setChecked(false); };
    connect(inherit, &QCheckBox::toggled, editor, [this, editor](bool checked)
    {
        if (checked) { editor->setValue(defaultStyle); }
    });
    body->addStretch(1);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults);
    buttons->button(QDialogButtonBox::Save)->setText("保存");
    buttons->button(QDialogButtonBox::Save)->setObjectName("primary");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    buttons->button(QDialogButtonBox::RestoreDefaults)->setText("恢复默认样式");
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, &dialog, [=, this]
    {
        inherit->setChecked(true);
        editor->setValue(defaultStyle);
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    body->addWidget(buttons);
    if (!curveDialogGeometry.isEmpty()) { dialog.restoreGeometry(curveDialogGeometry); }
    const auto result = dialog.exec();
    curveDialogGeometry = dialog.saveGeometry();
    if (result != QDialog::Accepted) { return; }
    if (inherit->isChecked()) { resetCurveStyle(name); }
    else { setCurveStyle(name, editor->value()); }
}

void WavePlot::openPlotSettings()
{
    QDialog dialog(this);
    dialog.setObjectName("waveSettingsDialog");
    dialog.setWindowTitle("波形设置");
    dialog.setMinimumWidth(560);
    auto body = new QVBoxLayout(&dialog);
    body->setContentsMargins(20, 18, 20, 18);
    body->setSpacing(14);
    auto tabs = new QTabWidget();
    body->addWidget(tabs);
    auto axes = new QWidget();
    auto form = new QFormLayout(axes);
    form->setContentsMargins(14, 16, 14, 16);
    form->setVerticalSpacing(12);
    auto xName = new QLineEdit(xTitle);
    xName->setObjectName("waveXTitle");
    xName->setMaxLength(64);
    auto yName = new QLineEdit(yTitle);
    yName->setObjectName("waveYTitle");
    yName->setMaxLength(64);
    QLineEdit *xUnits;
    QLineEdit *yUnits;
    form->addRow("X 轴名称", xName);
    form->addRow("X 轴单位", unitEditor(&xUnits, "waveXUnit"));
    auto increment = new SampleIntervalBox();
    increment->setObjectName("waveXIncrement");
    increment->setDecimals(9);
    increment->setRange(1e-9, 1e9);
    increment->setValue(xIncrement);
    increment->setSingleStep(0.001);
    form->addRow("每采样间隔", increment);
    auto note = new QLabel("X 刻度 = 采样序号 × 间隔。按时间显示时，请填写实际采样周期。\nY 单位只用于显示，请使用与原始数值一致的单位。");
    note->setWordWrap(true);
    form->addRow(note);
    form->addRow("Y 轴名称", yName);
    form->addRow("Y 轴单位", unitEditor(&yUnits, "waveYUnit"));
    xUnits->setText(xUnit);
    yUnits->setText(yUnit);
    tabs->addTab(axes, "坐标轴与单位");
    auto defaults = new QWidget();
    auto defaultBody = new QVBoxLayout(defaults);
    defaultBody->setContentsMargins(14, 16, 14, 16);
    defaultBody->setSpacing(14);
    auto explanation = new QLabel("所有未单独设置的通道使用以下样式。\n在图例中的通道名称上右键，可单独设置颜色、线宽和线型。");
    explanation->setWordWrap(true);
    defaultBody->addWidget(explanation);
    auto editor = new StyleEditor(theme.accent);
    editor->setValue(defaultStyle);
    defaultBody->addWidget(editor);
    defaultBody->addStretch();
    tabs->addTab(defaults, "默认曲线样式");
    tabs->setCurrentIndex(plotSettingsTab);
    bool resetAll = false;
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults);
    buttons->button(QDialogButtonBox::Save)->setText("保存");
    buttons->button(QDialogButtonBox::Save)->setObjectName("primary");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    auto reset = buttons->button(QDialogButtonBox::RestoreDefaults);
    reset->setText("全部恢复默认");
    reset->setToolTip("保存后恢复默认坐标轴、曲线样式和通道可见性");
    connect(reset, &QPushButton::clicked, &dialog, [&, editor]
    {
        resetAll = true;
        xName->setText("采样序号");
        yName->clear();
        xUnits->clear();
        yUnits->clear();
        increment->setValue(1);
        editor->setValue({});
        reset->setText("已恢复默认 · 保存后生效");
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    body->addWidget(buttons);
    if (!plotDialogGeometry.isEmpty()) { dialog.restoreGeometry(plotDialogGeometry); }
    const auto result = dialog.exec();
    plotDialogGeometry = dialog.saveGeometry();
    plotSettingsTab = tabs->currentIndex();
    if (result != QDialog::Accepted) { return; }
    auto settings = resetAll ? QJsonObject() : presentationSettings();
    settings["defaults"] = styleJson(editor->value());
    settings["x_title"] = xName->text();
    settings["y_title"] = yName->text();
    settings["x_unit"] = xUnits->text();
    settings["y_unit"] = yUnits->text();
    settings["x_increment"] = increment->value();
    restorePresentation(settings);
    emit presentationChanged();
}
} // namespace sw
