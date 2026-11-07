// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "log_filter.h"
#include <QtCore>
#include <QTcpSocket>
#include <memory>

namespace sw
{
// One service per board: its GUI page and MCP share one job and configuration.
class OpenOcdService : public QObject
{
    Q_OBJECT
public:
    explicit OpenOcdService(QObject *parent = nullptr);
    ~OpenOcdService() override;
    QString viewId = QUuid::createUuid().toString(QUuid::Id128);
    QString title = "OpenOCD";
    bool hasPage = false;

    QJsonObject configuration() const { return config; }
    qint64 revision() const { return configRevision; }
    bool busy() const { return running; }
    void restore(const QJsonObject &saved);
    void configure(const QJsonObject &patch, qint64 expectedRevision);
    QJsonObject exportConfiguration() const;
    void importConfiguration(const QJsonObject &document, qint64 expectedRevision);
    QJsonObject catalog(const QString &executable = {}, const QString &scriptsDirectory = {}) const;
    QJsonObject preview(const QString &operation) const;
    QJsonObject state(qint64 after = 0, int limit = 100) const;
    void start(const QString &operation, qint64 expectedRevision);
    void cancel(const QString &expectedJobId);
    QJsonObject readRtt(qint64 after = 0, int limit = 100) const;
    const SearchFilter &rttFilter() const { return rttSearchFilter; }
    void setRttFilter(const SearchFilter &filter);
    QJsonObject searchRtt(int offset, int limit, const QString &snapshot = {}, qint64 after = 0, qint64 through = -1) const;
    bool ownsProbe() const;

signals:
    void configurationChanged();
    void stateChanged();
    void rttDataAvailable();
    void rttFilterChanged();

private:
    QJsonObject config;
    qint64 configRevision = 0;
    QProcess process;
    QTimer deadline;
    QTimer killTimer;
    QElapsedTimer elapsed;
    std::unique_ptr<QTemporaryDir> workspace;
    QStringDecoder decoder {QStringDecoder::Utf8};
    QString jobId;
    QString operation;
    QString status = "idle";
    QString error;
    QString marker;
    QString markerPending;
    bool completedMarker = false;
    bool running = false;
    int exitCode = -1;
    qint64 durationMs = 0;
    qint64 sequence = 0;
    qsizetype logBytes = 0;
    QList<QJsonObject> records;

    QTcpSocket rttSocket;
    QTimer rttReadTimer;
    QTimer rttConnectRetry;
    QElapsedTimer rttConnectedSince;
    QStringDecoder rttDecoder {QStringDecoder::Utf8};
    QString rttJob;
    QString rttStatus = "idle";
    QString rttHost;
    int rttPort = 0;
    int rttConnectAttempts = 0;
    int rttConnection = 0;
    int rttUnstableDrops = 0;
    bool rttClosing = false;
    qint64 rttSequence = 0;
    qint64 rttStartSequence = 0;
    qint64 rttReceived = 0;
    qint64 rttBytesAtConnect = 0;
    qsizetype rttBytes = 0;
    QList<QJsonObject> rttRecords;
    SearchFilter rttSearchFilter;
    qint64 rttFilterRevision = 0;

    void initializeRtt();
    void resetRtt(int port);
    void connectRtt();
    bool reconnectRtt(const QString &reason);
    QString rttEndpoint() const;
    void receiveRtt();
    void closeRtt();
    void appendLog(const QString &text);
    void readOutput();
    void finish(int code, QProcess::ExitStatus exitStatus);
    void stop(const QString &reason);
};
}
