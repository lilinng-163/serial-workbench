#include "window.h"
#include "ssh_profiles.h"
#include <QTextCodec>
#include <numeric>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace sw
{
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
    result->setCursor(Qt::PointingHandCursor);
    return result;
}

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

Instrument::Instrument(const QString &type, QWidget *parent) : QWidget(parent), kind(type)
{
    setObjectName("page");
    auto body = new QVBoxLayout(this);
    body->setContentsMargins(16, 12, 16, 8);
    auto sourceRow = horizontal();
    sourceRow->addWidget(label("数据来源", "muted"));
    source = new Combo();
    source->setObjectName("instrumentSource");
    source->setMinimumWidth(180);
    source->setMaximumWidth(360);
    sourceRow->addWidget(source, 1);
    sourceRow->addStretch();
    body->addLayout(sourceRow);
    auto tools = horizontal();
    freeze = new QCheckBox("冻结显示");
    tools->addWidget(freeze);
    tools->addStretch();
    body->addLayout(tools);
    if (kind == "waveform")
    {
        points = new QSpinBox();
        points->setRange(100, 5000);
        points->setValue(1000);
        points->setSuffix(" 点");
        points->setFixedWidth(112);
        auto fit = button("自动缩放");
        auto help = button("数据格式");
        tools->addWidget(points);
        tools->addWidget(fit);
        tools->addWidget(help);
        plot = new WavePlot();
        body->addWidget(plot, 1);
        connect(fit, &QPushButton::clicked, this, [this]
        {
            plot->zoom = 1;
            plot->offset = 0;
            plot->update();
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
        if (kind == "values")
        {
            table->setHeaderLabels({"变量", "当前值", "来源", "采样序号"});
            table->setColumnWidth(0, 200);
            table->setColumnWidth(1, 150);
            table->setColumnWidth(2, 180);
            body->addWidget(table, 1);
        }
        else
        {
            parser = combo({"JSON / XML", "仅 JSON", "仅 XML"});
            parser->setMinimumWidth(145);
            filter = new SearchBox();
            filter->setPlaceholderText("筛选解析记录");
            filter->setMinimumWidth(100);
            filter->setMaximumWidth(210);
            sourceRow->addWidget(filter);
            connect(filter, &QLineEdit::textChanged, this, [this](const QString &text)
            {
                for (int i = 0; i < table->topLevelItemCount(); ++i)
                {
                    auto item = table->topLevelItem(i);
                    item->setHidden(!item->data(0, Qt::UserRole).toJsonObject()["text"].toString().contains(text, Qt::CaseInsensitive));
                }
            });
            tools->addWidget(label("解析格式", "muted"));
            tools->addWidget(parser);
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
    }
    info = label(kind == "values" ? "显示每个来源的最新数值，后台持续采集" : "等待设备数据", "muted");
    body->addWidget(info);
}

QJsonObject Instrument::settings() const
{
    QJsonObject result {{"kind", kind}, {"frozen", freeze->isChecked()}};
    if (points)
    {
        result["points"] = points->value();
    }
    if (parser)
    {
        result["parser"] = parser->currentIndex();
    }
    return result;
}

void Instrument::tick(Channel *channel)
{
    if (!channel || freeze->isChecked())
    {
        return;
    }
    if (plot)
    {
        const auto data = channel->waveform(points->value());
        plot->setSamples(data);
        qint64 count = 0;
        int rejected = 0;
        const auto sources = data["sources"].toObject();
        for (const auto &value : sources)
        {
            count += value.toObject()["total_samples"].toInteger();
            rejected += value.toObject()["rejected_lines"].toInt();
        }
        info->setText(QString("%1 个采样    %2 条曲线    忽略 %3 行").arg(count).arg(plot->series.size()).arg(rejected));
    }
    else if (kind == "values")
    {
        table->clear();
        const auto sources = channel->waveform(1)["sources"].toObject();
        for (auto source = sources.begin(); source != sources.end(); ++source)
        {
            const auto samples = source.value().toObject()["samples"].toArray();
            if (samples.isEmpty())
            {
                continue;
            }
            const auto sample = samples.last().toObject();
            const auto values = sample["values"].toObject();
            for (auto value = values.begin(); value != values.end(); ++value)
            {
                new QTreeWidgetItem(table, {value.key(), QString::number(value.value().toDouble(), 'g', 9), source.key() == "default" ? "本连接" : source.key(), QString::number(sample["index"].toInteger())});
            }
        }
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
                if ((parser->currentIndex() == 1 && record["format"] != "json") || (parser->currentIndex() == 2 && record["format"] != "xml"))
                {
                    continue;
                }
                auto item = new QTreeWidgetItem(table, {record["time"].toString().mid(11, 12), record["format"].toString().toUpper(), source.key(), record["text"].toString().left(200)});
                item->setData(0, Qt::UserRole, record);
                item->setHidden(!record["text"].toString().contains(filter->text(), Qt::CaseInsensitive));
                table->setCurrentItem(item);
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
        const auto direction = width() < 730 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
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
    auto body = new QVBoxLayout(this);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    if (!instrumentKind.isEmpty())
    {
        instrument = new Instrument(instrumentKind, this);
        instrument->helpWindow = window->rootWindow();
        body->addWidget(instrument, 1);
        connect(instrument->source, &QComboBox::activated, this, [this](int index)
        {
            sourceKey = instrument->source->itemData(index).toString();
            refreshSource();
            if (window->active == this) { window->syncParameterPane(true); }
            window->updateStatus();
        });
        if (instrument->parser)
        {
            // Parse both formats once; each page filters independently.
            connect(instrument->parser, &QComboBox::currentIndexChanged, this, [this] { enableParser(); });
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
    tools->setContentsMargins(16, 8, 14, 6);
    tools->setSpacing(4);
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
    tools->addSpacing(8);
    tools->addStretch();
    filter = new SearchBox();
    filter->setPlaceholderText("筛选内容");
    filter->setToolTip("按文本筛选当前收发记录");
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
    monitor->addWidget(toolbar);
    splitter = new Splitter(Qt::Vertical);
    display = new QStackedWidget();
    log = new LogView();
    terminal = new TerminalView();
    display->addWidget(log);
    display->addWidget(terminal);
    splitter->addWidget(display);
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
    encoding->setFixedWidth(108);
    encoding->setToolTip("收发文本编码");
    newline = combo({"None · 无换行", "LF · \\n", "CR · \\r", "CRLF · \\r\\n"});
    newline->setCurrentIndex(3);
    newline->menuPopup = true;
    newline->setFixedWidth(152);
    newline->setToolTip("发送时追加的换行符");
    const QStringList lineNames {"None · 不追加换行", "LF · \\n · Line Feed", "CR · \\r · Carriage Return", "CRLF · \\r\\n · Carriage Return + Line Feed"};
    for (int i = 0; i < lineNames.size(); ++i)
    {
        newline->setItemData(i, lineNames[i], Qt::ToolTipRole);
    }
    sendHex = new QCheckBox("HEX");
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
    connect(filter, &QLineEdit::textChanged, this, [this](const QString &value) { log->filter = value; replay(); });
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
                channel->send(data, peer->currentText());
            }
        }
        catch (const std::exception &exception)
        {
            window->error(QString::fromUtf8(exception.what()));
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
    if (instrument && channel != value)
    {
        instrument->cursors = {};
        if (instrument->table) { instrument->table->clear(); }
        if (instrument->detail) { instrument->detail->clear(); }
        if (instrument->plot) { instrument->plot->setSamples({}); }
        instrument->info->setText(value ? "等待设备数据" : "等待数据来源连接");
    }
    channel = value;
    if (value)
    {
        draft = value->config;
        if (instrument) { sourceKey = value->config.key(); }
        if (!instrument && value->config.kind == "ssh") { setTerminal(true); }
    }
    enableParser();
}

void Pane::enableParser()
{
    if (instrument && instrument->parser && channel)
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
    source->addItem("选择已有连接", QString());
    Channel *selected = nullptr;
    for (auto candidate : window->engine->channels)
    {
        source->addItem(candidate->name + " · " + stateText(candidate->state), candidate->config.key());
        if (candidate->config.key() == sourceKey)
        {
            selected = candidate;
            source->setCurrentIndex(source->count() - 1);
        }
    }
    if (!sourceKey.isEmpty() && !selected)
    {
        const auto name = sourceKey.startsWith("serial:") ? sourceKey.mid(7).toUpper() : sourceKey;
        source->addItem(name + " · 未建立连接", sourceKey);
        source->setCurrentIndex(source->count() - 1);
    }
    source->setToolTip(sourceKey.isEmpty() ? "从收发页面建立连接后，在这里选择数据来源" : sourceKey);
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
    if (instrument)
    {
        if (instrument->plot)
        {
            instrument->plot->theme = window->theme;
            instrument->plot->update();
        }
        if (instrument->detail)
        {
            instrument->detail->setFont(window->dataFont);
        }
        return;
    }
    log->theme = window->theme;
    terminal->theme = window->theme;
    const QString fontStyle = QString("font-family: '%1'; font-size: %2pt;").arg(window->dataFont.family()).arg(window->dataFont.pointSizeF());
    log->setFont(window->dataFont);
    log->setStyleSheet(fontStyle);
    payload->setFont(window->dataFont);
    payload->setStyleSheet(fontStyle);
    terminal->setFont(window->dataFont);
    terminal->setStyleSheet(fontStyle);
    terminal->update();
    for (auto control : findChildren<IconButton *>())
    {
        control->theme = window->theme;
        control->update();
    }
}

void Pane::tick()
{
    if (instrument)
    {
        if (isVisible()) { instrument->tick(channel); }
        return;
    }
    const bool ready = channel && (channel->state == "connected" || channel->state == "listening");
    sendButton->setEnabled(ready && channel->config.kind != "demo");
    repeat->setEnabled(ready && channel->config.kind != "demo");
    terminal->enabledInput = ready && terminalMode;
    exportButton->setEnabled(channel);
    if (!ready)
    {
        repeat->setChecked(false);
    }
    peer->setVisible(channel && (channel->config.kind == "tcp-server" || channel->config.kind == "udp"));
    if (!channel || !isVisible())
    {
        return;
    }
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
            terminal->reset(encoding->currentText());
        }
        for (const auto &value : batch["events"].toArray())
        {
            const auto event = value.toObject();
            log->feed(event);
            if (event["stream_reset"].toBool())
            {
                terminal->reset(encoding->currentText());
            }
            if (event["direction"] == "rx")
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
    filter->setVisible(!enabled);
    follow->setVisible(!enabled);
    timestamps->setVisible(!enabled);
    hexDisplay->setVisible(!enabled);
    wrapLines->setVisible(!enabled);
    cursorShape->setVisible(enabled);
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
    terminal->reset(encoding->currentText());
}

void Pane::replay()
{
    if (instrument) { return; }
    cursor = viewStart;
    log->reset();
    terminal->reset(encoding->currentText());
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
    return {{"timestamps", log->timestamps}, {"hex_display", log->hex}, {"wrap_lines", wrapLines->isChecked()},
        {"block_cursor", terminal->blockCursor},
        {"terminal_mode", terminalMode}, {"encoding", encoding->currentText()},
        {"sender_sizes", QJsonArray {sizes.value(0, 600), sizes.value(1, 150)}}};
}

void Pane::restore(const QJsonObject &settings)
{
    if (instrument)
    {
        instrument->freeze->setChecked(settings["frozen"].toBool());
        if (instrument->points) { instrument->points->setValue(settings["points"].toInt(1000)); }
        if (instrument->parser) { instrument->parser->setCurrentIndex(qBound(0, settings["parser"].toInt(), 2)); }
        sourceKey = settings["source_key"].toString();
        if (!settings["title"].toString().isEmpty())
        {
            pageTitle = settings["title"].toString();
            window->tabs->setTabText(paneIndex(window->tabs, this), pageTitle);
        }
        refreshSource();
        return;
    }
    log->timestamps = settings.value("timestamps").toBool(true);
    timestamps->setChecked(log->timestamps);
    hexDisplay->setChecked(settings["hex_display"].toBool());
    wrapLines->setChecked(settings.value("wrap_lines").toBool(true));
    cursorShape->setChecked(settings.value("block_cursor").toBool(true));
    encoding->setCurrentText(settings.value("encoding").toString("utf-8"));
    const auto sizes = settings["sender_sizes"].toArray();
    if (sizes.size() == 2 && sizes[0].toInt() > 0 && sizes[1].toInt() > 0)
    {
        senderSizes = {sizes[0].toInt(), sizes[1].toInt()};
        QTimer::singleShot(0, this, [this] { splitter->setSizes(senderSizes); });
    }
    setTerminal((channel && channel->config.kind == "ssh") || settings["terminal_mode"].toBool());
    window->restoreInstruments(settings, channel ? channel->config.key() : Config::fromJson(window->saved["connection"].toObject()).key());
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
    rootWindow()->hosts.append(this);
    if (primary)
    {
        portProvider = primary->portProvider;
    }
    setWindowTitle("Serial Workbench · Native " + Version);
    setWindowIcon(icon("port", Theme::make(false).accent, 32));
    setFont(Theme::interfaceFont());
    setMinimumSize(480, 320);
    resize(1400, 900);
    QFile settingsFile(path);
    if (settingsFile.open(QIODevice::ReadOnly))
    {
        saved = QJsonDocument::fromJson(settingsFile.readAll()).object();
    }
    viewSettings = saved["views"].toObject();
    templates = saved["templates"].toArray();
    const auto appearance = saved["appearance"].toObject();
    theme = Theme::make(appearance["theme"] == "dark");
    dataFont = Theme::defaultDataFont();
    const auto savedFamily = appearance["font_family"].toString();
    const bool previousDefault = !appearance["readable_type"].toBool()
        && savedFamily == Theme::fontFamily(12) && appearance["font_size"].toInt() == 18;
    if (!savedFamily.isEmpty() && !previousDefault)
    {
        dataFont.setFamily(savedFamily);
        dataFont.setPointSize(qBound(8, appearance["font_size"].toInt(12), 32));
        dataFont.setStyleStrategy(savedFamily.startsWith("Fusion Pixel") ? QFont::NoAntialias : QFont::PreferAntialias);
    }
    if (!primary)
    {
        engine->allowControl = saved["allow_ai_control"].toBool();
    }
    else
    {
        setAttribute(Qt::WA_DeleteOnClose);
        theme = primary->theme;
        dataFont = primary->dataFont;
        templates = primary->templates;
        viewSettings = primary->viewSettings;
    }

    auto root = new QWidget();
    root->setObjectName("root");
    auto layout = horizontal(root);
    layout->setSpacing(0);
    rail = new QFrame();
    rail->setObjectName("rail");
    rail->setFixedWidth(54);
    auto railBody = new QVBoxLayout(rail);
    railBody->setContentsMargins(9, 16, 9, 12);
    auto workbench = new IconButton("port", "显示 / 隐藏连接参数");
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
    sidebar->setFixedWidth(246);
    connect(workbench, &QAbstractButton::toggled, this, [this](bool visible)
    {
        parametersVisible = visible;
        auto scroll = findChild<QScrollArea *>("parameterScroll");
        if (scroll)
        {
            scroll->setVisible(visible && active);
            if (visible) { scroll->raise(); }
        }
    });
    connect(workbench, &QAbstractButton::toggled, this, [this] { QTimer::singleShot(0, this, &Window::adjustTabs); });
    auto side = new QVBoxLayout(sidebar);
    side->setContentsMargins(16, 16, 16, 16);
    side->setSpacing(12);
    auto brand = new QWidget();
    brand->setObjectName("pixelBrand");
    auto brandBody = new QVBoxLayout(brand);
    brandBody->setContentsMargins(14, 14, 14, 12);
    brandBody->setSpacing(10);
    auto brandTop = horizontal();
    auto emblem = new QLabel();
    emblem->setPixmap(icon("port", QColor("#edaa50"), 32).pixmap(32, 32));
    emblem->setFixedSize(32, 32);
    brandTop->addWidget(emblem);
    brandTop->addStretch();
    brandTop->addWidget(label("SW", "brandMark"));
    brandBody->addLayout(brandTop);
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
    port->setFixedHeight(38);
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
    baud->setFixedHeight(38);
    baud->setToolTip("连接后选择新值会立即应用，并回读 Windows 驱动确认");
    serialBody->addWidget(baud);
    bits = combo({"5", "6", "7", "8"}, "8");
    parity = combo({"N", "E", "O", "M", "S"});
    stopbits = combo({"1", "1.5", "2"});
    flow = combo({"none", "rtscts", "xonxoff"});
    parity->setToolTip("N: None · 无校验\nE: Even · 偶校验\nO: Odd · 奇校验\nM: Mark\nS: Space");
    flow->setToolTip("none: 无流控\nrtscts: 硬件流控\nxonxoff: 软件流控");
    auto serialGrid = new QGridLayout();
    serialGrid->setHorizontalSpacing(10);
    serialGrid->setVerticalSpacing(7);
    serialGrid->addWidget(label("数据位", "muted"), 0, 0);
    serialGrid->addWidget(label("校验", "muted"), 0, 1);
    serialGrid->addWidget(bits, 1, 0);
    serialGrid->addWidget(parity, 1, 1);
    serialGrid->addWidget(label("停止位", "muted"), 2, 0);
    serialGrid->addWidget(label("流控", "muted"), 2, 1);
    serialGrid->addWidget(stopbits, 3, 0);
    serialGrid->addWidget(flow, 3, 1);
    serialBody->addSpacing(6);
    serialBody->addLayout(serialGrid);
    dtr = new QCheckBox("DTR");
    rts = new QCheckBox("RTS");
    auto signalControls = horizontal();
    signalControls->addWidget(dtr);
    signalControls->addWidget(rts);
    signalControls->addStretch();
    serialBody->addLayout(signalControls);
    serialInfo = label("尚未选择串口", "muted");
    serialInfo->setWordWrap(true);
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
    auto sshTargetRow = horizontal();
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
    sshTargetRow->addWidget(sshTarget, 1);
    auto sshRefresh = new IconButton("refresh", "重新读取 ~/.ssh/config");
    sshRefresh->setFixedSize(34, 38);
    connect(sshRefresh, &QAbstractButton::clicked, this, &Window::refreshSshProfiles);
    sshTargetRow->addWidget(sshRefresh);
    sshBody->addLayout(sshTargetRow);
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
    configStack->addWidget(sshPanel);
    refreshSshProfiles();
    side->addWidget(configStack);
    side->addWidget(label("连接名称", "muted"));
    connectionName = new QLineEdit();
    connectionName->setPlaceholderText("可选 · 使用端口或主机名");
    side->addWidget(connectionName);
    connectButton = button("建立连接", "primary");
    connectButton->setMinimumHeight(26);
    connect(connectButton, &QPushButton::clicked, this, &Window::connectionAction);
    side->addWidget(connectButton);
    side->addStretch();
    connectionHint = label("参数修改实时应用\n波特率以驱动回读为准", "connectionHint");
    connectionHint->setWordWrap(true);
    side->addWidget(connectionHint);
    auto sideScroll = new QScrollArea();
    sideScroll->setWidgetResizable(true);
    sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideScroll->setWidget(sidebar);
    sideScroll->setFixedWidth(246);
    sideScroll->setMinimumHeight(0);
    sideScroll->setObjectName("parameterScroll");
    layout->addWidget(sideScroll);
    auto right = new QWidget();
    auto rightBody = new QVBoxLayout(right);
    rightBody->setContentsMargins(0, 0, 0, 0);
    rightBody->setSpacing(0);
    tabHeader = new QWidget();
    tabHeader->setObjectName("tabHeader");
    tabHeader->setFixedHeight(52);
    auto header = horizontal(tabHeader);
    header->setContentsMargins(12, 0, 12, 0);
    header->setSpacing(0);
    tabs = new ConnectionBar();
    tabs->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabs, &QWidget::customContextMenuRequested, this, &Window::tabMenu);
    connect(tabs, &QTabBar::currentChanged, this, &Window::activate);
    connect(tabs, &QTabBar::tabMoved, this, [this] { activate(tabs->currentIndex()); });
    connect(tabs, &TearTabBar::tearRequested, this, [this](int index, const QPoint &position)
    {
        detachConnection(index, position, true);
    });
    header->addWidget(tabs);
    plus = new IconButton("plus", "新建页面");
    plus->setObjectName("newPage");
    plus->setFixedSize(26, 34);
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
    setCentralWidget(root);
    status = label("尚未连接", "muted");
    mcpStatus = label("", "muted");
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(status, 1);
    statusBar()->addPermanentWidget(mcpStatus);
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
        for (const auto &value : saved["widget_pages"].toArray())
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
                auditTable->setUpdatesEnabled(false);
                auditTable->clear();
                for (const auto &value : mcp->audit)
                {
                    const auto item = value.toObject();
                    new QTreeWidgetItem(auditTable, {item["time"].toString().mid(11, 12), item["client"].toString(), item["tool"].toString(), item["status"] == "ok" ? "成功" : "失败", QString::number(item["duration_ms"].toInteger()), item["error"].toString()});
                }
                auditTable->setUpdatesEnabled(true);
            }
        }
    });
    timer.start();
    refreshPorts();
    portTimer.setInterval(500);
    connect(&portTimer, &QTimer::timeout, this, &Window::refreshPorts);
    portTimer.start();
    port->setFocus();
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
    if (!QStringList {"waveform", "values", "structured"}.contains(kind))
    {
        return nullptr;
    }
    auto previous = active;
    auto page = new Pane(this, kind);
    const QString baseTitle = kind == "waveform" ? "数值波形" : kind == "values" ? "实时数值" : "JSON / XML";
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
    page->sourceKey = source ? source->config.key() : pendingKey;
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
    auto receive = menu->addAction(icon("port", theme.text), "新建收发页面", this, [this] { newConnection(); });
    receive->setObjectName("newReceivePage");
    menu->addSeparator();
    auto widgets = menu->addMenu(icon("widgets", theme.text), "新建控件页面");
    widgets->setObjectName("newWidgetPage");
    const QStringList kinds {"waveform", "values", "structured"};
    const QStringList titles {"数值波形", "实时数值", "JSON / XML"};
    for (int i = 0; i < kinds.size(); ++i)
    {
        const auto kind = kinds[i];
        auto action = widgets->addAction(titles[i], this, [this, kind]
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
    const QString title = pane->isInstrument() ? pane->pageTitle : "新收发页";
    const int index = tabs->addTab(title);
    if (pane->instrument)
    {
        tabs->setTabIcon(index, icon(pane->instrument->plot ? "wave" : "widgets", theme.muted));
        pane->refreshSource();
    }
    tabs->setTabData(index, QVariant::fromValue(static_cast<QObject *>(pane)));
    auto close = new IconButton("close", pane->isInstrument() ? "关闭控件页面" : "关闭收发页面及连接");
    close->setFixedSize(24, 24);
    close->theme = theme;
    connect(close, &QAbstractButton::clicked, this, [this, pane] { closeTab(paneIndex(tabs, pane)); });
    tabs->setTabButton(index, QTabBar::RightSide, tabCloseSlot(close));
    tabs->blockSignals(false);
    tabs->setCurrentIndex(index);
    activate(index);
    adjustTabs();
    pane->applyAppearance();
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
    parameterToggle->setEnabled(true);
    updateStatus();
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
    configStack->setCurrentIndex(index == 0 ? 0 : index == 4 ? 2 : 1);
    connectionHint->setText(index == 4
        ? "读取 ~/.ssh/config 中的主机别名\n使用系统密钥或代理登录"
        : "参数修改实时应用\n波特率以驱动回读为准");
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
    const QSignalBlocker blocker(sshTarget);
    sshTarget->clear();
    sshTarget->addItems(SshProfiles::aliases());
    sshTarget->setCurrentIndex(-1);
    sshTarget->setEditText(current);
    sshTarget->lineEdit()->setCursorPosition(0);
    sshTarget->setToolTip(current.isEmpty() ? "选择或输入 SSH 主机" : current);
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
        tabs->setTabText(index, channel->name);
        tabs->setTabToolTip(index, channel->name + " · " + stateText(channel->state) + "\n" + channel->error);
    }
    for (auto pane : panes)
    {
        if (pane->instrument)
        {
            pane->refreshSource();
            const int index = paneIndex(tabs, pane);
            tabs->setTabIcon(index, icon(pane->instrument->plot ? "wave" : "widgets", theme.muted));
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
            : parameterChannel->state == "listening" ? "停止监听" : "断开连接";
    }
    connectButton->setText(actionText);
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

    QString serialText = selectedPort == "NULL" ? "尚未选择串口"
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
        auto floating = qobject_cast<FloatingPage *>(widget);
        if (!floating)
        {
            continue;
        }
        floating->setStyleSheet(theme.stylesheet());
        floating->setPalette(palette());
        floating->setFont(Theme::interfaceFont());
        for (auto control : floating->findChildren<IconButton *>())
        {
            control->theme = theme;
            control->update();
        }
    }
}

void Window::applyLocalTheme(bool dark)
{
    theme = Theme::make(dark);
    setStyleSheet(theme.stylesheet());
    QPalette palette;
    palette.setColor(QPalette::Window, theme.background);
    palette.setColor(QPalette::Base, theme.panel);
    palette.setColor(QPalette::Button, theme.panel);
    palette.setColor(QPalette::Text, theme.text);
    palette.setColor(QPalette::WindowText, theme.text);
    palette.setColor(QPalette::ButtonText, theme.text);
    palette.setColor(QPalette::Highlight, theme.accent);
    palette.setColor(QPalette::HighlightedText, theme.onAccent);
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
}

void Window::setDataFont(const QFont &font)
{
    if (primary)
    {
        primary->setDataFont(font);
        return;
    }
    dataFont = font;
    dataFont.setPointSize(qBound(8, font.pointSize(), 32));
    dataFont.setStyleStrategy(dataFont.family().startsWith("Fusion Pixel") ? QFont::NoAntialias : QFont::PreferAntialias);
    for (auto hostWindow : hosts)
    {
        if (!hostWindow) { continue; }
        hostWindow->dataFont = dataFont;
        for (auto pane : hostWindow->panes) { pane->applyAppearance(); }
    }
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
    if (!pane->isInstrument() && pane->channel)
    {
        auto channel = pane->channel;
        const bool ready = channel->state == "connected" || channel->state == "listening" || channel->state == "connecting" || channel->state == "reconnecting";
        auto disconnect = menu.addAction("断开连接", this, [channel, pane] { pane->repeat->setChecked(false); channel->close(); });
        disconnect->setEnabled(ready);
        auto reconnect = menu.addAction("重新连接", this, [channel] { channel->open(); });
        reconnect->setEnabled(!ready);
        menu.addAction("重命名…", this, [this, channel]
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
    appearance["pixel_theme"] = true;
    appearance["readable_type"] = true;
    appearance["font_family"] = dataFont.family();
    appearance["font_size"] = dataFont.pointSize();
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
            {"widget_pages", widgetPages}, {"templates", templates}, {"allow_ai_control", engine->allowControl}}).toJson(QJsonDocument::Indented));
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
        scroll->hide();
        const QSignalBlocker blocker(parameterToggle);
        parameterToggle->setChecked(false);
    }
    else if (!narrow && floating)
    {
        body->insertWidget(1, scroll);
        scroll->setVisible(parametersVisible && active);
        const QSignalBlocker blocker(parameterToggle);
        parameterToggle->setChecked(parametersVisible);
    }
    if (narrow)
    {
        scroll->setGeometry(54, 0, 246, centralWidget()->height());
        scroll->raise();
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
    dialog->resize(640, 540);
    auto body = new QVBoxLayout(dialog);
    auto content = new QTextBrowser();
    content->setHtml(R"(<h2>每个采样发送一行</h2><p>用 LF 或 CRLF 结束，串口、TCP、UDP 均支持。</p>
        <h3>CSV</h3><pre>1.2,3.4,5.6</pre><h3>命名数值</h3><pre>temperature=25.6,voltage=3.3</pre>
        <h3>JSON</h3><pre>{"temperature":25.6,"voltage":3.3}\n[1.2,3.4]</pre>
        <h3>单片机 C 示例</h3><pre>printf("temperature=%.2f,voltage=%.3f\n", temperature, voltage);</pre>
        <p>每行最多 16 个有限数值，每个来源保留 5000 个采样。普通日志会被忽略并计数。</p>
        <p>滚轮缩放、拖动平移、双击恢复自动范围。波形显示数值报文，不测量 UART 物理电平。</p>)");
    body->addWidget(content);
    auto done = button("关闭");
    connect(done, &QPushButton::clicked, dialog, &QDialog::close);
    body->addWidget(done);
    dialog->show();
}

