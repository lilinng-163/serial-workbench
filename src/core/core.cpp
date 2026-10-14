// SPDX-License-Identifier: Apache-2.0

#include "core.h"

#include <QTextCodec>
#include <cmath>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace sw
{
void fail(const QString &message)
{
    throw std::runtime_error(message.toStdString());
}

QString now()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QString stateText(const QString &state)
{
    static const QMap<QString, QString> values {
        {"connected", "已连接"}, {"connecting", "连接中"}, {"listening", "监听中"},
        {"closed", "未连接"}, {"error", "连接失败"}, {"reconnecting", "等待设备恢复"}
    };
    return values.value(state, state);
}

QJsonObject Config::json() const
{
    return {{"kind", kind}, {"name", name}, {"port", port}, {"baudrate", baudrate},
        {"bytesize", bytesize}, {"parity", parity}, {"stopbits", stopbits}, {"flow", flow},
        {"dtr", dtr}, {"rts", rts}, {"host", host}, {"network_port", networkPort},
        {"bind_host", bindHost}, {"bind_port", bindPort}, {"ssh_host", sshHost},
        {"ssh_user", sshUser}, {"ssh_port", sshPort}, {"ssh_identity", sshIdentity}};
}

Config Config::fromJson(const QJsonObject &o)
{
    Config c;
    c.kind = o.value("kind").toString(c.kind);
    c.name = o.value("name").toString();
    c.port = o.value("port").toString(c.port);
    c.baudrate = o.value("baudrate").toInt(c.baudrate);
    c.bytesize = o.value("bytesize").toInt(c.bytesize);
    c.parity = o.value("parity").toString(c.parity);
    c.stopbits = o.value("stopbits").toDouble(c.stopbits);
    c.flow = o.value("flow").toString(c.flow);
    c.dtr = o.value("dtr").toBool();
    c.rts = o.value("rts").toBool();
    c.host = o.value("host").toString(c.host);
    c.networkPort = o.value("network_port").toInt(c.networkPort);
    c.bindHost = o.value("bind_host").toString(c.bindHost);
    c.bindPort = o.value("bind_port").toInt(c.bindPort);
    c.sshHost = o.value("ssh_host").toString();
    c.sshUser = o.value("ssh_user").toString();
    c.sshPort = o.value("ssh_port").toInt();
    c.sshIdentity = o.value("ssh_identity").toString();
    return c;
}

QString Config::key() const
{
    return kind == "serial" ? "serial:" + port.toLower().remove("\\\\.\\")
        : kind == "ssh" ? QString("ssh:%1:%2:%3:%4").arg(sshHost.toLower(), sshUser, QString::number(sshPort), sshIdentity)
        : QString("%1:%2:%3:%4:%5").arg(kind, host).arg(networkPort).arg(bindHost).arg(bindPort);
}

void Config::validate() const
{
    if (!QStringList {"serial", "tcp-client", "tcp-server", "udp", "ssh", "demo"}.contains(kind))
    {
        fail("不支持的连接协议");
    }
    if (kind == "serial" && (port.trimmed().isEmpty() || port.compare("NULL", Qt::CaseInsensitive) == 0))
    {
        fail("请先选择串口，NULL 表示尚未选择");
    }
    if (baudrate < 1 || baudrate > 12000000 || bytesize < 5 || bytesize > 8
        || !QStringList {"N", "E", "O", "M", "S"}.contains(parity)
        || !QList<double> {1, 1.5, 2}.contains(stopbits)
        || !QStringList {"none", "rtscts", "xonxoff"}.contains(flow))
    {
        fail("串口参数无效，请检查波特率、数据位、校验、停止位和流控");
    }
    if (bindPort < 0 || bindPort > 65535 || networkPort < 1 || networkPort > 65535)
    {
        fail("网络端口无效");
    }
    if (kind == "ssh" && (sshHost.isEmpty() || sshHost.startsWith('-')
        || sshHost.contains(QRegularExpression(R"([\s\x00-\x1f])"))
        || sshUser.startsWith('-') || sshUser.contains(QRegularExpression(R"([\r\n])"))
        || sshIdentity.contains(QRegularExpression(R"([\r\n])"))
        || sshPort < 0 || sshPort > 65535))
    {
        fail("SSH 主机、用户名或端口无效");
    }
}

QJsonObject Journal::append(const QString &id, const QString &direction, const QByteArray &data,
    const QString &peer, const QString &message, bool reset)
{
    QJsonObject event {{"seq", ++sequence}, {"time", now()}, {"channel_id", id},
        {"direction", direction}, {"hex", QString::fromLatin1(data.toHex(' '))},
        {"text", QString::fromUtf8(data)}, {"size", data.size()}, {"peer", peer}, {"message", message}};
    if (reset)
    {
        event["stream_reset"] = true;
    }
    const qsizetype size = QJsonDocument(event).toJson(QJsonDocument::Compact).size();
    events.emplace_back(event, size);
    bytes += size;
    while (events.size() > 20000 || bytes > 8 * 1024 * 1024)
    {
        bytes -= events.front().second;
        events.pop_front();
    }
    return event;
}

QJsonObject Journal::read(qint64 after, int limit, const QString &id) const
{
    if (after < 0 || limit < 1 || limit > 1000)
    {
        fail("after >= 0，limit 范围 1..1000");
    }
    QJsonArray page;
    bool more = false;
    for (const auto &entry : events)
    {
        const auto &event = entry.first;
        if (event["seq"].toInteger() > after && (id.isEmpty() || event["channel_id"].toString() == id))
        {
            if (page.size() >= limit)
            {
                more = true;
                break;
            }
            page.append(event);
        }
    }
    const qint64 oldest = events.empty() ? sequence + 1 : events.front().first["seq"].toInteger();
    return {{"session_id", session}, {"events", page}, {"next_cursor", more ? page.last().toObject()["seq"].toInteger() : sequence},
        {"latest_cursor", sequence}, {"oldest_cursor", oldest}, {"has_more", more},
        {"gap", after < oldest - 1}, {"cursor_reset_required", after > sequence}};
}

Engine::Engine(QObject *parent) : QObject(parent)
{
}

Channel *Engine::get(const QString &id) const
{
    if (!channels.contains(id))
    {
        fail("连接不存在，请选择连接或调用 list_connections");
    }
    return channels.value(id);
}

Channel *Engine::open(const Config &config)
{
    config.validate();
    if (config.kind == "serial")
    {
        for (auto channel : channels)
        {
            if (channel->config.key() == config.key())
            {
                if (channel->state == "connected")
                {
                    channel->applySerial(config);
                    return channel;
                }
                channel->close();
                channel->config = config;
                channel->open();
                return channel;
            }
        }
    }
    if (channels.size() >= 64)
    {
        fail("最多保留 64 个连接，请关闭不需要的标签");
    }
    auto channel = new Channel(this, config);
    channels.insert(channel->id, channel);
    channel->open();
    emit channelsChanged();
    return channel;
}

void Engine::remove(const QString &id)
{
    auto channel = get(id);
    channel->close();
    channels.remove(id);
    channel->deleteLater();
    emit channelsChanged();
}

QJsonArray Engine::list() const
{
    QJsonArray result;
    for (auto channel : channels)
    {
        result.append(channel->describe());
    }
    return result;
}

void Engine::checkSerialDevices(const QJsonArray &available)
{
    QStringList names;
    for (const auto &value : available)
    {
        names.append(value.toObject()["port"].toString().toLower());
    }
    for (auto channel : channels.values())
    {
        const auto name = channel->config.port.toLower().remove("\\\\.\\");
        if (!names.contains(name))
        {
            channel->deviceMissing();
        }
    }
}

QJsonArray Engine::ports()
{
    QJsonArray result;
    for (const auto &port : QSerialPortInfo::availablePorts())
    {
        result.append(QJsonObject {{"port", port.portName()}, {"description", port.description()},
            {"hwid", port.serialNumber()}, {"manufacturer", port.manufacturer()},
            {"serial_number", port.serialNumber()}, {"system_location", port.systemLocation()},
            {"vendor_id", port.hasVendorIdentifier() ? QJsonValue(port.vendorIdentifier()) : QJsonValue()},
            {"product_id", port.hasProductIdentifier() ? QJsonValue(port.productIdentifier()) : QJsonValue()},
            {"usb_serial_jtag", port.vendorIdentifier() == 0x303a && port.productIdentifier() == 0x1001}});
    }
    return result;
}

QByteArray Engine::encode(const QString &payload, bool hex, const QString &encoding, const QString &newline)
{
    QByteArray bytes;
    if (hex)
    {
        QString cleaned = payload;
        cleaned.remove(QRegularExpression("\\s+"));
        if (cleaned.size() % 2 || cleaned.contains(QRegularExpression("[^0-9a-fA-F]")))
        {
            fail("HEX 需要完整的十六进制字节，例如 01 03 00 00");
        }
        bytes = QByteArray::fromHex(cleaned.toLatin1());
    }
    else
    {
        auto codec = QTextCodec::codecForName(encoding.toLatin1());
        if (!codec)
        {
            fail("不支持的文本编码");
        }
        bytes = codec->fromUnicode(payload);
    }
    static const QMap<QString, QByteArray> endings {{"none", {}}, {"lf", "\n"}, {"cr", "\r"}, {"crlf", "\r\n"}};
    if (!endings.contains(newline))
    {
        fail("换行参数无效");
    }
    bytes += endings.value(newline);
    if (bytes.size() > 1024 * 1024)
    {
        fail("单次发送不能超过 1 MiB");
    }
    return bytes;
}

Channel::Channel(Engine *owner, const Config &configuration) : QObject(owner), engine(owner), config(configuration)
{
    name = config.name.isEmpty() ? (config.kind == "serial" ? config.port
        : config.kind == "ssh" ? config.sshHost
        : config.kind + " · " + QString::number(config.kind == "tcp-client" ? config.networkPort : config.bindPort)) : config.name;
    demoTimer.setInterval(40);
    connect(&demoTimer, &QTimer::timeout, this, [this]
    {
        const double t = QDateTime::currentMSecsSinceEpoch() / 1000.0;
        receive(QString("@wave temperature=%1,voltage=%2,current=%3\r\n").arg(25 + 3 * std::sin(t), 0, 'f', 3).arg(3.3 + 0.2 * std::cos(t * 0.7), 0, 'f', 3).arg(0.4 + 0.1 * std::sin(t * 1.8), 0, 'f', 3).toUtf8());
    });
    retryTimer.setInterval(300);
    connect(&retryTimer, &QTimer::timeout, this, [this]
    {
        if (!manualClose)
        {
            ++attempts;
            open();
        }
    });
}

Channel::~Channel()
{
    releaseTransport();
}

void Channel::status(const QString &value, const QString &message, bool reset)
{
    state = value;
    error = value == "error" || value == "reconnecting" ? message : QString();
    engine->journal.append(id, "status", {}, {}, message.isEmpty() ? value : message, reset);
    emit engine->channelsChanged();
    emit engine->eventAdded();
}

void Channel::releaseTransport()
{
    if (scriptProcessor) { scriptProcessor->configure(scriptEnabled ? scriptProgram : nullptr); }
    demoTimer.stop();
    for (auto object : {static_cast<QObject *>(serial), static_cast<QObject *>(client), static_cast<QObject *>(listener), static_cast<QObject *>(udp)})
    {
        if (object)
        {
            object->disconnect(this);
            object->deleteLater();
        }
    }
    if (serial)
    {
        serial->close();
    }
    if (client)
    {
        client->abort();
    }
    if (listener)
    {
        listener->close();
    }
    if (udp)
    {
        udp->close();
    }
    if (ssh)
    {
        ssh->disconnect(this);
        if (ssh->state() != QProcess::NotRunning)
        {
            ssh->terminate();
            if (!ssh->waitForFinished(300))
            {
                ssh->kill();
                ssh->waitForFinished(300);
            }
        }
        ssh->deleteLater();
    }
    for (auto socket : peers)
    {
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
    peers.clear();
    serial = nullptr;
    client = nullptr;
    listener = nullptr;
    udp = nullptr;
    ssh = nullptr;
    sshError.clear();
    appliedBaud = 0;
}

void Channel::close()
{
    manualClose = true;
    retryTimer.stop();
    releaseTransport();
    status("closed");
}

void Channel::deviceMissing()
{
    if (manualClose || config.kind != "serial" || config.port == "loop://" || state != "connected")
    {
        return;
    }
    releaseTransport();
    status("reconnecting", QString("串口 %1 已断开，等待设备恢复").arg(config.port));
    retryTimer.start();
}

void Channel::applySerial(const Config &next)
{
    next.validate();
    if (config.port == "loop://")
    {
        config = next;
        appliedBaud = next.baudrate;
        return;
    }
    if (!serial || !serial->isOpen())
    {
        fail("串口未打开");
    }
    const QMap<QString, QSerialPort::Parity> parities {{"N", QSerialPort::NoParity}, {"E", QSerialPort::EvenParity}, {"O", QSerialPort::OddParity}, {"M", QSerialPort::MarkParity}, {"S", QSerialPort::SpaceParity}};
    const auto stops = next.stopbits == 2 ? QSerialPort::TwoStop : next.stopbits == 1.5 ? QSerialPort::OneAndHalfStop : QSerialPort::OneStop;
    const auto flow = next.flow == "rtscts" ? QSerialPort::HardwareControl : next.flow == "xonxoff" ? QSerialPort::SoftwareControl : QSerialPort::NoFlowControl;
    if (!serial->setBaudRate(next.baudrate) || !serial->setDataBits(static_cast<QSerialPort::DataBits>(next.bytesize))
        || !serial->setParity(parities.value(next.parity)) || !serial->setStopBits(stops) || !serial->setFlowControl(flow)
        || !serial->setDataTerminalReady(next.dtr)
        || (next.flow != "rtscts" && !serial->setRequestToSend(next.rts)))
    {
        const auto message = "驱动拒绝串口参数：" + serial->errorString();
        releaseTransport();
        status("error", message);
        fail(message);
    }
    int actual = serial->baudRate();
#ifdef Q_OS_WIN
    DCB dcb {};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(reinterpret_cast<HANDLE>(serial->handle()), &dcb))
    {
        releaseTransport();
        status("error", "无法从 Windows 驱动回读串口参数");
        fail(error);
    }
    actual = static_cast<int>(dcb.BaudRate);
#endif
    if (actual != next.baudrate)
    {
        const auto message = QString("驱动返回的波特率 %1 与请求值 %2 不一致").arg(actual).arg(next.baudrate);
        releaseTransport();
        status("error", message);
        fail(message);
    }
    config = next;
    appliedBaud = actual;
    if (!config.name.isEmpty())
    {
        name = config.name;
    }
    emit engine->channelsChanged();
}

void Channel::watchSocket(QTcpSocket *socket, const QString &peer)
{
    connect(socket, &QTcpSocket::readyRead, this, [this, socket, peer]
    {
        receive(socket->readAll(), peer);
    });
    connect(socket, &QTcpSocket::disconnected, this, [this, socket, peer]
    {
        if (peer.isEmpty())
        {
            status("closed");
        }
        else
        {
            peers.remove(peer);
            socket->deleteLater();
            emit engine->channelsChanged();
        }
    });
}

void Channel::open()
{
    const bool recovering = state == "reconnecting";
    releaseTransport();
    manualClose = false;
    opening = true;
    try
    {
        if (config.kind == "serial")
        {
            if (config.port == "loop://")
            {
                appliedBaud = config.baudrate;
            }
            else
            {
                serial = new QSerialPort(this);
                serial->setPortName(config.port);
                serial->setReadBufferSize(1024 * 1024);
                if (!serial->open(QIODevice::ReadWrite))
                {
                    fail(serial->errorString());
                }
                applySerial(config);
                connect(serial, &QSerialPort::readyRead, this, [this]
                {
                    receive(serial->readAll());
                });
                connect(serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError value)
                {
                    if (!opening && !manualClose && (value == QSerialPort::ResourceError || value == QSerialPort::DeviceNotFoundError || value == QSerialPort::ReadError || value == QSerialPort::WriteError))
                    {
                        const QString message = serial->errorString();
                        releaseTransport();
                        status("reconnecting", QString("串口 %1 已断开，等待设备恢复（%2）").arg(config.port, message));
                        retryTimer.start();
                    }
                });
            }
            ++generation;
            retryTimer.stop();
            for (const auto &source : sources)
            {
                source->wavePending.clear();
                source->parsePending.clear();
                source->decoder.reset();
            }
            status("connected", {}, recovering);
        }
        else if (config.kind == "tcp-client")
        {
            client = new QTcpSocket(this);
            watchSocket(client, {});
            connect(client, &QTcpSocket::connected, this, [this]
            {
                localAddress = client->localAddress().toString() + ":" + QString::number(client->localPort());
                status("connected");
            });
            connect(client, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError)
            {
                status("error", client->errorString());
            });
            status("connecting");
            client->connectToHost(config.host, config.networkPort);
        }
        else if (config.kind == "tcp-server")
        {
            listener = new QTcpServer(this);
            listener->setMaxPendingConnections(16);
            if (!listener->listen(QHostAddress(config.bindHost), config.bindPort))
            {
                fail(listener->errorString());
            }
            localAddress = listener->serverAddress().toString() + ":" + QString::number(listener->serverPort());
            connect(listener, &QTcpServer::newConnection, this, [this]
            {
                while (listener->hasPendingConnections())
                {
                    auto socket = listener->nextPendingConnection();
                    if (peers.size() >= 16)
                    {
                        socket->abort();
                        socket->deleteLater();
                        continue;
                    }
                    const QString peer = socket->peerAddress().toString() + ":" + QString::number(socket->peerPort());
                    peers.insert(peer, socket);
                    watchSocket(socket, peer);
                }
                emit engine->channelsChanged();
            });
            status("listening");
        }
        else if (config.kind == "udp")
        {
            udp = new QUdpSocket(this);
            if (!udp->bind(QHostAddress(config.bindHost), config.bindPort))
            {
                fail(udp->errorString());
            }
            localAddress = udp->localAddress().toString() + ":" + QString::number(udp->localPort());
            connect(udp, &QUdpSocket::readyRead, this, [this]
            {
                while (udp->hasPendingDatagrams())
                {
                    auto datagram = udp->receiveDatagram();
                    receive(datagram.data(), datagram.senderAddress().toString() + ":" + QString::number(datagram.senderPort()));
                }
            });
            status("connected");
        }
        else if (config.kind == "ssh")
        {
            ssh = new QProcess(this);
            QStringList arguments {"-tt", "-o", "BatchMode=yes", "-o", "ConnectTimeout=15",
                "-o", "ServerAliveInterval=30", "-o", "ServerAliveCountMax=3"};
            if (!config.sshUser.isEmpty())
            {
                arguments << "-l" << config.sshUser;
            }
            if (config.sshPort > 0)
            {
                arguments << "-p" << QString::number(config.sshPort);
            }
            if (!config.sshIdentity.isEmpty())
            {
                arguments << "-i" << config.sshIdentity;
            }
            arguments << config.sshHost;
            ssh->setProgram(engine->sshExecutable);
            ssh->setArguments(arguments);
            auto environment = QProcessEnvironment::systemEnvironment();
            environment.insert("TERM", "xterm-256color");
            ssh->setProcessEnvironment(environment);
            connect(ssh, &QProcess::readyReadStandardOutput, this, [this]
            {
                const QByteArray data = ssh->readAllStandardOutput();
                if (state == "connecting") { status("connected"); }
                receive(data);
            });
            connect(ssh, &QProcess::readyReadStandardError, this, [this]
            {
                const QByteArray data = ssh->readAllStandardError();
                sshError = (sshError + QString::fromUtf8(data)).right(2048);
                receive(data);
            });
            connect(ssh, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error)
            {
                if (error == QProcess::FailedToStart)
                {
                    status("error", "无法启动系统 OpenSSH 客户端 ssh.exe");
                }
            });
            connect(ssh, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                [this](int code, QProcess::ExitStatus)
            {
                if (code == 0 && state == "connected")
                {
                    status("closed", "SSH 会话已结束");
                }
                else if (state != "error")
                {
                    const QString reason = sshError.trimmed().section('\n', -1).trimmed();
                    status("error", reason.isEmpty() ? QString("SSH 连接结束，退出码 %1").arg(code) : reason);
                }
            });
            status("connecting");
            ssh->start();
        }
        else
        {
            demoTimer.start();
            status("connected");
        }
    }
    catch (const std::exception &exception)
    {
        releaseTransport();
        if (recovering)
        {
            // Keep the first outage notification; retries are background work.
            retryTimer.start();
        }
        else
        {
            status("error", QString::fromUtf8(exception.what()));
        }
    }
    opening = false;
}

