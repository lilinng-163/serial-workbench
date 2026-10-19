// SPDX-License-Identifier: Apache-2.0

#include "window.h"
#include "window_chrome.h"
#include "instrument_config.h"
#include "serial_control_panel.h"
#include "json_tools.h"
#include "ssh_profiles.h"
#include "parameter_grip.h"
#include "agent_config.h"
#include "agent_panel.h"
#include "file_transfer_dialog.h"
#include "update_dialog.h"
#include "release_history_panel.h"
#include "ssh_dialogs.h"
#include <QFileInfo>
#include <QProcess>
#include <QProgressBar>
#include <QTextCodec>
#include <QGuiApplication>
#include <QScreen>
#include <numeric>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace sw
{
static QString instrumentIconName(const QString &kind)
{
    if (kind == "waveform")
    {
        return "wave";
    }
    if (kind == "control") { return "control"; }
    if (kind == "json" || kind == "xml")
    {
        return kind;
    }
    return "widgets";
}

static QLabel *label(const QString &text, const QString &name = {})
{
    auto result = new QLabel(text);
    result->setObjectName(name);
    return result;
}

static QHBoxLayout *horizontal(QWidget *parent = nullptr)
{
    auto layout = new QHBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    return layout;
}

static Combo *combo(const QStringList &items, const QString &selected = {}, bool editable = false)
{
    auto control = new Combo();
    control->addItems(items);
    control->setEditable(editable);
    if (!selected.isEmpty())
    {
        control->setCurrentText(selected);
    }
    if (control->lineEdit())
    {
        control->lineEdit()->setTextMargins(0, 0, 0, 0);
    }
    return control;
}

static QPushButton *button(const QString &text, const QString &name = {})
{
    auto result = new QPushButton(text);
    result->setObjectName(name);
    result->setProperty("variant", name);
    result->setCursor(Qt::PointingHandCursor);
    return result;
}

static QString prettyJsonText(const QString &raw)
{
    const auto document = QJsonDocument::fromJson(raw.toUtf8());
    if (document.isObject()) { return QString::fromUtf8(QJsonDocument(document.object()).toJson(QJsonDocument::Indented)); }
    if (document.isArray()) { return QString::fromUtf8(QJsonDocument(document.array()).toJson(QJsonDocument::Indented)); }
    return raw;
}

static QString mcpAuditDetail(const QJsonObject &entry)
{
    QString text;
    text += QString("调用 #%1 · %2\n").arg(entry["seq"].toInteger()).arg(entry["tool"].toString());
    text += QString("时间：%1\n客户端：%2\n结果：%3\n耗时：%4 ms\n")
        .arg(entry["time"].toString(), entry["client"].toString(),
             entry["status"].toString() == "ok" ? QString("成功") : QString("失败"))
        .arg(entry["duration_ms"].toInteger());
    const auto summary = entry["summary"].toString();
    if (!summary.isEmpty()) { text += "摘要：" + summary + "\n"; }
    const auto arguments = entry["arguments"].toObject();
    text += "\n参数：\n" + QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Indented));
    if (!entry["error"].toString().isEmpty())
    {
        text += "\n错误：\n" + entry["error"].toString() + "\n";
    }
    else if (!entry["result"].toString().isEmpty())
    {
        text += "\n返回：\n" + prettyJsonText(entry["result"].toString()) + "\n";
    }
    return text;
}

class TabTitleEditor : public QLineEdit
{
public:
    using QLineEdit::QLineEdit;
    std::function<void(bool)> onFinish;
    bool finished = false;

    void finish(bool accept)
    {
        if (finished)
        {
            return;
        }
        finished = true;
        if (onFinish)
        {
            onFinish(accept);
        }
        deleteLater();
    }

    void focusOutEvent(QFocusEvent *event) override
    {
        QLineEdit::focusOutEvent(event);
        finish(true);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape)
        {
            finish(false);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        {
            finish(true);
            event->accept();
            return;
        }
        QLineEdit::keyPressEvent(event);
    }
};

static QWidget *tabCloseSlot(IconButton *close)
{
    auto slot = new QWidget();
    slot->setFixedSize(close->width() + 8, close->height());
    auto layout = horizontal(slot);
    layout->setContentsMargins(0, 0, 8, 0);
    layout->addWidget(close);
    return slot;
}

static Pane *tabPane(ConnectionBar *bar, int index)
{
    return index < 0 ? nullptr : qobject_cast<Pane *>(bar->tabData(index).value<QObject *>());
}

static int paneIndex(ConnectionBar *bar, Pane *pane)
{
    for (int i = 0; i < bar->count(); ++i)
    {
        if (tabPane(bar, i) == pane)
        {
            return i;
        }
    }
    return -1;
}

static QString usbIdentity(const QJsonObject &metadata)
{
    QStringList identifiers;
    for (const auto &key : {"vendor_id", "product_id"})
    {
        if (!metadata[key].isDouble())
        {
            continue;
        }
        const auto value = QString::number(metadata[key].toInt(), 16).rightJustified(4, '0').toUpper();
        identifiers.append(QString(key == QString("vendor_id") ? "VID %1" : "PID %1").arg(value));
    }
    return identifiers.join(" · ");
}

static QString serialNumber(const QJsonObject &metadata)
{
    return metadata["serial_number"].toString(metadata["hwid"].toString());
}

static QString portDescription(const QJsonObject &metadata, bool includeLocation = false)
{
    QStringList lines;
    if (!metadata["description"].toString().isEmpty())
    {
        lines.append(metadata["description"].toString());
    }
    if (!metadata["manufacturer"].toString().isEmpty())
    {
        lines.append("制造商：" + metadata["manufacturer"].toString());
    }
    const auto usb = usbIdentity(metadata);
    if (!usb.isEmpty())
    {
        lines.append(usb);
    }
    const auto serial = serialNumber(metadata);
    if (!serial.isEmpty())
    {
        lines.append("序列号：" + serial);
    }
    if (includeLocation && !metadata["system_location"].toString().isEmpty())
    {
        lines.append("设备路径：" + metadata["system_location"].toString());
    }
    return lines.join('\n');
}

class PortDelegate : public QStyledItemDelegate
{
public:
    explicit PortDelegate(Window *owner) : QStyledItemDelegate(owner), window(owner) {}
    Window *window;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const bool device = !index.data(Qt::UserRole + 2).toJsonObject().isEmpty();
        return QSize(430, option.fontMetrics.height() * (device ? 2 : 1) + 18);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const auto metadata = index.data(Qt::UserRole + 2).toJsonObject();
        if (metadata.isEmpty())
        {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        QStyleOptionViewItem background(option);
        initStyleOption(&background, index);
        background.text.clear();
        auto style = option.widget ? option.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &background, painter, option.widget);

        QString title = metadata["port"].toString();
        if (!metadata["description"].toString().isEmpty())
        {
            title += " · " + metadata["description"].toString();
        }
        QStringList details;
        if (!metadata["manufacturer"].toString().isEmpty())
        {
            details.append(metadata["manufacturer"].toString());
        }
        if (!usbIdentity(metadata).isEmpty())
        {
            details.append(usbIdentity(metadata));
        }
        if (!serialNumber(metadata).isEmpty())
        {
            details.append("SN " + serialNumber(metadata));
        }
        const auto area = option.rect.adjusted(10, 7, -10, -7);
        const int lineHeight = option.fontMetrics.height();
        painter->save();
        painter->setClipRect(option.rect);
        painter->setFont(option.font);
        painter->setPen(window->theme.text);
        painter->drawText(area.x(), area.y() + option.fontMetrics.ascent(),
            option.fontMetrics.elidedText(title, Qt::ElideRight, area.width()));
        painter->setPen(window->theme.muted);
        painter->drawText(area.x(), area.y() + lineHeight + option.fontMetrics.ascent(),
            option.fontMetrics.elidedText(details.isEmpty() ? "串口设备" : details.join(" · "), Qt::ElideRight, area.width()));
        painter->restore();
    }
};

Instrument::Instrument(const QString &type, QWidget *parent) : QWidget(parent), kind(type == "values" ? "waveform" : type == "structured" ? "json" : type)
{
    setObjectName("page");
    setAttribute(Qt::WA_StyledBackground);
    auto body = new QVBoxLayout(this);
    body->setContentsMargins(16, 12, 16, 8);
    auto sourceRow = horizontal();
    sourceRow->addWidget(label(kind == "control" ? "控制目标" : "数据来源", "muted"));
    source = new Combo();
    source->setObjectName("instrumentSource");
    source->setMinimumWidth(180);
    source->setMaximumWidth(360);
    sourceRow->addWidget(source, 1);
    sourceRow->addStretch();
    body->addLayout(sourceRow);
    if (kind == "control")
    {
        controls = new SerialControlPanel(this);
        body->addWidget(controls, 1);
        info = controls->status;
        return;
    }
    auto tools = horizontal();
    freeze = new IconButton("pause", "暂停显示，后台继续接收");
    freeze->setObjectName("pauseInstrumentDisplay");
    freeze->setCheckable(true);
    connect(freeze, &QAbstractButton::toggled, this, [this](bool paused)
    {
        freeze->setToolTip(paused ? "继续显示，后台仍在接收" : "暂停显示，后台继续接收");
        freeze->setAccessibleName(freeze->toolTip());
    });
    tools->addWidget(freeze);
    auto clear = new IconButton("trash", "清空当前页面，保留共享历史");
    clear->setObjectName("clearInstrumentDisplay");
    tools->addWidget(clear);
    connect(clear, &QAbstractButton::clicked, this, &Instrument::clearDisplay);
    tools->addStretch();
    body->addLayout(tools);
    if (kind == "waveform")
    {
        points = new QSpinBox();
        points->setRange(100, 5000);
        points->setValue(1000);
        points->setToolTip("保留的采样上限；滚轮调整可见范围");
        points->setFixedWidth(112);
        auto fit = button("自动缩放");
        auto waveSettings = button("波形设置");
        waveSettings->setObjectName("waveSettings");
        auto help = button("数据格式");
        waveFollow = new QCheckBox("跟随最新");
        waveFollow->setChecked(true);
        waveFollow->setToolTip("保留当前横轴跨度和纵轴范围，平移到最新采样；自动缩放可恢复完整范围");
        tools->addWidget(waveFollow);
        tools->addWidget(points);
        tools->addWidget(fit);
        tools->addWidget(waveSettings);
        tools->addWidget(help);
        plot = new WavePlot();
        connect(waveSettings, &QPushButton::clicked, plot, &WavePlot::openPlotSettings);
        waveXAxis = new QCheckBox("X 轴");
        waveYAxis = new QCheckBox("Y 轴");
        waveXAxis->setObjectName("waveXAxis");
        waveYAxis->setObjectName("waveYAxis");
        waveXAxis->setChecked(true);
        waveYAxis->setChecked(true);
        waveXAxis->setToolTip("显示横轴刻度与采样序号；隐藏后仍可缩放和拖动");
        waveYAxis->setToolTip("显示纵轴数值刻度；隐藏后仍可缩放和拖动");
        sourceRow->addWidget(waveXAxis);
        sourceRow->addWidget(waveYAxis);
        connect(waveXAxis, &QCheckBox::toggled, plot, &WavePlot::setXAxisVisible);
        connect(waveYAxis, &QCheckBox::toggled, plot, &WavePlot::setYAxisVisible);
        valuesToggle = button("实时数值");
        valuesToggle->setObjectName("waveValuesToggle");
        valuesToggle->setCheckable(true);
        valuesToggle->setToolTip("显示或隐藏波形页内的实时数值面板");
        tools->insertWidget(1, valuesToggle);
        valuesPanel = new QFrame();
        valuesPanel->setObjectName("waveValuesPanel");
        valuesPanel->setMinimumWidth(260);
        auto panelBody = new QVBoxLayout(valuesPanel);
        panelBody->setContentsMargins(10, 8, 6, 6);
        auto heading = horizontal();
        heading->addWidget(label("实时数值"));
        heading->addStretch();
        auto close = new IconButton("close", "隐藏实时数值面板");
        heading->addWidget(close);
        panelBody->addLayout(heading);
        table = new QTreeWidget();
        table->setObjectName("waveValues");
        table->setRootIsDecorated(false);
        table->setUniformRowHeights(true);
        table->setAlternatingRowColors(true);
        table->setHeaderLabels({"变量", "当前值", "来源", "采样序号"});
        table->setColumnWidth(0, 115);
        table->setColumnWidth(1, 135);
        table->setColumnWidth(2, 80);
        table->setColumnHidden(2, true);
        table->setColumnHidden(3, true);
        table->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        panelBody->addWidget(table, 1);
        waveSplit = new Splitter(Qt::Horizontal);
        waveSplit->setObjectName("waveValuesSplitter");
        waveSplit->addWidget(plot);
        waveSplit->addWidget(valuesPanel);
        waveSplit->setStretchFactor(0, 3);
        waveSplit->setStretchFactor(1, 1);
        valuesPanel->hide();
        body->addWidget(waveSplit, 1);
        connect(valuesToggle, &QPushButton::toggled, this, [this](bool visible)
        {
            if (!visible) { valuesSizes = waveSplit->sizes(); }
            valuesPanel->setVisible(visible);
            if (visible)
            {
                waveSplit->setSizes(valuesSizes);
                valuesSnapshot = {};
                if (!freeze->isChecked()) { refreshValues(waveChannel); }
            }
        });
        connect(close, &QAbstractButton::clicked, valuesToggle, &QPushButton::click);
        if (type == "values") { valuesToggle->setChecked(true); }
        connect(waveFollow, &QCheckBox::toggled, plot, &WavePlot::setFollowLatest);
        connect(plot, &WavePlot::followLatestChanged, waveFollow, &QCheckBox::setChecked);
        frameTimer.setTimerType(Qt::PreciseTimer);
        frameTimer.setInterval(16);
        connect(&frameTimer, &QTimer::timeout, this, [this]
        {
            if (isVisible() && !window()->isMinimized()) { tick(waveChannel); }
        });
        connect(freeze, &QAbstractButton::toggled, this, [this](bool frozen)
        {
            if (frozen) { frameTimer.stop(); }
            else if (isVisible()) { frameTimer.start(); tick(waveChannel); }
        });
        connect(points, &QSpinBox::valueChanged, this, [this] { tick(waveChannel); });
        connect(fit, &QPushButton::clicked, this, [this]
        {
            plot->resetView();
        });
        connect(help, &QPushButton::clicked, this, [this]
        {
            if (helpWindow)
            {
                helpWindow->waveHelp();
            }
        });
    }
    else
    {
        table = new QTreeWidget();
        table->setRootIsDecorated(false);
        table->setAlternatingRowColors(true);
        table->setUniformRowHeights(true);
        table->header()->setStretchLastSection(true);
        filter = new FilterBox();
        filter->setPlaceholderText("筛选解析记录");
        sourceRow->addWidget(filter);
        connect(filter, &FilterBox::criteriaChanged, this, [this]
        {
            QTreeWidgetItem *firstVisible = nullptr;
            for (int index = 0; index < table->topLevelItemCount(); ++index)
            {
                auto item = table->topLevelItem(index);
                item->setHidden(!filter->criteria().matches(item->data(0, Qt::UserRole).toJsonObject()["text"].toString()));
                if (!item->isHidden() && !firstVisible) { firstVisible = item; }
            }
            if (!table->currentItem() || table->currentItem()->isHidden())
            {
                table->setCurrentItem(firstVisible);
            }
        });
        if (kind == "json")
        {
            auto assistant = button("JSON 助手");
            assistant->setObjectName("jsonAssistant");
            tools->addWidget(assistant);
            connect(assistant, &QPushButton::clicked, this, [this]
            {
                const auto item = table->currentItem();
                const auto text = item ? item->data(0, Qt::UserRole).toJsonObject()["text"].toString() : QString();
                showJsonAssistant(this, text);
            });
        }
        table->setHeaderLabels({"时间", "格式", "来源", "内容"});
        table->setColumnWidth(0, 130);
        table->setColumnWidth(1, 80);
        table->setColumnWidth(2, 150);
        detail = new QPlainTextEdit();
        detail->setReadOnly(true);
        auto split = new Splitter(Qt::Vertical);
        split->addWidget(table);
        split->addWidget(detail);
        split->setSizes({360, 200});
        body->addWidget(split, 1);
        connect(table, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *current)
        {
            detail->setPlainText(current ? current->data(0, Qt::UserRole).toJsonObject()["pretty"].toString() : QString());
        });
    }
    info = label("等待设备数据", "muted");
    body->addWidget(info);
}

void Instrument::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (plot && !freeze->isChecked()) { frameTimer.start(); }
}

void Instrument::hideEvent(QHideEvent *event)
{
    frameTimer.stop();
    QWidget::hideEvent(event);
}

QJsonObject Instrument::settings() const
{
    if (controls) { return {{"kind", kind}, {"serial_controls", controls->settings()}}; }
    QJsonObject result {{"kind", kind}, {"frozen", freeze->isChecked()}};
    if (filter) { result["search_filter"] = filter->settings(); }
    if (points)
    {
        result["points"] = points->value();
        result["x_axis"] = waveXAxis->isChecked();
        result["y_axis"] = waveYAxis->isChecked();
        result["wave_presentation"] = plot->presentationSettings();
    }
    if (valuesToggle)
    {
        result["show_values"] = valuesToggle->isChecked();
        const auto sizes = valuesToggle->isChecked() ? waveSplit->sizes() : valuesSizes;
        result["values_sizes"] = QJsonArray {sizes.value(0, 760), sizes.value(1, 320)};
    }
    return result;
}

void Instrument::tick(Channel *channel)
{
    if (controls) { controls->setChannel(channel); return; }
    waveChannel = channel;
    if (!channel || freeze->isChecked())
    {
        return;
    }
    if (plot)
    {
        plot->updateChannel(channel, points->value());
        qint64 count = 0;
        int rejected = 0;
        for (const auto &source : channel->sources)
        {
            count += source->count;
            rejected += source->rejected;
        }
        if (valuesToggle->isChecked()) { refreshValues(channel); }
        info->setText(QString("%1 个采样    %2 条曲线    格式错误 %3 帧").arg(count).arg(plot->series.size()).arg(rejected));
    }
    else
    {
        const auto sources = channel->structured(0, 100, cursors)["sources"].toObject();
        for (auto source = sources.begin(); source != sources.end(); ++source)
        {
            const auto batch = source.value().toObject();
            cursors[source.key()] = batch["next_cursor"];
            for (const auto &value : batch["records"].toArray())
            {
                const auto record = value.toObject();
                if (record["format"].toString() != kind)
                {
                    continue;
                }
                auto item = new QTreeWidgetItem(table, {record["time"].toString().mid(11, 12), record["format"].toString().toUpper(), source.key(), record["text"].toString().left(200)});
                item->setData(0, Qt::UserRole, record);
                item->setHidden(!filter->criteria().matches(record["text"].toString()));
                if (!item->isHidden()) { table->setCurrentItem(item); }
                while (table->topLevelItemCount() > 200)
                {
                    delete table->takeTopLevelItem(0);
                }
            }
        }
        info->setText(QString("%1 条解析记录 · 原始报文仍保留在收发页").arg(table->topLevelItemCount()));
    }
}

class SendFooter : public QWidget
{
public:
    QBoxLayout *body = new QBoxLayout(QBoxLayout::LeftToRight, this);

    SendFooter()
    {
        body->setContentsMargins(0, 0, 0, 0);
        body->setSpacing(8);
    }

