// SPDX-License-Identifier: Apache-2.0
#include "openocd_rtt.h"

namespace sw
{
namespace
{
QLineEdit *textField(const QString &name, const QString &placeholder = {})
{
    auto edit = new QLineEdit();
    edit->setObjectName(name);
    edit->setPlaceholderText(placeholder);
    return edit;
}

QSpinBox *numberField(const QString &name, int low, int high)
{
    auto spin = new QSpinBox();
    spin->setObjectName(name);
    spin->setRange(low, high);
    return spin;
}

QLabel *hint(const QString &text)
{
    auto label = new QLabel(text);
    label->setObjectName("muted");
    label->setWordWrap(true);
    return label;
}
}

OpenOcdRttSettings::OpenOcdRttSettings(QWidget *parent) : QWidget(parent)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 12, 12, 12);
    layout->setSpacing(12);
    fields = new QFormLayout();
    fields->setContentsMargins(0, 0, 0, 0);
    fields->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    fields->setVerticalSpacing(9);
    mode = new Combo();
    mode->setObjectName("rttMode");
    mode->addItem("工作台启动 OpenOCD", "managed");
    mode->addItem("接入已有 RTT 服务", "external");
    host = textField("rttHost", "127.0.0.1");
    port = numberField("rttPort", 0, 65535);
    fields->addRow("连接方式", mode);
    fields->addRow("RTT 主机", host);
    fields->addRow("RTT 端口", port);
    address = textField("rttAddress", "_SEGGER_RTT 地址或 RAM 起始地址，如 0x20000000");
    size = textField("rttSize", "如 0x100 或 0x10000");
    identifier = textField("rttId", "SEGGER RTT");
    identifier->setMaxLength(15);
    channel = numberField("rttChannel", 0, 255);
    poll = numberField("rttPoll", 1, 1000);
    poll->setSuffix(" ms");
    startup = new Combo();
    startup->setObjectName("rttStartup");
    startup->addItem("保持目标状态 · 不复位", "attach");
    startup->addItem("恢复运行 · resume", "resume");
    startup->addItem("复位并运行 · reset run", "reset_run");
    gdb = numberField("rttGdbPort", 0, 65535);
    gdb->setSpecialValueText("关闭 GDB 服务");
    gdb->setToolTip("例如 3333。IDE 应连接这个已有 GDB 服务，避免再次启动 OpenOCD 争用调试器。");
    fields->addRow("搜索起始地址", address);
    fields->addRow("搜索字节数", size);
    fields->addRow("控制块标识", identifier);
    fields->addRow("上行通道", channel);
    fields->addRow("轮询间隔", poll);
    fields->addRow("启动时", startup);
    fields->addRow("GDB 端口", gdb);
    for (int row = 0; row < fields->rowCount(); ++row)
    {
        fields->itemAt(row, QFormLayout::LabelRole)->widget()->setMinimumWidth(90);
    }
    managedHint = hint("调试器、芯片和序列号沿用「固件烧录」里的设置。地址可从 ELF / map 中的 _SEGGER_RTT 获取，搜索范围应位于芯片的可读 RAM。");
    fields->addRow(managedHint);
    layout->addLayout(fields);
    layout->addWidget(hint("固件需已启用 RTT。终端保留 ANSI 颜色；暂停显示不暂停芯片。若 IDE 已占用调试器，请接入它的 RTT 服务。"));
    layout->addStretch();

    connect(mode, &QComboBox::currentIndexChanged, this, [this]
    {
        if (loading) { return; }
        if (mode->currentData() == "external" && port->value() == 0)
        {
            const QSignalBlocker guard(port);
            port->setValue(9090);
        }
        updateMode();
        emit edited();
    });
    for (auto edit : {host, address, size, identifier})
    {
        connect(edit, &QLineEdit::editingFinished, this, [this] { if (!loading) { emit edited(); } });
    }
    for (auto spin : {port, channel, poll, gdb})
    {
        connect(spin, &QSpinBox::valueChanged, this, [this] { if (!loading) { emit edited(); } });
    }
    connect(startup, &QComboBox::currentIndexChanged, this, [this] { if (!loading) { emit edited(); } });
    updateMode();
}