QJsonObject Channel::describe() const
{
    return {{"id", id}, {"name", name}, {"state", state}, {"error", error}, {"config", config.json()},
        {"rx_bytes", rx}, {"tx_bytes", tx}, {"peers", QJsonArray::fromStringList(peers.keys())},
        {"local_address", localAddress}, {"serial_generation", generation}, {"reconnect_attempts", attempts},
        {"parser_mode", parserMode}, {"applied_baudrate", appliedBaud},
        {"script_enabled", scriptEnabled}, {"script_error", scriptError}, {"script_frames", scriptFrames}};
}

void Channel::receive(const QByteArray &data, const QString &peer)
{
    if (data.isEmpty())
    {
        return;
    }
    rx += data.size();
    auto event = engine->journal.append(id, "rx", data, peer);
    const QString key = peer.isEmpty() ? "default" : peer;
    if (!sources.contains(key))
    {
        if (sources.size() >= 32)
        {
            sources.erase(sources.begin());
        }
        sources.insert(key, std::make_shared<Source>());
    }
    auto &source = *sources[key];
    if (scriptEnabled && scriptProcessor) { scriptProcessor->submit(data, peer, event["seq"].toInteger()); }
    parseWave(source, data);
    if (parserMode != "off")
    {
        parseStructured(source, data, event["seq"].toInteger(), peer);
    }
    emit engine->eventAdded();
}