    QSize minimumSizeHint() const override
    {
        return QSize(300, body->minimumSize().height());
    }

    void resizeEvent(QResizeEvent *event) override
    {
        // Measure the real one-row minimum instead of using a fixed 730 px
        // threshold, which forced a second row and squeezed the payload box.
        int oneRow = body->contentsMargins().left() + body->contentsMargins().right();
        for (int index = 0; index < body->count(); ++index)
        {
            if (auto item = body->itemAt(index)) { oneRow += item->minimumSize().width(); }
            if (index > 0) { oneRow += body->spacing(); }
        }
        const auto direction = width() < oneRow ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
        if (body->direction() != direction)
        {
            body->setDirection(direction);
            updateGeometry();
        }
        QWidget::resizeEvent(event);
    }
};

Pane::Pane(Window *owner, const QString &instrumentKind) : QWidget(owner), window(owner)
{
    setObjectName("workspace");
    setAttribute(Qt::WA_StyledBackground);
    auto body = new QVBoxLayout(this);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    if (!instrumentKind.isEmpty())
    {
        instrument = new Instrument(instrumentKind, this);
        instrument->helpWindow = window->rootWindow();
        if (instrument->controls)
        {
            connect(instrument->controls, &SerialControlPanel::changed, this, [this] { window->saveSettings(); });
            connect(instrument->controls, &SerialControlPanel::zoomRequested, this, [this](int steps)
            {
                auto font = window->dataFont;
                font.setPointSize(qBound(8, font.pointSize() + steps, 32));
                window->setDataFont(font);
            });
        }
        if (instrument->plot)
        {
            connect(instrument->plot, &WavePlot::presentationChanged, this, [this] { window->saveSettings(); });
        }
        body->addWidget(instrument, 1);
        connect(instrument->source, &QComboBox::activated, this, [this](int index)
        {
            const QString previous = sourceKey;
            sourceKey = instrument->source->itemData(index).toString();
            try
            {
                if (sourceKey == Config::waveformDemo().key()) { window->engine->waveformDemo(); }
                else if (sourceKey == Config::structuredDemo().key()) { window->engine->structuredDemo(); }
            }
            catch (const std::exception &failure)
            {
                sourceKey = previous;
                refreshSource();
                window->error(QString::fromUtf8(failure.what()));
                return;
            }
            refreshSource();
            if (window->active == this) { window->syncParameterPane(true); }
            window->updateStatus();
        });
        if (instrument->valuesToggle)
        {
            connect(instrument->valuesToggle, &QPushButton::clicked, this, [this] { window->saveSettings(); });
        }
        applyAppearance();
        return;
    }
    monitorPage = new QWidget();
    monitorPage->setObjectName("page");
    auto monitor = new QVBoxLayout(monitorPage);
    monitor->setContentsMargins(0, 0, 0, 0);
    monitor->setSpacing(0);
    auto toolbar = new QWidget();
    toolbar->setObjectName("monitorToolbar");
    auto tools = horizontal(toolbar);
    tools->setContentsMargins(14, 8, 12, 6);
    tools->setSpacing(3);
    modeButton = new IconButton("terminal", "切换到交互终端");
    modeButton->setObjectName("terminalMode");
    modeButton->setCheckable(true);
    tools->addWidget(modeButton);
    connect(modeButton, &QAbstractButton::toggled, this, [this](bool enabled)
    {
        if (terminalMode != enabled) { setTerminal(enabled); }
    });
    timestamps = new IconButton("clock", "显示时间戳");
    timestamps->setObjectName("timestamps");
    timestamps->setCheckable(true);
    timestamps->setChecked(true);
    tools->addWidget(timestamps);
    hexDisplay = new IconButton("hex", "十六进制显示");
    hexDisplay->setObjectName("hexDisplay");
    hexDisplay->setCheckable(true);
    tools->addWidget(hexDisplay);
    wrapLines = new IconButton("wrap", "自动换行（关闭后可横向滚动）");
    wrapLines->setObjectName("wrapLines");
    wrapLines->setCheckable(true);
    wrapLines->setChecked(true);
    tools->addWidget(wrapLines);
    cursorShape = new IconButton("cursor", "光标形状：方块 / 竖线 · Insert");
    cursorShape->setCheckable(true);
    cursorShape->setChecked(true);
    cursorShape->hide();
    tools->addWidget(cursorShape);
    clipboardButton = new IconButton("clipboard", "终端复制与粘贴设置");
    clipboardButton->setObjectName("terminalClipboard");
    clipboardButton->hide();
    connect(clipboardButton, &QAbstractButton::clicked, this, &Pane::clipboardMenu);
    tools->addWidget(clipboardButton);
    tools->addSpacing(8);
    tools->addStretch();
    filter = new FilterBox();
    filter->setPlaceholderText("筛选内容");
    tools->addWidget(filter, 1);
    pause = new IconButton("pause", "暂停显示，后台继续接收");
    pause->setObjectName("pauseDisplay");
    pause->setCheckable(true);
    connect(pause, &QAbstractButton::toggled, this, [this](bool paused)
    {
        pause->setToolTip(paused ? "继续显示，后台仍在接收" : "暂停显示，后台继续接收");
        pause->setAccessibleName(pause->toolTip());
    });
    tools->addWidget(pause);
    auto clear = new IconButton("trash", "清空当前显示，保留共享历史");
    clear->setObjectName("clearDisplay");
    connect(clear, &QAbstractButton::clicked, this, &Pane::clearView);
    tools->addWidget(clear);
    follow = new IconButton("follow", "跟随最新数据");
    follow->setObjectName("followLatest");
    follow->setCheckable(true);
    follow->setChecked(true);
    tools->addWidget(follow);
    exportButton = new IconButton("export", "导出连接日志");
    exportButton->setObjectName("exportLog");
    exportButton->setEnabled(false);
    connect(exportButton, &QAbstractButton::clicked, this, &Pane::exportLog);
    tools->addWidget(exportButton);
    transferButton = new IconButton("file-transfer", "串口文件传输：原始字节 / XMODEM-CRC");
    transferButton->setObjectName("serialFileTransfer");
    transferButton->setEnabled(false);
    tools->addWidget(transferButton);
    connect(transferButton, &QAbstractButton::clicked, this, [this]
    {
        if (!channel || !channel->serial || channel->transferOwner) { return; }
        repeat->setChecked(false);
        auto dialog = new FileTransferDialog(channel, window);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowModality(Qt::WindowModal);
        connect(dialog, &FileTransferDialog::starting, this, [this] { repeat->setChecked(false); tick(); });
        dialog->show();
    });
    monitor->addWidget(toolbar);
    splitter = new Splitter(Qt::Vertical);
    display = new QStackedWidget();
    log = new LogView();
    terminal = new TerminalView();
    display->addWidget(log);
    display->addWidget(terminal);
    receiveSurface = new BackgroundSurface();
    receiveSurface->setObjectName("receiveBackgroundSurface");
    auto receiveLayout = new QVBoxLayout(receiveSurface);
    receiveLayout->setContentsMargins(0, 0, 0, 0);
    receiveLayout->setSpacing(0);
    receiveLayout->addWidget(display);
    receiveSplitter = new Splitter(Qt::Vertical);
    receiveSplitter->setObjectName("receiveSplitter");
    receiveSplitter->addWidget(receiveSurface);
    buildScriptResults();
    splitter->addWidget(receiveSplitter);
    monitor->addWidget(splitter, 1);

    sender = new QWidget();
    sender->setObjectName("sender");
    auto sendBody = new QVBoxLayout(sender);
    sendBody->setContentsMargins(18, 8, 18, 12);
    sendBody->setSpacing(8);
    auto senderHeading = horizontal();
    senderHeading->addWidget(label("发送缓冲", "senderLabel"));
    senderHeading->addStretch();
    senderHeading->addWidget(label("Ctrl + Enter", "senderLabel"));
    sendBody->addLayout(senderHeading);
    payload = new QPlainTextEdit();
    payload->viewport()->installEventFilter(this);
    payload->setMinimumHeight(54);
    payload->setPlaceholderText("输入待发送内容");
    sendBody->addWidget(payload, 1);
    auto sendTools = horizontal();
    sendTools->setSpacing(6);
    encoding = combo({"utf-8", "gbk", "ascii", "latin-1"});
    encoding->setMinimumWidth(84);
    encoding->setToolTip("收发文本编码");
    newline = combo({"None · 无换行", "LF · \\n", "CR · \\r", "CRLF · \\r\\n"});
    newline->setCurrentIndex(3);
    newline->menuPopup = true;
    newline->setMinimumWidth(116);
    newline->setToolTip("发送时追加的换行符");
    const QStringList lineNames {"None · 不追加换行", "LF · \\n · Line Feed", "CR · \\r · Carriage Return", "CRLF · \\r\\n · Carriage Return + Line Feed"};
    for (int i = 0; i < lineNames.size(); ++i)
    {
        newline->setItemData(i, lineNames[i], Qt::ToolTipRole);
    }
    sendHex = new IconButton("hex", "十六进制发送");
    sendHex->setObjectName("sendHex");
    sendHex->setCheckable(true);
    sendTools->addWidget(encoding);
    sendTools->addWidget(newline);
    sendTools->addWidget(sendHex);
    peer = combo({}, {}, true);
    peer->lineEdit()->setPlaceholderText("目标 IP:端口");
    peer->setMaximumWidth(210);
    peer->hide();
    auto actions = horizontal();
    actions->setSpacing(6);
    actions->addWidget(peer);
    actions->addStretch();
    auto templateButton = new IconButton("template", "报文模板");
    connect(templateButton, &QAbstractButton::clicked, this, &Pane::templateMenu);
    actions->addWidget(templateButton);
    repeat = new IconButton("repeat", "循环发送");
    repeat->setObjectName("repeatSend");
    repeat->setCheckable(true);
    repeat->setEnabled(false);
    actions->addWidget(repeat);
    interval = new QSpinBox();
    interval->setObjectName("repeatInterval");
    interval->setRange(20, 3600000);
    interval->setValue(1000);
    interval->setSuffix(" ms");
    interval->setFixedWidth(126);
    interval->setToolTip("循环发送间隔 · 毫秒（ms）");
    connect(interval, &QSpinBox::valueChanged, &repeatTimer, qOverload<int>(&QTimer::setInterval));
    actions->addWidget(interval);
    sendButton = button("发送", "primary");
    sendButton->setFixedWidth(94);
    connect(sendButton, &QPushButton::clicked, this, &Pane::sendCurrent);
    actions->addWidget(sendButton);
    auto footer = new SendFooter();
    footer->setObjectName("sendFooter");
    footer->body->addLayout(sendTools);
    footer->body->addLayout(actions, 1);
    sendBody->addWidget(footer);
    splitter->addWidget(sender);
    splitter->setStretchFactor(0, 1);
    splitter->setSizes(senderSizes);
    body->addWidget(monitorPage, 1);

    connect(timestamps, &QAbstractButton::toggled, this, [this](bool checked) { log->timestamps = checked; replay(); });
    connect(hexDisplay, &QAbstractButton::toggled, this, [this](bool checked) { log->hex = checked; replay(); });
    connect(wrapLines, &QAbstractButton::toggled, log, &LogView::setWrapping);
    connect(cursorShape, &QAbstractButton::toggled, this, [this](bool block)
    {
        terminal->blockCursor = block;
        terminal->update();
    });
    connect(terminal, &TerminalView::cursorShapeChanged, cursorShape, &QAbstractButton::setChecked);
    connect(follow, &QAbstractButton::toggled, this, [this](bool checked)
    {
        log->follow = checked;
        if (checked)
        {
            log->verticalScrollBar()->setValue(log->verticalScrollBar()->maximum());
        }
    });
    connect(filter, &FilterBox::criteriaChanged, this, [this] { log->setFilter(filter->criteria()); });
    connect(encoding, &QComboBox::currentTextChanged, this, [this](const QString &value)
    {
        log->encoding = value;
        replay();
        for (auto host : window->rootWindow()->hosts)
        {
            if (!host) { continue; }
            for (auto page : host->panes)
            {
                if (page->channel == channel) { page->enableParser(); }
            }
        }
    });
    connect(repeat, &QAbstractButton::toggled, this, [this](bool checked)
    {
        repeat->setToolTip(checked ? "停止循环发送" : "循环发送");
        repeat->setAccessibleName(repeat->toolTip());
        if (!checked)
        {
            repeatTimer.stop();
            return;
        }
        try
        {
            if (!channel || (channel->state != "connected" && channel->state != "listening"))
            {
                fail("请先连接设备");
            }
            repeatedPayload = Engine::encode(payload->toPlainText(), sendHex->isChecked(), encoding->currentText(), QStringList {"none", "lf", "cr", "crlf"}[newline->currentIndex()]);
            repeatedPeer = peer->currentText();
            repeatTimer.start(interval->value());
            sendCurrent();
        }
        catch (const std::exception &exception)
        {
            repeat->setChecked(false);
            window->error(QString::fromUtf8(exception.what()));
        }
    });
    connect(&repeatTimer, &QTimer::timeout, this, [this]
    {
        try
        {
            if (!channel)
            {
                fail("连接已关闭");
            }
            channel->send(repeatedPayload, repeatedPeer);
        }
        catch (const std::exception &exception)
        {
            repeat->setChecked(false);
            window->error(QString::fromUtf8(exception.what()));
        }
    });
    connect(terminal, &TerminalView::transmit, this, [this](const QByteArray &data)
    {
        try
        {
            if (channel)
            {
                QByteArray input = data;
#ifdef Q_OS_WIN
                if (channel->config.kind == "ssh")
                {
                    // ConPTY accepts the standard VT Home/End key forms.
                    if (input == "\x1b[7~") { input = "\x1b[H"; }
                    else if (input == "\x1b[4~") { input = "\x1b[F"; }
                }
#endif
                channel->send(input, peer->currentText());
            }
        }
        catch (const std::exception &exception)
        {
            window->error(QString::fromUtf8(exception.what()));
        }
    });
    connect(terminal, &TerminalView::gridSizeChanged, this, [this](const QSize &size)
    {
        if (channel && channel->config.kind == "ssh" && terminalMode && terminal->isVisible())
        {
            channel->resizeTerminal(size);
        }
    });
    const auto zoom = [this](int steps)
    {
        auto font = window->dataFont;
        font.setPointSize(qBound(8, font.pointSize() + steps, 32));
        window->setDataFont(font);
    };
    connect(log, &LogView::zoomRequested, this, zoom);
    connect(terminal, &TerminalView::zoomRequested, this, zoom);
    auto shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), payload);
    connect(shortcut, &QShortcut::activated, this, &Pane::sendCurrent);
    applyAppearance();
}

void Pane::setChannel(Channel *value)
{
    if (!instrument && channel != value)
    {
        scriptCursors = {};
        scriptViewStart = 0;
        scriptTable->clear();
    }
    if (instrument && channel != value)
    {
        instrument->cursors = {};
        instrument->valuesSnapshot = {};
        if (instrument->table) { instrument->table->clear(); }
        if (instrument->detail) { instrument->detail->clear(); }
        if (instrument->plot) { instrument->plot->setSamples({}); }
        instrument->info->setText(value ? "等待设备数据" : "等待数据来源连接");
    }
    channel = value;
    if (instrument && instrument->controls) { instrument->controls->setChannel(value); }
    if (instrument) { instrument->waveChannel = value; }
    if (value)
    {
        draft = value->config;
        if (instrument) { sourceKey = value->config.key(); }
        if (!instrument && value->config.kind == "ssh")
        {
            setTerminal(true);
            QTimer::singleShot(0, this, [this]
            {
                if (channel && terminal->isVisible()) { channel->resizeTerminal(QSize(terminal->columns, terminal->rows)); }
            });
        }
        if (supportsScript())
        {
            if (value->scriptEnabled)
            {
                scriptCode = value->scriptCode;
                scriptRequested = true;
            }
            else if (scriptRequested)
            {
                try { value->configureScript(scriptCode, true); }
                catch (const ScriptError &failure)
                {
                    scriptRequested = false;
                    window->error(failure.display());
                }
            }
            else { value->scriptCode = scriptCode; }
        }
        else if (!instrument) { scriptRequested = false; }
    }
    enableParser();
    if (!instrument) { refreshScriptResults(); }
}

void Pane::enableParser()
{
    if (instrument && instrument->isStructured() && channel)
    {
        auto sourcePane = window->findPane(channel);
        const QString encoding = sourcePane ? sourcePane->encoding->currentText() : "utf-8";
        channel->configureParser("auto", encoding);
    }
}

void Pane::refreshSource()
{
    if (!instrument)
    {
        return;
    }
    auto source = instrument->source;
    const QSignalBlocker blocker(source);
    source->clear();
    source->addItem(instrument->controls ? "选择串口连接" : "选择数据来源", QString());
    const bool structured = instrument->isStructured();
    const auto demo = structured ? Config::structuredDemo() : Config::waveformDemo();
    int demoIndex = -1;
    if (instrument->plot || structured)
    {
        source->addItem(demo.name, demo.key());
        demoIndex = source->count() - 1;
        source->setItemData(demoIndex, structured ? "独立生成 JSON 对象和 XML 报文，无需连接设备"
            : "本地生成正弦波、三角波、方波和锯齿波，无需连接设备", Qt::ToolTipRole);
        if (sourceKey == demo.key()) { source->setCurrentIndex(demoIndex); }
    }
    Channel *selected = nullptr;
    const auto serialAvailable = [this](const QString &key)
    {
        if (key == "serial:loop://") { return true; }
        for (const auto &value : window->lastPorts)
        {
            if ("serial:" + value.toObject()["port"].toString().toLower() == key) { return true; }
        }
        return false;
    };
    for (auto candidate : window->engine->channels)
    {
        if (instrument->controls && candidate->config.kind != "serial") { continue; }
        if (candidate->config.kind == "serial" && candidate->state != "connected"
            && !serialAvailable(candidate->config.key())) { continue; }
        if ((structured && candidate->config.kind == "demo" && !candidate->config.isStructuredDemo())
            || (!structured && candidate->config.isStructuredDemo())) { continue; }
        int index = demoIndex;
        const QString title = candidate->name + " · " + stateText(candidate->state);
        if (index >= 0 && candidate->config.key() == demo.key()) { source->setItemText(index, title); }
        else
        {
            source->addItem(title, candidate->config.key());
            index = source->count() - 1;
        }
        if (candidate->config.key() == sourceKey)
        {
            selected = candidate;
            source->setCurrentIndex(index);
        }
    }
    const bool missingSerial = sourceKey.startsWith("serial:") && !selected && !serialAvailable(sourceKey);
    if (!sourceKey.isEmpty() && !selected && !missingSerial && !(demoIndex >= 0 && sourceKey == demo.key())
        && (!instrument->controls || sourceKey.startsWith("serial:")))
    {
        const auto name = sourceKey.startsWith("serial:") ? sourceKey.mid(7).toUpper() : sourceKey;
        source->addItem(name + " · 未建立连接", sourceKey);
        source->setCurrentIndex(source->count() - 1);
    }
    source->setToolTip(missingSerial ? "之前选择的串口当前不可用；连接设备后可重新选择"
        : sourceKey == demo.key() ? (structured ? "独立 JSON/XML 演示 · 每秒更新 · 不占用串口"
        : "本地演示波形 · 50 Hz · 不占用串口")
        : sourceKey.isEmpty() ? (instrument->plot || structured ? "选择 Demo 预览，或选择已建立的设备连接"
            : "从收发页面建立连接后，在这里选择数据来源") : sourceKey);
    if (channel != selected) { setChannel(selected); }
}

