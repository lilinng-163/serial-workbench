// SPDX-License-Identifier: Apache-2.0
// Test-only RTT server. Never links a debug probe or accesses hardware.
#include <QtCore>
#include <QtNetwork>

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const auto arguments = application.arguments();
    const int fileIndex = arguments.indexOf("-f");
    if (fileIndex < 0 || fileIndex + 1 >= arguments.size()) { return 2; }
    QFile file(arguments[fileIndex + 1]);
    if (!file.open(QIODevice::ReadOnly)) { return 3; }
    const auto script = QString::fromUtf8(file.readAll());
    const auto port = QRegularExpression("rtt server start (\\d+)").match(script).captured(1).toUShort();
    const auto marker = QRegularExpression("echo (SW_OPENOCD_COMPLETE_[a-z0-9]+)").match(script).captured(1);
    if (!port || marker.isEmpty()) { return 4; }
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, port)) { return 5; }
    QTimer samples;
    samples.setInterval(30);
    QObject::connect(&server, &QTcpServer::newConnection, &application, [&]
    {
        while (server.hasPendingConnections())
        {
            auto socket = server.nextPendingConnection();
            QObject::connect(&samples, &QTimer::timeout, socket, [socket]
            {
                if (socket->state() == QAbstractSocket::ConnectedState)
                {
                    socket->write("\x1b[32m[INFO] RTT heartbeat\x1b[0m\r\n");
                }
            });
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
    QTextStream(stdout) << marker << Qt::endl;
    samples.start();
    return application.exec();
}