QJsonObject Channel::send(const QByteArray &data, const QString &peer, const QString &origin)
{
    if (state != "connected" && state != "listening")
    {
        fail("连接未就绪，不能发送");
    }
    qint64 written = -1;
    if (config.kind == "serial" && config.port == "loop://")
    {
        written = data.size();
        QTimer::singleShot(0, this, [this, data] { receive(data); });
    }
    else if (serial)
    {
        written = serial->write(data);
    }
    else if (client)
    {
        written = client->write(data);
    }
    else if (listener)
    {
        QString target = peer;
        if (target.isEmpty() && peers.size() == 1)
        {
            target = peers.firstKey();
        }
        if (!peers.contains(target))
        {
            fail("请选择目标 TCP 客户端");
        }
        written = peers[target]->write(data);
    }
    else if (udp)
    {
        const QString host = peer.isEmpty() ? config.host : peer.section(':', 0, -2);
        const int port = peer.isEmpty() ? config.networkPort : peer.section(':', -1).toInt();
        if (QHostAddress(host).isNull() || port < 1 || port > 65535)
        {
            fail("UDP 目标需要有效 IP:端口");
        }
        written = udp->writeDatagram(data, QHostAddress(host), port);
    }
    else if (ssh)
    {
        written = ssh->write(data);
    }
    else if (config.kind == "demo")
    {
        fail("模拟信号为只读数据源");
    }
    if (written != data.size())
    {
        fail("驱动未接受完整报文，未自动重发");
    }
    tx += written;
    // Interactive SSH input can contain passwords typed into remote programs.
    // Only the byte count is shared with journal readers.
    if (config.kind != "ssh")
    {
        engine->journal.append(id, "tx", data, peer, origin);
    }
    emit engine->eventAdded();
    return {{"channel_id", id}, {"bytes_sent", written}, {"peer", peer}};
}

