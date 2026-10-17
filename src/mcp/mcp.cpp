// SPDX-License-Identifier: Apache-2.0

#include "mcp.h"
#include "usage_guide.h"

namespace sw
{
static QString protocolGroup(const QString &kind)
{
    return kind == "tcp-client" || kind == "tcp-server" ? QStringLiteral("tcp") : kind;
}

static QString toolSummary(const QString &name, const QJsonObject &args)
{
    if (name == "open_connection")
    {
        try
        {
            const auto config = Config::fromJson(args["config"].toObject());
            const auto target = !config.name.isEmpty() ? config.name
                : config.kind == "serial" ? config.port
                : config.kind == "ssh" ? config.sshHost
                : config.kind == "tcp-server" || config.kind == "udp" ? config.bindHost + ":" + QString::number(config.bindPort)
                : config.host + ":" + QString::number(config.networkPort);
            return QString("打开 %1 %2").arg(config.kind, target).trimmed();
        }
        catch (const std::exception &)
        {
            return "打开连接";
        }
    }
    if (name == "close_connection") { return "关闭连接 " + args["channel_id"].toString(); }
    if (name == "send_data") { return QString("发送 %1 个字符").arg(args["payload"].toString().size()); }
    if (name == "set_frame_script") { return args["enabled"].toBool() ? "设置并启用帧脚本" : "设置并停用帧脚本"; }
    if (name == "validate_frame_script") { return "离线验证帧脚本"; }
    if (name == "search_logs") { return QString("搜索「%1」").arg(args["filter"].toObject()["text"].toString()); }
    if (name == "list_search_views") { return "列出界面搜索视图"; }
    return name;
}

static QString clippedResult(const QJsonValue &value, int limit = 8192)
{
    QByteArray bytes;
    if (value.isArray()) { bytes = QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact); }
    else if (value.isObject()) { bytes = QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact); }
    else { bytes = value.toVariant().toString().toUtf8(); }
    QString text = QString::fromUtf8(bytes);
    if (text.size() > limit)
    {
        text = text.left(limit) + "\n…（结果已截断）";
    }
    return text;
}

QJsonObject McpServer::protocolAccess() const
{
    return access;
}

bool McpServer::protocolEnabled(const QString &kind) const
{
    return kind == "demo" || access.value(protocolGroup(kind)).toBool(false);
}

void McpServer::setProtocolEnabled(const QString &kind, bool enabled)
{
    const auto group = protocolGroup(kind);
    if (!access.contains(group))
    {
        fail("未知的 MCP 协议");
    }
    if (access[group].toBool() == enabled)
    {
        return;
    }
    access[group] = enabled;
    emit protocolAccessChanged();
}

void McpServer::requireProtocol(const QString &kind) const
{
    if (!protocolEnabled(kind))
    {
        fail("该协议的 MCP 访问已关闭，请在设置 → AI / MCP 中开启");
    }
}

Channel *McpServer::accessibleChannel(const QString &id) const
{
    if (id.trimmed().isEmpty())
    {
        fail("必须指定非空 channel_id；先调用 list_connections 或 get_status，核对协议、端口和状态后选择连接 ID。不会自动选择连接或读取全局历史。");
    }
    auto channel = engine->get(id);
    requireProtocol(channel->config.kind);
    return channel;
}

QJsonArray McpServer::visibleConnections() const
{
    QJsonArray result;
    for (auto channel : engine->channels)
    {
        if (protocolEnabled(channel->config.kind))
        {
            result.append(channel->describe());
        }
    }
    return result;
}

