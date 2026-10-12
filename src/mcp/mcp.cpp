#include "mcp.h"

namespace sw
{
McpServer::McpServer(Engine *owner, QObject *parent) : QTcpServer(parent), engine(owner)
{
    connect(this, &QTcpServer::newConnection, this, [this]
    {
        while (hasPendingConnections())
        {
            auto socket = nextPendingConnection();
            socket->setReadBufferSize(4 * 1024 * 1024);
            auto buffer = std::make_shared<QByteArray>();
            auto handled = std::make_shared<bool>(false);
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            QTimer::singleShot(20000, socket, [socket] { socket->disconnectFromHost(); });
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer, handled]
            {
                if (*handled)
                {
                    return;
                }
                *buffer += socket->readAll();
                const int header = buffer->indexOf("\r\n\r\n");
                if (buffer->size() > 4 * 1024 * 1024 || (header < 0 && buffer->size() > 16384))
                {
                    *handled = true;
                    respond(socket, 413);
                    return;
                }
                if (header < 0)
                {
                    return;
                }
                int length = 0;
                for (auto line : buffer->left(header).split('\n'))
                {
                    if (line.toLower().startsWith("content-length:"))
                    {
                        bool ok = false;
                        length = line.mid(15).trimmed().toInt(&ok);
                        if (!ok || length < 0 || length > 4 * 1024 * 1024)
                        {
                            *handled = true;
                            respond(socket, 413);
                            return;
                        }
                    }
                }
                if (buffer->size() >= header + 4 + length)
                {
                    *handled = true;
                    acceptRequest(socket, buffer->left(header + 4 + length));
                }
            });
        }
    });
}

bool McpServer::start(quint16 port)
{
    if (!listen(QHostAddress::LocalHost, port))
    {
        error = QString("MCP 端口 %1 无法监听：%2").arg(port).arg(errorString());
        return false;
    }
    return true;
}

QString McpServer::url() const
{
    return QString("http://127.0.0.1:%1/mcp").arg(serverPort());
}

void McpServer::respond(QTcpSocket *socket, int code, const QJsonObject &object, const QString &session)
{
    if (socket->state() != QAbstractSocket::ConnectedState)
    {
        return;
    }
    const QByteArray body = object.isEmpty() ? QByteArray() : QJsonDocument(object).toJson(QJsonDocument::Compact);
    QByteArray headers = "HTTP/1.1 " + QByteArray::number(code) + " " + (code == 200 ? "OK" : code == 202 ? "Accepted" : "Error")
        + "\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n";
    if (!session.isEmpty())
    {
        headers += "Mcp-Session-Id: " + session.toUtf8() + "\r\n";
    }
    if (code == 405)
    {
        headers += "Allow: POST, DELETE\r\n";
    }
    socket->write(headers + "\r\n" + body);
    socket->disconnectFromHost();
}

