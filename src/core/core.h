#pragma once

#include "workbench_version.h"
#include <QtCore>
#include <QtNetwork>
#include <QtSerialPort>
#include <QTextCodec>
#include <deque>

namespace sw
{
inline const QString Version = QStringLiteral(SERIAL_WORKBENCH_VERSION);

struct Config
{
    QString kind = "serial";
    QString name;
    QString port = "NULL";
    int baudrate = 115200;
    int bytesize = 8;
    QString parity = "N";
    double stopbits = 1;
    QString flow = "none";
    bool dtr = false;
    bool rts = false;
    QString host = "127.0.0.1";
    int networkPort = 9000;
    QString bindHost = "127.0.0.1";
    int bindPort = 9001;

    QJsonObject json() const;
    static Config fromJson(const QJsonObject &object);
    void validate() const;
    QString key() const;
};

class Journal
{
public:
    QString session = QUuid::createUuid().toString(QUuid::Id128);
    qint64 sequence = 0;
    qsizetype bytes = 0;
    std::deque<QPair<QJsonObject, qsizetype>> events;

    QJsonObject append(const QString &id, const QString &direction, const QByteArray &data = {}, const QString &peer = {}, const QString &message = {}, bool reset = false);
    QJsonObject read(qint64 after, int limit = 200, const QString &id = {}) const;
};

struct Source
{
    QByteArray wavePending;
    bool discarding = false;
    std::deque<QJsonObject> samples;
    qint64 count = 0;
    int rejected = 0;
    QString parsePending;
    std::unique_ptr<QTextDecoder> decoder;
    std::deque<QJsonObject> records;
    qint64 parsedCount = 0;
    qsizetype parsedBytes = 0;
};

class Engine;

class Channel : public QObject
{
    Q_OBJECT
public:
    Channel(Engine *owner, const Config &configuration);
    ~Channel() override;

    Engine *engine;
    Config config;
    QString id = QUuid::createUuid().toString(QUuid::Id128).left(10);
    QString name;
    QString state = "closed";
    QString error;
    QString parserMode = "off";
    QString parserEncoding = "utf-8";
    qint64 rx = 0;
    qint64 tx = 0;
    int appliedBaud = 0;
    int generation = 0;
    int attempts = 0;
    QString localAddress;
    QSerialPort *serial = nullptr;
    QTcpSocket *client = nullptr;
    QTcpServer *listener = nullptr;
    QUdpSocket *udp = nullptr;
    QMap<QString, QTcpSocket *> peers;
    QMap<QString, std::shared_ptr<Source>> sources;
    QTimer demoTimer;
    QTimer retryTimer;
    bool manualClose = true;
    bool opening = false;

    QJsonObject describe() const;
    void open();
    void close();
    void applySerial(const Config &next);
    void receive(const QByteArray &data, const QString &peer = {});
    QJsonObject send(const QByteArray &data, const QString &peer = {}, const QString &origin = "gui");
    void status(const QString &value, const QString &message = {}, bool reset = false);
    void configureParser(const QString &mode, const QString &encoding = "utf-8");
    QJsonObject waveform(int limit = 1000) const;
    QJsonObject structured(qint64 after = 0, int limit = 100, const QJsonObject &cursors = {}) const;

private:
    void parseWave(Source &source, const QByteArray &data);
    void parseStructured(Source &source, const QByteArray &data, qint64 eventSequence, const QString &peer);
    void releaseTransport();
    void watchSocket(QTcpSocket *socket, const QString &peer);
};

class Engine : public QObject
{
    Q_OBJECT
public:
    explicit Engine(QObject *parent = nullptr);
    Journal journal;
    QMap<QString, Channel *> channels;
    bool allowControl = false;

    Channel *open(const Config &config);
    Channel *get(const QString &id) const;
    void remove(const QString &id);
    QJsonArray list() const;
    static QJsonArray ports();
    static QByteArray encode(const QString &payload, bool hex, const QString &encoding, const QString &newline);

signals:
    void channelsChanged();
    void eventAdded();
};

QString stateText(const QString &state);
QString now();
[[noreturn]] void fail(const QString &message);
}