bool Pane::eventFilter(QObject *object, QEvent *event)
{
    if (object == payload->viewport() && event->type() == QEvent::Wheel)
    {
        auto wheel = static_cast<QWheelEvent *>(event);
        if (wheel->modifiers() & Qt::ControlModifier)
        {
            auto font = window->dataFont;
            font.setPointSize(qBound(8, font.pointSize() + wheel->angleDelta().y() / 120, 32));
            window->setDataFont(font);
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
}

void Pane::applyAppearance()
{
    const QString fontStyle = QString("font-family: '%1'; font-size: %2pt; font-weight: %3; font-style: %4;")
        .arg(window->dataFont.families().join("', '"))
        .arg(window->dataFont.pointSizeF())
        .arg(int(window->dataFont.weight()))
        .arg(window->dataFont.italic() ? "italic" : "normal");
    if (instrument)
    {
        if (instrument->controls)
        {
            instrument->controls->setTheme(window->theme);
            instrument->controls->setDataFont(window->dataFont);
        }
        for (auto control : instrument->findChildren<IconButton *>())
        {
            control->theme = window->theme;
            control->update();
        }
        if (instrument->plot)
        {
            instrument->plot->theme = window->theme;
            instrument->plot->setFont(window->dataFont);
            instrument->plot->setStyleSheet(fontStyle);
            instrument->plot->update();
        }
        if (instrument->detail)
        {
            instrument->detail->setFont(window->dataFont);
            instrument->detail->setStyleSheet(fontStyle);
        }
        if (instrument->table)
        {
            instrument->table->setFont(window->dataFont);
            instrument->table->setStyleSheet(fontStyle);
        }
        return;
    }
    log->theme = window->theme;
    terminal->theme = window->theme;
    const bool workspaceImage = !window->windowBackgroundImage.isNull();
    const bool receiveImage = !window->receiveBackgroundImage.isNull();
    const bool transparent = workspaceImage || receiveImage;
    const QString logStyle = fontStyle + (transparent ? "background: transparent;" : "");
    log->setFont(window->dataFont);
    if (log->styleSheet() != logStyle) { log->setStyleSheet(logStyle); }
    payload->setFont(window->dataFont);
    payload->setStyleSheet(fontStyle);
    terminal->setFont(window->dataFont);
    terminal->setStyleSheet(fontStyle);
    receiveSurface->background.set(window->receiveBackground, window->receiveBackgroundImage);
    receiveSurface->base = window->theme.panel;
    receiveSurface->transparentBase = workspaceImage && !receiveImage;
    receiveSurface->setAttribute(Qt::WA_OpaquePaintEvent, !receiveSurface->transparentBase);
    receiveSurface->update();
    log->transparentBackground = transparent;
    terminal->transparentBackground = transparent;
    log->viewport()->setAutoFillBackground(!transparent);
    auto viewportPalette = log->viewport()->palette();
    viewportPalette.setColor(QPalette::Base, transparent ? QColor(Qt::transparent) : window->theme.panel);
    log->viewport()->setPalette(viewportPalette);
    if (scriptTable)
    {
        scriptTable->setFont(window->dataFont);
        scriptTable->setStyleSheet(fontStyle);
        scriptTable->setColumnWidth(0, QFontMetrics(window->dataFont).horizontalAdvance("00:00:00.000") + 22);
    }
    terminal->update();
    for (auto control : findChildren<IconButton *>())
    {
        control->theme = window->theme;
        control->update();
    }
}

void Pane::tick(bool includeHidden)
{
    if (instrument)
    {
        if (isVisible() || includeHidden) { instrument->tick(channel); }
        return;
    }
    const bool ready = channel && (channel->state == "connected" || channel->state == "listening");
    if (channel)
    {
        scriptRequested = channel->scriptEnabled;
        scriptCode = channel->scriptCode;
    }
    const bool available = ready && !channel->transferOwner;
    sendButton->setEnabled(available && channel->config.kind != "demo");
    repeat->setEnabled(available && channel->config.kind != "demo");
    terminal->enabledInput = available && terminalMode;
    const bool ssh = (channel ? channel->config.kind : draft.kind) == "ssh";
    terminal->keyboardSelectionEnabled = ssh;
    modeButton->setVisible(!ssh);
    if (ssh)
    {
        if (!terminalMode) { setTerminal(true); }
    }
    else if (terminalMode != userTerminalMode)
    {
        setTerminal(userTerminalMode);
    }
    transferButton->setVisible((channel ? channel->config.kind : draft.kind) == "serial");
    transferButton->setEnabled(available && channel->serial);
    exportButton->setEnabled(channel);
    if (!available)
    {
        repeat->setChecked(false);
    }
    peer->setVisible(channel && (channel->config.kind == "tcp-server" || channel->config.kind == "udp"));
    if (!channel || (!isVisible() && !includeHidden))
    {
        if (!channel && isVisible()) { refreshScriptResults(); }
        return;
    }
    refreshScriptResults();
    if (channel->config.kind == "tcp-server")
    {
        QStringList current;
        for (int i = 0; i < peer->count(); ++i)
        {
            current.append(peer->itemText(i));
        }
        if (current != channel->peers.keys())
        {
            const auto selected = peer->currentText();
            peer->clear();
            peer->addItems(channel->peers.keys());
            if (channel->peers.contains(selected))
            {
                peer->setCurrentText(selected);
            }
        }
    }
    if (!pause->isChecked())
    {
        const auto batch = window->engine->journal.read(cursor, 500, channel->id);
        cursor = batch["next_cursor"].toInteger();
        if (batch["gap"].toBool())
        {
            log->append("部分旧数据已被历史缓冲区覆盖");
            log->streams.clear();
            terminal->reset(channel && channel->config.kind == "ssh" ? "utf-8" : encoding->currentText());
        }
        for (const auto &value : batch["events"].toArray())
        {
            const auto event = value.toObject();
            if (!ssh) { log->feed(event); }
            if (event["stream_reset"].toBool())
            {
                terminal->reset(channel && channel->config.kind == "ssh" ? "utf-8" : encoding->currentText());
            }
            if (event["direction"] == "rx" && event["message"] != "file-transfer")
            {
                terminal->feed(QByteArray::fromHex(event["hex"].toString().toLatin1()));
            }
        }
    }

}

void Pane::sendCurrent()
{
    if (instrument) { return; }
    try
    {
        if (!channel)
        {
            fail("请先连接设备");
        }
        channel->send(Engine::encode(payload->toPlainText(), sendHex->isChecked(), encoding->currentText(), QStringList {"none", "lf", "cr", "crlf"}[newline->currentIndex()]), peer->currentText());
    }
    catch (const std::exception &exception)
    {
        window->error(QString::fromUtf8(exception.what()));
    }
}

void Pane::setTerminal(bool enabled)
{
    if (instrument) { return; }
    const bool ssh = (channel ? channel->config.kind : draft.kind) == "ssh";
    if (ssh) { enabled = true; }
    if (!ssh) { userTerminalMode = enabled; }
    modeButton->setVisible(!ssh);
    if (enabled && !sender->isHidden())
    {
        const auto sizes = splitter->sizes();
        if (sizes.size() == 2 && sizes[0] > 0 && sizes[1] > 0)
        {
            senderSizes = sizes;
        }
    }
    terminalMode = enabled;
    repeat->setChecked(false);
    pause->setChecked(false);
    display->setCurrentWidget(enabled ? static_cast<QWidget *>(terminal) : log);
    sender->setVisible(!enabled);
    if (modeButton && modeButton->isChecked() != enabled)
    {
        modeButton->setChecked(enabled);
    }
    modeButton->setToolTip(enabled ? "返回收发日志" : "切换到交互终端");
    modeButton->setAccessibleName(modeButton->toolTip());
    modeButton->glyph = enabled ? "log" : "terminal";
    modeButton->update();
    filter->setVisible(!enabled);
    follow->setVisible(!enabled);
    timestamps->setVisible(!enabled);
    hexDisplay->setVisible(!enabled);
    wrapLines->setVisible(!enabled);
    cursorShape->setVisible(enabled);
    clipboardButton->setVisible(enabled);
    if (enabled)
    {
        terminal->setFocus();
    }
    else
    {
        QTimer::singleShot(0, this, [this] { splitter->setSizes(senderSizes); });
    }
}

void Pane::clearView()
{
    if (instrument) { return; }
    cursor = window->engine->journal.sequence;
    viewStart = cursor;
    log->reset();
    terminal->reset(channel && channel->config.kind == "ssh" ? "utf-8" : encoding->currentText());
    clearScriptResults();
}

void Pane::replay()
{
    if (instrument) { return; }
    cursor = viewStart;
    log->reset();
    terminal->reset(channel && channel->config.kind == "ssh" ? "utf-8" : encoding->currentText());
}

void Pane::exportLog()
{
    if (instrument) { return; }
    if (!channel)
    {
        return;
    }
    const auto path = QFileDialog::getSaveFileName(this, "导出连接日志", "serial-log.jsonl", "JSON Lines (*.jsonl)");
    if (path.isEmpty())
    {
        return;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        window->error(file.errorString());
        return;
    }
    for (const auto &entry : window->engine->journal.events)
    {
        if (entry.first["channel_id"] == channel->id)
        {
            file.write(QJsonDocument(entry.first).toJson(QJsonDocument::Compact) + '\n');
        }
    }
    if (!file.commit())
    {
        window->error(file.errorString());
    }
}

void Pane::templateMenu()
{
    if (instrument) { return; }
    QMenu menu(this);
    for (const auto &value : window->rootWindow()->templates)
    {
        const auto item = value.toObject();
        menu.addAction(item["name"].toString(), this, [this, item]
        {
            payload->setPlainText(item["payload"].toString());
            sendHex->setChecked(item["hex_mode"].toBool());
            encoding->setCurrentText(item["encoding"].toString("utf-8"));
            newline->setCurrentIndex(item["newline"].toInt());
        });
    }
    if (!window->rootWindow()->templates.isEmpty())
    {
        menu.addSeparator();
    }
    menu.addAction("保存当前报文…", this, [this]
    {
        bool accepted = false;
        const QString name = QInputDialog::getText(this, "保存报文模板", "模板名称", QLineEdit::Normal, {}, &accepted);
        if (accepted && !name.trimmed().isEmpty())
        {
            window->rootWindow()->templates.append(QJsonObject {{"name", name.trimmed()}, {"payload", payload->toPlainText()}, {"hex_mode", sendHex->isChecked()}, {"encoding", encoding->currentText()}, {"newline", newline->currentIndex()}});
            window->saveSettings();
        }
    });
    menu.addAction("导入模板…", this, [this]
    {
        QFile file(QFileDialog::getOpenFileName(this, "导入模板", {}, "JSON (*.json)"));
        if (!file.open(QIODevice::ReadOnly))
        {
            return;
        }
        const auto document = QJsonDocument::fromJson(file.readAll());
        if (!document.isArray())
        {
            window->error("模板文件需要 JSON 数组");
            return;
        }
        for (const auto &value : document.array())
        {
            const auto item = value.toObject();
            if (!item["name"].isString() || !item["payload"].isString() || item["newline"].toInt() < 0 || item["newline"].toInt() > 3)
            {
                window->error("模板字段无效，未导入");
                return;
            }
        }
        window->rootWindow()->templates = document.array();
        window->saveSettings();
    });
    menu.addAction("导出模板…", this, [this]
    {
        const auto path = QFileDialog::getSaveFileName(this, "导出模板", "templates.json", "JSON (*.json)");
        if (path.isEmpty())
        {
            return;
        }
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly))
        {
            file.write(QJsonDocument(window->rootWindow()->templates).toJson(QJsonDocument::Indented));
            file.commit();
        }
    });
    menu.exec(QCursor::pos());
}

void Pane::clipboardMenu()
{
    if (instrument || !terminal) { return; }
    QMenu menu(this);
    auto *copyShortcut = menu.addAction("复制快捷键（有选区时 Ctrl+C / Ctrl+Shift+C）");
    copyShortcut->setToolTip("有选区时复制并清除选区；无选区时 Ctrl+C 发送中断。Ctrl+Shift+C 仅复制。");
    copyShortcut->setCheckable(true);
    copyShortcut->setChecked(terminal->copyShortcutEnabled);
    auto *pasteShortcut = menu.addAction("粘贴快捷键（Ctrl + Shift + V）");
    pasteShortcut->setCheckable(true);
    pasteShortcut->setChecked(terminal->pasteShortcutEnabled);
    auto *rightClickPaste = menu.addAction("右键粘贴");
    rightClickPaste->setCheckable(true);
    rightClickPaste->setChecked(terminal->rightClickPasteEnabled);
    menu.addSeparator();
    auto *copyNow = menu.addAction("复制当前选区");
    copyNow->setEnabled(terminal->hasSelection());
    auto *pasteNow = menu.addAction("粘贴剪贴板");
    pasteNow->setEnabled(terminal->enabledInput);
    connect(copyShortcut, &QAction::toggled, this, [this](bool enabled)
    {
        terminal->copyShortcutEnabled = enabled;
        window->saveSettings();
    });
    connect(pasteShortcut, &QAction::toggled, this, [this](bool enabled)
    {
        terminal->pasteShortcutEnabled = enabled;
        window->saveSettings();
    });
    connect(rightClickPaste, &QAction::toggled, this, [this](bool enabled)
    {
        terminal->rightClickPasteEnabled = enabled;
        window->saveSettings();
    });
    connect(copyNow, &QAction::triggered, terminal, &TerminalView::copySelection);
    connect(pasteNow, &QAction::triggered, terminal, &TerminalView::pasteClipboard);
    menu.exec(clipboardButton->mapToGlobal(QPoint(0, clipboardButton->height())));
}

QJsonObject Pane::settings() const
{
    if (instrument)
    {
        auto settings = instrument->settings();
        settings["source_key"] = sourceKey;
        settings["title"] = pageTitle;
        return settings;
    }
    const auto sizes = terminalMode ? senderSizes : splitter->sizes();
    QJsonObject result {{"tab_title", pageTitle}, {"timestamps", log->timestamps}, {"hex_display", log->hex}, {"wrap_lines", wrapLines->isChecked()},
        {"script_code", channel ? channel->scriptCode : scriptCode},
        {"script_test_hex", scriptTestHex},
        {"script_enabled", channel ? channel->scriptEnabled : scriptRequested},
        {"script_collapsed", scriptCollapsed}, {"script_panel_height", scriptPanelHeight},
        {"block_cursor", terminal->blockCursor}, {"search_filter", filter->settings()},
        {"copy_shortcut", terminal->copyShortcutEnabled}, {"paste_shortcut", terminal->pasteShortcutEnabled},
        {"right_click_paste", terminal->rightClickPasteEnabled},
        {"terminal_mode", terminalMode}, {"encoding", encoding->currentText()},
        {"sender_sizes", QJsonArray {sizes.value(0, 600), sizes.value(1, 150)}}};
    if (!supportsScript())
    {
        result.remove("script_test_hex");
        result.remove("script_code");
        result.remove("script_enabled");
    }
    return result;
}

void Pane::restore(const QJsonObject &settings)
{
    if (instrument)
    {
        if (instrument->freeze) { instrument->freeze->setChecked(settings["frozen"].toBool()); }
        if (instrument->controls) { instrument->controls->restore(settings["serial_controls"].toObject()); }
        if (instrument->filter) { instrument->filter->restore(settings["search_filter"].toObject()); }
        if (instrument->points)
        {
            instrument->points->setValue(settings["points"].toInt(1000));
            instrument->waveXAxis->setChecked(settings["x_axis"].toBool(true));
            instrument->waveYAxis->setChecked(settings["y_axis"].toBool(true));
            instrument->plot->restorePresentation(settings["wave_presentation"].toObject());
        }
        if (instrument->valuesToggle)
        {
            const auto sizes = settings["values_sizes"].toArray();
            if (sizes.size() == 2 && sizes[0].toInt() > 0 && sizes[1].toInt() > 0)
            {
                instrument->valuesSizes = {sizes[0].toInt(), sizes[1].toInt()};
            }
            instrument->valuesToggle->setChecked(settings["show_values"].toBool());
        }
        sourceKey = settings["source_key"].toString();
        if (instrument->controls && !sourceKey.startsWith("serial:")) { sourceKey.clear(); }
        if (instrument->isStructured() && sourceKey.startsWith("demo:")
            && sourceKey != Config::structuredDemo().key())
        {
            sourceKey = Config::structuredDemo().key();
        }
        try
        {
            if (sourceKey == Config::waveformDemo().key()) { window->engine->waveformDemo(); }
            else if (sourceKey == Config::structuredDemo().key()) { window->engine->structuredDemo(); }
        }
        catch (const std::exception &failure) { window->error(QString::fromUtf8(failure.what())); }
        if (!settings["title"].toString().isEmpty())
        {
            pageTitle = settings["title"].toString();
            window->tabs->setTabText(paneIndex(window->tabs, this), pageTitle);
        }
        refreshSource();
        return;
    }
    if (settings.contains("tab_title"))
    {
        pageTitle = settings["tab_title"].toString().trimmed();
        const int index = paneIndex(window->tabs, this);
        if (index >= 0 && !pageTitle.isEmpty())
        {
            window->tabs->setTabText(index, pageTitle);
            window->adjustTabs();
        }
    }
    log->timestamps = settings.value("timestamps").toBool(true);
    filter->restore(settings["search_filter"].toObject());
    timestamps->setChecked(log->timestamps);
    hexDisplay->setChecked(settings["hex_display"].toBool());
    wrapLines->setChecked(settings.value("wrap_lines").toBool(true));
    cursorShape->setChecked(settings.value("block_cursor").toBool(true));
    terminal->copyShortcutEnabled = settings.value("copy_shortcut").toBool(true);
    terminal->pasteShortcutEnabled = settings.value("paste_shortcut").toBool(true);
    terminal->rightClickPasteEnabled = settings.value("right_click_paste").toBool(true);
    encoding->setCurrentText(settings.value("encoding").toString("utf-8"));
    scriptCollapsed = settings["script_collapsed"].toBool();
    scriptPanelHeight = qBound(96, settings["script_panel_height"].toInt(164), 1200);
    const auto sizes = settings["sender_sizes"].toArray();
    if (sizes.size() == 2 && sizes[0].toInt() > 0 && sizes[1].toInt() > 0)
    {
        senderSizes = {sizes[0].toInt(), sizes[1].toInt()};
        QTimer::singleShot(0, this, [this] { splitter->setSizes(senderSizes); });
    }
    const bool ssh = (channel ? channel->config.kind : draft.kind) == "ssh";
    userTerminalMode = channel && !ssh && settings.value("terminal_mode").toBool();
    setTerminal(ssh || userTerminalMode);
    if (!supportsScript()) { scriptRequested = false; }
    else if (settings.contains("script_code"))
    {
        scriptTestHex = settings["script_test_hex"].toString(exampleTestHex());
        scriptCode = settings["script_code"].toString();
        scriptRequested = settings["script_enabled"].toBool();
        try
        {
            if (channel) { channel->configureScript(scriptCode, scriptRequested); }
            else if (scriptRequested) { FrameScriptProgram::compile(scriptCode); }
        }
        catch (const ScriptError &failure)
        {
            scriptRequested = false;
            window->error(failure.display());
        }
    }
    window->restoreInstruments(settings, channel ? channel->config.key() : Config::fromJson(window->saved["connection"].toObject()).key());
    refreshScriptResults();
    resizeScriptResults();
}