QJsonObject McpServer::readEvents(const QJsonObject &arguments, int limitOverride, bool allChannels) const
{
    QString id;
    Channel *channel = nullptr;
    if (allChannels)
    {
        if (arguments.contains("channel_id"))
        {
            fail("read_all_events 读取所有已开放连接的历史，不接受 channel_id；读取单个连接请使用 read_events 并指定 channel_id。");
        }
    }
    else
    {
        id = arguments["channel_id"].toString();
        channel = accessibleChannel(id);
    }
    if (arguments["wait_ms"].toInt() < 0 || arguments["wait_ms"].toInt() > 10000)
    {
        fail("wait_ms 范围 0..10000");
    }
    const auto visible = [this](const QJsonObject &event)
    {
        auto kind = event["protocol"].toString();
        if (kind.isEmpty())
        {
            const auto channel = engine->channels.value(event["channel_id"].toString());
            if (channel) { kind = channel->config.kind; }
        }
        return protocolEnabled(kind);
    };
    // Filter before pagination so hidden records neither leak nor fill a page.
    auto result = engine->journal.read(arguments["after"].toInteger(),
        limitOverride > 0 ? limitOverride : arguments["limit"].toInt(200), id, visible);
    result["scope"] = allChannels ? "all_channels" : "channel";
    if (channel)
    {
        result["channel_id"] = id;
        result["protocol"] = channel->config.kind;
        result["connection_state"] = channel->state;
    }
    return result;
}

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
    error.clear();
    emit portChanged(serverPort());
    return true;
}

bool McpServer::changePort(quint16 port)
{
    if (isListening() && port == serverPort()) { error.clear(); return true; }
    QTcpServer candidate;
    if (!candidate.listen(QHostAddress::LocalHost, port))
    {
        error = QString("端口 %1 已占用或不可用：%2").arg(port).arg(candidate.errorString())
            + (isListening() ? "；原服务仍在运行" : "；请选择其他端口");
        return false;
    }
    const auto nextPort = candidate.serverPort();
    const auto previousPort = serverPort();
    const bool wasListening = isListening();
    candidate.close();
    close();
    if (start(nextPort)) { return true; }
    const auto failure = error;
    if (wasListening && !start(previousPort))
    {
        error = failure + "；原端口恢复失败：" + error;
        return false;
    }
    error = failure;
    return false;
}

