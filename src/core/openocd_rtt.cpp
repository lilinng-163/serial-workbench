// SPDX-License-Identifier: Apache-2.0
#include "openocd.h"
#include "core.h"

namespace sw
{
namespace
{
constexpr qsizetype MaxRttBytes = 512 * 1024;
constexpr int MaxRttRecords = 2000;
constexpr int RttTcpTimeoutMs = 5000;
constexpr int MaxRttConnectAttempts = 12;
}

void OpenOcdService::initializeRtt()
{
    rttSocket.setReadBufferSize(256 * 1024);
    rttReadTimer.setSingleShot(true);
    rttReadTimer.setInterval(16);
    rttConnectRetry.setSingleShot(true);
    rttConnectRetry.setInterval(250);
    connect(&rttConnectRetry, &QTimer::timeout, this, &OpenOcdService::connectRtt);
    connect(&rttReadTimer, &QTimer::timeout, this, &OpenOcdService::receiveRtt);
    connect(&rttSocket, &QTcpSocket::readyRead, this, [this]
    {
        if (!rttReadTimer.isActive()) { rttReadTimer.start(); }
    });
    connect(&rttSocket, &QTcpSocket::connected, this, [this]
    {
        if (!running || operation != "rtt" || rttClosing || rttStatus != "connecting") { return; }
        rttStatus = "connected";
        // The deadline bounds startup, not an ongoing log session.
        deadline.stop();
        rttConnectRetry.stop();
        appendLog("RTT TCP 已连接 " + rttEndpoint() + "\n");
        emit stateChanged();
    });
    connect(&rttSocket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError socketError)
    {
        if (!running || status != "running" || operation != "rtt" || rttClosing) { return; }
        if (rttStatus == "connecting" && socketError == QAbstractSocket::ConnectionRefusedError
            && deadline.isActive() && rttConnectAttempts < MaxRttConnectAttempts)
        {
            if (rttConnectAttempts == 1)
            {
                appendLog("RTT 服务暂未接受连接，正在短暂重试 " + rttEndpoint() + "\n");
                emit stateChanged();
            }
            // Retry after Qt has finished processing the failed socket attempt.
            rttConnectRetry.start();
            return;
        }
        const auto message = rttStatus == "connected" ? "RTT 服务已断开 " : "无法连接 RTT 服务 ";
        error = message + rttEndpoint() + "：" + rttSocket.errorString();
        appendLog(error + "\n");
        stop("failed");
    });
    connect(&rttSocket, &QTcpSocket::disconnected, this, [this]
    {
        if (!running || status != "running" || operation != "rtt" || rttClosing || rttStatus != "connected") { return; }
        error = "RTT 服务已断开 " + rttEndpoint();
        appendLog(error + "\n");
        stop("failed");
    });
}

bool OpenOcdService::ownsProbe() const
{
    return running && operation != "version"
        && !(operation == "rtt" && config["rtt_mode"] == "external");
}

void OpenOcdService::resetRtt(int port)
{
    closeRtt();
    rttJob = jobId;
    rttStartSequence = rttSequence;
    rttRecords.clear();
    rttReceived = 0;
    rttBytes = 0;
    rttDecoder.resetState();
    rttHost = config["rtt_mode"] == "external" ? config["rtt_host"].toString().trimmed() : "127.0.0.1";
    rttPort = port;
    rttConnectAttempts = 0;
    rttStatus = "starting";
}

void OpenOcdService::connectRtt()
{
    if (!running || status != "running" || operation != "rtt" || rttClosing || !deadline.isActive()) { return; }
    if (rttStatus != "starting" && rttStatus != "connecting") { return; }
    if (config["rtt_mode"] == "managed" && process.state() != QProcess::Running) { return; }

    if (rttStatus == "starting")
    {
        rttStatus = "connecting";
        deadline.start(qMin(deadline.remainingTime(), RttTcpTimeoutMs));
        appendLog("正在连接 RTT 服务 " + rttEndpoint() + "\n");
    }
    if (rttSocket.state() != QAbstractSocket::UnconnectedState)
    {
        const QScopedValueRollback<bool> guard(rttClosing, true);
        rttSocket.abort();
    }
    ++rttConnectAttempts;
    rttSocket.connectToHost(rttHost, quint16(rttPort));
    emit stateChanged();
}

QString OpenOcdService::rttEndpoint() const
{
    const auto host = rttHost.contains(':') ? "[" + rttHost + "]" : rttHost;
    return host + ":" + QString::number(rttPort);
}

void OpenOcdService::receiveRtt()
{
    bool received = false;
    qsizetype remaining = 64 * 1024;
    while (remaining > 0 && rttSocket.bytesAvailable() > 0)
    {
        const auto bytes = rttSocket.read(qMin<qsizetype>(8192, remaining));
        if (bytes.isEmpty()) { break; }
        remaining -= bytes.size();
        rttReceived += bytes.size();
        rttBytes += bytes.size();
        const QString text = rttDecoder.decode(bytes);
        rttRecords.append(QJsonObject {
            {"seq", ++rttSequence},
            {"time", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {"size", bytes.size()},
            {"hex", QString::fromLatin1(bytes.toHex())},
            {"text", text}
        });
        while (!rttRecords.isEmpty() && (rttBytes > MaxRttBytes || rttRecords.size() > MaxRttRecords))
        {
            rttBytes -= rttRecords.takeFirst()["size"].toInteger();
        }
        received = true;
    }
    if (received) { emit rttDataAvailable(); }
    if (!rttClosing && rttSocket.bytesAvailable() > 0) { rttReadTimer.start(); }
}

void OpenOcdService::closeRtt()
{
    const QScopedValueRollback<bool> guard(rttClosing, true);
    rttConnectRetry.stop();
    rttReadTimer.stop();
    // Drain the bounded socket buffer before aborting so the final line survives.
    while (rttSocket.bytesAvailable() > 0)
    {
        const auto buffered = rttSocket.bytesAvailable();
        receiveRtt();
        if (rttSocket.bytesAvailable() >= buffered) { break; }
    }
    rttSocket.abort();
    if (rttStatus != "idle") { rttStatus = "closed"; }
}

QJsonObject OpenOcdService::readRtt(qint64 after, int limit) const
{
    if (after < 0 || after > rttSequence || limit < 1 || limit > 200)
    {
        fail("RTT after 应为 0..latest_cursor，limit 应为 1..200");
    }
    const auto effectiveAfter = qMax(after, rttStartSequence);
    QJsonArray page;
    qint64 cursor = rttRecords.isEmpty() ? rttSequence : effectiveAfter;
    for (const auto &record : rttRecords)
    {
        if (record["seq"].toInteger() <= effectiveAfter) { continue; }
        page.append(record);
        cursor = record["seq"].toInteger();
        if (page.size() >= limit) { break; }
    }
    return {
        {"view_id", viewId}, {"job_id", rttJob}, {"status", rttStatus},
        {"host", rttHost}, {"port", rttPort}, {"connect_attempts", rttConnectAttempts}, {"received_bytes", rttReceived},
        {"records", page}, {"next_cursor", cursor}, {"latest_cursor", rttSequence},
        {"gap", rttRecords.isEmpty() ? effectiveAfter < rttSequence : effectiveAfter + 1 < rttRecords.first()["seq"].toInteger()},
        {"has_more", cursor < rttSequence}
    };
}
}
