// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "core.h"
#include <optional>

namespace sw
{
class McpServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit McpServer(Engine *engine, QObject *parent = nullptr);
    Engine *engine;
    QString error;
    qint64 calls = 0;
    QJsonArray audit;
    QMap<QString, QString> sessions;
    bool start(quint16 port);
    bool changePort(quint16 port);
    bool startAvailable(quint16 preferredPort);
    QString url() const;
    QJsonValue call(const QString &name, const QJsonObject &arguments);
    QJsonArray tools() const;
    QJsonObject protocolAccess() const;
    bool protocolEnabled(const QString &kind) const;
    void setProtocolEnabled(const QString &kind, bool enabled);
    std::function<QJsonObject(Channel *, const QJsonObject &)> searchHandler;
    std::function<void(Channel *, const QString &, bool, const std::optional<QString> &)> scriptHandler;
    std::function<QString(Channel *)> scriptSampleHandler;
    std::function<QJsonObject(const QString &, const QJsonObject &)> modelHandler;
    QJsonArray agentChecks;

signals:
    void portChanged(quint16 port);
    void protocolAccessChanged();
    void toolCalled(const QJsonObject &entry);
    void guiConnectionOpened(Channel *channel);

private:
    QJsonObject access {{"serial", true}, {"tcp", true}, {"udp", true}, {"ssh", true}};
    void requireProtocol(const QString &kind) const;
    Channel *accessibleChannel(const QString &id) const;
    QJsonArray visibleConnections() const;
    QJsonObject readEvents(const QJsonObject &arguments, int limitOverride = 0, bool allChannels = false) const;
    QJsonObject scriptTool(const QString &name, const QJsonObject &arguments);
    void acceptRequest(QTcpSocket *socket, const QByteArray &request);
    void respond(QTcpSocket *socket, int code, const QJsonObject &body = {}, const QString &session = {});
    void finishCall(QTcpSocket *socket, const QJsonObject &rpc, const QString &session);
};
}