void Window::openSettings()
{
    if (settingsDialog)
    {
        settingsDialog->show();
        settingsDialog->raise();
        settingsDialog->activateWindow();
        return;
    }
    settingsDialog = new QDialog(this);
    settingsDialog->setWindowTitle("设置");
    settingsDialog->resize(920, 680);
    auto body = new QVBoxLayout(settingsDialog);
    body->setContentsMargins(20, 16, 20, 16);
    auto pages = new DetachableTabs();
    auto appearance = new QWidget();
    auto appearanceBody = new QVBoxLayout(appearance);
    appearanceBody->setContentsMargins(20, 24, 20, 20);
    appearanceBody->addWidget(label("像素掌机", "themeTitle"));
    appearanceBody->addWidget(label("灰绿面板 / 琥珀按键 / 清晰文字", "muted"));
    appearanceBody->addSpacing(20);
    auto formLayout = new QFormLayout();
    formLayout->setVerticalSpacing(18);
    auto themeChoice = combo({"像素掌机 · 浅色", "像素掌机 · 深色"});
    themeChoice->setCurrentIndex(theme.dark ? 1 : 0);
    connect(themeChoice, &QComboBox::currentIndexChanged, this, [this](int index) { applyTheme(index == 1); });
    auto font = button(dataFont.family(), "outline");
    connect(font, &QPushButton::clicked, this, [this, font]
    {
        QFontDialog dialog(dataFont, settingsDialog);
        dialog.setOption(QFontDialog::DontUseNativeDialog);
        dialog.setWindowTitle("数据字体");
        dialog.setPalette(palette());
        dialog.setStyleSheet(theme.stylesheet());
        if (dialog.exec() == QDialog::Accepted)
        {
            const auto selected = dialog.selectedFont();
            setDataFont(selected);
            font->setText(selected.family());
        }
    });
    auto size = new QSpinBox();
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
    auto resetFont = button("恢复推荐字体", "outline");
    connect(resetFont, &QPushButton::clicked, this, [this, font, size]
    {
        setDataFont(Theme::defaultDataFont());
        font->setText(dataFont.family());
        const QSignalBlocker blocker(size);
        size->setValue(dataFont.pointSize());
    });
    appearanceBody->addWidget(resetFont, 0, Qt::AlignLeft);
    appearanceBody->addWidget(label("界面使用清晰中文字体，像素字仅用于品牌标题。数据区可自选字体，Ctrl + 滚轮调整字号。", "muted"));
    appearanceBody->addStretch();
    pages->addTab(appearance, "外观");

    auto mcpPage = new QWidget();
    auto mcpBody = new QVBoxLayout(mcpPage);
    mcpBody->setContentsMargins(16, 16, 16, 8);
    auto address = new QLineEdit(mcp->url());
    address->setReadOnly(true);
    auto addressRow = horizontal();
    addressRow->addWidget(address, 1);
    auto copy = button("复制地址");
    addressRow->addWidget(copy);
    connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(mcp->url()); });
    mcpBody->addLayout(addressRow);
    auto permission = new QCheckBox("允许 AI 发送报文和控制连接");
    permission->setChecked(engine->allowControl);
    connect(permission, &QCheckBox::toggled, this, [this](bool checked)
    {
        engine->allowControl = checked;
        saveSettings();
    });
    mcpBody->addWidget(permission);
    mcpSummary = label("界面与 AI 共享同一连接，各自读取，不会消耗对方的数据。", "muted");
    mcpBody->addWidget(mcpSummary);
    auto mcpTabs = new DetachableTabs();
    mcpConnections = new QTreeWidget();
    mcpConnections->setRootIsDecorated(false);
    mcpConnections->setHeaderLabels({"连接", "协议", "状态", "连接 ID", "RX / TX"});
    mcpConnections->setColumnWidth(0, 160);
    mcpConnections->setColumnWidth(1, 110);
    mcpConnections->setColumnWidth(2, 140);
    mcpTabs->addTab(mcpConnections, "连接总览");
    auto clients = new QWidget();
    auto clientBody = new QVBoxLayout(clients);
    clientBody->setContentsMargins(12, 12, 12, 12);
    const QStringList names {"Codex", "Claude Code", "OpenCode CLI", "Claude Desktop"};
    for (int i = 0; i < names.size(); ++i)
    {
        auto row = horizontal();
        row->addWidget(label(names[i], "brand"));
        row->addStretch();
        auto configure = button("配置并检查", "outline");
        configure->setAutoDefault(false);
        configure->setDefault(false);
        row->addWidget(configure);
        clientBody->addLayout(row);
        auto result = label("保留其他配置，修改前创建备份", "muted");
        result->setWordWrap(true);
        clientBody->addWidget(result);
        connect(configure, &QPushButton::clicked, this, [this, i, result] { configureClient(i, result); });
    }
    auto preview = new QPlainTextEdit();
    preview->setReadOnly(true);
    preview->setPlainText(QString("HTTP MCP: %1\n\nSTDIO: %2/SerialWorkbenchMCP.exe --url %1\n\n修改后请在客户端重新加载 MCP 配置。").arg(mcp->url(), QCoreApplication::applicationDirPath()));
    clientBody->addWidget(preview, 1);
    mcpTabs->addTab(clients, "客户端接入");
    auditTable = new QTreeWidget();
    auditTable->setRootIsDecorated(false);
    auditTable->setHeaderLabels({"时间", "客户端", "工具", "结果", "耗时 ms", "错误"});
    auditTable->setColumnWidth(0, 115);
    auditTable->setColumnWidth(1, 160);
    auditTable->setColumnWidth(2, 160);
    mcpTabs->addTab(auditTable, "调用日志");
    mcpBody->addWidget(mcpTabs, 1);
    pages->addTab(mcpPage, "AI / MCP");
    auto about = new QWidget();
    auto aboutBody = new QVBoxLayout(about);
    aboutBody->setContentsMargins(24, 24, 24, 24);
    aboutBody->addWidget(label("Serial Workbench", "section"));
    aboutBody->addWidget(label("原生 Qt C++ · " + Version, "muted"));
    auto info = label("串口 · TCP · UDP\n收发记录 · ANSI 终端 · 数值控件\nHTTP / STDIO MCP 共享会话", "muted");
    info->setWordWrap(true);
    aboutBody->addWidget(info);
    aboutBody->addStretch();
    pages->addTab(about, "关于");
    body->addWidget(pages, 1);
    auto footer = horizontal();
    footer->addStretch();
    auto done = button("完成", "primary");
    connect(done, &QPushButton::clicked, this, [this] { saveSettings(); settingsDialog->hide(); });
    footer->addWidget(done);
    body->addLayout(footer);
    applyTheme(theme.dark);
    settingsDialog->show();
}