class AdaptiveStack : public QStackedWidget
{
public:
    QSize sizeHint() const override
    {
        return currentWidget() ? currentWidget()->sizeHint() : QSize();
    }
    QSize minimumSizeHint() const override
    {
        return currentWidget() ? currentWidget()->minimumSizeHint() : QSize();
    }
};

Window::Window(Engine *owner, McpServer *service, const QString &path, Window *main)
    : QMainWindow(main, Qt::Window), primary(main), engine(owner), mcp(service), settingsPath(path)
{
    installWindowChrome();
    rootWindow()->hosts.append(this);
    if (primary)
    {
        portProvider = primary->portProvider;
    }
    setWindowTitle("Serial Workbench · Native " + Version);
    setWindowIcon(QIcon(":/brand/lightning.svg"));
    setFont(Theme::interfaceFont());
    setMinimumSize(480, 320);
    const QScreen *screen = QGuiApplication::primaryScreen();
    resize(1400, qMin(960, screen ? screen->availableGeometry().height() : 960));
    QFile settingsFile(path);
    if (settingsFile.open(QIODevice::ReadOnly))
    {
        saved = QJsonDocument::fromJson(settingsFile.readAll()).object();
    }
    viewSettings = saved["views"].toObject();
    templates = saved["templates"].toArray();
    const auto appearance = saved["appearance"].toObject();
    parameterWidth = qBound(220, appearance["parameter_width"].toInt(246), 4096);
    themeColor = Theme::parseColor(appearance["theme_color"].toString());
    themeContrast = Theme::parseColor(appearance["contrast_color"].toString());
    theme = Theme::make(appearance["theme"] == "dark", themeColor, themeContrast);
    dataFont = Theme::defaultDataFont();
    const auto savedFamily = appearance["font_family"].toString();
    const bool previousDefault = !appearance["readable_type"].toBool()
        && savedFamily == Theme::fontFamily(12) && appearance["font_size"].toInt() == 18;
    if (!savedFamily.isEmpty() && !previousDefault)
    {
        const auto specification = appearance["font_spec"].toString();
        if (!specification.isEmpty()) { dataFont.fromString(specification); }
        dataFont.setFamilies({savedFamily, "Microsoft YaHei UI"});
        dataFont.setPointSize(qBound(8, appearance["font_size"].toInt(12), 32));
        dataFont.setStyleStrategy(savedFamily.startsWith("Fusion Pixel") ? QFont::NoAntialias : QFont::PreferAntialias);
    }
    if (!primary)
    {
        connect(engine, &Engine::sshPromptRequested, this, [this](SshAuthPrompt *prompt)
        {
            showSshAuthPrompt(prompt, this, theme);
        });
        engine->allowControl = saved["allow_ai_control"].toBool();
        const auto protocols = saved["mcp_protocols"].toObject();
        for (const auto &kind : {"serial", "tcp", "udp", "ssh"})
        {
            mcp->setProtocolEnabled(kind, protocols.value(kind).toBool(true));
        }
    }
    else
    {
        setAttribute(Qt::WA_DeleteOnClose);
        theme = primary->theme;
        themeColor = primary->themeColor;
        themeContrast = primary->themeContrast;
        dataFont = primary->dataFont;
        templates = primary->templates;
        viewSettings = primary->viewSettings;
        parameterWidth = primary->parameterWidth;
    }

    if (primary)
    {
        windowBackground = primary->windowBackground;
        receiveBackground = primary->receiveBackground;
        windowBackgroundImage = primary->windowBackgroundImage;
        receiveBackgroundImage = primary->receiveBackgroundImage;
    }
    else
    {
        windowBackground = BackgroundSpec::fromJson(appearance["window_background"].toObject());
        receiveBackground = BackgroundSpec::fromJson(appearance["receive_background"].toObject());
        const QDir directory(QFileInfo(settingsPath).absolutePath());
        if (!windowBackground.file.isEmpty()) { windowBackgroundImage = BackgroundImage::read(directory.filePath(windowBackground.file)); }
        if (!receiveBackground.file.isEmpty()) { receiveBackgroundImage = BackgroundImage::read(directory.filePath(receiveBackground.file)); }
    }

    auto root = new BackgroundSurface();
    backgroundSurface = root;
    root->windowAligned = true;
    root->setObjectName("root");
    auto layout = horizontal(root);
    layout->setSpacing(0);
    rail = new QFrame();
    rail->setObjectName("rail");
    rail->setFixedWidth(54);
    auto railBody = new QVBoxLayout(rail);
    railBody->setContentsMargins(9, 16, 9, 12);
    auto workbench = new IconButton("lightning", "显示 / 隐藏连接参数");
    parameterToggle = workbench;
    workbench->setCheckable(true);
    workbench->setChecked(true);
    railBody->addWidget(workbench);
    railBody->addStretch();
    gear = new IconButton("gear", "设置");
    railBody->addWidget(gear);
    connect(gear, &QAbstractButton::clicked, this, &Window::openSettings);
    layout->addWidget(rail);

    sidebar = new QFrame();
    sidebar->setObjectName("sidebar");
    sidebar->setMinimumWidth(200);
    sidebar->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    connect(workbench, &QAbstractButton::toggled, this, [this](bool visible)
    {
        parametersVisible = visible;
        auto scroll = findChild<QScrollArea *>("parameterScroll");
        if (scroll)
        {
            scroll->setVisible(visible && active);
            parameterGrip->setVisible(visible && active);
            if (visible) { scroll->raise(); }
            arrangeParameters();
        }
    });
    connect(workbench, &QAbstractButton::toggled, this, [this] { QTimer::singleShot(0, this, &Window::adjustTabs); });
    auto side = new QVBoxLayout(sidebar);
    side->setContentsMargins(16, 16, 24, 16);
    side->setSpacing(12);
    auto brand = new QWidget();
    brand->setObjectName("pixelBrand");
    auto brandBody = new QVBoxLayout(brand);
    brandBody->setContentsMargins(14, 14, 14, 12);
    brandBody->setSpacing(10);
    brandBody->addWidget(label("Serial\nWorkbench", "brandTitle"));
    brandBody->addWidget(label("串口 / 网络 / SSH", "brandSubtitle"));
    side->addWidget(brand);
    side->addSpacing(4);
    side->addWidget(label("连接参数", "section"));
    protocol = new ProtocolCombo();
    side->addWidget(protocol);
    configStack = new AdaptiveStack();
    configStack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto serialPanel = new QWidget();
    auto serialBody = new QVBoxLayout(serialPanel);
    serialBody->setContentsMargins(0, 4, 0, 0);
    serialBody->setSpacing(8);
    serialBody->addWidget(label("端口", "muted"));
    port = combo({"NULL"}, "NULL", true);
    port->setObjectName("serialPort");
    port->setItemDelegate(new PortDelegate(this));
    port->view()->setMinimumWidth(430);
    port->setInsertPolicy(QComboBox::NoInsert);
    port->setCompleter(nullptr);
    port->lineEdit()->setReadOnly(true);
    port->setAttribute(Qt::WA_InputMethodEnabled, false);
    port->lineEdit()->setFocusPolicy(Qt::NoFocus);
    port->lineEdit()->setCursor(Qt::ArrowCursor);
    port->setFixedHeight(40);
    serialBody->addWidget(port);
    portDetails = label("", "portDetails");
    portDetails->setTextFormat(Qt::PlainText);
    portDetails->setWordWrap(true);
    portDetails->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    portDetails->setForegroundRole(QPalette::PlaceholderText);
    portDetails->hide();
    serialBody->addWidget(portDetails);
    serialBody->addSpacing(7);
    serialBody->addWidget(label("波特率", "muted"));
    baud = combo({"1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600", "1000000", "2000000"}, "115200", true);
    baud->setObjectName("baudRate");
    baud->setFixedHeight(40);
    baud->setToolTip("连接后选择新值会立即应用，并回读 Windows 驱动确认");
    serialBody->addWidget(baud);
    bits = combo({"5", "6", "7", "8"}, "8");
    parity = combo({"N", "E", "O", "M", "S"});
    stopbits = combo({"1", "1.5", "2"});
    flow = combo({"none", "rtscts", "xonxoff"});
    bits->setMinimumContentsLength(1);
    parity->setMinimumContentsLength(1);
    stopbits->setMinimumContentsLength(3);
    flow->setMinimumContentsLength(7);
    flow->setMinimumWidth(flow->fontMetrics().horizontalAdvance("xonxoff") + 48);
    parity->setToolTip("N: None · 无校验\nE: Even · 偶校验\nO: Odd · 奇校验\nM: Mark\nS: Space");
    flow->setToolTip("none: 无流控\nrtscts: 硬件流控\nxonxoff: 软件流控");
    auto serialGrid = new QGridLayout();
    serialGrid->setColumnStretch(0, 2);
    serialGrid->setColumnStretch(1, 3);
    serialGrid->setHorizontalSpacing(8);
    serialGrid->setVerticalSpacing(7);
    serialGrid->addWidget(label("数据位", "muted"), 0, 0);
    serialGrid->addWidget(label("校验", "muted"), 0, 1);
    serialGrid->addWidget(bits, 1, 0);
    serialGrid->addWidget(parity, 1, 1);
    serialGrid->addWidget(label("停止位", "muted"), 2, 0, 1, 2);
    serialGrid->addWidget(stopbits, 3, 0, 1, 2);
    serialGrid->addWidget(label("流控", "muted"), 4, 0, 1, 2);
    serialGrid->addWidget(flow, 5, 0, 1, 2);
    serialBody->addSpacing(6);
    serialBody->addLayout(serialGrid);
    dtr = new QCheckBox("DTR");
    rts = new QCheckBox("RTS");
    auto signalControls = horizontal();
    signalControls->addWidget(dtr);
    signalControls->addWidget(rts);
    signalControls->addStretch();
    serialBody->addLayout(signalControls);
    serialInfo = label({}, "muted");
    serialInfo->setWordWrap(true);
    serialInfo->hide();
    serialBody->addWidget(serialInfo);
    configStack->addWidget(serialPanel);
    auto networkPanel = new QWidget();
    auto network = new QVBoxLayout(networkPanel);
    network->setContentsMargins(0, 4, 0, 0);
    network->setSpacing(8);
    host = new QLineEdit("127.0.0.1");
    remotePort = new QSpinBox();
    remotePort->setRange(1, 65535);
    remotePort->setValue(9000);
    bindHost = new QLineEdit("127.0.0.1");
    bindPort = new QSpinBox();
    bindPort->setRange(0, 65535);
    bindPort->setValue(9001);
    remoteLabel = label("远端地址 / 端口", "muted");
    localLabel = label("本地绑定 / 端口", "muted");
    network->addWidget(remoteLabel);
    network->addWidget(host);
    network->addWidget(remotePort);
    network->addWidget(localLabel);
    network->addWidget(bindHost);
    network->addWidget(bindPort);
    configStack->addWidget(networkPanel);
    auto sshPanel = new QWidget();
    auto sshBody = new QVBoxLayout(sshPanel);
    sshBody->setContentsMargins(0, 4, 0, 0);
    sshBody->setSpacing(8);
    sshBody->addWidget(label("主机别名或地址", "muted"));
    sshTarget = combo({}, {}, true);
    sshTarget->setObjectName("sshHost");
    sshTarget->setInsertPolicy(QComboBox::NoInsert);
    sshTarget->lineEdit()->setPlaceholderText("选择配置或输入地址");
    sshTarget->view()->setMinimumWidth(320);
    connect(sshTarget->lineEdit(), &QLineEdit::textChanged, this, [this](const QString &text)
    {
        sshTarget->setToolTip(text.isEmpty() ? "选择或输入 SSH 主机" : text);
    });
    connect(sshTarget, qOverload<int>(&QComboBox::activated), this, [this]
    {
        sshTarget->lineEdit()->setCursorPosition(0);
    });
    sshBody->addWidget(sshTarget);
    sshBody->addWidget(label("用户名（可选）", "muted"));
    sshUser = new QLineEdit();
    sshUser->setPlaceholderText("User");
    sshBody->addWidget(sshUser);
    sshBody->addWidget(label("端口（可选）", "muted"));
    sshPort = new QLineEdit();
    sshPort->setPlaceholderText("Port / 22");
    sshPort->setValidator(new QIntValidator(1, 65535, sshPort));
    sshBody->addWidget(sshPort);
    sshBody->addWidget(label("私钥文件（可选）", "muted"));
    sshIdentity = new QLineEdit();
    sshIdentity->setPlaceholderText("IdentityFile");
    sshBody->addWidget(sshIdentity);
    for (auto edit : {sshUser, sshPort, sshIdentity})
    {
        edit->setToolTip("留空时使用 ~/.ssh/config 中的设置");
    }
    auto clearSsh = button("清除 SSH 缓存", "outline");
    clearSsh->setObjectName("sshClearCache");
    clearSsh->setToolTip("清除当前主机保存的密码及指纹记录，重新进行连接认证");
    clearSsh->setEnabled(!sshTarget->currentText().trimmed().isEmpty());
    connect(sshTarget->lineEdit(), &QLineEdit::textChanged, clearSsh, [clearSsh](const QString &text)
    {
        clearSsh->setEnabled(!text.trimmed().isEmpty());
    });
    connect(clearSsh, &QPushButton::clicked, this, [this]
    {
        try { clearSshCache(form(), engine, this, theme); }
        catch (const std::exception &exception) { error(QString::fromUtf8(exception.what())); }
    });
    sshBody->addWidget(clearSsh);
    configStack->addWidget(sshPanel);
    refreshSshProfiles();
    sshConfigTimer.setInterval(500);
    connect(&sshConfigTimer, &QTimer::timeout, this, [this]
    {
        if (SshProfiles::fingerprint() != sshConfigFingerprint)
        {
            refreshSshProfiles();
        }
    });
    sshConfigTimer.start();
    side->addWidget(configStack);
    side->addWidget(label("连接名称", "muted"));
    connectionName = new QLineEdit();
    connectionName->setPlaceholderText("可选 · 使用端口或主机名");
    side->addWidget(connectionName);
    auto connectionStatusRow = horizontal();
    connectionStatusRow->setSpacing(5);
    connectionLed = new StatusLed();
    connectionLed->setObjectName("connectionLed");
    connectionState = label("未连接", "connectionState");
    connectionStatusRow->addWidget(connectionLed);
    connectionStatusRow->addWidget(connectionState, 1);
    side->addLayout(connectionStatusRow);
    connectButton = button("建立连接", "primary");
    connectButton->setMinimumHeight(40);
    connect(connectButton, &QPushButton::clicked, this, &Window::connectionAction);
    side->addWidget(connectButton);
    auto scriptRow = horizontal();
    scriptToggle = new QCheckBox("脚本解析");
    scriptToggle->setObjectName("scriptToggle");
    scriptToggle->setToolTip("将新收到的字节交给当前连接的 main(data, length)");
    scriptRow->addWidget(scriptToggle, 1);
    scriptEdit = new IconButton("code", "编辑解析脚本与 HEX 试解析");
    scriptEdit->setObjectName("scriptEdit");
    scriptRow->addWidget(scriptEdit);
    scriptControls = new QWidget();
    scriptControls->setObjectName("scriptControls");
    scriptControls->setLayout(scriptRow);
    side->addWidget(scriptControls);
    scriptHint = label({}, "muted");
    scriptHint->setObjectName("scriptHint");
    scriptHint->setWordWrap(true);
    scriptHint->hide();
    side->addWidget(scriptHint);
    connect(scriptToggle, &QCheckBox::toggled, this, [this](bool enabled)
    {
        if (!parameterPane) { return; }
        try { parameterPane->applyScript(parameterPane->scriptCode, enabled); }
        catch (const ScriptError &failure)
        {
            const QSignalBlocker blocker(scriptToggle);
            scriptToggle->setChecked(!enabled);
            error(failure.display());
        }
    });
    connect(scriptEdit, &QAbstractButton::clicked, this, [this]
    {
        if (parameterPane) { parameterPane->openScriptEditor(); }
    });
    side->addStretch();
    connectionHint = label({}, "connectionHint");
    connectionHint->setWordWrap(true);
    connectionHint->hide();
    side->addWidget(connectionHint);
    auto sideScroll = new QScrollArea();
    sideScroll->setWidgetResizable(true);
    sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideScroll->setWidget(sidebar);
    sidebar->setAutoFillBackground(false);
    sideScroll->viewport()->setAutoFillBackground(false);
    sideScroll->setFixedWidth(qMin(parameterWidth, qMax(220, width() - 54 - 10 - 320)));
    sideScroll->setMinimumHeight(0);
    sideScroll->setObjectName("parameterScroll");
    layout->addWidget(sideScroll);
    parameterGrip = new ParameterGrip(this, root);
    layout->addWidget(parameterGrip);
    auto right = new QWidget();
    auto rightBody = new QVBoxLayout(right);
    rightBody->setContentsMargins(0, 0, 0, 0);
    rightBody->setSpacing(0);
    tabHeader = new QWidget();
    tabHeader->setObjectName("tabHeader");
    tabHeader->setFixedHeight(49);
    auto header = horizontal(tabHeader);
    header->setContentsMargins(6, 4, 12, 0);
    header->setSpacing(0);
    tabs = new ConnectionBar();
    tabs->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabs, &QWidget::customContextMenuRequested, this, &Window::tabMenu);
    auto renameTabShortcut = new QShortcut(QKeySequence(Qt::Key_F2), this);
    renameTabShortcut->setObjectName("renameCurrentTab");
    connect(renameTabShortcut, &QShortcut::activated, this, [this]
    {
        if (!tabs->findChild<QLineEdit *>("tabTitleEditor"))
        {
            beginTabRename(tabs->currentIndex());
        }
    });
    connect(tabs, &QTabBar::currentChanged, this, &Window::activate);
    connect(tabs, &QTabBar::tabCloseRequested, this, &Window::closeTab);
    connect(tabs, &QTabBar::tabMoved, this, [this] { activate(tabs->currentIndex()); });
    connect(tabs, &TearTabBar::tearRequested, this, [this](int index, const QPoint &position)
    {
        detachConnection(index, position, true);
    });
    header->addWidget(tabs, 0, Qt::AlignBottom);
    plus = new IconButton("plus", "新建页面");
    plus->setObjectName("newPage");
    plus->setFixedSize(30, 36);
    connect(plus, &QAbstractButton::clicked, this, [this]
    {
        auto menu = newPageMenu();
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(plus->mapToGlobal(QPoint(0, plus->height() + 4)));
    });
    header->addWidget(plus);
    header->addStretch();
    if (primary)
    {
        auto dock = new IconButton("dock", "将本窗口的页面返回主工作台");
        connect(dock, &QAbstractButton::clicked, this, &QWidget::close);
        header->addWidget(dock);
    }
    rightBody->addWidget(tabHeader);
    stack = new QStackedWidget();
    rightBody->addWidget(stack, 1);
    layout->addWidget(right, 1);
    workspace = right;
    aiToast = new QFrame(right);
    aiToast->setObjectName("aiToast");
    aiToast->setProperty("roundedPopup", true);
    aiToast->setAttribute(Qt::WA_StyledBackground, true);
    aiToast->setMaximumWidth(560);
    auto toastBody = new QHBoxLayout(aiToast);
    toastBody->setContentsMargins(12, 7, 7, 7);
    toastBody->setSpacing(8);
    aiToastText = label("", "aiToastText");
    toastBody->addWidget(aiToastText, 1);
    auto toastDetail = button("详情", "outline");
    toastDetail->setObjectName("aiToastDetail");
    toastDetail->setFixedHeight(26);
    connect(toastDetail, &QPushButton::clicked, this, [this] { showAuditDetail(aiToastEntry); });
    toastBody->addWidget(toastDetail);
    auto toastAll = button("全部记录", "outline");
    toastAll->setObjectName("aiToastAll");
    toastAll->setFixedHeight(26);
    connect(toastAll, &QPushButton::clicked, this, &Window::openAiAudit);
    toastBody->addWidget(toastAll);
    auto toastClose = new IconButton("close", "关闭 AI 活动提示");
    toastClose->setObjectName("aiToastClose");
    toastClose->setFixedSize(24, 24);
    connect(toastClose, &QAbstractButton::clicked, aiToast, &QWidget::hide);
    toastBody->addWidget(toastClose);
    aiToast->hide();
    aiToastTimer.setSingleShot(true);
    aiToastTimer.setInterval(10000);
    connect(&aiToastTimer, &QTimer::timeout, aiToast, &QWidget::hide);
    setCentralWidget(root);
    status = label("尚未连接", "muted");
    aiStatus = label("", "muted");
    aiStatus->hide();
    aiStatus->setMaximumWidth(420);
    mcpStatus = label("", "muted");
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(status, 1);
    statusBar()->addPermanentWidget(aiStatus);
    statusBar()->addPermanentWidget(mcpStatus);
    if (!primary)
    {
        updater = new UpdateService(this);
        updateButton = button("", "updateButton");
        updateButton->setToolTip("查看新版本及更新说明");
        updateProgress = new QProgressBar();
        updateProgress->setObjectName("updateProgress");
        updateProgress->setFixedWidth(90);
        updateProgress->setFixedHeight(8);
        updateProgress->setTextVisible(false);
        statusBar()->addPermanentWidget(updateProgress);
        statusBar()->addPermanentWidget(updateButton);
        updateProgress->hide();
        updateButton->hide();

        connect(updater, &UpdateService::updateAvailable, this, [this](const QString &version)
        {
            updateButton->setText(QString("发现新版本 v%1  ↓").arg(version));
            updateButton->setToolTip(QString("查看 Serial Workbench v%1 的更新说明").arg(version));
            updateButton->show();
        });
        connect(updateButton, &QAbstractButton::clicked, this, [this]
        {
            if (!updateDialog)
            {
                updateDialog = new UpdateDialog(updater, this);
                connect(updateDialog, &UpdateDialog::installRequested, this, &Window::installUpdate);
            }
            updateDialog->refresh();
            updateDialog->showNormal();
            updateDialog->raise();
            updateDialog->activateWindow();
        });
        connect(updater, &UpdateService::downloadProgress, this, [this](qint64 received, qint64 total)
        {
            if (total > 0)
            {
                updateProgress->show();
                updateProgress->setValue(int(received * 100 / total));
                updateButton->setText(QString("正在下载更新 %1% ↓").arg(received * 100 / total));
                updateProgress->setToolTip(QString("已下载 %1 / %2 MB")
                    .arg(received / 1024.0 / 1024.0, 0, 'f', 1)
                    .arg(total / 1024.0 / 1024.0, 0, 'f', 1));
            }
        });
        connect(updater, &UpdateService::downloadFailed, this, [this](const QString &message)
        {
            updateProgress->hide();
            updateButton->setEnabled(true);
            updateButton->setText("更新下载未完成 · 查看并重试");
            statusBar()->showMessage(message, 10000);
        });
        connect(updater, &UpdateService::downloadReady, this, [this](const QString &)
        {
            updateProgress->hide();
            updateButton->setText("更新已下载 · 查看并安装");
            updateButton->setToolTip("安装包已校验，点击打开更新窗口");
        });
    }
    auto versionStatus = label(QString("v%1").arg(Version), "muted");
    versionStatus->setObjectName("currentVersionStatus");
    versionStatus->setContentsMargins(12, 0, 0, 0);
    versionStatus->setToolTip("当前运行版本：Serial Workbench " + Version);
    versionStatus->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    statusBar()->addPermanentWidget(versionStatus);
    statusBar()->setContentsMargins(12, 2, 12, 2);
    connect(protocol, &QComboBox::currentIndexChanged, this, &Window::protocolChanged);
    connect(baud, &QComboBox::activated, this, [this] { applyBaud(); });
    connect(baud->lineEdit(), &QLineEdit::editingFinished, this, &Window::applyBaud);
    connect(port, &QComboBox::currentTextChanged, this, [this] { updateStatus(); });
    connect(port, &QComboBox::activated, this, [this] { refreshPorts(); });
    for (auto control : {bits, parity, stopbits, flow})
    {
        connect(control, &QComboBox::activated, this, [this] { applyBaud(); });
    }
    connect(dtr, &QCheckBox::toggled, this, [this] { applyBaud(); });
    connect(rts, &QCheckBox::toggled, this, [this] { applyBaud(); });
    connect(connectionName, &QLineEdit::editingFinished, this, [this]
    {
        if (parameterPane && parameterPane->channel && !connectionName->text().trimmed().isEmpty())
        {
            parameterPane->channel->name = connectionName->text().trimmed();
            parameterPane->channel->config.name = connectionName->text().trimmed();
            syncConnections();
        }
    });
    newConnection();
    Config initial = Config::fromJson(saved["connection"].toObject());
    if (initial.port == "loop://")
    {
        initial.port = "NULL";
    }
    active->draft = initial;
    loadForm(initial);
    auto initialPane = active;
    if (!primary)
    {
        active->restore(appearance);
        for (auto view = viewSettings.begin(); view != viewSettings.end(); ++view)
        {
            restoreInstruments(view.value().toObject(), view.key());
        }
        for (const auto &value : migrateInstrumentPages(saved["widget_pages"].toArray()))
        {
            const auto settings = value.toObject();
            auto page = newInstrument(settings["kind"].toString(), nullptr, false, settings["source_key"].toString());
            if (page) { page->restore(settings); }
        }
    }
    tabs->setCurrentIndex(paneIndex(tabs, initialPane));
    activate(tabs->currentIndex());
    applyLocalTheme(theme.dark);
    connect(engine, &Engine::channelsChanged, this, &Window::syncConnections);
    timer.setInterval(40);
    settingsRefresh.start();
    connect(&timer, &QTimer::timeout, this, [this]
    {
        for (auto pane : panes)
        {
            pane->tick();
        }
        updateStatus();
        if (settingsDialog && mcpConnections && settingsRefresh.elapsed() >= 250
            && (settingsDialog->isVisible() || mcpConnections->isVisible() || auditTable->isVisible()))
        {
            settingsRefresh.restart();
            mcpConnections->setUpdatesEnabled(false);
            mcpConnections->clear();
            for (auto channel : engine->channels)
            {
                new QTreeWidgetItem(mcpConnections, {channel->name, channel->config.kind, stateText(channel->state), channel->id, QString("%1 / %2").arg(channel->rx).arg(channel->tx)});
            }
            mcpConnections->setUpdatesEnabled(true);
            if (auditTable->property("lastSequence").toLongLong() != mcp->calls)
            {
                auditTable->setProperty("lastSequence", mcp->calls);
                const auto scroll = auditTable->verticalScrollBar();
                const bool follow = scroll->value() >= scroll->maximum() - 2;
                const int previousScroll = scroll->value();
                const auto selected = auditTable->currentItem();
                const auto top = auditTable->itemAt(QPoint(1, 1));
                const qint64 selectedSequence = selected ? selected->data(0, Qt::UserRole).toJsonObject()["seq"].toInteger() : -1;
                const qint64 topSequence = top ? top->data(0, Qt::UserRole).toJsonObject()["seq"].toInteger() : -1;
                QTreeWidgetItem *restoredSelection = nullptr;
                QTreeWidgetItem *restoredTop = nullptr;
                auditTable->setUpdatesEnabled(false);
                auditTable->clear();
                for (const auto &value : mcp->audit)
                {
                    const auto item = value.toObject();
                    auto row = new QTreeWidgetItem(auditTable, {item["time"].toString().mid(11, 12), item["client"].toString(),
                        item["summary"].toString().isEmpty() ? item["tool"].toString() : item["summary"].toString(),
                        item["status"] == "ok" ? "成功" : "失败", QString::number(item["duration_ms"].toInteger()), item["error"].toString()});
                    row->setData(0, Qt::UserRole, item);
                    row->setToolTip(0, item["tool"].toString());
                    if (item["seq"].toInteger() == selectedSequence) { restoredSelection = row; }
                    if (item["seq"].toInteger() == topSequence) { restoredTop = row; }
                }
                if (restoredSelection) { auditTable->setCurrentItem(restoredSelection); }
                auditTable->setUpdatesEnabled(true);
                if (follow && auditTable->topLevelItemCount() > 0)
                {
                    auditTable->scrollToItem(auditTable->topLevelItem(auditTable->topLevelItemCount() - 1));
                }
                else if (restoredTop)
                {
                    auditTable->scrollToItem(restoredTop, QAbstractItemView::PositionAtTop);
                }
                else
                {
                    scroll->setValue(previousScroll);
                }
            }
        }
    });
    timer.start();
    refreshPorts();
    portTimer.setInterval(500);
    connect(&portTimer, &QTimer::timeout, this, &Window::refreshPorts);
    portTimer.start();
    port->setFocus();
    if (!primary) { installMcpUi(); }
    if (updater && QGuiApplication::platformName() != "offscreen"
        && !QCoreApplication::arguments().contains("--screenshot"))
    {
        QTimer::singleShot(2000, updater, &UpdateService::check);
        auto updateTimer = new QTimer(updater);
        updateTimer->setInterval(6 * 60 * 60 * 1000);
        connect(updateTimer, &QTimer::timeout, updater, &UpdateService::check);
        updateTimer->start();
    }
}