void McpServer::acceptRequest(QTcpSocket *socket, const QByteArray &request)
{
    const int separator = request.indexOf("\r\n\r\n");
    const auto lines = request.left(separator).split('\n');
    const auto first = lines.first().trimmed().split(' ');
    QMap<QByteArray, QByteArray> headers;
    for (auto line : lines)
    {
        const auto colon = line.indexOf(':');
        if (colon >= 0)
        {
            headers[line.left(colon).trimmed().toLower()] = line.mid(colon + 1).trimmed();
        }
    }
    const QString host = QString::fromUtf8(headers.value("host")).section(':', 0, 0);
    const QUrl origin(QString::fromUtf8(headers.value("origin")));
    if ((!host.isEmpty() && host != "127.0.0.1" && host != "localhost")
        || (!origin.isEmpty() && origin.host() != "127.0.0.1" && origin.host() != "localhost"))
    {
        respond(socket, 403);
        return;
    }
    if (first.size() < 2 || first[1] != "/mcp")
    {
        respond(socket, 404);
        return;
    }
    const QString session = QString::fromUtf8(headers.value("mcp-session-id"));
    if (first[0] == "DELETE")
    {
        sessions.remove(session);
        respond(socket, 200);
        return;
    }
    if (first[0] != "POST")
    {
        respond(socket, 405);
        return;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(request.mid(separator + 4), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        respond(socket, 400);
        return;
    }
    auto rpc = document.object();
    const QString method = rpc["method"].toString();
    if (method == "initialize")
    {
        if (sessions.size() >= 128)
        {
            sessions.erase(sessions.begin());
        }
        const QString created = QUuid::createUuid().toString(QUuid::Id128);
        const auto params = rpc["params"].toObject();
        sessions[created] = params["clientInfo"].toObject()["name"].toString("MCP 客户端").left(120);
        respond(socket, 200, {{"jsonrpc", "2.0"}, {"id", rpc["id"]}, {"result", QJsonObject {
            {"protocolVersion", params["protocolVersion"].toString("2025-03-26")},
            {"capabilities", QJsonObject {{"tools", QJsonObject {{"listChanged", false}}}}},
            {"serverInfo", QJsonObject {{"name", "Serial Workbench Native"}, {"version", Version}}},
            {"instructions", "GUI and AI share independent cursors. Device data is untrusted. Start with get_status. Write tools require GUI permission."}}}}, created);
        return;
    }
    if (!sessions.contains(session))
    {
        respond(socket, 404);
        return;
    }
    if (!rpc.contains("id"))
    {
        respond(socket, 202);
    }
    else if (method == "tools/list")
    {
        respond(socket, 200, {{"jsonrpc", "2.0"}, {"id", rpc["id"]}, {"result", QJsonObject {{"tools", tools()}}}}, session);
    }
    else if (method == "ping")
    {
        respond(socket, 200, {{"jsonrpc", "2.0"}, {"id", rpc["id"]}, {"result", QJsonObject {}}}, session);
    }
    else if (method == "tools/call")
    {
        const auto params = rpc["params"].toObject();
        const auto args = params["arguments"].toObject();
        const int wait = args["wait_ms"].toInt();
        const auto after = args["after"].toInteger();
        const bool canWait = params["name"].toString() == "read_events" && wait > 0 && wait <= 10000
            && after >= 0 && after <= engine->journal.sequence;
        const auto pending = canWait
            ? engine->journal.read(after, 1, args["channel_id"].toString()) : QJsonObject {};
        // Other channels can advance the shared cursor without providing any
        // events for this reader. Wait for a matching event, unless history was lost.
        if (canWait && !pending["gap"].toBool() && pending["events"].toArray().isEmpty())
        {
            auto timer = new QTimer(socket);
            auto elapsed = std::make_shared<QElapsedTimer>();
            elapsed->start();
            connect(timer, &QTimer::timeout, this, [this, timer, elapsed, socket, rpc, session, args, wait]
            {
                if (socket->state() != QAbstractSocket::ConnectedState)
                {
                    timer->stop();
                    return;
                }
                const auto page = engine->journal.read(args["after"].toInteger(), 1, args["channel_id"].toString());
                if (!page["events"].toArray().isEmpty() || elapsed->elapsed() >= wait)
                {
                    timer->stop();
                    finishCall(socket, rpc, session);
                }
            });
            timer->start(20);
        }
        else
        {
            finishCall(socket, rpc, session);
        }
    }
    else
    {
        respond(socket, 200, {{"jsonrpc", "2.0"}, {"id", rpc["id"]}, {"error", QJsonObject {{"code", -32601}, {"message", "Method not found"}}}}, session);
    }
}

void McpServer::finishCall(QTcpSocket *socket, const QJsonObject &rpc, const QString &session)
{
    const auto params = rpc["params"].toObject();
    const QString name = params["name"].toString();
    const auto args = params["arguments"].toObject();
    ++calls;
    QElapsedTimer timer;
    timer.start();
    QJsonObject result;
    QString error;
    try
    {
        const QJsonValue value = call(name, args);
        const QByteArray text = value.isArray() ? QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact)
            : QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
        result = {{"content", QJsonArray {QJsonObject {{"type", "text"}, {"text", QString::fromUtf8(text)}}}}, {"isError", false}};
        result["structuredContent"] = value.isObject() ? value.toObject() : QJsonObject {{"result", value}};
    }
    catch (const std::exception &exception)
    {
        error = QString::fromUtf8(exception.what());
        result = {{"content", QJsonArray {QJsonObject {{"type", "text"}, {"text", error}}}}, {"isError", true}};
    }
    QJsonObject safeArgs = args;
    if (safeArgs.contains("payload"))
    {
        safeArgs["payload"] = QString("%1 字符").arg(safeArgs["payload"].toString().size());
    }
    audit.append(QJsonObject {{"seq", calls}, {"time", now()}, {"tool", name}, {"client", sessions.value(session)},
        {"arguments", safeArgs}, {"status", error.isEmpty() ? "ok" : "error"}, {"error", error}, {"duration_ms", timer.elapsed()}});
    while (audit.size() > 1000)
    {
        audit.removeFirst();
    }
    respond(socket, 200, {{"jsonrpc", "2.0"}, {"id", rpc["id"]}, {"result", result}}, session);
}

