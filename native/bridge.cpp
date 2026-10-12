#include <QtCore>
#include <QtNetwork>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QString url = "http://127.0.0.1:8765/mcp";
    const auto arguments = application.arguments();
    const auto index = arguments.indexOf("--url");
    if (index >= 0 && index + 1 < arguments.size())
    {
        url = arguments[index + 1];
    }
    QNetworkAccessManager manager;
    QByteArray session;
    std::string line;
    while (std::getline(std::cin, line))
    {
        const auto rpc = QJsonDocument::fromJson(QByteArray::fromStdString(line)).object();
        if (rpc.isEmpty())
        {
            continue;
        }
        QNetworkRequest request {QUrl(url)};
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Accept", "application/json, text/event-stream");
        if (!session.isEmpty())
        {
            request.setRawHeader("Mcp-Session-Id", session);
        }
        auto reply = manager.post(request, QByteArray::fromStdString(line));
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timeout.start(15000);
        loop.exec();
        if (reply->hasRawHeader("Mcp-Session-Id"))
        {
            session = reply->rawHeader("Mcp-Session-Id");
        }
        if (rpc.contains("id"))
        {
            QByteArray result = reply->readAll();
            if (reply->error() != QNetworkReply::NoError || result.isEmpty())
            {
                result = QJsonDocument(QJsonObject {{"jsonrpc", "2.0"}, {"id", rpc["id"]},
                    {"error", QJsonObject {{"code", -32000}, {"message", "桌面 MCP 未就绪：" + reply->errorString()}}}}).toJson(QJsonDocument::Compact);
            }
            std::cout << result.toStdString() << std::endl;
        }
        reply->deleteLater();
    }
    return 0;
}