Window *Window::rootWindow()
{
    return primary ? primary : this;
}

Pane *Window::findPane(Channel *channel)
{
    for (auto hostWindow : rootWindow()->hosts)
    {
        if (!hostWindow)
        {
            continue;
        }
        for (auto pane : hostWindow->panes)
        {
            if (!pane->isInstrument() && pane->channel == channel)
            {
                return pane;
            }
        }
    }
    return nullptr;
}

void Window::showPane(Pane *pane)
{
    if (!pane || !pane->window)
    {
        return;
    }
    Window *host = pane->window;
    const int index = paneIndex(host->tabs, pane);
    if (index < 0)
    {
        return;
    }
    host->tabs->setCurrentIndex(index);
    host->activate(index);
    if (host->isMinimized())
    {
        host->showNormal();
    }
    host->raise();
    host->activateWindow();
}

void Window::showAuditDetail(const QJsonObject &entry)
{
    if (entry.isEmpty())
    {
        return;
    }
    auto dialog = new QDialog(settingsDialog ? static_cast<QWidget *>(settingsDialog) : this);
    dialog->setObjectName("mcpCallDetail");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("MCP 调用详情 · " + entry["tool"].toString());
    dialog->setStyleSheet(theme.stylesheet());
    dialog->resize(820, 620);
    auto body = new QVBoxLayout(dialog);
    auto text = new QPlainTextEdit(mcpAuditDetail(entry));
    text->setObjectName("mcpCallDetailText");
    text->setReadOnly(true);
    text->setFont(dataFont);
    text->setLineWrapMode(QPlainTextEdit::NoWrap);
    body->addWidget(text, 1);
    auto actions = new QHBoxLayout();
    auto copy = button("复制详情", "outline");
    connect(copy, &QPushButton::clicked, dialog, [text] { QApplication::clipboard()->setText(text->toPlainText()); });
    auto close = button("关闭", "primary");
    connect(close, &QPushButton::clicked, dialog, &QDialog::accept);
    actions->addStretch();
    actions->addWidget(copy);
    actions->addWidget(close);
    body->addLayout(actions);
    dialog->show();
}

void Window::showAiActivity(const QJsonObject &entry)
{
    aiToastEntry = entry;
    const bool ok = entry["status"].toString() == "ok";
    QString action = entry["summary"].toString();
    if (action.isEmpty()) { action = entry["tool"].toString(); }
    const QString status = ok ? QStringLiteral("成功") : QStringLiteral("失败");
    if (aiToastText)
    {
        aiToastText->setText(aiToastText->fontMetrics().elidedText(
            QString("AI · %1 · %2").arg(action, status), Qt::ElideMiddle, 300));
        QString tip = QString("%1\n客户端：%2\n时间：%3\n耗时：%4 ms")
            .arg(action, entry["client"].toString(), entry["time"].toString())
            .arg(entry["duration_ms"].toInteger());
        if (!ok) { tip += "\n错误：" + entry["error"].toString(); }
        aiToastText->setToolTip(tip);
    }
    if (aiToast)
    {
        aiToast->adjustSize();
        positionAiToast();
        aiToast->show();
        aiToast->raise();
        aiToastTimer.start();
    }
    if (aiStatus)
    {
        const auto elided = aiStatus->fontMetrics().elidedText(action, Qt::ElideMiddle, 240);
        aiStatus->setText(QString("AI · %1 · %2").arg(elided, status));
        aiStatus->setToolTip(aiToastText ? aiToastText->toolTip() : action);
        aiStatus->show();
    }
}

void Window::positionAiToast()
{
    if (!aiToast || !workspace || !tabHeader)
    {
        return;
    }
    aiToast->adjustSize();
    aiToast->move(qMax(8, workspace->width() - aiToast->width() - 14), tabHeader->height() + 52);
}

void Window::openAiAudit()
{
    openSettings();
    if (!settingsDialog)
    {
        return;
    }
    if (auto pages = settingsDialog->findChild<DetachableTabs *>("settingsPages"))
    {
        pages->setCurrentIndex(1);
    }
    if (auto tabs = settingsDialog->findChild<DetachableTabs *>("mcpPages"))
    {
        tabs->setCurrentIndex(2);
    }
    settingsDialog->raise();
    settingsDialog->activateWindow();
}

Window *Window::detachConnection(int index, const QPoint &position, bool systemMove)
{
    auto pane = tabPane(tabs, index);
    if (!pane)
    {
        return nullptr;
    }
    auto detached = new Window(engine, mcp, settingsPath, rootWindow());
    // Remove only the new window's unused placeholder; the real Pane is moved intact.
    auto placeholder = detached->active;
    detached->tabs->blockSignals(true);
    detached->tabs->removeTab(0);
    detached->tabs->blockSignals(false);
    detached->stack->removeWidget(placeholder);
    detached->panes.clear();
    detached->active = nullptr;
    delete placeholder;
    transferPane(pane, detached);
    detached->resize(1100, 760);
    detached->move(position - QPoint(100, 14));
    detached->show();
    detached->raise();
    detached->activateWindow();
    if (systemMove)
    {
        beginNativeMove(detached);
    }
    return detached;
}

void Window::transferPane(Pane *pane, Window *target)
{
    if (!pane || target == this || !panes.contains(pane))
    {
        return;
    }
    if (active == pane && !pane->isInstrument() && !pane->channel)
    {
        try
        {
            pane->draft = form();
        }
        catch (...)
        {
        }
    }
    if (pane->repeat) { pane->repeat->setChecked(false); }
    const QSignalBlocker blocker(tabs);
    tabs->removeTab(paneIndex(tabs, pane));
    panes.removeOne(pane);
    stack->removeWidget(pane);
    if (active == pane)
    {
        active = nullptr;
    }
    target->attachPane(pane);
    if (!closing)
    {
        if (panes.isEmpty())
        {
            newConnection();
        }
        else
        {
            activate(tabs->currentIndex());
        }
    }
    adjustTabs();
    target->syncConnections();
}

Pane *Window::newConnection()
{
    auto pane = new Pane(this);
    attachPane(pane);
    return pane;
}

Pane *Window::newInstrument(const QString &kind, Channel *source, bool activatePage, const QString &pendingKey)
{
    const QString canonical = kind == "values" ? "waveform" : kind == "structured" ? "json" : kind;
    if (!QStringList {"waveform", "json", "xml", "control"}.contains(canonical))
    {
        return nullptr;
    }
    if (canonical == "control" && source && source->config.kind != "serial") { source = nullptr; }
    if (source && source->config.kind == "demo")
    {
        try
        {
            if ((canonical == "json" || canonical == "xml") && !source->config.isStructuredDemo()) { source = engine->structuredDemo(); }
            else if (canonical == "waveform" && source->config.isStructuredDemo()) { source = engine->waveformDemo(); }
        }
        catch (const std::exception &failure)
        {
            error(QString::fromUtf8(failure.what()));
            return nullptr;
        }
    }
    auto previous = active;
    auto page = new Pane(this, kind);
    const QString baseTitle = canonical == "waveform" ? "数值波形" : canonical == "control" ? "串口控制" : canonical.toUpper();
    QStringList titles;
    for (auto host : rootWindow()->hosts)
    {
        if (!host) { continue; }
        for (auto existing : host->panes)
        {
            if (existing->instrument) { titles.append(existing->pageTitle); }
        }
    }
    page->pageTitle = baseTitle;
    int number = 2;
    while (titles.contains(page->pageTitle))
    {
        page->pageTitle = baseTitle + " " + QString::number(number++);
    }
    page->sourceKey = source ? source->config.key() : (canonical != "control" || pendingKey.startsWith("serial:") ? pendingKey : QString());
    attachPane(page);
    if (!activatePage && previous && panes.contains(previous))
    {
        tabs->setCurrentIndex(paneIndex(tabs, previous));
        activate(tabs->currentIndex());
    }
    return page;
}

QMenu *Window::newPageMenu()
{
    auto menu = new QMenu(this);
    menu->setObjectName("newPageMenu");
    auto receive = menu->addAction(icon("receive", theme.text), "新建收发页面", this, [this] { newConnection(); });
    receive->setObjectName("newReceivePage");
    menu->addSeparator();
    auto widgets = menu->addMenu(icon("widgets", theme.text), "新建控件页面");
    widgets->setObjectName("newWidgetPage");
    const QStringList kinds {"waveform", "json", "xml", "control"};
    const QStringList titles {"数值波形", "JSON", "XML", "串口控制"};
    for (int i = 0; i < kinds.size(); ++i)
    {
        const auto kind = kinds[i];
        auto action = widgets->addAction(icon(instrumentIconName(kind), theme.muted), titles[i], this, [this, kind]
        {
            newInstrument(kind, active ? active->channel.data() : nullptr);
        });
        action->setObjectName("new_" + kind);
    }
    return menu;
}

