// SPDX-License-Identifier: Apache-2.0
#include "serial_transfer.h"
#include "file_transfer_dialog.h"
#include "window.h"
#include <QtTest>

using sw::SerialTransfer;

namespace
{
QByteArray contents(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { return {}; }
    return file.readAll();
}

bool store(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

QByteArray fixture(int size)
{
    QByteArray result(size, 0);
    for (int i = 0; i < size; ++i) { result[i] = char((i * 37 + i / 19) & 255); }
    if (size) { result[size - 1] = 0x1a; } // A real trailing SUB must never be trimmed.
    return result;
}

struct Wire : QObject
{
    SerialTransfer sender;
    SerialTransfer receiver;
    bool corruptOnce = false;
    bool dropAckOnce = false;
    bool dropFinalAckOnce = false;
    int packets = 0;
    Wire()
    {
        sender.responseTimeoutMs = 60;
        sender.handshakeTimeoutMs = 100;
        receiver.responseTimeoutMs = 200;
        receiver.receiverLingerMs = 150;
        sender.write = [this](QByteArray bytes)
        {
            if (bytes.size() == 133)
            {
                ++packets;
                if (corruptOnce) { bytes[20] ^= 0x40; corruptOnce = false; }
            }
            QTimer::singleShot(0, this, [this, bytes]
            {
                // Arbitrary UART chunks, not whole packets; raw send uses drain acknowledgement.
                for (int i = 0; i < bytes.size(); i += 17) { receiver.feed(bytes.mid(i, 17)); }
                sender.drained();
            });
            return true;
        };
        receiver.write = [this](const QByteArray &bytes)
        {
            if (bytes == QByteArray(1, 6) && dropAckOnce)
            {
                dropAckOnce = false;
                return true;
            }
            if (bytes == QByteArray(1, 6) && receiver.completed() == receiver.total() && dropFinalAckOnce && !receiver.message.startsWith("正在接收"))
            {
                dropFinalAckOnce = false;
                return true;
            }
            QTimer::singleShot(0, this, [this, bytes] { sender.feed(bytes); });
            return true;
        };
    }
};
}

class SerialTransferTests : public QObject
{
    Q_OBJECT
private slots:
    void crcReference()
    {
        QCOMPARE(SerialTransfer::crc("123456789"), quint16(0x31c3));
    }

    void roundTrip_data()
    {
        QTest::addColumn<int>("length");
        QTest::addColumn<bool>("raw");
        for (int length : {0, 1, 127, 128, 129, 4096, 33001})
        {
            QTest::newRow(qPrintable(QString("crc-%1").arg(length))) << length << false;
            QTest::newRow(qPrintable(QString("raw-%1").arg(length))) << length << true;
        }
    }

