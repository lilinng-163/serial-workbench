// SPDX-License-Identifier: Apache-2.0
#include "file_transfer_dialog.h"
#include "controls.h"

namespace sw
{
namespace
{
QString amount(qint64 value)
{
    if (value < 0) { return "未知大小"; }
    if (value >= 1024 * 1024) { return QString::number(value / 1048576.0, 'f', 2) + " MiB"; }
    if (value >= 1024) { return QString::number(value / 1024.0, 'f', 1) + " KiB"; }
    return QString::number(value) + " B";
}
}

FileTransferDialog::FileTransferDialog(SerialTransfer *engine, QWidget *parent)
    : QDialog(parent), transfer(engine)
{
    setWindowTitle("串口文件传输");
    setWindowIcon(QIcon(":/brand/lightning.svg"));
    resize(620, 430);
    setMinimumSize(500, 390);
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(16);
    auto title = new QLabel("文件传输", this);
    QFont heading = font();
    heading.setPointSize(14);
    heading.setWeight(QFont::DemiBold);
    title->setFont(heading);
    layout->addWidget(title);
    form = new QFormLayout();
    form->setVerticalSpacing(12);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    direction = new Combo();
    direction->addItems({"发送到设备", "接收并保存"});
    direction->setObjectName("transferDirection");
    protocol = new Combo();
    protocol->addItems({"原始字节", "XMODEM-CRC"});
    protocol->setCurrentIndex(1);
    protocol->setObjectName("transferProtocol");
    form->addRow("方向", direction);
    form->addRow("协议", protocol);
    auto fileRow = new QHBoxLayout();
    path = new QLineEdit();
    path->setPlaceholderText("选择文件路径");
    path->setObjectName("transferPath");
    browse = new QPushButton("选择文件");
    fileRow->addWidget(path, 1);
    fileRow->addWidget(browse);
    form->addRow("文件", fileRow);
    expectedSize = new QLineEdit();
    expectedSize->setPlaceholderText("字节数，可留空");
    expectedSize->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]{0,18}"), expectedSize));
    form->addRow("接收大小", expectedSize);
    layout->addLayout(form);
    hint = new QLabel();
    hint->setWordWrap(true);
    layout->addWidget(hint);
    progress = new QProgressBar();
    progress->setObjectName("fileTransferProgress");
    progress->setRange(0, 1000);
    progress->setValue(0);
    progress->setFormat("0.0%");
    progress->setMinimumHeight(24);
    // Use the inherited theme palette instead of a fixed light/dark background.
    const auto foreground = parent ? parent->palette().color(QPalette::WindowText).name() : QString("#28392f");
    const auto chunk = QColor(foreground).lightness() > 128 ? QString("#385642") : QString("#86b889");
    progress->setStyleSheet("QProgressBar {border:1px solid #7f967b; border-radius:3px; text-align:center; color:"
        + foreground + ";} QProgressBar::chunk {background:" + chunk + "; border-radius:2px;}");
    layout->addWidget(progress);
    summary = new QLabel("0 B");
    status = new QLabel("准备就绪");
    status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(summary);
    layout->addWidget(status);
    layout->addStretch();
    auto actions = new QHBoxLayout();
    actions->addStretch();
    save = new QPushButton("停止并保存");
    stop = new QPushButton("关闭");
    start = new QPushButton("开始传输");
    start->setObjectName("transferStart");
    actions->addWidget(save);
    actions->addWidget(stop);
    actions->addWidget(start);
    layout->addLayout(actions);
    makeDialogScrollable(this, QSize(480, 280));
    connect(browse, &QPushButton::clicked, this, [this]
    {
        const auto selected = direction->currentIndex() == 0
            ? QFileDialog::getOpenFileName(this, "选择发送文件", path->text())
            : QFileDialog::getSaveFileName(this, "选择接收保存路径", path->text());
        if (!selected.isEmpty()) { path->setText(selected); }
    });
    connect(direction, &QComboBox::currentIndexChanged, this, &FileTransferDialog::refresh);
    connect(protocol, &QComboBox::currentIndexChanged, this, &FileTransferDialog::refresh);
    connect(start, &QPushButton::clicked, this, [this] { begin(); });
    connect(save, &QPushButton::clicked, transfer, &SerialTransfer::saveRawReceive);
    connect(stop, &QPushButton::clicked, this, [this]
    {
        if (transfer->active()) { transfer->cancel(); }
        else { reject(); }
    });
    connect(transfer, &SerialTransfer::changed, this, &FileTransferDialog::refresh);
    connect(transfer, &SerialTransfer::finished, this, [this](bool ok)
    {
        succeeded = ok;
        releaseChannel();
        refresh();
    });
    auto updateTimer = new QTimer(this);
    updateTimer->setInterval(250);
    connect(updateTimer, &QTimer::timeout, this, [this] { if (transfer->active()) { refresh(); } });
    updateTimer->start();
    refresh();
}