void Channel::parseWave(Source &source, const QByteArray &data)
{
    static const QRegularExpression fieldPattern(QStringLiteral(
        R"(\A[ \t]*([\p{L}_][\p{L}\p{N}_]{0,63})[ \t]*=[ \t]*([+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?)[ \t]*\z)"));

    source.wavePending += data;
    int end = -1;
    while (true)
    {
        const int cr = source.wavePending.indexOf('\r');
        const int lf = source.wavePending.indexOf('\n');
        end = cr < 0 ? lf : lf < 0 ? cr : qMin(cr, lf);
        if (end < 0) { break; }
        const QByteArray line = source.wavePending.left(end);
        source.wavePending.remove(0, end + 1);
        if (source.discarding)
        {
            source.discarding = false;
            continue;
        }
        if (!line.startsWith("@wave "))
        {
            continue;
        }
        if (line.size() > 8192)
        {
            ++source.rejected;
            continue;
        }

        const auto fields = QString::fromUtf8(line.mid(6)).split(',', Qt::KeepEmptyParts);
        QJsonObject values;
        bool valid = fields.size() <= 16;

        for (const auto &field : fields)
        {
            if (!valid) { break; }
            const auto match = fieldPattern.match(field);
            bool numberValid = false;
            const double number = match.captured(2).toDouble(&numberValid);
            const QString name = match.captured(1);

            if (!match.hasMatch() || !numberValid || !std::isfinite(number) || values.contains(name))
            {
                valid = false;
                break;
            }
            values.insert(name, number);
        }

        if (valid)
        {
            source.samples.push_back({{"index", ++source.count}, {"time", QDateTime::currentMSecsSinceEpoch() / 1000.0}, {"values", values}});
            if (source.samples.size() > 5000)
            {
                source.samples.pop_front();
            }
        }
        else
        {
            ++source.rejected;
        }
    }
    if (source.wavePending.size() > 8192)
    {
        if (!source.discarding && source.wavePending.startsWith("@wave "))
        {
            ++source.rejected;
        }
        source.wavePending.clear();
        source.discarding = true;
    }
}