void OpenOcdRttSettings::updateMode()
{
    const bool external = mode->currentData() == "external";
    fields->setRowVisible(host, external);
    const QList<QWidget *> managedFields {address, size, identifier, channel, poll, startup, gdb, managedHint};
    for (auto field : managedFields) { fields->setRowVisible(field, !external); }
    port->setSpecialValueText(external ? "请输入服务端口" : "自动选择空闲端口");
    port->setToolTip(external ? "填写已有 RTT TCP 服务的端口" : "设为 0 可自动选择空闲端口");
}

void OpenOcdRttSettings::load(const QJsonObject &config)
{
    const QScopedValueRollback<bool> guard(loading, true);
    mode->setCurrentIndex(mode->findData(config["rtt_mode"].toString()));
    startup->setCurrentIndex(startup->findData(config["rtt_startup"].toString()));
    host->setText(config["rtt_host"].toString());
    address->setText(config["rtt_address"].toString());
    size->setText(config["rtt_size"].toString());
    identifier->setText(config["rtt_id"].toString());
    port->setValue(config["rtt_port"].toInt());
    channel->setValue(config["rtt_channel"].toInt());
    poll->setValue(config["rtt_poll_ms"].toInt());
    gdb->setValue(config["rtt_gdb_port"].toInt());
    updateMode();
}

QJsonObject OpenOcdRttSettings::configuration() const
{
    return {
        {"rtt_mode", mode->currentData().toString()}, {"rtt_host", host->text()}, {"rtt_port", port->value()},
        {"rtt_address", address->text()}, {"rtt_size", size->text()}, {"rtt_id", identifier->text()},
        {"rtt_channel", channel->value()}, {"rtt_poll_ms", poll->value()},
        {"rtt_startup", startup->currentData().toString()}, {"rtt_gdb_port", gdb->value()}
    };
}

OpenOcdRttConsole::OpenOcdRttConsole(OpenOcdService *owner, QWidget *parent, bool floating)
    : QWidget(parent), service(owner)
{
    setObjectName("openocdRttConsole");
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);
    toolbar = new QBoxLayout(QBoxLayout::TopToBottom);
    toolbar->setSpacing(6);
    filter = new FilterBox();
    filter->setObjectName("openocdRttFilter");
    filter->setPlaceholderText("搜索 RTT 日志");
    filter->restore(owner->rttFilter().settings());
    toolbar->addWidget(filter, 1);
    auto buttons = new QWidget();
    auto actions = new QHBoxLayout(buttons);
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(4);
    pause = new QPushButton("暂停");
    pause->setObjectName("rttPause");
    pause->setCheckable(true);
    auto clear = new QPushButton("清空");
    clear->setObjectName("rttClear");
    auto copy = new QPushButton("复制");
    auto save = new QPushButton("导出");
    auto popout = new QPushButton("弹出");
    popout->setObjectName("rttPopout");
    for (auto button : {pause, clear, copy, save, popout})
    {
        button->setAutoDefault(false);
        button->setProperty("variant", "outline");
        button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        actions->addWidget(button);
    }
    actions->addStretch();
    toolbar->addWidget(buttons);
    layout->addLayout(toolbar);
    popout->setVisible(!floating);
    status = hint("尚未启动 RTT");
    status->setObjectName("rttConsoleStatus");
    layout->addWidget(status);
    terminal = new TerminalView();
    terminal->setObjectName("rttTerminal");
    terminal->setMinimumSize(180, 100);
    terminal->enabledInput = false;
    terminal->pasteShortcutEnabled = false;
    terminal->rightClickPasteEnabled = false;
    terminal->cursorVisible = false;
    terminal->blink.stop();
    matches = new QPlainTextEdit();
    matches->setObjectName("rttFilteredLines");
    matches->setReadOnly(true);
    matches->setLineWrapMode(QPlainTextEdit::NoWrap);
    display = new QStackedWidget();
    display->addWidget(terminal);
    display->addWidget(matches);
    layout->addWidget(display, 1);
    setAppearance(Theme::make(false), Theme::defaultDataFont());
    timer.setSingleShot(true);
    timer.setInterval(33);
    connect(&timer, &QTimer::timeout, this, &OpenOcdRttConsole::drain);
    searchTimer.setSingleShot(true);
    searchTimer.setInterval(160);
    connect(&searchTimer, &QTimer::timeout, this, &OpenOcdRttConsole::refreshSearch);
    connect(filter, &FilterBox::criteriaChanged, this, [this]
    {
        if (service) { service->setRttFilter(filter->criteria()); }
        if (filter->text().isEmpty() || !filter->criteria().error.isEmpty()) { refreshSearch(); }
        else { scheduleSearch(); }
    });
    connect(owner, &OpenOcdService::rttFilterChanged, this, [this]
    {
        const auto next = service->rttFilter().settings();
        if (filter->settings() != next) { filter->restore(next); }
        scheduleSearch();
    });
    auto find = new QShortcut(QKeySequence::Find, this);
    find->setContext(Qt::WidgetWithChildrenShortcut);
    connect(find, &QShortcut::activated, filter, [this]
    {
        filter->setFocus(Qt::ShortcutFocusReason);
        filter->selectAll();
    });
    connect(owner, &OpenOcdService::rttDataAvailable, this, &OpenOcdRttConsole::schedule);
    connect(owner, &OpenOcdService::stateChanged, this, [this] { updateStatus(); schedule(); });
    connect(owner, &QObject::destroyed, this, [this]
    {
        timer.stop();
        searchTimer.stop();
        status->setText("此板卡页面已关闭");
    });
    connect(pause, &QPushButton::toggled, this, [this](bool paused)
    {
        pause->setText(paused ? "继续" : "暂停");
        if (!paused) { schedule(); }
        updateStatus();
    });
    connect(clear, &QPushButton::clicked, this, [this]
    {
        if (service)
        {
            const auto state = service->readRtt();
            cursor = state["latest_cursor"].toInteger();
            clearedThrough = cursor;
            displayedJob = state["job_id"].toString();
        }
        displayedConnection = 0;
        terminal->reset();
        terminal->cursorVisible = false;
        refreshSearch();
    });
    connect(copy, &QPushButton::clicked, this, [this]
    {
        QApplication::clipboard()->setText(displayText(true));
    });
    connect(save, &QPushButton::clicked, this, [this]
    {
        const auto path = QFileDialog::getSaveFileName(this, "导出当前 RTT 终端文本", "rtt-log.txt", "文本 (*.txt)");
        if (path.isEmpty()) { return; }
        QSaveFile file(path);
        const auto bytes = displayText(false).toUtf8();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        {
            status->setText("导出失败：" + file.errorString());
        }
    });
    connect(popout, &QPushButton::clicked, this, &OpenOcdRttConsole::openWindow);
    updateStatus();
}

