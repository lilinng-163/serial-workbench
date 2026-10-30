// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QtCore>
#include <QtNetwork>

namespace sw
{
QString defaultSettingsPath();
QString instanceName(const QString &settings);
QString siblingExecutable(const QString &name);
bool maintenanceInProgress();
QJsonObject instanceRequest(const QString &settings, const QString &command);
QJsonObject ensureInstance(const QString &settings, bool allowControl = false);

class McpClient
{
public:
    explicit McpClient(const QString &url);
    QJsonObject request(const QJsonObject &rpc);
    void initialize();
    void close();

private:
    QUrl endpoint;
    QNetworkAccessManager manager;
    QByteArray session;
};

int runStdio(const QString &settings, const QString &url);
}