void Channel::configureScript(const QString &code, bool enabled)
{
    if (enabled && config.kind != "serial")
    {
        throw ScriptError(0, "脚本解析仅适用于串口会话");
    }
    auto program = enabled ? FrameScriptProgram::compile(code) : nullptr;
    if (!scriptProcessor && enabled)
    {
        scriptProcessor = new FrameScriptProcessor(this);
        connect(scriptProcessor, &FrameScriptProcessor::samplesReady, this,
                [this](const QString &peer, const QJsonArray &samples, qint64 eventSequence)
        {
            if (!scriptEnabled) { return; }
            const auto key = peer.isEmpty() ? QString("default") : peer;
            if (!sources.contains(key)) { return; }
            auto &source = *sources[key];
            for (const auto &sample : samples)
            {
                const auto values = sample.toObject();
                ++scriptFrames;

                const QString text = QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Compact));
                const QJsonObject record {{"seq", ++source.scriptCount}, {"event_seq", eventSequence},
                    {"time", now()}, {"format", "script"}, {"peer", peer}, {"text", text},
                    {"pretty", QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Indented))}, {"value", values}};
                source.scriptBytes += QJsonDocument(record).toJson().size();
                source.scriptRecords.push_back(record);
                while (source.scriptRecords.size() > 500 || source.scriptBytes > 2 * 1024 * 1024)
                {
                    source.scriptBytes -= QJsonDocument(source.scriptRecords.front()).toJson().size();
                    source.scriptRecords.pop_front();
                }
            }
            emit engine->eventAdded();
        });
        connect(scriptProcessor, &FrameScriptProcessor::failed, this, [this](const QString &message)
        {
            if (!scriptEnabled) { return; }
            scriptEnabled = false;
            scriptError = message;
            scriptProcessor->configure(nullptr);
            engine->journal.append(id, "status", {}, {}, "脚本解析已停用：" + message);
            emit engine->eventAdded();
            emit engine->channelsChanged();
        });
    }
    scriptCode = code;
    scriptProgram = program;
    scriptEnabled = enabled;
    scriptError.clear();
    scriptFrames = 0;
    if (scriptProcessor) { scriptProcessor->configure(program); }
    emit engine->channelsChanged();
}