FileTransferDialog::FileTransferDialog(Channel *uart, QWidget *parent)
    : FileTransferDialog(new SerialTransfer(), parent)
{
    transfer->setParent(this);
    channel = uart;
    setWindowTitle("串口文件传输 · " + uart->config.port);
    transfer->write = [this](const QByteArray &bytes)
    {
        if (!channel || channel->state != "connected") { return false; }
        try { channel->send(bytes, {}, "file-transfer"); return true; }
        catch (const std::exception &) { return false; }
    };
    transfer->discardPendingWrites = [this]
    {
        if (channel && channel->serial && channel->serial->isOpen()) { channel->serial->clear(QSerialPort::Output); }
    };
    connect(uart, &Channel::bytesReceived, transfer, &SerialTransfer::feed);
    if (uart->serial)
    {
        connect(uart->serial, &QSerialPort::bytesWritten, transfer, [this]
        {
            if (channel && channel->serial && channel->serial->bytesToWrite() == 0) { transfer->drained(); }
        });
    }
    connect(uart->engine, &Engine::channelsChanged, this, [this]
    {
        if (transfer->active() && (!channel || channel->state != "connected"))
        {
            transfer->cancel("串口连接已断开，传输已停止");
        }
    });
    connect(uart, &QObject::destroyed, this, [this] { transfer->cancel("串口会话已关闭"); });
}

bool FileTransferDialog::begin()
{
    if (transfer->active()) { return false; }
    if (path->text().trimmed().isEmpty()) { status->setText("请先选择文件"); return false; }
    if (channel)
    {
        if (!channel->serial || channel->state != "connected" || channel->transferOwner)
        {
            status->setText("请选择已连接且未在传输的物理串口");
            return false;
        }
        if (channel->serial->bytesToWrite() != 0)
        {
            status->setText("串口仍有待发送数据，请等待发送完成后重试");
            return false;
        }
        channel->transferOwner = this;
    }
    emit starting();
    elapsed.start();
    succeeded = false;
    const auto mode = protocol->currentIndex() == 0 ? SerialTransfer::Protocol::Raw : SerialTransfer::Protocol::XmodemCrc;
    bool ok = false;
    if (direction->currentIndex() == 0) { ok = transfer->startSend(path->text(), mode); }
    else
    {
        bool valid = true;
        const qint64 size = expectedSize->text().isEmpty() ? -1 : expectedSize->text().toLongLong(&valid);
        if (!valid) { releaseChannel(); status->setText("接收大小无效"); return false; }
        ok = transfer->startReceive(path->text(), mode, size);
    }
    if (!ok) { releaseChannel(); status->setText(transfer->message); }
    refresh();
    return ok;
}

FileTransferDialog::~FileTransferDialog()
{
    transfer->cancel();
    releaseChannel();
}

void FileTransferDialog::releaseChannel()
{
    if (channel && channel->transferOwner == this)
    {
        channel->transferOwner.clear();
        emit channel->engine->channelsChanged();
    }
}

void FileTransferDialog::refresh()
{
    const bool active = transfer->active();
    direction->setEnabled(!active);
    protocol->setEnabled(!active);
    path->setEnabled(!active);
    browse->setEnabled(!active);
    expectedSize->setEnabled(!active && direction->currentIndex() == 1);
    form->setRowVisible(expectedSize, direction->currentIndex() == 1);
    path->setToolTip(path->text());
    start->setEnabled(!active);
    stop->setText(active ? "取消传输" : "关闭");
    save->setVisible(active && transfer->receiving() && transfer->protocol() == SerialTransfer::Protocol::Raw && transfer->total() < 0);
    if (direction->currentIndex() == 0)
    {
        hint->setText(protocol->currentIndex() == 0
            ? "文件内容按原始字节发送，不添加帧头或校验。进度表示驱动发送量，设备端不提供接收确认。"
            : "请先启动设备端的 XMODEM-CRC 接收程序。每块 128 字节，CRC 校验并支持重传；进度按设备端确认更新。");
    }
    else
    {
        hint->setText(protocol->currentIndex() == 0
            ? "原始字节不添加文件名、帧头或校验。接收大小留空时，请在数据发送完毕后点击“停止并保存”。"
            : "请先启动设备端的 XMODEM-CRC 发送程序。协议不携带文件名或长度；接收大小留空时会保留末块 0x1A 填充字节。");
    }
    const auto total = transfer->total();
    const auto done = transfer->completed();
    if (active && total < 0) { progress->setRange(0, 0); progress->setFormat("接收中"); }
    else
    {
        progress->setRange(0, 1000);
        const int value = succeeded ? 1000 : total > 0 ? qMin(999, int(1000.0L * done / total)) : 0;
        progress->setValue(value);
        progress->setFormat(QString::number(value / 10.0, 'f', 1) + "%");
    }
    const double seconds = elapsed.isValid() ? qMax(0.001, elapsed.elapsed() / 1000.0) : 0;
    const double speed = seconds > 0 ? done / seconds : 0;
    QString text = amount(done) + " / " + amount(total);
    if (seconds > 0)
    {
        text += "  ·  " + amount(qint64(speed)) + "/s";
        if (active && total > done && speed > 0) { text += QString("  ·  剩余约 %1 秒").arg(qCeil((total - done) / speed)); }
    }
    text += QString("  ·  重试 %1 次").arg(transfer->retries());
    summary->setText(text);
    if (!transfer->message.isEmpty()) { status->setText(transfer->message); }
}

void FileTransferDialog::reject()
{
    transfer->cancel();
    releaseChannel();
    QDialog::reject();
}

void FileTransferDialog::closeEvent(QCloseEvent *event)
{
    transfer->cancel();
    releaseChannel();
    QDialog::closeEvent(event);
}
}
