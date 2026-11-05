// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QtCore>
#include <functional>

namespace sw
{
struct SerialSendOptions
{
    static constexpr int MaxChunkBytes = 4096;
    static constexpr int MaxGapMs = 60000;
    int rawChunkBytes = MaxChunkBytes;
    int rawGapMs = 0;
    int baudRate = 0;
    int halfBitsPerByte = 20; // Start, data, parity and stop bits, in half-bit units.
};

// Protocol engine: transport owns the UART, this class owns the file and framing.
class SerialTransfer : public QObject
{
    Q_OBJECT
public:
    enum class Protocol { Raw, XmodemCrc, Xmodem1k };
    explicit SerialTransfer(QObject *parent = nullptr);
    ~SerialTransfer() override;

    std::function<bool(const QByteArray &)> write;
    std::function<void()> discardPendingWrites;
    bool startSend(const QString &path, Protocol protocol, const SerialSendOptions &options = {});
    bool startReceive(const QString &path, Protocol protocol, qint64 expectedSize = -1);
    void feed(const QByteArray &bytes);
    void drained();
    void cancel(const QString &reason = QStringLiteral("已取消传输"));
    void saveRawReceive();
    bool active() const { return running; }
    bool receiving() const { return receiveMode; }
    Protocol protocol() const { return mode; }
    qint64 completed() const { return done; }
    qint64 total() const { return size; }
    int retries() const { return retryCount; }
    QString message;
    // Configurable timing lets protocol tests exercise real timeouts quickly.
    int responseTimeoutMs = 10000;
    int handshakeTimeoutMs = 30000;
    int receiverLingerMs = 2000;
    static quint16 crc(const QByteArray &bytes);

signals:
    void changed();
    void finished(bool success, const QString &message);

private:
    enum class State { Idle, SendNext, RawSend, RawReceive, WaitStart, WaitAck, WaitEot, Receive, Linger };
    State state = State::Idle;
    Protocol mode = Protocol::Raw;
    bool running = false;
    bool receiveMode = false;
    bool firstEot = false;
    bool sawCancel = false;
    int block = 1;
    int retryCount = 0;
    int consecutiveRetries = 0;
    qint64 done = 0;
    qint64 size = -1;
    qint64 chunkSize = 0;
    qint64 wireTimeMs = 0;
    SerialSendOptions sendOptions;
    QFile input;
    std::unique_ptr<QSaveFile> output;
    QByteArray pending;
    QByteArray packet;
    QTimer timer;
    QTimer sendTimer;
    QElapsedTimer deadline;
    bool begin(Protocol protocol, bool receive);
    bool transmit(const QByteArray &bytes);
    void queueNext(int delayMs = 0);
    void sendNext();
    void resend();
    void timeout();
    void complete(bool success, const QString &text);
    bool commit();
    void receivePacket(const QByteArray &frame, int payloadSize);
};
}