void Channel::configureParser(const QString &mode, const QString &encoding)
{
    if (!QStringList {"off", "auto", "json", "xml"}.contains(mode))
    {
        fail("解析格式无效");
    }
    if (mode != parserMode || encoding != parserEncoding)
    {
        for (auto source : sources)
        {
            source->parsePending.clear();
            source->decoder.reset();
        }
    }
    parserMode = mode;
    parserEncoding = encoding;
}

void Channel::parseStructured(Source &source, const QByteArray &data, qint64 eventSequence, const QString &peer)
{
    if (!source.decoder)
    {
        auto codec = QTextCodec::codecForName(parserEncoding.toUtf8());
        if (!codec)
        {
            fail("不支持的解析编码");
        }
        source.decoder.reset(codec->makeDecoder());
    }
    source.parsePending += source.decoder->toUnicode(data);
    source.parsePending.remove(QRegularExpression("\\x1b\\[[0-?]*[ -/]*[@-~]"));
    while (!source.parsePending.isEmpty())
    {
        const auto start = source.parsePending.indexOf(QRegularExpression(parserMode == "json" ? "[\\[{]" : parserMode == "xml" ? "<" : "[\\[{<]"));
        if (start < 0)
        {
            source.parsePending.clear();
            break;
        }
        source.parsePending.remove(0, start);
        const bool xml = source.parsePending.startsWith('<');
        int end = -1;
        bool valid = false;
        QJsonValue value;
        QString pretty;
        if (!xml)
        {
            int depth = 0;
            bool quoted = false;
            bool escape = false;
            for (int i = 0; i < source.parsePending.size(); ++i)
            {
                const QChar c = source.parsePending[i];
                if (escape)
                {
                    escape = false;
                }
                else if (quoted && c == '\\')
                {
                    escape = true;
                }
                else if (c == '"')
                {
                    quoted = !quoted;
                }
                else if (!quoted)
                {
                    if (c == '{' || c == '[')
                    {
                        ++depth;
                    }
                    else if (c == '}' || c == ']')
                    {
                        --depth;
                        if (depth <= 0)
                        {
                            end = i + 1;
                            break;
                        }
                    }
                    if (depth > 128)
                    {
                        end = i + 1;
                        break;
                    }
                }
            }
            if (end >= 0)
            {
                const auto document = QJsonDocument::fromJson(source.parsePending.left(end).toUtf8());
                valid = !document.isNull();
                value = document.isObject() ? QJsonValue(document.object()) : QJsonValue(document.array());
                pretty = QString::fromUtf8(document.toJson(QJsonDocument::Indented));
            }
        }
        else
        {
            QXmlStreamReader reader(source.parsePending);
            int depth = 0;
            bool root = false;
            while (!reader.atEnd())
            {
                reader.readNext();
                if (reader.isDTD() || reader.isEntityReference())
                {
                    end = static_cast<int>(reader.characterOffset());
                    break;
                }
                if (reader.isStartElement())
                {
                    root = true;
                    ++depth;
                }
                if (reader.isEndElement())
                {
                    --depth;
                    if (root && depth == 0)
                    {
                        end = static_cast<int>(reader.characterOffset());
                        valid = true;
                        pretty = source.parsePending.left(end);
                        break;
                    }
                }
                if (depth > 128)
                {
                    end = static_cast<int>(reader.characterOffset());
                    break;
                }
            }
            if (reader.hasError() && reader.error() != QXmlStreamReader::PrematureEndOfDocumentError)
            {
                end = std::max(1, static_cast<int>(reader.characterOffset()));
            }
        }
        if (end < 0)
        {
            if (source.parsePending.size() > 65536)
            {
                source.parsePending.clear();
            }
            break;
        }
        const QString text = source.parsePending.left(end);
        source.parsePending.remove(0, end);
        if (valid)
        {
            if (xml)
            {
                pretty.clear();
                QXmlStreamReader reader(text);
                QXmlStreamWriter writer(&pretty);
                writer.setAutoFormatting(true);
                while (!reader.atEnd())
                {
                    reader.readNext();
                    writer.writeCurrentToken(reader);
                }
            }
            QJsonObject record {{"seq", ++source.parsedCount}, {"time", now()}, {"event_seq", eventSequence},
                {"format", xml ? "xml" : "json"}, {"peer", peer}, {"text", text}, {"pretty", pretty}, {"value", value}};
            source.parsedBytes += QJsonDocument(record).toJson().size();
            source.records.push_back(record);
            while (source.records.size() > 500 || source.parsedBytes > 2 * 1024 * 1024)
            {
                source.parsedBytes -= QJsonDocument(source.records.front()).toJson().size();
                source.records.pop_front();
            }
        }
    }
}

