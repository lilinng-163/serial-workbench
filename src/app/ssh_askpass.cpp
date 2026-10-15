// SPDX-License-Identifier: Apache-2.0
#include <QtCore>
#include <QtNetwork>
#include <cstdio>
#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto endpoint = qEnvironmentVariable("SW_SSH_AUTH_SERVER");
    const auto token = qEnvironmentVariable("SW_SSH_AUTH_TOKEN");
    if (endpoint.isEmpty() || token.isEmpty() || app.arguments().size() != 2) { return 1; }
    QLocalSocket socket;
    socket.connectToServer(endpoint);
    if (!socket.waitForConnected(3000)) { return 1; }
    const QJsonObject request {{"token", token}, {"prompt", app.arguments()[1]},
        {"hint", qEnvironmentVariable("SSH_ASKPASS_PROMPT")}};
    socket.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
    if (!socket.waitForBytesWritten(3000)) { return 1; }
    QByteArray response;
    QElapsedTimer deadline;
    deadline.start();
    while (!response.contains('\n') && deadline.elapsed() < 300000)
    {
        if (!socket.waitForReadyRead(int(300000 - deadline.elapsed()))) { return 1; }
        response += socket.readAll();
        if (response.size() > 16384) { return 1; }
    }
    const auto answer = QJsonDocument::fromJson(response.trimmed()).object();
    if (!answer["accepted"].toBool()) { return 1; }
    auto secret = answer["answer"].toString().toUtf8();
    // Only OpenSSH's private askpass pipe receives this value.
#ifdef Q_OS_WIN
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    const bool written = std::fwrite(secret.constData(), 1, size_t(secret.size()), stdout) == size_t(secret.size());
    std::fputc('\n', stdout);
    const bool flushed = std::fflush(stdout) == 0;
    secret.fill('\0');
    return written && flushed ? 0 : 1;
}