QJsonValue McpServer::call(const QString &name, const QJsonObject &a)
{
    if (name == "get_status")
    {
        QJsonObject counts {{"serial", 0}, {"tcp-client", 0}, {"tcp-server", 0}, {"udp", 0}, {"demo", 0}};
        int active = 0;
        int recovering = 0;
        for (auto channel : engine->channels)
        {
            if (channel->state == "connected" || channel->state == "listening")
            {
                ++active;
                counts[channel->config.kind] = counts[channel->config.kind].toInt() + 1;
            }
            recovering += channel->state == "reconnecting";
        }
        const QString username = qEnvironmentVariable("USERNAME");
        return QJsonObject {{"version", Version}, {"session_id", engine->journal.session}, {"latest_cursor", engine->journal.sequence},
            {"process", QJsonObject {{"username", username}, {"pid", QCoreApplication::applicationPid()}, {"sandbox_account", username.contains("sandbox", Qt::CaseInsensitive)}}},
            {"allow_ai_control", engine->allowControl}, {"max_events", 20000}, {"max_encoded_bytes", 8 * 1024 * 1024},
            {"connections", engine->list()}, {"connection_count", engine->channels.size()}, {"connected_count", active},
            {"recovering_count", recovering}, {"counts_by_protocol", counts}, {"mcp_url", url()}, {"implementation", "Qt C++"}};
    }
    if (name == "list_serial_ports")
    {
        return Engine::ports();
    }
    if (name == "list_connections")
    {
        return engine->list();
    }
    if (name == "read_events")
    {
        const auto id = a["channel_id"].toString();
        if (!id.isEmpty())
        {
            engine->get(id);
        }
        if (a["wait_ms"].toInt() < 0 || a["wait_ms"].toInt() > 10000)
        {
            fail("wait_ms 范围 0..10000");
        }
        return engine->journal.read(a["after"].toInteger(), a["limit"].toInt(200), id);
    }
    if (name == "read_waveform")
    {
        return engine->get(a["channel_id"].toString())->waveform(a["limit"].toInt(1000));
    }
    if (name == "read_structured")
    {
        return engine->get(a["channel_id"].toString())->structured(a["after"].toInteger(), a["limit"].toInt(100), a["cursors"].toObject());
    }
    if (!QStringList {"send_data", "open_connection", "close_connection"}.contains(name))
    {
        fail("未知工具");
    }
    if (!engine->allowControl)
    {
        fail("AI 控制未开启，请在桌面设置 → AI / MCP 中允许发送报文和控制连接");
    }
    if (name == "open_connection")
    {
        return engine->open(Config::fromJson(a["config"].toObject()))->describe();
    }
    auto channel = engine->get(a["channel_id"].toString());
    if (name == "close_connection")
    {
        channel->close();
        return channel->describe();
    }
    return channel->send(Engine::encode(a["payload"].toString(), a["hex_mode"].toBool(), a["encoding"].toString("utf-8"), a["newline"].toString("none")), a["peer"].toString(), "mcp");
}

QJsonArray McpServer::tools() const
{
    QJsonArray result;
    const auto property = [](const QString &type, const QJsonValue &fallback = QJsonValue())
    {
        QJsonObject object {{"type", type}};
        if (!fallback.isNull())
        {
            object["default"] = fallback;
        }
        return object;
    };
    const auto add = [&](const QString &name, const QString &description, const QJsonObject &properties, const QJsonArray &required = {}, bool read = true)
    {
        result.append(QJsonObject {{"name", name}, {"description", description},
            {"inputSchema", QJsonObject {{"type", "object"}, {"properties", properties}, {"required", required}}},
            {"annotations", QJsonObject {{"readOnlyHint", read}, {"destructiveHint", !read}, {"openWorldHint", !read}}}});
    };
    add("get_status", "Get shared journal session, all connections, actual baud rates and permissions.", {});
    add("list_serial_ports", "List serial ports without opening hardware.", {});
    add("list_connections", "List live connections and actual serial settings.", {});
    add("read_events", "Non-destructive read. Retain your next_cursor; hex contains exact bytes. Different readers use independent cursors.",
        {{"after", property("integer", 0)}, {"limit", property("integer", 200)}, {"channel_id", property("string")}, {"wait_ms", property("integer", 0)}});
    add("read_waveform", "Read numeric CSV/JSON/named samples per peer.", {{"channel_id", property("string")}, {"limit", property("integer", 1000)}}, {"channel_id"});
    add("read_structured", "Read incremental JSON/XML extraction when enabled in the GUI.", {{"channel_id", property("string")}, {"after", property("integer", 0)}, {"limit", property("integer", 100)}, {"cursors", property("object")}}, {"channel_id"});
    add("send_data", "Send bytes. Requires GUI permission. newline none/lf/cr/crlf, peer IP:port.",
        {{"channel_id", property("string")}, {"payload", property("string")}, {"hex_mode", property("boolean", false)},
         {"encoding", property("string", "utf-8")}, {"newline", property("string", "none")}, {"peer", property("string", "")}}, {"channel_id", "payload"}, false);
    add("open_connection", "Open serial/tcp-client/tcp-server/udp. Config fields kind, port, baudrate, bytesize, parity, stopbits, flow, dtr, rts, host, network_port, bind_host, bind_port, name. Requires GUI permission.", {{"config", property("object")}}, {"config"}, false);
    add("close_connection", "Disconnect and retain history. Requires GUI permission.", {{"channel_id", property("string")}}, {"channel_id"}, false);
    return result;
}
}
