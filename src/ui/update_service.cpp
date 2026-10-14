// SPDX-License-Identifier: Apache-2.0

#include "update_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVersionNumber>

namespace sw
{
namespace
{
constexpr qint64 MaximumInstallerBytes = 300LL * 1024 * 1024;
const QUrl ReleasesApi("https://api.github.com/repos/lilinng-163/serial-workbench/releases/latest");
const QUrl MirrorManifest("https://162.14.72.194/updates/latest.json");
}

UpdateService::UpdateService(QObject *parent) : QObject(parent)
{
}

std::optional<UpdateRelease> UpdateService::parseRelease(const QByteArray &json, const QString &currentVersion)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        return std::nullopt;
    }

    const auto object = document.object();
    if (object.value("draft").toBool() || object.value("prerelease").toBool())
    {
        return std::nullopt;
    }

    const auto tag = object.value("tag_name").toString();
    const auto version = tag.startsWith('v') ? tag.mid(1) : tag;
    static const QRegularExpression versionPattern(R"(^\d+\.\d+\.\d+$)");
    if (!versionPattern.match(version).hasMatch()
        || QVersionNumber::compare(QVersionNumber::fromString(version), QVersionNumber::fromString(currentVersion)) <= 0)
    {
        return std::nullopt;
    }

    const auto fileName = QString("SerialWorkbench-%1-windows-x64-setup.exe").arg(version);
    for (const auto &value : object.value("assets").toArray())
    {
        const auto asset = value.toObject();
        if (asset.value("name").toString() != fileName || asset.value("state").toString() != "uploaded")
        {
            continue;
        }

        const auto url = QUrl(asset.value("browser_download_url").toString());
        const auto expectedPath = QString("/lilinng-163/serial-workbench/releases/download/%1/%2").arg(tag, fileName);
        const auto digest = asset.value("digest").toString();
        static const QRegularExpression digestPattern(R"(^sha256:([0-9a-fA-F]{64})$)");
        const auto match = digestPattern.match(digest);
        const auto size = asset.value("size").toInteger();
        if (!url.isValid() || url.scheme() != "https" || url.host() != "github.com"
            || url.path() != expectedPath || !url.query().isEmpty() || !url.fragment().isEmpty()
            || !match.hasMatch() || size <= 0 || size > MaximumInstallerBytes)
        {
            return std::nullopt;
        }

        return UpdateRelease {version, fileName, url, match.captured(1).toLatin1().toLower(), size};
    }
    return std::nullopt;
}

std::optional<UpdateRelease> UpdateService::parseMirrorManifest(const QByteArray &json, const QString &currentVersion)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        return std::nullopt;
    }

    const auto object = document.object();
    const auto version = object.value("version").toString();
    static const QRegularExpression versionPattern(R"(^\d+\.\d+\.\d+$)");
    if (!versionPattern.match(version).hasMatch()
        || QVersionNumber::compare(QVersionNumber::fromString(version), QVersionNumber::fromString(currentVersion)) <= 0)
    {
        return std::nullopt;
    }

    const auto fileName = QString("SerialWorkbench-%1-windows-x64-setup.exe").arg(version);
    const auto url = QUrl(object.value("url").toString());
    const auto digest = object.value("sha256").toString();
    static const QRegularExpression digestPattern(R"(^[0-9a-fA-F]{64}$)");
    const auto size = object.value("size").toInteger();
    if (object.value("filename").toString() != fileName
        || !url.isValid() || url.scheme() != "https" || url.host() != MirrorManifest.host()
        || url.port(-1) != -1 || !url.userInfo().isEmpty()
        || url.path() != "/updates/" + fileName || !url.query().isEmpty() || !url.fragment().isEmpty()
        || !digestPattern.match(digest).hasMatch() || size <= 0 || size > MaximumInstallerBytes)
    {
        return std::nullopt;
    }

    return UpdateRelease {version, fileName, url, digest.toLatin1().toLower(), size};
}

void UpdateService::check()
{
    if (checkReply || installerReply)
    {
        return;
    }

    startCheck(true);
}

void UpdateService::startCheck(bool mirror)
{
    QNetworkRequest request(mirror ? MirrorManifest : ReleasesApi);
    request.setHeader(QNetworkRequest::UserAgentHeader, "SerialWorkbench-Updater");
    request.setRawHeader("Accept", mirror ? "application/json" : "application/vnd.github+json");
    request.setTransferTimeout(mirror ? 8000 : 15000);
    auto reply = network.get(request);
    checkReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, mirror]
    {
        checkReply = nullptr;
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && status == 200
            && reply->bytesAvailable() <= 1024 * 1024)
        {
            const auto data = reply->readAll();
            release = mirror
                ? parseMirrorManifest(data, QCoreApplication::applicationVersion())
                : parseRelease(data, QCoreApplication::applicationVersion());
            if (release)
            {
                emit updateAvailable(release->version);
            }
            else if (mirror && !parseMirrorManifest(data, "0.0.0"))
            {
                reply->deleteLater();
                startCheck(false);
                return;
            }
        }
        else if (mirror)
        {
            reply->deleteLater();
            startCheck(false);
            return;
        }
        reply->deleteLater();
    });
}

void UpdateService::download()
{
    if (!release || installerReply)
    {
        return;
    }
    if (checkReply)
    {
        checkReply->abort();
    }

    const auto directory = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + "/SerialWorkbenchUpdates";
    if (!QDir().mkpath(directory))
    {
        emit downloadFailed("无法创建更新下载目录");
        return;
    }

    installerFile = std::make_unique<QSaveFile>(directory + "/" + release->fileName);
    if (!installerFile->open(QIODevice::WriteOnly))
    {
        installerFile.reset();
        emit downloadFailed("无法写入更新安装包");
        return;
    }

    receivedBytes = 0;
    installerHash.reset();
    QNetworkRequest request(release->url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "SerialWorkbench-Updater");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(60000);
    auto reply = network.get(request);
    installerReply = reply;

    connect(reply, &QNetworkReply::readyRead, this, [this, reply] { receiveChunk(reply); });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64, qint64)
    {
        if (release)
        {
            emit downloadProgress(receivedBytes, release->size);
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] { finishDownload(reply); });
}

void UpdateService::receiveChunk(QNetworkReply *reply)
{
    if (!installerFile || !release)
    {
        return;
    }

    const auto chunk = reply->readAll();
    if (chunk.size() > release->size - receivedBytes
        || installerFile->write(chunk) != chunk.size())
    {
        reply->abort();
        return;
    }

    installerHash.addData(chunk);
    receivedBytes += chunk.size();
    emit downloadProgress(receivedBytes, release->size);
}

void UpdateService::finishDownload(QNetworkReply *reply)
{
    installerReply = nullptr;
    receiveChunk(reply);
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool valid = release && installerFile
        && reply->error() == QNetworkReply::NoError && status == 200
        && receivedBytes == release->size && installerHash.result().toHex() == release->sha256;
    reply->deleteLater();

    if (!valid)
    {
        installerFile.reset();
        emit downloadFailed("下载失败或安装包校验未通过，请重试");
        return;
    }

    const auto path = installerFile->fileName();
    if (!installerFile->commit())
    {
        installerFile.reset();
        emit downloadFailed("无法保存更新安装包");
        return;
    }
    installerFile.reset();
    emit downloadReady(path);
}
}