void Window::restoreInstruments(const QJsonObject &settings, const QString &sourceKey)
{
    if (primary || saved.contains("widget_pages"))
    {
        return;
    }
    auto controls = settings["instruments"].toArray();
    if (!settings.contains("instruments") && settings["show_wave"].toBool())
    {
        controls.append(QJsonObject {{"kind", "waveform"}});
    }
    if (settings["parser"].toInt() && std::none_of(controls.begin(), controls.end(), [](const auto &value)
        { return value.toObject()["kind"] == "structured"; }))
    {
        controls.append(QJsonObject {{"kind", "structured"}, {"parser", settings["parser"].toInt() - 1}});
    }
    QJsonArray pending;
    for (const auto &entry : controls)
    {
        auto object = entry.toObject();
        object["source_key"] = sourceKey;
        pending.append(object);
    }
    controls = migrateInstrumentPages(pending);
    // Preserve repeated widgets while avoiding duplicate migrations on reconnect.
    QMap<QString, int> occurrences;
    for (const auto &value : controls)
    {
        auto object = value.toObject();
        const auto kind = object["kind"].toString();
        const int ordinal = ++occurrences[kind];
        int existing = 0;
        for (auto host : rootWindow()->hosts)
        {
            if (!host) { continue; }
            for (auto pane : host->panes)
            {
                if (pane->instrument && pane->sourceKey == sourceKey && pane->instrument->kind == kind)
                {
                    ++existing;
                }
            }
        }
        if (existing >= ordinal) { continue; }
        object["source_key"] = sourceKey;
        auto page = newInstrument(kind, nullptr, false, sourceKey);
        if (page) { page->restore(object); }
    }
}

void Window::attachPane(Pane *pane)
{
    pane->window = this;
    panes.append(pane);
    stack->addWidget(pane);
    tabs->blockSignals(true);
    const QString title = !pane->pageTitle.isEmpty() ? pane->pageTitle
        : pane->channel ? pane->channel->name : "新收发页";
    const int index = tabs->addTab(title);
    if (pane->instrument)
    {
        tabs->setTabIcon(index, icon(instrumentIconName(pane->instrument->kind), theme.muted));
        pane->refreshSource();
    }
    tabs->setTabData(index, QVariant::fromValue(static_cast<QObject *>(pane)));
    auto close = new IconButton("close", pane->isInstrument() ? "关闭控件页面" : "关闭收发页面及连接");
    close->setFixedSize(26, 26);
    close->theme = theme;
    connect(close, &QAbstractButton::clicked, this, [this, pane] { closeTab(paneIndex(tabs, pane)); });
    tabs->setTabButton(index, QTabBar::RightSide, tabCloseSlot(close));
    tabs->blockSignals(false);
    tabs->setCurrentIndex(index);
    activate(index);
    adjustTabs();
    pane->applyAppearance();
    if (primary)
    {
        setWindowTitle(title + " · Serial Workbench");
    }
}

void Window::adjustTabs()
{
    tabs->setFixedWidth(std::min(tabs->sizeHint().width(), std::max(64, tabHeader->width() - (primary ? 120 : 80))));
}

void Window::activate(int index)
{
    auto pane = tabPane(tabs, index);
    if (!pane || syncing)
    {
        return;
    }
    if (parameterPane && !parameterPane->channel)
    {
        try
        {
            parameterPane->draft = form();
        }
        catch (...)
        {
        }
    }
    if (active && active != pane && panes.contains(active) && active->repeat)
    {
        active->repeat->setChecked(false);
    }
    active = pane;
    stack->setCurrentWidget(pane);
    syncParameterPane(true);
    auto parameters = findChild<QScrollArea *>("parameterScroll");
    parameters->setVisible(parameterToggle->isChecked());
    parameterGrip->setVisible(parameterToggle->isChecked());
    parameterToggle->setEnabled(true);
    updateStatus();
    if (primary)
    {
        setWindowTitle(tabs->tabText(index) + " · Serial Workbench");
    }
}

void Window::syncParameterPane(bool force)
{
    Pane *target = active && !active->isInstrument() ? active : nullptr;
    Pane *fallback = nullptr;
    for (auto hostWindow : rootWindow()->hosts)
    {
        if (!hostWindow) { continue; }
        for (auto candidate : hostWindow->panes)
        {
            if (candidate->isInstrument()) { continue; }
            if (!fallback || candidate == parameterPane) { fallback = candidate; }
            if (!target && active && active->isInstrument())
            {
                const auto key = candidate->channel ? candidate->channel->config.key() : candidate->draft.key();
                if ((active->channel && candidate->channel == active->channel)
                    || (!active->sourceKey.isEmpty() && key == active->sourceKey))
                {
                    target = candidate;
                }
            }
        }
    }
    if (!target && (!active || !active->isInstrument() || active->sourceKey.isEmpty()))
    {
        target = fallback;
    }
    const bool configChanged = target && target->channel && target->channel->config.json() != parameterConfig;
    if (target != parameterPane || force || configChanged)
    {
        if (parameterPane && parameterPane != target && !parameterPane->channel)
        {
            try { parameterPane->draft = form(); }
            catch (const std::exception &) { }
        }
        parameterPane = target;
        if (target) { loadForm(target->channel ? target->channel->config : target->draft); }
    }
    protocol->setEnabled(target);
    configStack->setEnabled(target);
    connectionName->setEnabled(target);
}

Config Window::form() const
{
    Config c;
    c.kind = QStringList {"serial", "tcp-client", "tcp-server", "udp", "ssh"}[protocol->currentIndex()];
    c.name = connectionName->text().trimmed();
    c.port = port->currentText().trimmed();
    bool ok = false;
    c.baudrate = baud->currentText().toInt(&ok);
    if (!ok)
    {
        fail("波特率需要有效整数");
    }
    c.bytesize = bits->currentText().toInt();
    c.parity = parity->currentText();
    c.stopbits = stopbits->currentText().toDouble();
    c.flow = flow->currentText();
    c.dtr = dtr->isChecked();
    c.rts = rts->isChecked();
    c.host = host->text().trimmed();
    c.networkPort = remotePort->value();
    c.bindHost = bindHost->text().trimmed();
    c.bindPort = bindPort->value();
    c.sshHost = sshTarget->currentText().trimmed();
    c.sshUser = sshUser->text().trimmed();
    const QString sshPortValue = sshPort->text().trimmed();
    c.sshPort = sshPortValue.isEmpty() ? 0 : sshPortValue.toInt(&ok);
    if (c.kind == "ssh" && !sshPortValue.isEmpty() && (!ok || c.sshPort < 1 || c.sshPort > 65535))
    {
        fail("SSH 端口需要 1 到 65535 的整数");
    }
    c.sshIdentity = sshIdentity->text().trimmed();
    return c;
}

void Window::loadForm(const Config &c)
{
    syncing = true;
    parameterConfig = c.json();
    connectionName->setText(c.name);
    const int portIndex = port->findText(c.port, Qt::MatchFixedString);
    const QString selectedPort = portIndex >= 0 ? port->itemText(portIndex)
        : !portsLoaded || c.port == "loop://" || holdsSerialPort(c.port) ? c.port : "NULL";
    port->setCurrentText(selectedPort);
    if (parameterPane && !parameterPane->channel)
    {
        parameterPane->draft.port = selectedPort;
    }
    baud->setCurrentText(QString::number(c.baudrate));
    bits->setCurrentText(QString::number(c.bytesize));
    parity->setCurrentText(c.parity);
    stopbits->setCurrentText(QString::number(c.stopbits));
    flow->setCurrentText(c.flow);
    dtr->setChecked(c.dtr);
    rts->setChecked(c.rts);
    host->setText(c.host);
    remotePort->setValue(c.networkPort);
    bindHost->setText(c.bindHost);
    bindPort->setValue(c.bindPort);
    sshTarget->setCurrentText(c.sshHost);
    sshTarget->lineEdit()->setCursorPosition(0);
    sshTarget->setToolTip(c.sshHost.isEmpty() ? "选择或输入 SSH 主机" : c.sshHost);
    sshUser->setText(c.sshUser);
    sshPort->setText(c.sshPort ? QString::number(c.sshPort) : QString());
    sshIdentity->setText(c.sshIdentity);
    const int index = QStringList {"serial", "tcp-client", "tcp-server", "udp", "ssh"}.indexOf(c.kind);
    if (index >= 0)
    {
        protocol->setCurrentIndex(index);
    }
    protocolChanged();
    syncing = false;
}

void Window::protocolChanged()
{
    const int index = protocol->currentIndex();
    if (!syncing && parameterPane && !parameterPane->channel)
    {
        parameterPane->draft.kind = QStringList {"serial", "tcp-client", "tcp-server", "udp", "ssh"}[index];
    }
    configStack->setCurrentIndex(index == 0 ? 0 : index == 4 ? 2 : 1);
    connectionHint->setText(index == 4
        ? "读取 ~/.ssh/config 中的主机别名\n使用系统密钥或代理登录"
        : QString());
    connectionHint->setVisible(index == 4);
    for (auto widget : QList<QWidget *> {remoteLabel, host, remotePort})
    {
        widget->setVisible(index == 1 || index == 3);
    }
    for (auto widget : QList<QWidget *> {localLabel, bindHost, bindPort})
    {
        widget->setVisible(index == 2 || index == 3);
    }
    configStack->updateGeometry();
    if (!syncing) { updateStatus(); }
}

void Window::refreshSshProfiles()
{
    const QString current = sshTarget->currentText();
    const bool currentWasProfile = sshTarget->findText(current, Qt::MatchExactly) >= 0;
    const QStringList profiles = SshProfiles::aliases();
    const QSignalBlocker blocker(sshTarget);
    sshTarget->clear();
    sshTarget->addItems(profiles);
    sshTarget->setCurrentIndex(-1);
    const QString selected = currentWasProfile && !profiles.contains(current) ? QString() : current;
    sshTarget->setEditText(selected);
    sshTarget->lineEdit()->setCursorPosition(0);
    sshTarget->setToolTip(selected.isEmpty() ? "选择或输入 SSH 主机" : selected);
    sshConfigFingerprint = SshProfiles::fingerprint();
}

bool Window::holdsSerialPort(const QString &name) const
{
    auto channel = parameterPane ? parameterPane->channel.data() : nullptr;
    return channel && channel->config.kind == "serial" && channel->config.port == name
        && !channel->manualClose && (channel->state == "connected" || channel->state == "reconnecting");
}

void Window::refreshPorts()
{
    if (closing) { return; }
    const auto available = portProvider();
    engine->checkSerialDevices(available);
    const bool changed = !portsLoaded || available != lastPorts;
    portsLoaded = true;
    lastPorts = available;

    const auto selected = port->currentText();
    auto edit = port->lineEdit();
    const int cursor = edit->cursorPosition();
    const int start = edit->selectionStart();
    const int length = edit->selectedText().size();
    const QSignalBlocker blocker(port);
    const QSignalBlocker editBlocker(edit);

    if (changed)
    {
        QStringList names;
        for (const auto &value : available)
        {
            const auto name = value.toObject()["port"].toString();
            if (!name.isEmpty() && name != "NULL" && !names.contains(name))
            {
                names.append(name);
            }
        }

        // Keep the model and popup alive; change only affected rows.
        for (int index = port->count() - 1; index > 0; --index)
        {
            if (!names.contains(port->itemText(index)))
            {
                port->removeItem(index);
            }
        }
        for (int index = 0; index < names.size(); ++index)
        {
            const int row = index + 1;
            const auto &name = names[index];
            if (port->itemText(row) != name)
            {
                const int previous = port->findText(name);
                if (previous >= 0) { port->removeItem(previous); }
                port->insertItem(row, name);
            }
        }
        for (const auto &value : available)
        {
            const auto item = value.toObject();
            const int index = port->findText(item["port"].toString());
            if (index <= 0) { continue; }
            port->setItemData(index, item["port"].toString() + "\n" + portDescription(item, true), Qt::ToolTipRole);
            port->setItemData(index, item, Qt::UserRole + 2);
            port->setItemData(index, item["usb_serial_jtag"].toBool(), Qt::UserRole + 1);
        }
    }

    const int selectedIndex = port->findText(selected, Qt::MatchFixedString);
    const bool missing = selectedIndex < 0 && selected != "loop://" && !holdsSerialPort(selected);
    const auto next = missing || selected.isEmpty() ? QString("NULL")
        : selectedIndex >= 0 ? port->itemText(selectedIndex) : selected;
    if (port->currentText() != next)
    {
        port->setCurrentText(next);
    }
    if (next == selected)
    {
        edit->setCursorPosition(cursor);
        if (start >= 0) { edit->setSelection(start, length); }
    }
    if (parameterPane && !parameterPane->channel)
    {
        parameterPane->draft.port = next;
    }
    if (changed)
    {
        for (auto pane : panes)
        {
            if (pane->instrument) { pane->refreshSource(); }
        }
    }
    updateStatus();
}

void Window::connectionAction()
{
    if (!active) { return; }
    syncParameterPane();
    if (!parameterPane) { newConnection(); return; }
    QPointer<Pane> widgetPage = active->isInstrument() ? active : nullptr;
    auto draft = parameterPane.data();
    try
    {
        refreshPorts();
        const auto config = form();
        auto current = draft->channel;
        const bool missingActivePort = current && current->config.kind == "serial"
            && config.kind == "serial" && config.port == "NULL";
        if (current && (current->config.key() == config.key() || missingActivePort) && (current->state == "connected" || current->state == "listening" || current->state == "connecting" || current->state == "reconnecting"))
        {
            draft->repeat->setChecked(false);
            current->close();
        }
        else
        {
            const QSignalBlocker engineBlocker(engine);
            syncing = true;
            auto channel = engine->open(config);
            syncing = false;
            Pane *existing = findPane(channel);
            if (existing && !widgetPage)
            {
                auto hostWindow = existing->window;
                hostWindow->tabs->setCurrentIndex(paneIndex(hostWindow->tabs, existing));
                hostWindow->activate(paneIndex(hostWindow->tabs, existing));
                hostWindow->showNormal();
                hostWindow->raise();
                hostWindow->activateWindow();
            }
            else if (!existing && !draft->channel)
            {
                draft->setChannel(channel);
                if (rootWindow()->viewSettings.contains(config.key()))
                {
                    draft->restore(rootWindow()->viewSettings[config.key()].toObject());
                }
            }
            else if (!existing)
            {
                auto pane = newConnection();
                pane->setChannel(channel);
                if (rootWindow()->viewSettings.contains(config.key()))
                {
                    pane->restore(rootWindow()->viewSettings[config.key()].toObject());
                }
                loadForm(config);
            }
            if (widgetPage)
            {
                widgetPage->sourceKey = channel->config.key();
                widgetPage->refreshSource();
                tabs->setCurrentIndex(paneIndex(tabs, widgetPage));
                activate(paneIndex(tabs, widgetPage));
            }
            for (auto hostWindow : rootWindow()->hosts)
            {
                if (hostWindow) { hostWindow->syncConnections(); }
            }
        }
    }
    catch (const std::exception &exception)
    {
        syncing = false;
        error(QString::fromUtf8(exception.what()));
    }
    updateStatus();
}

void Window::applyBaud()
{
    auto channel = parameterPane ? parameterPane->channel.data() : nullptr;
    if (syncing || !channel || channel->state != "connected" || channel->config.kind != "serial")
    {
        return;
    }
    try
    {
        const auto config = form();
        if (config.key() == channel->config.key() && config.json() != channel->config.json())
        {
            channel->applySerial(config);
            for (auto hostWindow : rootWindow()->hosts)
            {
                if (hostWindow) { hostWindow->syncParameterPane(); }
            }
            statusBar()->showMessage(QString("驱动已确认波特率：%1 bps").arg(channel->appliedBaud), 4000);
        }
    }
    catch (const std::exception &exception)
    {
        const QString message = QString::fromUtf8(exception.what());
        loadForm(channel->config);
        error(message);
    }
    updateStatus();
}

void Window::syncConnections()
{
    if (syncing || closing)
    {
        return;
    }
    for (auto pane : QList<Pane *>(panes))
    {
        if (!pane->isInstrument() && pane->channel && !engine->channels.contains(pane->channel->id))
        {
            const int index = paneIndex(tabs, pane);
            tabs->blockSignals(true);
            tabs->removeTab(index);
            tabs->blockSignals(false);
            panes.removeOne(pane);
            stack->removeWidget(pane);
            pane->deleteLater();
        }
    }
    for (auto channel : engine->channels)
    {
        if (channel->config.isBuiltinDemo()) { continue; }
        Pane *pane = nullptr;
        for (auto item : panes)
        {
            if (!item->isInstrument() && item->channel == channel)
            {
                pane = item;
                break;
            }
        }
        if (!pane)
        {
            if (primary || findPane(channel))
            {
                continue;
            }
            const auto previous = active;
            pane = newConnection();
            pane->setChannel(channel);
            pane->restore(viewSettings[channel->config.key()].toObject());
            if (previous && panes.contains(previous))
            {
                tabs->setCurrentIndex(paneIndex(tabs, previous));
                activate(paneIndex(tabs, previous));
            }
        }
        const int index = paneIndex(tabs, pane);
        QColor color = channel->state == "connected" || channel->state == "listening" ? theme.green
            : channel->state == "error" ? theme.red : channel->state == "connecting" || channel->state == "reconnecting" ? theme.amber : theme.muted;
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRect(4, 4, 8, 8);
        painter.end();
        tabs->setTabIcon(index, QIcon(pixmap));
        const auto tabTitle = pane->pageTitle.isEmpty() ? channel->name : pane->pageTitle;
        tabs->setTabText(index, tabTitle);
        tabs->setTabToolTip(index, tabTitle + " · " + stateText(channel->state) + "\n" + channel->error);
    }
    for (auto pane : panes)
    {
        if (pane->instrument)
        {
            pane->refreshSource();
            const int index = paneIndex(tabs, pane);
            tabs->setTabIcon(index, icon(instrumentIconName(pane->instrument->kind), theme.muted));
            tabs->setTabToolTip(index, pane->sourceKey.isEmpty() ? "控件页面 · 请选择数据来源" : "数据来源：" + pane->sourceKey);
        }
    }
    if (panes.isEmpty())
    {
        active = nullptr;
        newConnection();
    }
    else if (!panes.contains(active))
    {
        active = nullptr;
        activate(tabs->currentIndex());
    }
    syncParameterPane();
    adjustTabs();
    updateStatus();
    if (primary && active)
    {
        setWindowTitle(tabs->tabText(paneIndex(tabs, active)) + " · Serial Workbench");
    }
}

void Window::closeTab(int index)
{
    auto pane = tabPane(tabs, index);
    if (!pane)
    {
        return;
    }
    if (!pane->isInstrument() && pane->channel)
    {
        rootWindow()->viewSettings[pane->channel->config.key()] = pane->settings();
        engine->remove(pane->channel->id);
        return;
    }
    tabs->blockSignals(true);
    tabs->removeTab(index);
    tabs->blockSignals(false);
    panes.removeOne(pane);
    stack->removeWidget(pane);
    pane->deleteLater();
    if (panes.isEmpty())
    {
        active = nullptr;
        newConnection();
    }
    else
    {
        active = nullptr;
        activate(tabs->currentIndex());
    }
    adjustTabs();
}