// Token-based JSONC edits preserve comments and all unrelated client entries.
struct JsonToken
{
    int start;
    int end;
    QString text;
};

static QList<JsonToken> jsonTokens(const QString &text)
{
    static const QRegularExpression expression(R"(\s+|//[^\r\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|[^\s])");
    QList<JsonToken> tokens;
    auto matches = expression.globalMatch(text);
    while (matches.hasNext())
    {
        const auto match = matches.next();
        const auto value = match.captured();
        if (!value.trimmed().isEmpty() && !value.startsWith("//") && !value.startsWith("/*"))
        {
            tokens.append({static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedEnd()), value});
        }
    }
    return tokens;
}

static QPair<QMap<QString, QPair<int, int>>, int> jsonMembers(const QString &text, int start)
{
    const auto tokens = jsonTokens(text.mid(start));
    if (tokens.isEmpty() || tokens.first().text != "{")
    {
        fail("配置节点需要 JSON 对象");
    }
    QMap<QString, QPair<int, int>> members;
    int i = 1;
    while (i < tokens.size())
    {
        if (tokens[i].text == "}")
        {
            return {members, tokens[i].start + start};
        }
        if (tokens[i].text == ",")
        {
            ++i;
            continue;
        }
        const auto nameDocument = QJsonDocument::fromJson(("[" + tokens[i].text + "]").toUtf8());
        if (!nameDocument.isArray() || !nameDocument.array()[0].isString() || i + 2 >= tokens.size() || tokens[i + 1].text != ":")
        {
            fail("JSON 配置语法无效");
        }
        const QString name = nameDocument.array()[0].toString();
        i += 2;
        const int begin = tokens[i].start + start;
        int end = begin;
        int depth = 0;
        while (i < tokens.size())
        {
            const QString token = tokens[i].text;
            if ((token == "," || token == "}") && depth == 0)
            {
                break;
            }
            if (token == "{" || token == "[")
            {
                ++depth;
            }
            else if (token == "}" || token == "]")
            {
                --depth;
            }
            end = tokens[i].end + start;
            ++i;
        }
        members[name] = {begin, end};
    }
    fail("JSON 配置对象未闭合");
}