    void roundTrip()
    {
        QFETCH(int, length);
        QFETCH(bool, raw);
        QTemporaryDir directory;
        const auto data = fixture(length);
        QVERIFY(store(directory.filePath("input.bin"), data));
        Wire wire;
        const auto protocol = raw ? SerialTransfer::Protocol::Raw : SerialTransfer::Protocol::XmodemCrc;
        QSignalSpy sent(&wire.sender, &SerialTransfer::finished);
        QSignalSpy received(&wire.receiver, &SerialTransfer::finished);
        QVERIFY(wire.sender.startSend(directory.filePath("input.bin"), protocol));
        QVERIFY(wire.receiver.startReceive(directory.filePath("output.bin"), protocol, length));
        QTRY_COMPARE_WITH_TIMEOUT(sent.size(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(received.size(), 1, 5000);
        QVERIFY2(sent[0][0].toBool(), qPrintable(wire.sender.message));
        QVERIFY2(received[0][0].toBool(), qPrintable(wire.receiver.message));
        QCOMPARE(contents(directory.filePath("output.bin")), data);
        QCOMPARE(wire.sender.completed(), qint64(length));
        QCOMPARE(wire.receiver.completed(), qint64(length));
    }

    void corruptionAndLostAcknowledgements()
    {
        QTemporaryDir directory;
        const auto data = fixture(513);
        QVERIFY(store(directory.filePath("input"), data));
        Wire wire;
        wire.corruptOnce = true;
        wire.dropAckOnce = true;
        wire.dropFinalAckOnce = true;
        QSignalSpy sent(&wire.sender, &SerialTransfer::finished);
        QSignalSpy received(&wire.receiver, &SerialTransfer::finished);
        QVERIFY(wire.sender.startSend(directory.filePath("input"), SerialTransfer::Protocol::XmodemCrc));
        QVERIFY(wire.receiver.startReceive(directory.filePath("output"), SerialTransfer::Protocol::XmodemCrc, data.size()));
        QTRY_COMPARE_WITH_TIMEOUT(sent.size(), 1, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(received.size(), 1, 3000);
        QVERIFY2(sent[0][0].toBool(), qPrintable(wire.sender.message));
        QVERIFY(received[0][0].toBool());
        QVERIFY(wire.sender.retries() >= 3);
        QVERIFY(wire.receiver.retries() >= 1);
        QCOMPARE(contents(directory.filePath("output")), data);
    }

    void progressRequiresAck()
    {
        QTemporaryDir directory;
        QVERIFY(store(directory.filePath("input"), fixture(256)));
        SerialTransfer sender;
        QList<QByteArray> writes;
        sender.write = [&](const QByteArray &bytes) { writes.append(bytes); return true; };
        QVERIFY(sender.startSend(directory.filePath("input"), SerialTransfer::Protocol::XmodemCrc));
        sender.feed("C");
        QCOMPARE(writes.size(), 1);
        QCOMPARE(sender.completed(), qint64(0));
        QCOMPARE(writes[0].size(), 133);
        sender.feed(QByteArray(1, 0x06));
        QCOMPARE(sender.completed(), qint64(128));
        sender.cancel();
        QVERIFY(!sender.active());
    }

    void cancellationPreservesDestination()
    {
        QTemporaryDir directory;
        const auto destination = directory.filePath("existing.bin");
        QVERIFY(store(destination, "original"));
        SerialTransfer receiver;
        receiver.write = [](const QByteArray &) { return true; };
        QVERIFY(receiver.startReceive(destination, SerialTransfer::Protocol::Raw));
        receiver.feed("partial new bytes");
        QCOMPARE(contents(destination), QByteArray("original"));
        receiver.cancel();
        QCOMPARE(contents(destination), QByteArray("original"));
        QVERIFY(receiver.startReceive(destination, SerialTransfer::Protocol::Raw));
        receiver.feed("new bytes");
        receiver.saveRawReceive();
        QCOMPARE(contents(destination), QByteArray("new bytes"));
    }

    void unknownXmodemLengthKeepsPadding()
    {
        QTemporaryDir directory;
        QVERIFY(store(directory.filePath("input"), "abc"));
        Wire wire;
        QSignalSpy received(&wire.receiver, &SerialTransfer::finished);
        QVERIFY(wire.sender.startSend(directory.filePath("input"), SerialTransfer::Protocol::XmodemCrc));
        QVERIFY(wire.receiver.startReceive(directory.filePath("output"), SerialTransfer::Protocol::XmodemCrc));
        QTRY_COMPARE(received.size(), 1);
        QCOMPARE(contents(directory.filePath("output")), QByteArray("abc") + QByteArray(125, 0x1a));
    }

    void receiveOneKilobyteFrame()
    {
        QTemporaryDir directory;
        SerialTransfer receiver;
        receiver.receiverLingerMs = 30;
        QList<QByteArray> replies;
        receiver.write = [&](const QByteArray &bytes) { replies.append(bytes); return true; };
        const auto data = fixture(1024);
        QVERIFY(receiver.startReceive(directory.filePath("output"), SerialTransfer::Protocol::XmodemCrc, 1024));
        QByteArray frame = QByteArray::fromHex("0201fe") + data;
        const auto checksum = SerialTransfer::crc(data);
        frame += char(checksum >> 8);
        frame += char(checksum & 255);
        receiver.feed(frame.left(500));
        QCOMPARE(receiver.completed(), qint64(0));
        receiver.feed(frame.mid(500));
        QCOMPARE(receiver.completed(), qint64(1024));
        QCOMPARE(replies.back(), QByteArray(1, 6));
        receiver.feed(QByteArray(1, 4));
        QCOMPARE(replies.back(), QByteArray(1, 0x15));
        receiver.feed(QByteArray(1, 4));
        QTRY_VERIFY(!receiver.active());
        QCOMPARE(contents(directory.filePath("output")), data);
    }

    void prematureEndAndOversizedRawPreserveDestination()
    {
        QTemporaryDir directory;
        const auto destination = directory.filePath("output");
        QVERIFY(store(destination, "original"));
        SerialTransfer receiver;
        receiver.write = [](const QByteArray &) { return true; };
        QVERIFY(receiver.startReceive(destination, SerialTransfer::Protocol::XmodemCrc, 512));
        receiver.feed(QByteArray(2, 4));
        QVERIFY(!receiver.active());
        QCOMPARE(contents(destination), QByteArray("original"));
        QVERIFY(receiver.startReceive(destination, SerialTransfer::Protocol::Raw, 3));
        receiver.feed("too long");
        QVERIFY(!receiver.active());
        QCOMPARE(contents(destination), QByteArray("original"));
    }

    void dialogCancelAndReceiveProgress()
    {
        QTemporaryDir directory;
        SerialTransfer receiver;
        receiver.write = [](const QByteArray &) { return true; };
        sw::FileTransferDialog dialog(&receiver);
        dialog.direction->setCurrentIndex(1);
        dialog.protocol->setCurrentIndex(0);
        dialog.path->setText(directory.filePath("output"));
        QVERIFY(dialog.begin());
        QCOMPARE(dialog.progress->maximum(), 0);
        QVERIFY(!dialog.start->isEnabled());
        receiver.feed("raw bytes");
        receiver.saveRawReceive();
        QCOMPARE(dialog.progress->value(), 1000);
        QVERIFY(dialog.start->isEnabled());
        QCOMPARE(contents(directory.filePath("output")), QByteArray("raw bytes"));
        QVERIFY(dialog.begin());
        receiver.feed("partial");
        dialog.reject();
        QVERIFY(!receiver.active());
        QCOMPARE(contents(directory.filePath("output")), QByteArray("raw bytes"));
    }

    void peerCancelAndTimeout()
    {
        QTemporaryDir directory;
        QVERIFY(store(directory.filePath("input"), fixture(10)));
        SerialTransfer sender;
        sender.handshakeTimeoutMs = 30;
        sender.write = [](const QByteArray &) { return true; };
        QSignalSpy done(&sender, &SerialTransfer::finished);
        QVERIFY(sender.startSend(directory.filePath("input"), SerialTransfer::Protocol::XmodemCrc));
        sender.feed(QByteArray(1, 0x18));
        QVERIFY(sender.active());
        sender.feed(QByteArray(1, 0x18));
        QVERIFY(!sender.active());
        QVERIFY(!done[0][0].toBool());
        QVERIFY(sender.startSend(directory.filePath("input"), SerialTransfer::Protocol::XmodemCrc));
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 1000);
        QVERIFY(!done[1][0].toBool());
        QCOMPARE(sender.completed(), qint64(0));
    }

    void retryLimitAndWriteFailure()
    {
        QTemporaryDir directory;
        QVERIFY(store(directory.filePath("input"), fixture(128)));
        SerialTransfer sender;
        sender.write = [](const QByteArray &) { return true; };
        QSignalSpy done(&sender, &SerialTransfer::finished);
        QVERIFY(sender.startSend(directory.filePath("input"), SerialTransfer::Protocol::XmodemCrc));
        sender.feed("C");
        for (int i = 0; i < 11; ++i) { sender.feed(QByteArray(1, 0x15)); }
        QCOMPARE(done.size(), 1);
        QVERIFY(!done[0][0].toBool());
        QCOMPARE(sender.completed(), qint64(0));
        sender.write = [](const QByteArray &) { return false; };
        QVERIFY(sender.startSend(directory.filePath("input"), SerialTransfer::Protocol::Raw));
        QTRY_COMPARE(done.size(), 2);
        QVERIFY(!done[1][0].toBool());
    }

    void channelExclusiveAndParserIsolation()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        QObject owner;
        channel->transferOwner = &owner;
        QVERIFY(channel->describe()["file_transfer_active"].toBool());
        QVERIFY_EXCEPTION_THROWN(channel->send("other", {}, "mcp"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(channel->applySerial(config), std::runtime_error);
        channel->configureParser("json");
        QSignalSpy input(channel, &sw::Channel::bytesReceived);
        channel->receive("@wave v=1\n{\"v\":1}\n");
        QCOMPARE(input.size(), 1);
        QCOMPARE(channel->sources.size(), 0);
        const auto event = engine.journal.events.back().first;
        QCOMPARE(event["message"].toString(), QString("file-transfer"));
        channel->transferOwner.clear();
        channel->receive("@wave v=2\n{\"v\":2}\n");
        QCOMPARE(channel->sources["default"]->count, qint64(1));
        QCOMPARE(channel->sources["default"]->parsedCount, qint64(1));
    }

    void middleClickLiveConnectionClosesTransport()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.port->setCurrentText("loop://");
        window.connectionAction();
        QVERIFY(window.active->channel);
        const auto id = window.active->channel->id;
        QTest::mouseClick(window.tabs, Qt::MiddleButton, Qt::NoModifier, window.tabs->tabRect(0).center());
        QVERIFY(!engine.channels.contains(id));
        QCOMPARE(window.tabs->count(), 1);
        QVERIFY(!window.active->channel);
        window.close();
    }

    void middleClickTabs()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.newConnection();
        const auto active = window.active;
        QTest::mouseClick(window.tabs, Qt::MiddleButton, Qt::NoModifier, window.tabs->tabRect(0).center());
        QCOMPARE(window.tabs->count(), 1);
        QCOMPARE(window.active, active);
        window.newConnection();
        QTest::mousePress(window.tabs, Qt::MiddleButton, Qt::NoModifier, window.tabs->tabRect(0).center());
        QTest::mouseRelease(window.tabs, Qt::MiddleButton, Qt::NoModifier, window.tabs->tabRect(1).center());
        QCOMPARE(window.tabs->count(), 2);
        window.closeTab(1);
        auto widget = window.newInstrument("waveform");
        QVERIFY(widget);
        QTest::mouseClick(window.tabs, Qt::MiddleButton, Qt::NoModifier, window.tabs->tabRect(1).center());
        QCOMPARE(window.tabs->count(), 1);
        window.newConnection();
        auto floating = window.detachConnection(1, QPoint(200, 180));
        QVERIFY(floating);
        auto previous = floating->active;
        QTest::mouseClick(floating->tabs, Qt::MiddleButton, Qt::NoModifier, floating->tabs->tabRect(0).center());
        QCOMPARE(floating->tabs->count(), 1); // Closing the final tab creates a new empty receive page.
        QVERIFY(floating->active != previous);
        floating->close();
        window.close();
    }
};

QTEST_MAIN(SerialTransferTests)
#include "serial_transfer_tests.moc"