void Window::updateStatus()
{
    if (!active || syncing)
    {
        return;
    }
    auto channel = active->channel;
    auto parameterChannel = parameterPane ? parameterPane->channel.data() : nullptr;
    const bool serialScript = parameterPane && parameterPane->supportsScript() && protocol->currentIndex() == 0;
    scriptControls->setVisible(serialScript);
    scriptToggle->setEnabled(serialScript);
    scriptEdit->setEnabled(serialScript);
    {
        const QSignalBlocker blocker(scriptToggle);
        scriptToggle->setChecked(serialScript && (parameterChannel ? parameterChannel->scriptEnabled : parameterPane->scriptRequested));
    }
    const QString scriptFault = parameterChannel ? parameterChannel->scriptError : QString();
    scriptHint->setVisible(serialScript && !scriptFault.isEmpty());
    scriptHint->setText(!scriptFault.isEmpty() ? "解析已停用：" + scriptFault
        : !parameterChannel ? "连接后开始接收字节" : QString("已解析 %1 帧").arg(parameterChannel->scriptFrames));
    const bool same = parameterChannel && parameterChannel->config.kind == QStringList {"serial", "tcp-client", "tcp-server", "udp", "ssh"}[protocol->currentIndex()]
        && (parameterChannel->config.kind != "serial" || parameterChannel->config.port == port->currentText() || port->currentText() == "NULL")
        && (parameterChannel->config.kind != "ssh" || (parameterChannel->config.sshHost == sshTarget->currentText().trimmed()
            && parameterChannel->config.sshUser == sshUser->text().trimmed()
            && parameterChannel->config.sshPort == sshPort->text().trimmed().toInt()
            && parameterChannel->config.sshIdentity == sshIdentity->text().trimmed()));
    const bool ready = parameterChannel && (parameterChannel->state == "connected" || parameterChannel->state == "listening" || parameterChannel->state == "connecting" || parameterChannel->state == "reconnecting");
    QString actionText = parameterPane ? "建立连接" : "新建收发页面";
    if (same && ready)
    {
        actionText = parameterChannel->state == "reconnecting" ? "停止重连"
            : parameterChannel->state == "connecting" ? "取消连接"
            : parameterChannel->state == "listening" ? "停止监听" : "断开连接";
    }
    connectButton->setText(actionText);

    const QString connectionStatus = same ? parameterChannel->state : QString();
    auto ledState = StatusLed::State::Offline;
    QString connectionText = "未连接";
    if (connectionStatus == "connected" || connectionStatus == "listening")
    {
        ledState = StatusLed::State::Online;
        connectionText = connectionStatus == "listening" ? "监听中" : "已连接";
    }
    else if (connectionStatus == "connecting" || connectionStatus == "reconnecting")
    {
        ledState = StatusLed::State::Pending;
        connectionText = connectionStatus == "reconnecting" ? "重连中" : "连接中";
    }
    else if (connectionStatus == "error")
    {
        ledState = StatusLed::State::Error;
        connectionText = "连接异常";
    }
    else if (connectionStatus == "closed")
    {
        connectionText = "已断开";
    }
    connectionLed->setState(ledState);
    connectionState->setText(connectionText);
    const QString connectionTip = same && !parameterChannel->error.isEmpty()
        ? connectionText + "\n" + parameterChannel->error : connectionText;
    connectionLed->setAccessibleName(connectionText);
    connectionLed->setToolTip(connectionTip);
    connectionState->setToolTip(connectionTip);
    connectButton->setAccessibleDescription(connectionText);

    if (channel)
    {
        QString text = (active->isInstrument() ? channel->name + "    " : QString()) + stateText(channel->state);
        if (channel->config.kind == "serial" && channel->appliedBaud)
        {
            text += QString("    %1 · %2 bps").arg(channel->config.port).arg(channel->appliedBaud);
        }
        text += QString("    接收 %1 B    发送 %2 B").arg(channel->rx).arg(channel->tx);
        status->setText(text);
        status->setToolTip(channel->error);
    }
    else
    {
        status->setText(active->isInstrument()
            ? (active->sourceKey.isEmpty() ? "控件页面    请选择数据来源" : "控件页面    等待数据来源连接")
            : protocol->currentIndex() == 4 ? "尚未连接    选择 SSH 主机后建立连接"
            : "尚未连接    选择端口开始接收");
    }
    const auto selectedPort = port->currentText();
    auto metadata = port->itemData(port->findText(selectedPort, Qt::MatchFixedString), Qt::UserRole + 2).toJsonObject();
    if (metadata["port"].toString().compare(selectedPort, Qt::CaseInsensitive) != 0)
    {
        metadata = {};
    }
    const bool serialConnection = parameterChannel && same && parameterChannel->config.kind == "serial";
    if (serialConnection && !metadata.isEmpty())
    {
        parameterPane->portMetadata = metadata;
    }
    else if (serialConnection && parameterPane->portMetadata["port"].toString().compare(selectedPort, Qt::CaseInsensitive) == 0)
    {
        metadata = parameterPane->portMetadata;
    }
    const auto details = portDescription(metadata);
    portDetails->setText(details);
    portDetails->setVisible(!details.isEmpty());
    const auto tooltip = selectedPort == "NULL" ? "选择串口；设备插拔时列表会自动更新"
        : selectedPort + "\n" + portDescription(metadata, true);
    port->setToolTip(tooltip);
    portDetails->setToolTip(tooltip);

    QString serialText = selectedPort == "NULL" ? QString()
        : serialConnection && parameterChannel->state == "reconnecting" ? QString("%1 已断开，等待设备恢复").arg(parameterChannel->config.port)
        : serialConnection && parameterChannel->appliedBaud ? QString("驱动已确认 %1 bps").arg(parameterChannel->appliedBaud) : QString();
    if (metadata["usb_serial_jtag"].toBool())
    {
        if (!serialText.isEmpty())
        {
            serialText += '\n';
        }
        serialText += "USB Serial/JTAG · 速率由 USB 决定";
    }
    serialInfo->setText(serialText);
    serialInfo->setVisible(!serialText.isEmpty());
    mcpStatus->setText(mcp->isListening() ? QString("MCP 就绪  ·  %1").arg(mcp->serverPort()) : "MCP 离线");
    mcpStatus->setToolTip(mcp->error.isEmpty() ? mcp->url() : mcp->error);
    QStringList mismatches;
    for (const auto &value : mcp->agentChecks)
    {
        const auto check = value.toObject();
        if (!check["matches"].toBool())
        {
            mismatches.append(check["name"].toString() + "：" + check["status"].toString());
        }
    }
    if (!mismatches.isEmpty())
    {
        mcpStatus->setText(mcpStatus->text() + QString(" · %1 个 Agent 待检查").arg(mismatches.size()));
        mcpStatus->setToolTip(mcpStatus->toolTip() + "\n" + mismatches.join('\n') + "\n设置 → AI / MCP → Agent 接入可更新配置");
    }
}

void Window::applyTheme(bool dark)
{
    if (primary)
    {
        primary->applyTheme(dark);
        return;
    }
    for (auto hostWindow : hosts)
    {
        if (hostWindow) { hostWindow->applyLocalTheme(dark); }
    }
    for (auto widget : QApplication::topLevelWidgets())
    {
        if (qobject_cast<QFileDialog *>(widget))
        {
            continue;
        }
        if (!qobject_cast<FloatingPage *>(widget) && !qobject_cast<QDialog *>(widget))
        {
            continue;
        }
        widget->setStyleSheet(theme.stylesheet());
        widget->setPalette(palette());
        widget->setFont(Theme::interfaceFont());
        for (auto control : widget->findChildren<IconButton *>())
        {
            control->theme = theme;
            control->update();
        }
    }
    // Styling a top-level dialog can reset child palettes; reapply the
    // theme-color controls (including the About Link color) afterwards.
    for (auto hostWindow : hosts)
    {
        if (hostWindow) { hostWindow->syncThemeColorControls(); }
    }
}

void Window::applyLocalTheme(bool dark)
{
    themeColor = rootWindow()->themeColor;
    themeContrast = rootWindow()->themeContrast;
    theme = Theme::make(dark, themeColor, themeContrast);
    connectionLed->theme = theme;
    connectionLed->update();
    setStyleSheet(workspaceStyle());
    backgroundSurface->base = theme.background;
    backgroundSurface->background.set(windowBackground, windowBackgroundImage);
    backgroundSurface->update();
    if (!primary) { updateWindowChrome(theme, windowBackground, windowBackgroundImage); }
    QPalette palette;
    palette.setColor(QPalette::Window, theme.background);
    palette.setColor(QPalette::Base, theme.panel);
    palette.setColor(QPalette::Button, theme.panel);
    palette.setColor(QPalette::Text, theme.text);
    palette.setColor(QPalette::WindowText, theme.text);
    palette.setColor(QPalette::ButtonText, theme.text);
    palette.setColor(QPalette::Highlight, theme.accent);
    palette.setColor(QPalette::HighlightedText, theme.onAccent);
    palette.setColor(QPalette::Link, theme.accent);
    palette.setColor(QPalette::LinkVisited, theme.accent);
    palette.setColor(QPalette::PlaceholderText, theme.muted);
    palette.setColor(QPalette::Mid, theme.muted);
    setPalette(palette);
    for (auto control : findChildren<IconButton *>())
    {
        control->theme = theme;
        control->update();
    }
    for (auto pane : panes)
    {
        pane->applyAppearance();
        pane->replay();
    }
    syncConnections();
    syncThemeColorControls();
}

void Window::setDataFont(const QFont &font)
{
    if (primary)
    {
        primary->setDataFont(font);
        return;
    }
    dataFont = font;
    dataFont.setFamilies({font.family(), "Microsoft YaHei UI"});
    dataFont.setPointSize(qBound(8, font.pointSize(), 32));
    dataFont.setStyleStrategy(dataFont.family().startsWith("Fusion Pixel") ? QFont::NoAntialias : QFont::PreferAntialias);
    for (auto hostWindow : hosts)
    {
        if (!hostWindow) { continue; }
        hostWindow->dataFont = dataFont;
        for (auto pane : hostWindow->panes) { pane->applyAppearance(); }
    }
    saveSettings();
}

void Window::error(const QString &message)
{
    statusBar()->showMessage("操作失败：" + message, 12000);
}

void Window::tabMenu(const QPoint &point)
{
    auto pane = tabPane(tabs, tabs->tabAt(point));
    if (!pane)
    {
        return;
    }
    QMenu menu(this);
    auto renameTab = menu.addAction("重命名标签\tF2", this, [this, pane]
    {
        QPointer<Pane> selected = pane;
        QTimer::singleShot(0, this, [this, selected]
        {
            if (selected)
            {
                beginTabRename(paneIndex(tabs, selected));
            }
        });
    });
    renameTab->setObjectName("renameTab");
    menu.addSeparator();
    if (!pane->isInstrument() && pane->channel)
    {
        auto channel = pane->channel;
        const bool ready = channel->state == "connected" || channel->state == "listening" || channel->state == "connecting" || channel->state == "reconnecting";
        auto disconnect = menu.addAction("断开连接", this, [channel, pane] { pane->repeat->setChecked(false); channel->close(); });
        disconnect->setEnabled(ready);
        auto reconnect = menu.addAction("重新连接", this, [channel] { channel->open(); });
        reconnect->setEnabled(!ready);
        menu.addAction("重命名连接名称", this, [this, channel]
        {
            bool accepted = false;
            const auto name = QInputDialog::getText(this, "重命名连接", "名称", QLineEdit::Normal, channel->name, &accepted);
            if (accepted && !name.trimmed().isEmpty())
            {
                channel->name = name.trimmed();
                channel->config.name = name.trimmed();
                if (active && active->channel == channel)
                {
                    connectionName->setText(name.trimmed());
                }
                syncConnections();
            }
        });
        menu.addSeparator();
        menu.addAction("导出日志…", pane, &Pane::exportLog);
    }
    if (!pane->isInstrument()) { menu.addAction("清空显示", pane, &Pane::clearView); }
    menu.addAction("移至独立窗口", this, [this, pane]
    {
        detachConnection(paneIndex(tabs, pane), QCursor::pos());
    });
    menu.addSeparator();
    menu.addAction(pane->isInstrument() ? "关闭控件页面" : "关闭收发页面及连接", this, [this, pane] { closeTab(paneIndex(tabs, pane)); });
    menu.exec(tabs->mapToGlobal(point));
}

void Window::beginTabRename(int index)
{
    auto pane = tabPane(tabs, index);
    if (!pane)
    {
        return;
    }
    if (auto previous = static_cast<TabTitleEditor *>(tabs->findChild<QLineEdit *>("tabTitleEditor")))
    {
        previous->finish(false);
    }
    tabs->setCurrentIndex(index);
    auto editor = new TabTitleEditor(tabs);
    editor->setObjectName("tabTitleEditor");
    editor->setFrame(false);
    editor->setMaxLength(80);
    editor->setText(tabs->tabText(index));
    QStyleOptionTab option;
    tabs->initStyleOption(&option, index);
    // Use the style's text area so icon sizes, padding and close buttons stay clear.
    auto textRect = tabs->style()->subElementRect(QStyle::SE_TabBarTabText, &option, tabs);
    const auto tabRect = tabs->tabRect(index);
    // Grow the editor across the free space up to the close button so the title
    // is not clipped by the field frame, while still clearing the tab icon.
    if (auto *closeButton = tabs->tabButton(index, QTabBar::RightSide))
    {
        if (closeButton->geometry().left() > textRect.left())
        {
            textRect.setRight(closeButton->geometry().left() - 4);
        }
    }
    // Anchor the underline just below the title text instead of the tab box.
    const int textHeight = editor->fontMetrics().height();
    const int center = tabRect.center().y();
    textRect.setTop(center - textHeight / 2 - 3);
    textRect.setBottom(center + textHeight / 2 + 2);
    editor->setGeometry(textRect);
    QPointer<Pane> selected = pane;
    editor->onFinish = [this, editor, selected](bool accept)
    {
        if (accept && selected && selected->window == this)
        {
            const auto name = editor->text().trimmed();
            const int currentIndex = paneIndex(tabs, selected);
            if (currentIndex >= 0 && !name.isEmpty())
            {
                selected->pageTitle = name;
                tabs->setTabText(currentIndex, name);
                if (primary && selected == active)
                {
                    setWindowTitle(name + " · Serial Workbench");
                }
                syncConnections();
                adjustTabs();
                saveSettings();
            }
        }
    };
    editor->show();
    editor->raise();
    editor->setFocus();
    editor->selectAll();
}

void Window::saveSettings()
{
    if (primary)
    {
        primary->saveSettings();
        return;
    }
    QJsonArray widgetPages;
    Pane *receivePage = nullptr;
    for (auto hostWindow : hosts)
    {
        if (!hostWindow) { continue; }
        if (hostWindow->settingsDialog && hostWindow->settingsDialog->isVisible())
        {
            saved["settings_geometry"] = QString::fromLatin1(hostWindow->settingsDialog->saveGeometry().toBase64());
        }
        for (auto pane : hostWindow->panes)
        {
            if (pane->isInstrument())
            {
                widgetPages.append(pane->settings());
                continue;
            }
            if (!receivePage || pane == active) { receivePage = pane; }
            if (pane->channel)
            {
                viewSettings[pane->channel->config.key()] = pane->settings();
            }
        }
    }
    QJsonObject appearance = receivePage ? receivePage->settings() : saved["appearance"].toObject();
    appearance.remove("instruments");
    appearance["theme"] = theme.dark ? "dark" : "light";
    if (themeColor.isValid())
    {
        appearance["theme_color"] = themeColor.name(QColor::HexRgb);
    }
    else
    {
        appearance.remove("theme_color");
    }
    if (themeContrast.isValid())
    {
        appearance["contrast_color"] = themeContrast.name(QColor::HexRgb);
    }
    else
    {
        appearance.remove("contrast_color");
    }
    appearance["pixel_theme"] = true;
    appearance["readable_type"] = true;
    appearance["font_spec"] = dataFont.toString();
    appearance["font_family"] = dataFont.family();
    appearance["font_size"] = dataFont.pointSize();
    appearance["parameter_width"] = parameterWidth;
    appearance["window_background"] = windowBackground.json();
    appearance["receive_background"] = receiveBackground.json();
    QJsonObject connection = saved["connection"].toObject();
    try
    {
        if (parameterPane) { connection = form().json(); }
        else if (receivePage) { connection = (receivePage->channel ? receivePage->channel->config : receivePage->draft).json(); }
    }
    catch (...)
    {
    }
    QSaveFile file(settingsPath);
    QDir().mkpath(QFileInfo(settingsPath).absolutePath());
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(QJsonObject {{"connection", connection}, {"appearance", appearance}, {"views", viewSettings},
            {"widget_pages", widgetPages}, {"templates", templates}, {"allow_ai_control", engine->allowControl},
            {"mcp_protocols", mcp->protocolAccess()},
            {"settings_geometry", saved["settings_geometry"]}, {"mcp_port", mcp->isListening() ? int(mcp->serverPort()) : saved["mcp_port"].toInt(8765)}}).toJson(QJsonDocument::Indented));
        if (!file.commit())
        {
            error(file.errorString());
        }
    }
    else
    {
        error(file.errorString());
    }
}

void Window::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    QTimer::singleShot(0, this, &Window::adjustTabs);
    QTimer::singleShot(0, this, &Window::arrangeParameters);
    positionAiToast();
}

void Window::resizeParameters(int width)
{
    if (!centralWidget()) { return; }
    const int preferred = qBound(220, width, 4096);
    for (auto host : rootWindow()->hosts)
    {
        if (!host) { continue; }
        host->parameterWidth = preferred;
        host->arrangeParameters();
    }
}

void Window::arrangeParameters()
{
    auto scroll = findChild<QScrollArea *>("parameterScroll");
    if (!scroll || !centralWidget())
    {
        return;
    }
    auto body = qobject_cast<QHBoxLayout *>(centralWidget()->layout());
    const bool narrow = width() < 900;
    const bool floating = body->indexOf(scroll) < 0;
    if (narrow && !floating)
    {
        body->removeWidget(scroll);
        body->removeWidget(parameterGrip);
        scroll->hide();
        parameterGrip->hide();
        const QSignalBlocker blocker(parameterToggle);
        parameterToggle->setChecked(false);
    }
    else if (!narrow && floating)
    {
        body->insertWidget(1, scroll);
        body->insertWidget(2, parameterGrip);
        scroll->setVisible(parametersVisible && active);
        const QSignalBlocker blocker(parameterToggle);
        parameterToggle->setChecked(parametersVisible);
    }
    // Reserve the workspace's real minimum width. A fixed 320 px was too small
    // and let the scroll area overlap the grip; the pane minimum is the real
    // boundary the layout enforces.
    const int reserve = workspace ? qMax(320, workspace->minimumSizeHint().width()) : 320;
    const int maximum = qMax(220, centralWidget()->width() - rail->width() - 10 - (narrow ? 0 : reserve));
    const int visibleWidth = qBound(220, parameterWidth, maximum);
    scroll->setFixedWidth(visibleWidth);
    parameterGrip->setVisible(!scroll->isHidden());
    if (narrow)
    {
        scroll->setGeometry(rail->width(), 0, visibleWidth, centralWidget()->height());
        parameterGrip->setGeometry(rail->width() + visibleWidth, 0, 10, centralWidget()->height());
        scroll->raise();
        parameterGrip->raise();
    }
    else
    {
        // Keep the grip aligned with the fixed-width scroll area even when the
        // layout pass is deferred; otherwise it can sit over the form controls.
        parameterGrip->setGeometry(rail->width() + visibleWidth, 0, 10, centralWidget()->height());
    }
    status->setMaximumWidth(qMax(80, width() - (narrow ? 70 : 210)));
    mcpStatus->setVisible(!narrow);
    body->activate();
    QTimer::singleShot(0, this, &Window::adjustTabs);
}