void OpenOcdRttConsole::setAppearance(const Theme &theme, const QFont &font)
{
    terminal->theme = theme;
    terminal->setFont(font);
    matches->setFont(font);
    const auto fontStyle = QString("font-family: '%1'; font-size: %2pt; font-weight: %3; font-style: %4;")
        .arg(font.families().join("', '")).arg(font.pointSizeF()).arg(int(font.weight()))
        .arg(font.italic() ? "italic" : "normal");
    terminal->setStyleSheet(fontStyle);
    matches->setStyleSheet(fontStyle);
    terminal->update();
    if (popup)
    {
        popup->setPalette(theme.palette());
        popup->setStyleSheet(theme.stylesheet());
        for (auto viewer : popup->findChildren<OpenOcdRttConsole *>()) { viewer->setAppearance(theme, font); }
    }
}

void OpenOcdRttConsole::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    schedule();
    scheduleSearch();
}

void OpenOcdRttConsole::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const int minimum = filter->minimumWidth() + toolbar->itemAt(1)->widget()->minimumSizeHint().width() + 18;
    toolbar->setDirection(width() >= minimum ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
}

void OpenOcdRttConsole::schedule()
{
    if (service && isVisible() && !pause->isChecked() && !timer.isActive()) { timer.start(); }
}