static QString mergeJson(QString text, const QString &section, const QJsonObject &entry)
{
    const auto tokens = jsonTokens(text);
    QString cleaned;
    for (int i = 0; i < tokens.size(); ++i)
    {
        if (!(tokens[i].text == "," && i + 1 < tokens.size() && (tokens[i + 1].text == "}" || tokens[i + 1].text == "]")))
        {
            cleaned += tokens[i].text;
        }
    }
    if (!QJsonDocument::fromJson(cleaned.toUtf8()).isObject())
    {
        fail("现有 JSON/JSONC 配置无效，未修改文件");
    }
    const auto root = jsonMembers(text, 0);
    const QString encoded = QString::fromUtf8(QJsonDocument(entry).toJson(QJsonDocument::Compact));
    if (!root.first.contains(section))
    {
        const auto preceding = jsonTokens(text.left(root.second));
        const bool comma = !root.first.isEmpty() && !preceding.isEmpty() && preceding.last().text != ",";
        text.insert(root.second, QString(comma ? "," : "") + QString("\n  \"%1\": {\"serial-workbench\": %2}\n").arg(section, encoded));
        return text;
    }
    const auto object = jsonMembers(text, root.first[section].first);
    if (object.first.contains("serial-workbench"))
    {
        const auto range = object.first["serial-workbench"];
        text.replace(range.first, range.second - range.first, encoded);
    }
    else
    {
        bool comma = !object.first.isEmpty();
        const auto preceding = jsonTokens(text.left(object.second));
        if (!preceding.isEmpty() && preceding.last().text == ",")
        {
            comma = false;
        }
        text.insert(object.second, QString(comma ? "," : "") + "\n    \"serial-workbench\": " + encoded + "\n  ");
    }
    return text;
}

