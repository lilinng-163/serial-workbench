// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>
#include <optional>
#include <memory>

class QNetworkReply;

namespace sw
{
struct UpdateRelease
{
    QString version;
    QString fileName;
    QUrl url;
    QByteArray sha256;
    qint64 size = 0;
    QString title;
    QString notes;
    QString publishedAt;
};

class UpdateService : public QObject
{
    Q_OBJECT

public:
    explicit UpdateService(QObject *parent = nullptr, QNetworkAccessManager *transport = nullptr);

    static std::optional<UpdateRelease> parseRelease(const QByteArray &json, const QString &currentVersion);
    static std::optional<UpdateRelease> parseMirrorManifest(const QByteArray &json, const QString &currentVersion);

    static QList<UpdateRelease> parseHistory(const QByteArray &json);
    static QList<UpdateRelease> bundledHistory();

    void checkHistory();
    const QList<UpdateRelease> &recentReleases() const { return history; }
    bool isCheckingHistory() const { return !historyReply.isNull(); }
    bool downloadRelease(const UpdateRelease &candidate);
    void check();
    void download();
    void cancelDownload();
    const std::optional<UpdateRelease> &latestRelease() const { return release; }
    bool isDownloading() const { return !installerReply.isNull(); }
    QString downloadedInstaller() const { return downloadedPath; }
    qint64 downloadedBytes() const { return receivedBytes; }

signals:
    void historyChanged();
    void historyFailed(const QString &message);
    void updateAvailable(const QString &version);
    void downloadProgress(qint64 received, qint64 total);
    void downloadReady(const QString &installerPath);
    void downloadFailed(const QString &message);

private:
    QNetworkAccessManager network;
    QNetworkAccessManager *networkClient;
    QPointer<QNetworkReply> checkReply;
    QPointer<QNetworkReply> historyReply;
    QList<UpdateRelease> history;
    QPointer<QNetworkReply> installerReply;
    std::optional<UpdateRelease> release;
    std::unique_ptr<QSaveFile> installerFile;
    QCryptographicHash installerHash {QCryptographicHash::Sha256};
    qint64 receivedBytes = 0;
    QString downloadedPath;

    void startCheck(bool mirror);
    void receiveChunk(QNetworkReply *reply);
    void finishDownload(QNetworkReply *reply);
};
}
