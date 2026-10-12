#pragma once
#include "core.h"

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
    QString url() const;
    QJsonValue call(const QString &name, const QJsonObject &arguments);
    QJsonArray tools() const;

private:
    void acceptRequest(QTcpSocket *socket, const QByteArray &request);
    void respond(QTcpSocket *socket, int code, const QJsonObject &body = {}, const QString &session = {});
    void finishCall(QTcpSocket *socket, const QJsonObject &rpc, const QString &session);
};
}