void Window::configureClient(int index, QLabel *result)
{
    try
    {
        if (!mcp->isListening())
        {
            fail("MCP 未启动，请先解决端口占用");
        }
        const QString home = QDir::homePath();
        QString path;
        if (index == 0)
        {
            path = QDir(qEnvironmentVariable("CODEX_HOME", home + "/.codex")).filePath("config.toml");
        }
        else if (index == 1)
        {
            path = home + "/.claude.json";
        }
        else if (index == 2)
        {
            const auto base = qEnvironmentVariable("XDG_CONFIG_HOME", home + "/.config") + "/opencode";
            path = base + (QFileInfo::exists(base + "/opencode.jsonc") ? "/opencode.jsonc" : "/opencode.json");
        }
        else
        {
            path = qEnvironmentVariable("APPDATA") + "/Claude/claude_desktop_config.json";
        }
        QFile file(path);
        QString text = index == 0 ? QString() : "{}";
        if (file.exists())
        {
            if (!file.open(QIODevice::ReadOnly))
            {
                fail(file.errorString());
            }
            text = QString::fromUtf8(file.readAll());
            file.close();
        }
        if (index == 0)
        {
            // Replace only this application's table; preserve every other section.
            const QString entry = "[mcp_servers.\"serial-workbench\"]\nurl = \"" + mcp->url() + "\"\n";
            const QRegularExpression section(R"((?m)^\[mcp_servers\.(?:"serial-workbench"|'serial-workbench'|serial-workbench)\][^\n]*\n[\s\S]*?(?=^\[|\z))");
            const auto match = section.match(text);
            if (match.hasMatch())
            {
                text.replace(match.capturedStart(), match.capturedLength(), entry + "\n");
            }
            else
            {
                text += "\n" + entry;
            }
        }
        else
        {
            QJsonObject entry;
            if (index == 3)
            {
                const QString bridge = QCoreApplication::applicationDirPath() + "/SerialWorkbenchMCP.exe";
                if (!QFileInfo::exists(bridge))
                {
                    fail("STDIO 桥接程序不存在");
                }
                entry = {{"command", bridge}, {"args", QJsonArray {"--url", mcp->url()}}};
            }
            else if (index == 2)
            {
                entry = {{"type", "remote"}, {"url", mcp->url()}, {"enabled", true}};
            }
            else
            {
                entry = {{"type", "http"}, {"url", mcp->url()}};
            }
            text = mergeJson(text, index == 2 ? "mcp" : "mcpServers", entry);
        }
        QDir().mkpath(QFileInfo(path).absolutePath());
        if (file.exists() && !QFile::copy(path, path + "." + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz") + ".bak"))
        {
            fail("配置备份失败，未修改原文件");
        }
        QSaveFile output(path);
        if (!output.open(QIODevice::WriteOnly) || output.write(text.toUtf8()) < 0 || !output.commit())
        {
            fail("无法写入客户端配置");
        }
        // Native transport is already live; verify discovery and status on the same service.
        const auto status = mcp->call("get_status", {}).toObject();
        if (mcp->tools().size() != 9 || status["session_id"] != engine->journal.session)
        {
            fail("MCP 检查失败");
        }
        result->setText("配置已保存，服务检查通过。请重载客户端配置。\n" + path);
    }
    catch (const std::exception &exception)
    {
        result->setText("未完成：" + QString::fromUtf8(exception.what()));
    }
}
}