void Window::closeEvent(QCloseEvent *event)
{
    closing = true;
    timer.stop();
    portTimer.stop();
    sshConfigTimer.stop();
    if (primary)
    {
        if (!primary->closing)
        {
            for (auto pane : QList<Pane *>(panes))
            {
                transferPane(pane, primary);
            }
            primary->raise();
        }
        primary->hosts.removeAll(this);
        event->accept();
        return;
    }
    saveSettings();
    for (auto hostWindow : QList<QPointer<Window>>(hosts))
    {
        if (hostWindow && hostWindow != this)
        {
            hostWindow->close();
        }
    }
    for (auto tabs : findChildren<DetachableTabs *>())
    {
        tabs->restoreAll();
    }
    engine->allowControl = false;
    for (auto channel : engine->channels)
    {
        channel->close();
    }
    event->accept();
}

bool Window::nativeEvent(const QByteArray &type, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    const auto native = static_cast<MSG *>(message);
    if (native->message == WM_DEVICECHANGE)
    {
        QTimer::singleShot(150, this, &Window::refreshPorts);
    }
    if (primary && native->message == WM_EXITSIZEMOVE)
    {
        for (auto target : primary->hosts)
        {
            if (target && target != this && cursorOver(target->tabs, QCursor::pos()))
            {
                QPointer<Window> guarded(target);
                QTimer::singleShot(0, this, [this, guarded] {
                    if (!guarded)
                    {
                        return;
                    }
                    closing = true;
                    for (auto pane : QList<Pane *>(panes))
                    {
                        transferPane(pane, guarded);
                    }
                    close();
                });
                break;
            }
        }
    }
#endif
    return QMainWindow::nativeEvent(type, message, result);
}

void Window::waveHelp()
{
    auto dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("数值报文格式");
    dialog->setMinimumSize(360, 240);
    dialog->setSizeGripEnabled(true);
    dialog->resize(640, 540);
    auto body = new QVBoxLayout(dialog);
    auto content = new QTextBrowser();
    content->setHtml(R"(<h2>波形专用协议 wave/1</h2>
        <pre>@wave count=120,temperature=25.50,voltage=3.300</pre>
        <p>每个采样独占一行，必须以小写 <b>@wave 加一个空格</b>开头，以 CR、LF 或 CRLF 结束。串口、TCP、UDP 均支持。</p>
        <p>字段名就是通道名，与字段顺序无关。名称可用字母、汉字、数字和下划线，不能以数字开头，最长 64 个字符；同一帧不能重名。</p>
        <p>用逗号分隔 name=value 字段，每帧 1～16 个通道。支持整数、负数、小数及科学计数法，例如 -12、3.3、1.25e-3。字段两侧可有空格或 Tab。</p>
        <h3>单片机 C / C++ 示例</h3>
        <pre>printf("@wave count=%d,temperature=%.2f,voltage=%.3f\r\n",
       count, (double)temperature, (double)voltage);</pre>
        <p>普通日志、无前缀数字/CSV/JSON、自定义二进制帧均不进入波形和实时数值。脚本结果与 JSON/XML 控件仍独立处理。</p>
        <p>无效专用帧整帧丢弃并计入格式错误，不生成部分采样。每行最多 8192 字节，每个来源保留 5000 个采样。</p>
        <p>滚轮缩放，放大后按住左键平移，双击恢复全部采样。</p>)");
    body->addWidget(content);
    auto done = button("关闭");
    connect(done, &QPushButton::clicked, dialog, &QDialog::close);
    body->addWidget(done);
    dialog->show();
}

void Window::openSettings()
{
    rootWindow()->checkAgentEndpoints();
    if (settingsDialog)
    {
        settingsDialog->show();
        settingsDialog->raise();
        settingsDialog->activateWindow();
        return;
    }
    settingsDialog = new QDialog(this);
    settingsDialog->setWindowTitle("设置");
    settingsDialog->setWindowFlag(Qt::WindowMaximizeButtonHint);
    settingsDialog->setSizeGripEnabled(true);
    settingsDialog->setMinimumSize(440, 320);
    settingsDialog->resize(920, 680);
    connect(settingsDialog, &QDialog::finished, this, [this]
    {
        rootWindow()->saved["settings_geometry"] = QString::fromLatin1(settingsDialog->saveGeometry().toBase64());
        saveSettings();
    });
    auto body = new QVBoxLayout(settingsDialog);
    body->setContentsMargins(20, 16, 20, 16);
    auto pages = new DetachableTabs();
    pages->setObjectName("settingsPages");
    auto appearance = new QWidget();
    auto appearanceBody = new QVBoxLayout(appearance);
    appearanceBody->setContentsMargins(20, 24, 20, 20);
    appearanceBody->addSpacing(4);
    auto formLayout = new QFormLayout();
    formLayout->setVerticalSpacing(18);
    formLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto themeChoice = combo({"浅色主题", "深色主题"});
    themeChoice->setObjectName("themeChoice");
    themeChoice->setCurrentIndex(theme.dark ? 1 : 0);
    connect(themeChoice, &QComboBox::currentIndexChanged, this, [this](int index)
    {
        applyTheme(index == 1);
        saveSettings();
    });
    auto font = button(dataFont.family(), "outline");
    font->setObjectName("dataFontChoice");
    connect(font, &QPushButton::clicked, this, [this, font]
    {
        // Keep standard dialog labels Chinese in standalone installations too.
        // Qt removes this scoped translator when the dialog has closed.
        QTranslator translations;
        if (translations.load(":/translations/qtbase_zh_CN.qm"))
        {
            QCoreApplication::installTranslator(&translations);
        }

        QFontDialog dialog(dataFont, settingsDialog);
        dialog.setOption(QFontDialog::DontUseNativeDialog);
        dialog.setWindowTitle("数据字体");
        dialog.setPalette(palette());
        dialog.setStyleSheet(theme.stylesheet());
        // The themed group boxes and list padding need more room than the
        // platform default, otherwise the effects section is clipped on open.
        dialog.setMinimumSize(560, 560);
        dialog.resize(620, 620);
        if (dialog.exec() == QDialog::Accepted)
        {
            const auto selected = dialog.selectedFont();
            setDataFont(selected);
            font->setText(selected.family());
            if (auto size = settingsDialog->findChild<QSpinBox *>("dataFontSize"))
            {
                const QSignalBlocker blocker(size);
                size->setValue(dataFont.pointSize());
            }
        }
    });
    auto size = new QSpinBox();
    size->setObjectName("dataFontSize");
    size->setRange(8, 32);
    size->setValue(dataFont.pointSize());
    connect(size, &QSpinBox::valueChanged, this, [this](int value)
    {
        auto font = dataFont;
        font.setPointSize(value);
        setDataFont(font);
    });
    formLayout->addRow("界面主题", themeChoice);
    formLayout->addRow("数据字体", font);
    formLayout->addRow("数据字号", size);
    appearanceBody->addLayout(formLayout);
    appearanceBody->addWidget(themeColorSettingsPanel());
    auto resetFont = button("恢复推荐字体", "outline");
    connect(resetFont, &QPushButton::clicked, this, [this, font, size]
    {
        setDataFont(Theme::defaultDataFont());
        font->setText(dataFont.family());
        const QSignalBlocker blocker(size);
        size->setValue(dataFont.pointSize());
    });
    appearanceBody->addWidget(resetFont, 0, Qt::AlignLeft);
    auto fontHint = label("界面使用清晰中文字体。数据区可自选字体，Ctrl + 滚轮调整字号。", "muted");
    fontHint->setWordWrap(true);
    appearanceBody->addWidget(fontHint);
    appearanceBody->addWidget(backgroundSettingsPanel());
    appearanceBody->addStretch();
    pages->addTab(scrollPanel(appearance, "appearanceScroll"), "外观");

    auto mcpPage = new QWidget();
    auto mcpBody = new QVBoxLayout(mcpPage);
    mcpBody->setContentsMargins(16, 16, 16, 8);
    auto portRow = horizontal();
    portRow->addWidget(label("监听端口"));
    auto mcpPort = new QSpinBox();
    mcpPort->setObjectName("mcpPort");
    mcpPort->setRange(1, 65535);
    mcpPort->setValue(mcp->isListening() ? mcp->serverPort() : saved["mcp_port"].toInt(8765));
    mcpPort->setFixedWidth(120);
    portRow->addWidget(mcpPort);
    auto applyPort = button("应用端口");
    applyPort->setObjectName("applyMcpPort");
    auto availablePort = button("自动选空闲");
    availablePort->setObjectName("availableMcpPort");
    portRow->addWidget(applyPort);
    portRow->addWidget(availablePort);
    portRow->addStretch();
    mcpBody->addLayout(portRow);
    auto address = new QLineEdit(mcp->url());
    address->setObjectName("mcpAddress");
    address->setReadOnly(true);
    auto addressRow = horizontal();
    addressRow->addWidget(address, 1);
    auto copy = button("复制地址");
    addressRow->addWidget(copy);
    connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(mcp->url()); });
    mcpBody->addLayout(addressRow);
    auto portResult = label("修改端口后，点击「一键更新已安装 Agent」同步接入地址。", "muted");
    portResult->setObjectName("mcpPortResult");
    portResult->setWordWrap(true);
    portResult->hide();
    applyPort->setToolTip("应用并保存端口，然后使用一键更新同步 Agent 地址");
    availablePort->setToolTip("立即切换到系统分配的空闲端口，再使用一键更新同步 Agent 地址");
    portResult->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    mcpBody->addWidget(portResult);
    auto permission = new QCheckBox("允许 AI 发送报文、控制连接和应用脚本");
    permission->setChecked(engine->allowControl);
    connect(permission, &QCheckBox::toggled, this, [this](bool checked)
    {
        engine->allowControl = checked;
        saveSettings();
    });
    mcpBody->addWidget(permission);
    auto protocolRow = horizontal();
    protocolRow->addWidget(label("向 AI 开放"));
    const QStringList protocolKinds {"serial", "tcp", "udp", "ssh"};
    const QStringList protocolTitles {"串口", "TCP", "UDP", "SSH"};
    for (int i = 0; i < protocolKinds.size(); ++i)
    {
        const auto kind = protocolKinds[i];
        auto enabled = new QCheckBox(protocolTitles[i]);
        enabled->setObjectName("mcpAccess_" + kind);
        enabled->setChecked(mcp->protocolEnabled(kind));
        enabled->setToolTip(kind == "tcp"
            ? "允许 AI 读取和访问 TCP 客户端、服务端会话"
            : "允许 AI 读取和访问此协议的会话");
        connect(enabled, &QCheckBox::toggled, this, [this, kind](bool checked)
        {
            mcp->setProtocolEnabled(kind, checked);
            saveSettings();
        });
        connect(mcp, &McpServer::protocolAccessChanged, enabled, [this, enabled, kind]
        {
            const QSignalBlocker blocker(enabled);
            enabled->setChecked(mcp->protocolEnabled(kind));
        });
        protocolRow->addWidget(enabled);
    }
    protocolRow->addStretch();
    mcpBody->addLayout(protocolRow);
    auto protocolHint = label("关闭后，AI 无法查看该协议的连接、历史和解析结果，也无法操作连接。本地连接不受影响。", "muted");
    protocolHint->setWordWrap(true);
    mcpBody->addWidget(protocolHint);
    mcpSummary = label("界面与 AI 共享同一连接，各自读取，不会消耗对方的数据。", "muted");
    mcpSummary->setWordWrap(true);

    auto mcpTabs = new DetachableTabs();
    mcpTabs->setObjectName("mcpPages");
    mcpTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    mcpTabs->setMinimumHeight(160);
    mcpConnections = new QTreeWidget();
    mcpConnections->setRootIsDecorated(false);
    mcpConnections->setHeaderLabels({"连接", "协议", "状态", "连接 ID", "RX / TX"});
    mcpConnections->setColumnWidth(0, 160);
    mcpConnections->setColumnWidth(1, 110);
    mcpConnections->setColumnWidth(2, 140);
    auto connectionPage = new QWidget();
    auto connectionBody = new QVBoxLayout(connectionPage);
    connectionBody->setContentsMargins(0, 8, 0, 0);
    connectionBody->addWidget(mcpSummary);
    connectionBody->addWidget(mcpConnections, 1);
    mcpTabs->addTab(connectionPage, "连接总览");
    auto clients = new AgentPanel(mcp, nullptr, rootWindow()->agentEnvironment);
    connect(clients, &AgentPanel::configurationUpdated, rootWindow(), &Window::checkAgentEndpoints);
    connect(mcp, &McpServer::portChanged, settingsDialog, [this, address, mcpPort, clients](quint16 port)
    {
        address->setText(mcp->url());
        mcpPort->setValue(port);
        clients->refresh();
    });
    mcpTabs->addTab(scrollPanel(clients, "clientsScroll"), "Agent 接入");
    mcpTabs->setCurrentIndex(1);
    const auto switchPort = [this, mcpPort, address, portResult, clients](quint16 requested)
    {
        portResult->show();
        if (!mcp->changePort(requested))
        {
            portResult->setText(mcp->error);
            return;
        }
        mcpPort->setValue(mcp->serverPort());
        address->setText(mcp->url());
        portResult->setText("端口已生效并保存。点击「一键更新已安装 Agent」同步配置；串口连接与数据继续保留。");
        clients->refresh();
        saveSettings();
        updateStatus();
    };
    connect(applyPort, &QPushButton::clicked, this, [switchPort, mcpPort] { switchPort(static_cast<quint16>(mcpPort->value())); });
    connect(availablePort, &QPushButton::clicked, this, [switchPort] { switchPort(0); });
    auditTable = new QTreeWidget();
    auditTable->setRootIsDecorated(false);
    auditTable->setHeaderLabels({"时间", "客户端", "调用", "结果", "耗时 ms", "错误"});
    auditTable->setColumnWidth(0, 115);
    auditTable->setColumnWidth(1, 150);
    auditTable->setColumnWidth(2, 220);
    auditTable->setColumnWidth(3, 70);
    auditTable->setColumnWidth(4, 80);
    auditTable->header()->setStretchLastSection(true);
    auditTable->setToolTip("双击一行查看参数、返回与错误详情");
    connect(auditTable, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item)
    {
        showAuditDetail(item->data(0, Qt::UserRole).toJsonObject());
    });
    mcpTabs->addTab(auditTable, "调用日志");
    mcpTabs->setTabToolTip(mcpTabs->count() - 1, "双击一行查看调用的参数、返回与错误详情");
    mcpBody->addWidget(mcpTabs, 1);
    pages->addTab(scrollPanel(mcpPage, "mcpScroll"), "AI / MCP");
    auto about = new QWidget();
    about->setObjectName("aboutPage");
    auto aboutBody = new QVBoxLayout(about);
    aboutBody->setContentsMargins(24, 24, 24, 24);
    aboutBody->setSpacing(14);
    const auto aboutText = [](const QString &text, const QString &name)
    {
        auto item = label(text, name);
        item->setWordWrap(true);
        item->setTextInteractionFlags(Qt::TextSelectableByMouse);
        item->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        return item;
    };
    aboutBody->addWidget(aboutText("Serial Workbench", "aboutTitle"));
    aboutBody->addWidget(aboutText(QString("版本 %1 · Qt %2 / C++17")
                                     .arg(Version, QString::fromLatin1(qVersion())), "aboutVersion"));
    auto sources = new QLabel(
        "<a href=\"https://github.com/lilinng-163/serial-workbench\">GitHub 仓库</a>"
        " &nbsp;·&nbsp; "
        "<a href=\"https://162.14.72.194/\">项目主页 / 镜像站</a>");
    sources->setObjectName("aboutSources");
    sources->setTextFormat(Qt::RichText);
    sources->setTextInteractionFlags(Qt::TextBrowserInteraction);
    sources->setOpenExternalLinks(true);
    sources->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    aboutBody->addWidget(sources);
    aboutBody->addWidget(aboutText("面向嵌入式开发的串口、网络与 SSH 调试工作台。", "aboutSummary"));
    aboutBody->addSpacing(8);

    auto features = new QGridLayout();
    features->setHorizontalSpacing(20);
    features->setVerticalSpacing(18);
    features->setColumnMinimumWidth(0, 84);
    features->setColumnStretch(1, 1);
    const auto addFeature = [&](int row, const QString &title, const QString &description)
    {
        features->addWidget(label(title, "aboutSection"), row, 0, Qt::AlignTop);
        features->addWidget(aboutText(description, "aboutDetail"), row, 1);
    };
    addFeature(0, "连接方式", "串口 · TCP 客户端 / 服务端 · UDP · SSH\n"
                                "自动识别串口插拔，读取 SSH 主机配置，支持密钥登录。");
    addFeature(1, "收发与终端", "文本 / HEX 收发、搜索、日志导出与循环发送。\n"
                                  "ANSI 交互终端，支持中文输入、复制粘贴与快捷键。");
    addFeature(2, "数据控件", "数值波形与实时数值面板 · JSON、XML 独立解析\n"
                                "自定义帧由 C 风格脚本解析，在收发页独立展示。");
    addFeature(3, "标签与窗口", "标签重命名、拖动排序、分离为独立窗口与重新停靠。");
    addFeature(4, "AI / MCP", "HTTP / STDIO 共享会话，界面与 AI 各自读取同一份数据。\n"
                              "支持 Codex、Claude Code、OpenCode CLI、Claude Desktop 和 Pi Agent。\n"
                              "发送报文与控制连接需在 AI / MCP 中启用权限。");
    aboutBody->addLayout(features);
    aboutBody->addStretch();
    pages->addTab(scrollPanel(about, "aboutScroll"), "关于");
    auto history = new ReleaseHistoryPanel();
    auto historyPage = scrollPanel(history, "releaseHistoryScroll");
    pages->addTab(historyPage, "版本回溯");
    connect(history, &ReleaseHistoryPanel::installRequested, this, &Window::installUpdate);
    connect(pages, &QTabWidget::currentChanged, history, [pages, history, historyPage, checked = false](int index) mutable
    {
        if (checked || pages->widget(index) != historyPage) { return; }
        checked = true;
        if (QGuiApplication::platformName() != "offscreen"
            && !QCoreApplication::arguments().contains("--screenshot"))
        {
            history->refreshHistory();
        }
    });
    body->addWidget(pages, 1);
    auto footer = horizontal();
    footer->addStretch();
    auto done = button("完成", "primary");
    connect(done, &QPushButton::clicked, settingsDialog, &QDialog::accept);
    footer->addWidget(done);
    body->addLayout(footer);
    applyTheme(theme.dark);
    const auto geometry = rootWindow()->saved["settings_geometry"].toString().toLatin1();
    if (!geometry.isEmpty())
    {
        settingsDialog->restoreGeometry(QByteArray::fromBase64(geometry));
    }
    settingsDialog->show();
}

void Window::installUpdate(const QString &path)
{
    if (!QProcess::startDetached(path, {}, QFileInfo(path).absolutePath()))
    {
        statusBar()->showMessage("无法启动更新安装程序", 10000);
        return;
    }
    QApplication::closeAllWindows();
}

void Window::configureClient(int index, QLabel *result)
{
    try
    {
        if (!mcp->isListening()) { fail("MCP 未启动，请先选择可用端口"); }
        const auto clients = AgentConfig::discover();
        if (index < 0 || index >= clients.size()) { fail("未知客户端"); }
        result->setText(AgentConfig::configure(clients[index], mcp->url(),
            QCoreApplication::applicationDirPath() + "/SerialWorkbenchMCP.exe"));
    }
    catch (const std::exception &exception)
    {
        result->setText("未完成：" + QString::fromUtf8(exception.what()));
    }
}
}
