// SPDX-License-Identifier: Apache-2.0
#include "serial_monitor_dialog.h"
#include <algorithm>
#include <stdexcept>

namespace sw
{
SerialMonitorDialog::SerialMonitorDialog(QWidget *parent) : QDialog(parent, Qt::Window)
{
    setObjectName("serialMonitorDialog");
    setWindowTitle("串口收发小窗");
    setModal(false);
    setMinimumSize(400, 300);
    resize(760, 440);
    auto body = new QVBoxLayout(this);
    body->setContentsMargins(12, 10, 12, 10);
    body->setSpacing(8);
    auto heading = new QHBoxLayout();
    source = new QLabel("未选择串口");
    source->setObjectName("serialMonitorSource");
    source->setTextFormat(Qt::PlainText);
    source->setWordWrap(true);
    heading->addWidget(source, 1);
    connection = new QLabel("未连接");
    connection->setObjectName("serialMonitorConnection");
    heading->addWidget(connection);
    body->addLayout(heading);

    auto tools = new QHBoxLayout();
    timestamps = new IconButton("clock", "显示时间戳", this);
    timestamps->setObjectName("serialMonitorTimestamps");
    timestamps->setCheckable(true);
    timestamps->setChecked(true);
    hex = new IconButton("hex", "十六进制显示", this);
    hex->setObjectName("serialMonitorHex");
    hex->setCheckable(true);
    pause = new IconButton("pause", "暂停小窗显示，后台继续接收", this);
    pause->setObjectName("serialMonitorPause");
    pause->setCheckable(true);
    follow = new IconButton("follow", "跟随最新收发内容", this);
    follow->setObjectName("serialMonitorFollow");
    follow->setCheckable(true);
    follow->setChecked(true);
    auto clear = new IconButton("trash", "仅清空此小窗显示，保留连接历史", this);
    clear->setObjectName("serialMonitorClear");
    for (auto button : {timestamps, hex, pause, follow, clear})
    {
        button->setFixedSize(28, 28);
        tools->addWidget(button);
    }
    tools->addStretch();
    encoding = new QComboBox();
    encoding->setObjectName("serialMonitorEncoding");
    encoding->addItems({"utf-8", "GBK", "ASCII"});
    encoding->setToolTip("小窗显示和手动发送的文本编码");
    tools->addWidget(encoding);
    body->addLayout(tools);
    log = new LogView(this);
    filter = new FilterBox(this);
    filter->setPlaceholderText("筛选小窗收发内容");
    filter->setMaximumWidth(QWIDGETSIZE_MAX);
    body->addWidget(filter);
    connect(filter, &FilterBox::criteriaChanged, this, [this]
    {
        log->setFilter(filter->criteria());
        emit configurationChanged();
    });
    log->setObjectName("serialMonitorLog");
    body->addWidget(log, 1);
    auto footer = new QHBoxLayout();
    auto legend = new QLabel("TX 发送  /  RX 接收");
    legend->setObjectName("serialMonitorLegend");
    footer->addWidget(legend);
    footer->addStretch();
    statistics = new QLabel();
    statistics->setObjectName("serialMonitorStatistics");
    footer->addWidget(statistics);
    body->addLayout(footer);

    auto sender = new QHBoxLayout();
    payload = new QLineEdit();
    payload->setObjectName("serialMonitorPayload");
    payload->setPlaceholderText("输入文本命令，回车发送");
    payload->setMaxLength(4096);
    sender->addWidget(payload, 1);
    newline = new QComboBox();
    newline->setObjectName("serialMonitorNewline");
    newline->addItem("CRLF", "crlf");
    newline->addItem("LF", "lf");
    newline->addItem("CR", "cr");
    newline->addItem("无行尾", "none");
    newline->setToolTip("手动发送的行尾");
    sender->addWidget(newline);
    sendButton = new QPushButton("发送");
    sendButton->setObjectName("serialMonitorSend");
    sender->addWidget(sendButton);
    body->addLayout(sender);
    message = new QLabel();
    message->setObjectName("serialMonitorMessage");
    message->setTextFormat(Qt::PlainText);
    message->setWordWrap(true);
    message->hide();
    body->addWidget(message);

    connect(clear, &QAbstractButton::clicked, this, &SerialMonitorDialog::clearView);
    connect(sendButton, &QPushButton::clicked, this, &SerialMonitorDialog::sendCurrent);
    connect(payload, &QLineEdit::returnPressed, this, &SerialMonitorDialog::sendCurrent);
    connect(pause, &QAbstractButton::toggled, this, [this](bool paused)
    {
        pause->setToolTip(paused ? "继续小窗显示" : "暂停小窗显示，后台继续接收");
        pause->setAccessibleName(pause->toolTip());
        if (!paused) { refresh(); }
    });
    connect(follow, &QAbstractButton::toggled, this, [this](bool enabled)
    {
        log->follow = enabled;
        if (enabled) { log->verticalScrollBar()->setValue(log->verticalScrollBar()->maximum()); }
        emit configurationChanged();
    });
    for (auto button : {hex, timestamps})
    {
        connect(button, &QAbstractButton::toggled, this, [this] { replay(); emit configurationChanged(); });
    }
    connect(encoding, &QComboBox::currentTextChanged, this, [this] { replay(); emit configurationChanged(); });
    connect(newline, &QComboBox::currentIndexChanged, this, &SerialMonitorDialog::configurationChanged);
    connect(log, &LogView::zoomRequested, this, &SerialMonitorDialog::zoomRequested);
    timer.setInterval(40);
    connect(&timer, &QTimer::timeout, this, &SerialMonitorDialog::refresh);
    saveTimer.setSingleShot(true);
    saveTimer.setInterval(250);
    connect(&saveTimer, &QTimer::timeout, this, &SerialMonitorDialog::configurationChanged);
    setAppearance(Theme::make(false), Theme::defaultDataFont());
    refresh();
}

void SerialMonitorDialog::setAppearance(const Theme &theme, const QFont &font)
{
    appearance = theme;
    dataFont = font;
    setStyleSheet(theme.stylesheet());
    for (auto button : findChildren<IconButton *>())
    {
        button->theme = theme;
        button->update();
    }
    log->theme = theme;
    log->setFont(font);
    log->setStyleSheet(QString("font-family: '%1'; font-size: %2pt; font-weight: %3; font-style: %4;")
        .arg(font.families().join("', '")).arg(font.pointSizeF()).arg(int(font.weight())).arg(font.italic() ? "italic" : "normal"));
    replay();
    refresh();
}

void SerialMonitorDialog::setChannel(Channel *channel)
{
    if (channel && channel->config.kind != "serial") { channel = nullptr; }
    const QString nextId = channel ? channel->id : QString();
    if (target == channel && boundId == nextId) { return; }
    target = channel;
    boundId = nextId;
    cursor = 0;
    viewStart = 0;
    payload->clear();
    message->hide();
    log->reset();
    replay();
    refresh();
}

void SerialMonitorDialog::replay()
{
    log->hex = hex->isChecked();
    log->timestamps = timestamps->isChecked();
    log->encoding = encoding->currentText();
    log->follow = follow->isChecked();
    log->reset();
    if (!target) { return; }
    const auto &journal = target->engine->journal;
    const qint64 upper = pause->isChecked() ? cursor : journal.sequence;
    QList<QJsonObject> recent;
    qsizetype bytes = 0;
    for (auto event = journal.events.rbegin(); event != journal.events.rend(); ++event)
    {
        const auto &record = event->first;
        if (record["channel_id"].toString() != target->id || record["seq"].toInteger() > upper
            || record["seq"].toInteger() <= viewStart) { continue; }
        if (recent.size() >= 200 || bytes >= 128 * 1024) { break; }
        recent.append(record);
        bytes += record["size"].toInteger();
    }
    for (auto record = recent.crbegin(); record != recent.crend(); ++record) { log->feed(*record); }
    cursor = upper;
}

void SerialMonitorDialog::refresh()
{
    if (!target && !boundId.isEmpty()) { setChannel(nullptr); return; }
    const bool ready = target && target->state == "connected" && !target->transferOwner;
    sendButton->setEnabled(ready);
    const QString name = target ? target->name : "未选择串口";
    if (source->text() != name) { source->setText(name); }
    setWindowTitle("串口收发 · " + name);
    const QString state = !target ? "未选择" : target->transferOwner ? "文件传输中" : target->state == "connected" ? "已连接" : "未连接";
    if (connection->text() != state)
    {
        connection->setText(state);
        connection->setStyleSheet(QString("color: %1;").arg(ready ? appearance.green.name() : appearance.muted.name()));
    }
    statistics->setText(target ? QString("发送 %1 B    接收 %2 B").arg(target->tx).arg(target->rx) : QString());
    if (!target || pause->isChecked() || !isVisible()) { return; }
    const auto batch = target->engine->journal.read(cursor, 500, target->id);
    if (batch["gap"].toBool())
    {
        log->append("部分旧数据已被历史缓冲区覆盖");
        log->streams.clear();
    }
    for (const auto &record : batch["events"].toArray()) { log->feed(record.toObject()); }
    cursor = batch["next_cursor"].toInteger();
}

void SerialMonitorDialog::clearView()
{
    log->reset();
    cursor = target ? target->engine->journal.sequence : 0;
    viewStart = cursor;
}

void SerialMonitorDialog::sendCurrent()
{
    try
    {
        if (!target || target->config.kind != "serial" || target->state != "connected")
        {
            throw std::runtime_error("请先在控件页选择已建立的串口连接");
        }
        const auto packet = Engine::encode(payload->text(), false, encoding->currentText(), newline->currentData().toString());
        target->send(packet, {}, "control-monitor");
        message->hide();
        refresh();
    }
    catch (const std::exception &failure)
    {
        message->setText(QString::fromUtf8(failure.what()));
        message->show();
    }
}

QJsonObject SerialMonitorDialog::settings() const
{
    return {{"geometry", QString::fromLatin1(saveGeometry().toBase64())}, {"hex", hex->isChecked()},
        {"timestamps", timestamps->isChecked()}, {"follow", follow->isChecked()},
        {"encoding", encoding->currentText()}, {"newline", newline->currentData().toString()},
        {"search_filter", filter->settings()}};
}

void SerialMonitorDialog::restore(const QJsonObject &saved)
{
    filter->restore(saved["search_filter"].toObject());
    const QSignalBlocker blockHex(hex);
    const QSignalBlocker blockTimestamps(timestamps);
    const QSignalBlocker blockFollow(follow);
    const QSignalBlocker blockEncoding(encoding);
    const QSignalBlocker blockNewline(newline);
    hex->setChecked(saved["hex"].toBool());
    timestamps->setChecked(saved["timestamps"].toBool(true));
    follow->setChecked(saved["follow"].toBool(true));
    encoding->setCurrentText(saved["encoding"].toString("utf-8"));
    newline->setCurrentIndex(qMax(0, newline->findData(saved["newline"].toString("crlf"))));
    const auto geometry = QByteArray::fromBase64(saved["geometry"].toString().toLatin1());
    if (!geometry.isEmpty()) { restoreGeometry(geometry); }
    replay();
}

void SerialMonitorDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    timer.start();
    refresh();
}

void SerialMonitorDialog::hideEvent(QHideEvent *event)
{
    timer.stop();
    QDialog::hideEvent(event);
    if (!event->spontaneous()) { emit windowHidden(); emit configurationChanged(); }
}

void SerialMonitorDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    saveTimer.start();
}

void SerialMonitorDialog::moveEvent(QMoveEvent *event)
{
    QDialog::moveEvent(event);
    saveTimer.start();
}
}