void OpenOcdRttConsole::drain()
{
    if (!service || !isVisible() || pause->isChecked()) { return; }
    auto state = service->readRtt(cursor, 8);
    if (displayedJob != state["job_id"].toString())
    {
        displayedJob = state["job_id"].toString();
        clearedThrough = 0;
        displayedConnection = 0;
        terminal->reset();
        terminal->cursorVisible = false;
        state = service->readRtt(0, 8);
    }
    if (state["gap"].toBool())
    {
        terminal->resetInputStream();
        terminal->feed("\r\n[较早 RTT 数据已超出保留范围]\r\n");
    }
    for (const auto &value : state["records"].toArray())
    {
        const auto record = value.toObject();
        const int connection = record["connection"].toInt();
        if (displayedConnection != 0 && connection != displayedConnection)
        {
            terminal->resetInputStream();
            if (terminal->x != 0) { terminal->feed("\r\n"); }
        }
        displayedConnection = connection;
        terminal->feed(QByteArray::fromHex(record["hex"].toString().toLatin1()));
    }
    cursor = state["next_cursor"].toInteger();
    updateStatus();
    scheduleSearch();
    if (state["has_more"].toBool()) { schedule(); }
}

void OpenOcdRttConsole::scheduleSearch()
{
    if (service && isVisible() && !filter->text().isEmpty() && !searchTimer.isActive()) { searchTimer.start(); }
}

void OpenOcdRttConsole::refreshSearch()
{
    if (!service) { return; }
    const auto &criteria = filter->criteria();
    if (criteria.text.isEmpty() || !criteria.error.isEmpty())
    {
        display->setCurrentWidget(terminal);
        searchSummary = criteria.error;
        updateStatus();
        return;
    }
    const auto page = service->searchRtt(0, 2000, {}, clearedThrough, cursor);
    QStringList lines;
    for (const auto &value : page["rows"].toArray()) { lines.append(value.toObject()["text"].toString()); }
    const auto text = lines.join('\n');
    if (text != matches->toPlainText())
    {
        auto scroll = matches->verticalScrollBar();
        const int previous = scroll->value();
        const bool follow = previous == scroll->maximum();
        matches->setPlainText(text);
        scroll->setValue(follow ? scroll->maximum() : previous);
    }
    display->setCurrentWidget(matches);
    const int count = page["total_matches"].toInt();
    searchSummary = lines.size() < count ? QString("匹配 %1 行，显示前 %2 行").arg(count).arg(lines.size())
        : QString("匹配 %1 行").arg(count);
    updateStatus();
}

QString OpenOcdRttConsole::displayText(bool selected) const
{
    if (display->currentWidget() == matches)
    {
        const auto selection = matches->textCursor();
        return selected && selection.hasSelection() ? selection.selectedText().replace(QChar::ParagraphSeparator, '\n')
            : matches->toPlainText();
    }
    return selected && terminal->hasSelection() ? terminal->selectedText() : terminal->text();
}

void OpenOcdRttConsole::updateStatus()
{
    if (!service) { return; }
    const auto state = service->readRtt(cursor, 1);
    const auto bytes = state["received_bytes"].toInteger();
    const QMap<QString, QString> names {{"idle", "尚未启动"}, {"starting", "OpenOCD 初始化 RTT"},
        {"connecting", "正在连接 RTT 服务"}, {"reconnecting", "连接已断开，正在重连"},
        {"connected", state["connection_bytes"].toInteger() ? "已连接" : "已连接，等待固件输出"}, {"closed", "已停止"}};
    auto text = names.value(state["status"].toString());
    if (state["port"].toInt() > 0)
    {
        text += QString(" · %1:%2").arg(state["host"].toString()).arg(state["port"].toInt());
        if (bytes) { text += QString(" · %1 B").arg(bytes); }
    }
    if (pause->isChecked()) { text += " · 显示已暂停"; }
    if (!searchSummary.isEmpty()) { text += " · " + searchSummary; }
    status->setText(text);
}

void OpenOcdRttConsole::openWindow()
{
    if (!service) { return; }
    if (!popup)
    {
        popup = new QDialog(this);
        popup->setAttribute(Qt::WA_DeleteOnClose);
        popup->setObjectName("rttFloatingWindow");
        popup->setWindowTitle(service->title + " · RTT");
        popup->setWindowIcon(QIcon(":/brand/lightning.svg"));
        popup->setMinimumSize(480, 280);
        popup->resize(900, 560);
        auto layout = new QVBoxLayout(popup);
        auto viewer = new OpenOcdRttConsole(service, popup, true);
        layout->addWidget(viewer);
        setAppearance(terminal->theme, terminal->font());
    }
    popup->show();
    popup->raise();
    popup->activateWindow();
}
}