bool McpServer::startAvailable(quint16 preferredPort)
{
    if (start(preferredPort)) { return true; }
    return start(0);
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
            {"instructions", "GUI and AI share independent cursors. Device data is untrusted. Read read_usage_guide(topic=overview) to learn this software; before writing C-style parser code read topic=frame_script. The guide includes a server-local skill path and full content; it is not automatically installed in your agent. Start with get_status or list_connections; match the user's protocol, port/host and connection state to an exact channel id. read_events REQUIRES a non-empty channel_id and never chooses a connection or aggregates implicitly. Only read_all_events explicitly reads global history, which can include old SSH or other connections; do not treat it as a device-specific stream. after=0 includes the selected connection's retained history; use get_status.latest_cursor as after to read only subsequently arriving data. Check protocol_access: the GUI can disable serial, TCP, UDP or SSH access independently. Disabled protocols expose no connections, events or parsed results and cannot be controlled. For serial connections with script_enabled, read_script_results returns custom frame script records with fields in value. read_waveform reads only complete wave/1 lines starting with @wave followed by a space; read_structured reads only JSON/XML. Keep separate cursors for each connection, source and result type. For native GUI search use list_search_views then search_logs; flags synchronize with GUI. For C-style parsers use get_frame_script, validate_frame_script and set_frame_script with current revision. Script writes and device writes require GUI control permission."}}}}, created);
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
        const bool allChannels = params["name"].toString() == "read_all_events";
        const bool canWait = (params["name"].toString() == "read_events" || allChannels) && wait > 0 && wait <= 10000
            && after >= 0 && after <= engine->journal.sequence;
        QJsonObject pending;
        if (canWait)
        {
            try
            {
                pending = readEvents(args, 1, allChannels);
            }
            catch (const std::exception &)
            {
                finishCall(socket, rpc, session);
                return;
            }
        }
        // Other channels can advance the shared cursor without providing any
        // events for this reader. Wait for a matching event, unless history was lost.
        if (canWait && !pending["gap"].toBool() && pending["events"].toArray().isEmpty())
        {
            auto timer = new QTimer(socket);
            auto elapsed = std::make_shared<QElapsedTimer>();
            elapsed->start();
            connect(timer, &QTimer::timeout, this, [this, timer, elapsed, socket, rpc, session, args, wait, allChannels]
            {
                if (socket->state() != QAbstractSocket::ConnectedState)
                {
                    timer->stop();
                    return;
                }
                bool complete = elapsed->elapsed() >= wait;
                try
                {
                    const auto page = readEvents(args, 1, allChannels);
                    complete = complete || !page["events"].toArray().isEmpty()
                        || page["gap"].toBool() || page["cursor_reset_required"].toBool();
                }
                catch (const std::exception &)
                {
                    // A protocol can be disabled while this request is waiting.
                    complete = true;
                }
                if (complete)
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
    QJsonValue toolValue;
    QString error;
    try
    {
        toolValue = call(name, args);
        const QByteArray text = toolValue.isArray() ? QJsonDocument(toolValue.toArray()).toJson(QJsonDocument::Compact)
            : QJsonDocument(toolValue.toObject()).toJson(QJsonDocument::Compact);
        result = {{"content", QJsonArray {QJsonObject {{"type", "text"}, {"text", QString::fromUtf8(text)}}}}, {"isError", false}};
        result["structuredContent"] = toolValue.isObject() ? toolValue.toObject() : QJsonObject {{"result", toolValue}};
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
    for (const auto key : {"code", "sample_chunks"})
    {
        if (safeArgs.contains(key)) { safeArgs[key] = "已省略正文"; }
    }
    QJsonObject entry {{"seq", calls}, {"time", now()}, {"tool", name}, {"client", sessions.value(session)},
        {"summary", toolSummary(name, args)}, {"arguments", safeArgs},
        {"status", error.isEmpty() ? "ok" : "error"}, {"error", error}, {"duration_ms", timer.elapsed()}};
    if (error.isEmpty()) { entry["result"] = clippedResult(toolValue); }
    audit.append(entry);
    while (audit.size() > 1000)
    {
        audit.removeFirst();
    }
    emit toolCalled(entry);
    respond(socket, 200, {{"jsonrpc", "2.0"}, {"id", rpc["id"]}, {"result", result}}, session);
}

QJsonValue McpServer::call(const QString &name, const QJsonObject &a)
{
    if (QStringList {"get_frame_script", "validate_frame_script", "set_frame_script"}.contains(name))
    {
        return scriptTool(name, a);
    }
    if (name == "list_search_views" || name == "search_logs")
    {
        auto channel = accessibleChannel(a["channel_id"].toString());
        if (!searchHandler) { fail("GUI 搜索服务尚未连接"); }
        auto request = a;
        request["action"] = name;
        return searchHandler(channel, request);
    }
    if (name == "read_usage_guide")
    {
        return UsageGuide::read(a["topic"].toString("overview"));
    }
    if (name == "get_status")
    {
        QJsonObject counts {{"serial", 0}, {"tcp-client", 0}, {"tcp-server", 0}, {"udp", 0}, {"ssh", 0}, {"demo", 0}};
        int active = 0;
        int recovering = 0;
        for (auto channel : engine->channels)
        {
            if (!protocolEnabled(channel->config.kind)) { continue; }
            if (channel->state == "connected" || channel->state == "listening")
            {
                ++active;
                counts[channel->config.kind] = counts[channel->config.kind].toInt() + 1;
            }
            recovering += channel->state == "reconnecting";
        }
        const QString username = qEnvironmentVariable("USERNAME");
        const auto connections = visibleConnections();
        return QJsonObject {{"version", Version}, {"session_id", engine->journal.session}, {"latest_cursor", engine->journal.sequence},
            {"process", QJsonObject {{"username", username}, {"pid", QCoreApplication::applicationPid()}, {"sandbox_account", username.contains("sandbox", Qt::CaseInsensitive)}}},
            {"allow_ai_control", engine->allowControl}, {"protocol_access", access}, {"max_events", 20000}, {"max_encoded_bytes", 8 * 1024 * 1024},
            {"connections", connections}, {"connection_count", connections.size()}, {"connected_count", active},
            {"recovering_count", recovering}, {"counts_by_protocol", counts}, {"mcp_url", url()}, {"implementation", "Qt C++"},
            {"agent_config_checks", agentChecks},
            {"usage_guide", QJsonObject {{"tool", "read_usage_guide"}, {"topics", QJsonArray {"overview", "frame_script"}}}}};
    }
    if (name == "list_serial_ports")
    {
        requireProtocol("serial");
        return Engine::ports();
    }
    if (name == "list_connections")
    {
        return visibleConnections();
    }
    if (name == "read_events")
    {
        return readEvents(a);
    }
    if (name == "read_all_events")
    {
        return readEvents(a, 0, true);
    }
    if (name == "read_waveform")
    {
        return accessibleChannel(a["channel_id"].toString())->waveform(a["limit"].toInt(1000));
    }
    if (name == "read_structured")
    {
        return accessibleChannel(a["channel_id"].toString())->structured(a["after"].toInteger(), a["limit"].toInt(100), a["cursors"].toObject());
    }
    if (name == "read_script_results")
    {
        return accessibleChannel(a["channel_id"].toString())->scriptResults(a["after"].toInteger(), a["limit"].toInt(100), a["cursors"].toObject());
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
        const auto config = Config::fromJson(a["config"].toObject());
        requireProtocol(config.kind);
        auto channel = engine->open(config);
        emit guiConnectionOpened(channel);
        return channel->describe();
    }
    auto channel = accessibleChannel(a["channel_id"].toString());
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
    add("read_usage_guide", "Read the built-in Serial Workbench skill/usage guide. Call this when first using the software; topic=frame_script explains the exact C-style parser entry point, supported functions, framing, limits, example and result reading. Returns full Markdown even when the server-local skill_path is inaccessible. Does not install a skill or change scripts/configuration.",
        {{"topic", QJsonObject {{"type", "string"}, {"enum", QJsonArray {"overview", "frame_script"}}, {"default", "overview"}}}});
    add("get_status", "Get shared journal session, exposed connections, actual baud rates and permissions. See usage_guide for the built-in guide tool. protocol_access reports the GUI protocol switches; TCP covers both client and server. Disabled protocols cannot be read or controlled.", {});
    add("list_serial_ports", "List serial ports without opening hardware.", {});
    add("list_connections", "List exposed connections, including disconnected retained sessions, with ids, protocol, state and actual serial settings. Match the requested port/host before choosing channel_id; never assume the first connection is the current device.", {});
    add("read_events", "Read exactly one connection, non-destructively. channel_id is required: obtain it from list_connections/get_status and verify protocol, port/host and state. Missing or invalid ids never fall back to global history or another connection. after=0 includes this connection's history; use get_status.latest_cursor for only new arrivals. Retain next_cursor separately per connection and reader; hex contains exact bytes. The response identifies its scope, channel_id, protocol and connection_state.",
        {{"after", property("integer", 0)}, {"limit", property("integer", 200)},
         {"channel_id", QJsonObject {{"type", "string"}, {"minLength", 1}, {"description", "Exact connection id from list_connections/get_status; not a port name or wildcard."}}},
         {"wait_ms", property("integer", 0)}}, {"channel_id"});
    add("read_all_events", "Explicit global history read across ALL exposed connections, including disconnected or removed sessions and historical SSH traffic. Use only when an aggregate log is requested. For a specific device use read_events with its channel_id. Retain an independent next_cursor for this aggregate stream; protocol switches still apply.",
        {{"after", property("integer", 0)}, {"limit", property("integer", 200)}, {"wait_ms", property("integer", 0)}});
    add("read_waveform", "Read wave/1 samples per peer: only complete @wave name=value,... lines are accepted. Names identify channels; integer, decimal and scientific values can be mixed. Plain CSV/JSON/logs are ignored. Custom script results use read_script_results.", {{"channel_id", property("string")}, {"limit", property("integer", 1000)}}, {"channel_id"});
    add("read_structured", "Read incremental JSON/XML extraction enabled in the GUI. Custom script results use read_script_results.", {{"channel_id", property("string")}, {"after", property("integer", 0)}, {"limit", property("integer", 100)}, {"cursors", property("object")}}, {"channel_id"});
    add("read_script_results", "Read custom serial frame script results shown in the receive page. Records contain numeric value fields and event_seq linking the original bytes. Retain each source next_cursor independently of JSON/XML cursors. Read access needs no control permission.", {{"channel_id", property("string")}, {"after", property("integer", 0)}, {"limit", property("integer", 100)}, {"cursors", property("object")}}, {"channel_id"});
    add("send_data", "Send bytes. Requires GUI permission. newline none/lf/cr/crlf, peer IP:port.",
        {{"channel_id", property("string")}, {"payload", property("string")}, {"hex_mode", property("boolean", false)},
         {"encoding", property("string", "utf-8")}, {"newline", property("string", "none")}, {"peer", property("string", "")}}, {"channel_id", "payload"}, false);
    add("open_connection", "Open serial/tcp-client/tcp-server/udp/ssh. SSH config fields: kind=ssh, ssh_host (alias from ~/.ssh/config or address), optional ssh_user, ssh_port, ssh_identity. Uses the system OpenSSH client and configured key or agent. Requires GUI permission; the matching receive page opens and activates in the GUI.", {{"config", property("object")}}, {"config"}, false);
    add("close_connection", "Disconnect and retain history. Requires GUI permission.", {{"channel_id", property("string")}}, {"channel_id"}, false);
    add("list_search_views", "List GUI receive/JSON/XML search views for exactly one channel. Returns ephemeral view_id values, saved search flags and display mode. Select an exact view before search_logs; no shell/grep needed.",
        {{"channel_id", property("string")}}, {"channel_id"});
    add("search_logs", "Search the GUI's retained display using its native filter. view_id comes from list_search_views. Optional filter updates the actual GUI search box and its case_sensitive, whole_word, regex, logical switches. Returns matching displayed lines/records, not raw journal events; paused and cleared views keep their GUI scope. Does not consume journal cursors or send device data. Offsets refer to this live display; retry from zero if snapshot changes.",
        {{"channel_id", property("string")}, {"view_id", property("string")},
         {"filter", QJsonObject {{"type", "object"}, {"additionalProperties", false},
             {"properties", QJsonObject {{"text", property("string")}, {"case_sensitive", property("boolean")},
                 {"whole_word", property("boolean")}, {"regex", property("boolean")}, {"logical", property("boolean")}}}}},
         {"offset", property("integer", 0)}, {"limit", property("integer", 200)}, {"snapshot", property("string")}},
        {"channel_id", "view_id"}, false);
    auto searchTool = result.last().toObject();
    searchTool["annotations"] = QJsonObject {{"readOnlyHint", false}, {"destructiveHint", false}, {"openWorldHint", false}};
    result.last() = searchTool;
    add("get_frame_script", "Read the current serial parser code, enabled state, error and revision. Use the returned revision for set_frame_script to avoid overwriting concurrent GUI changes.",
        {{"channel_id", property("string")}}, {"channel_id"});
    add("validate_frame_script", "Compile a C-style frame parser and optionally test HEX sample_chunks offline, preserving partial-frame state between chunks. Does not send bytes or change live parser state. Read read_usage_guide(topic=frame_script) first. Returns valid, error/line or decoded frames. Maximum 64 KiB source/input, 128 chunks and 1024 output frames.",
        {{"code", property("string")}, {"sample_chunks", QJsonObject {{"type", "array"}, {"items", property("string")}, {"maxItems", 128}}}}, {"code"});
    add("set_frame_script", "Save serial parser code and explicitly enable/disable it, synchronizing the GUI. Requires protocol access and GUI AI control permission, the current expected_revision, and no active file transfer or unsaved GUI editor changes. Validate code and sample chunks first. Applies only to newly received bytes; does not send data.",
        {{"channel_id", property("string")}, {"code", property("string")}, {"enabled", property("boolean")},
         {"expected_revision", property("string")}}, {"channel_id", "code", "enabled", "expected_revision"}, false);
    return result;
}
}