QJsonObject Channel::waveform(int limit) const
{
    if (limit < 1 || limit > 5000)
    {
        fail("波形采样范围 1..5000");
    }
    QJsonObject result;
    for (auto it = sources.begin(); it != sources.end(); ++it)
    {
        QJsonArray samples;
        const auto &source = *it.value();
        const int skip = std::max(0, static_cast<int>(source.samples.size()) - limit);
        for (int i = skip; i < static_cast<int>(source.samples.size()); ++i)
        {
            samples.append(source.samples[i]);
        }
        result[it.key()] = QJsonObject {{"samples", samples}, {"total_samples", source.count}, {"rejected_lines", source.rejected}};
    }
    return {{"channel_id", id}, {"protocol", "wave/1"}, {"sources", result}};
}

QJsonObject Channel::structured(qint64 after, int limit, const QJsonObject &cursors) const
{
    auto result = readRecords(after, limit, cursors, false);
    result["mode"] = parserMode;
    return result;
}

QJsonObject Channel::scriptResults(qint64 after, int limit, const QJsonObject &cursors) const
{
    auto result = readRecords(after, limit, cursors, true);
    result["enabled"] = scriptEnabled;
    result["error"] = scriptError;
    return result;
}

QJsonObject Channel::readRecords(qint64 after, int limit, const QJsonObject &cursors, bool script) const
{
    if (after < 0 || limit < 1 || limit > 500)
    {
        fail("解析游标 >= 0，limit 范围 1..500");
    }
    QJsonObject result;
    for (auto it = sources.begin(); it != sources.end(); ++it)
    {
        const auto &source = *it.value();
        const auto &history = script ? source.scriptRecords : source.records;
        const qint64 count = script ? source.scriptCount : source.parsedCount;
        const qint64 cursor = cursors.value(it.key()).toInteger(after);
        if (cursor < 0) { fail("解析游标不能为负数"); }
        QJsonArray records;
        bool more = false;
        for (const auto &record : history)
        {
            if (record["seq"].toInteger() > cursor)
            {
                if (records.size() >= limit)
                {
                    more = true;
                    break;
                }
                records.append(record);
            }
        }
        const qint64 oldest = history.empty() ? count + 1 : history.front()["seq"].toInteger();
        result[it.key()] = QJsonObject {{"records", records}, {"next_cursor", more ? records.last().toObject()["seq"].toInteger() : count},
            {"has_more", more}, {"gap", cursor < oldest - 1}, {"oldest_cursor", oldest}};
    }
    return {{"channel_id", id}, {"sources", result}};
}
}
