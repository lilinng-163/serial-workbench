// SPDX-License-Identifier: Apache-2.0
#include "serial_transfer.h"

namespace sw
{
namespace
{
constexpr char Soh = 0x01;
constexpr char Stx = 0x02;
constexpr char Eot = 0x04;
constexpr char Ack = 0x06;
constexpr char Nak = 0x15;
constexpr char Can = 0x18;
constexpr char CrcRequest = 'C';
}

SerialTransfer::SerialTransfer(QObject *parent) : QObject(parent)
{
    timer.setInterval(25);
    connect(&timer, &QTimer::timeout, this, &SerialTransfer::timeout);
}

SerialTransfer::~SerialTransfer()
{
    // QSaveFile removes an uncommitted temporary file automatically.
    timer.stop();
}

quint16 SerialTransfer::crc(const QByteArray &bytes)
{
    quint16 result = 0;
    for (unsigned char byte : bytes)
    {
        result ^= quint16(byte) << 8;
        for (int bit = 0; bit < 8; ++bit)
        {
            result = result & 0x8000 ? quint16((result << 1) ^ 0x1021) : quint16(result << 1);
        }
    }
    return result;
}

bool SerialTransfer::begin(Protocol protocol, bool receive)
{
    if (running) { return false; }
    mode = protocol;
    receiveMode = receive;
    done = 0;
    block = 1;
    retryCount = 0;
    consecutiveRetries = 0;
    firstEot = false;
    sawCancel = false;
    pending.clear();
    packet.clear();
    chunkSize = 0;
    message.clear();
    input.close();
    output.reset();
    return true;
}

bool SerialTransfer::startSend(const QString &path, Protocol protocol)
{
    if (!begin(protocol, false)) { return false; }
    input.setFileName(path);
    if (!input.open(QIODevice::ReadOnly))
    {
        message = input.errorString();
        emit changed();
        return false;
    }
    size = input.size();
    running = true;
    deadline.start();
    timer.start();
    state = mode == Protocol::Raw ? State::RawSend : State::WaitStart;
    message = mode == Protocol::Raw ? "正在发送原始字节" : "等待接收端发送 C（CRC 模式）";
    emit changed();
    if (mode == Protocol::Raw) { QTimer::singleShot(0, this, &SerialTransfer::sendNext); }
    return true;
}

bool SerialTransfer::startReceive(const QString &path, Protocol protocol, qint64 expectedSize)
{
    if (expectedSize < -1 || !begin(protocol, true)) { return false; }
    output = std::make_unique<QSaveFile>(path);
    output->setDirectWriteFallback(false);
    if (!output->open(QIODevice::WriteOnly))
    {
        message = output->errorString();
        output.reset();
        emit changed();
        return false;
    }
    size = expectedSize;
    running = true;
    deadline.start();
    timer.start();
    state = mode == Protocol::Raw ? State::RawReceive : State::Receive;
    message = mode == Protocol::Raw ? "正在接收原始字节" : "等待 XMODEM-CRC 发送端";
    emit changed();
    if (mode == Protocol::XmodemCrc) { transmit(QByteArray(1, CrcRequest)); }
    else if (size == 0 && commit()) { complete(true, "接收完成"); }
    return true;
}

bool SerialTransfer::transmit(const QByteArray &bytes)
{
    if (!running) { return false; }
    deadline.restart();
    if (!write || !write(bytes))
    {
        complete(false, "串口写入失败，传输已停止");
        return false;
    }
    return true;
}

void SerialTransfer::sendNext()
{
    if (!running || receiveMode) { return; }
    if (done == size)
    {
        if (mode == Protocol::Raw) { complete(true, "发送完成（驱动已发送，设备端无确认）"); }
        else
        {
            state = State::WaitEot;
            packet = QByteArray(1, Eot);
            transmit(packet);
        }
        return;
    }
    const qint64 length = qMin<qint64>(mode == Protocol::Raw ? 4096 : 128, size - done);
    QByteArray bytes = input.read(length);
    if (bytes.size() != length)
    {
        complete(false, "源文件读取失败或在传输期间发生变化");
        return;
    }
    chunkSize = length;
    if (mode == Protocol::Raw)
    {
        state = State::RawSend;
        transmit(bytes);
    }
    else
    {
        bytes.append(QByteArray(128 - bytes.size(), char(0x1a)));
        packet = QByteArray(1, Soh);
        packet += char(block & 255);
        packet += char(255 - (block & 255));
        packet += bytes;
        const auto checksum = crc(bytes);
        packet += char(checksum >> 8);
        packet += char(checksum & 255);
        state = State::WaitAck;
        transmit(packet);
    }
}

void SerialTransfer::drained()
{
    if (!running || state != State::RawSend || chunkSize == 0) { return; }
    done += chunkSize;
    chunkSize = 0;
    deadline.restart();
    emit changed();
    QTimer::singleShot(0, this, &SerialTransfer::sendNext);
}

void SerialTransfer::resend()
{
    if (++consecutiveRetries > 10)
    {
        cancel("连续重试超过 10 次，传输失败");
        return;
    }
    ++retryCount;
    message = QString("正在重传 · 第 %1 次").arg(retryCount);
    emit changed();
    transmit(packet);
}

bool SerialTransfer::commit()
{
    if (!output || !output->commit())
    {
        complete(false, output ? "保存失败：" + output->errorString() : "目标文件不可用");
        return false;
    }
    output.reset();
    return true;
}

void SerialTransfer::receivePacket(const QByteArray &frame, int payloadSize)
{
    const auto number = quint8(frame[1]);
    const auto inverse = quint8(frame[2]);
    const auto bytes = frame.mid(3, payloadSize);
    const quint16 checksum = (quint16(quint8(frame[3 + payloadSize])) << 8) | quint8(frame[4 + payloadSize]);
    if ((number ^ inverse) != 255 || checksum != crc(bytes))
    {
        if (++consecutiveRetries > 10) { cancel("连续无效数据块超过 10 次"); return; }
        ++retryCount;
        message = "数据块校验失败，请求重传";
        emit changed();
        transmit(QByteArray(1, Nak));
        return;
    }
    if (block > 1 && number == ((block - 1) & 255))
    {
        // Lost ACK: validate the duplicate, acknowledge it, never append twice.
        transmit(QByteArray(1, Ack));
        return;
    }
    if (number != (block & 255)) { cancel("XMODEM 数据块序号不连续"); return; }
    if (size >= 0 && done >= size) { cancel("发送端的数据超过指定文件大小"); return; }
    const qint64 length = size < 0 ? payloadSize : qMin<qint64>(payloadSize, size - done);
    if (!output || output->write(bytes.constData(), length) != length)
    {
        cancel("目标文件写入失败");
        return;
    }
    done += length;
    ++block;
    consecutiveRetries = 0;
    firstEot = false;
    message = "正在接收 · CRC 已校验";
    emit changed();
    transmit(QByteArray(1, Ack));
}

void SerialTransfer::feed(const QByteArray &bytes)
{
    if (!running || bytes.isEmpty()) { return; }
    if (state == State::RawReceive)
    {
        if (size >= 0 && bytes.size() > size - done) { cancel("收到的数据超过指定文件大小"); return; }
        if (output->write(bytes) != bytes.size()) { cancel("目标文件写入失败"); return; }
        done += bytes.size();
        emit changed();
        if (size >= 0 && done == size && commit()) { complete(true, "接收完成"); }
        return;
    }
    if (mode != Protocol::XmodemCrc) { return; }
    if (pending.size() + bytes.size() > 4096)
    {
        cancel("收到过多非 XMODEM 数据，请停止设备日志后重试");
        return;
    }
    pending += bytes;
    while (running && !pending.isEmpty())
    {
        const char command = pending[0];
        if (receiveMode && state == State::Receive && (command == Soh || command == Stx))
        {
            const int length = command == Soh ? 128 : 1024;
            if (pending.size() < length + 5) { break; }
            const auto frame = pending.left(length + 5);
            pending.remove(0, length + 5);
            sawCancel = false;
            receivePacket(frame, length);
            continue;
        }
        pending.remove(0, 1);
        if (command == Can)
        {
            if (sawCancel) { complete(false, "对端取消了传输"); return; }
            sawCancel = true;
            continue;
        }
        sawCancel = false;
        if (receiveMode && command == Eot)
        {
            if (state == State::Linger) { transmit(QByteArray(1, Ack)); }
            else if (!firstEot)
            {
                firstEot = true;
                transmit(QByteArray(1, Nak));
            }
            else if (size >= 0 && done != size) { cancel("文件提前结束，大小与预期不符"); }
            else if (commit())
            {
                state = State::Linger;
                message = "文件已保存，正在确认传输结束";
                emit changed();
                transmit(QByteArray(1, Ack));
            }
        }
        else if (!receiveMode)
        {
            if (state == State::WaitStart && command == CrcRequest)
            {
                message = "正在发送 · 等待数据块确认";
                sendNext();
            }
            else if (state == State::WaitAck && command == Ack)
            {
                done += chunkSize;
                chunkSize = 0;
                ++block;
                consecutiveRetries = 0;
                // Consume one ACK per packet; schedule next send to avoid recursion.
                state = State::Idle;
                emit changed();
                QTimer::singleShot(0, this, &SerialTransfer::sendNext);
            }
            else if (state == State::WaitEot && command == Ack) { complete(true, "发送完成，对端已确认"); }
            else if (state == State::WaitEot && command == Nak && !firstEot)
            {
                firstEot = true;
                transmit(packet);
            }
            else if ((state == State::WaitAck || state == State::WaitEot) && command == Nak) { resend(); }
        }
    }
}

void SerialTransfer::timeout()
{
    if (!running) { return; }
    if (state == State::Linger)
    {
        if (deadline.elapsed() >= receiverLingerMs) { complete(true, "接收完成，文件已保存"); }
        return;
    }
    if (state == State::RawReceive) { return; } // No framing: user chooses when the stream ends.
    if (state == State::WaitStart)
    {
        if (deadline.elapsed() >= handshakeTimeoutMs) { cancel("等待接收端 C 超时，请先启动设备端接收程序"); }
        return;
    }
    const int wait = state == State::WaitEot ? qMin(1000, responseTimeoutMs)
        : state == State::Receive && block == 1 && pending.isEmpty() ? 1000 : responseTimeoutMs;
    if (deadline.elapsed() < wait) { return; }
    if (state == State::RawSend) { cancel("串口发送超时"); return; }
    if (state == State::WaitAck || state == State::WaitEot) { resend(); return; }
    if (state == State::Receive)
    {
        pending.clear(); // Discard an incomplete packet before requesting retransmission.
        if (++consecutiveRetries > 30) { cancel("等待发送端超时"); return; }
        ++retryCount;
        transmit(QByteArray(1, block == 1 && !firstEot ? CrcRequest : Nak));
        emit changed();
    }
}

void SerialTransfer::cancel(const QString &reason)
{
    if (!running) { return; }
    if (discardPendingWrites) { discardPendingWrites(); }
    if (mode == Protocol::XmodemCrc && write) { write(QByteArray(2, Can)); }
    complete(false, reason);
}

void SerialTransfer::saveRawReceive()
{
    if (running && state == State::RawReceive && commit()) { complete(true, "接收已停止，文件已保存"); }
}

void SerialTransfer::complete(bool success, const QString &text)
{
    if (!running) { return; }
    running = false;
    state = State::Idle;
    timer.stop();
    input.close();
    if (output) { output->cancelWriting(); output.reset(); }
    message = text;
    emit changed();
    emit finished(success, text);
}
}
