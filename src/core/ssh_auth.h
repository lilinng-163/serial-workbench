// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QtCore>
#include <QtNetwork>
#include <functional>

namespace sw
{
struct Config;

class SshCredentialCache
{
public:
    static QString directory();
    static QString read(const QString &root, const QString &host, const QString &key);
    static void write(const QString &root, const QString &host, const QString &key, const QString &secret);
    static void remove(const QString &root, const QString &host, const QString &key = {});
};

struct SshHostRecords
{
    QString hostname;
    int port = 22;
    QStringList identifiers;
    QStringList files;
};

class SshHostCache
{
public:
    static SshHostRecords resolve(const Config &config, const QString &program = "ssh");
    static SshHostRecords parse(const QByteArray &configuration, const QString &originalHost);
    static int remove(const QString &path, const QStringList &identifiers);
};

class SshAuthPrompt : public QObject
{
    Q_OBJECT
public:
    explicit SshAuthPrompt(QObject *parent = nullptr) : QObject(parent) {}
    QString host;
    QString message;
    bool confirmation = false;
    bool retry = false;
    bool canRemember = false;
    QString cacheError;
    void answer(const QString &value, bool remember = false);
    void cancel();

signals:
    void finished();

private:
    friend class SshAuthSession;
    std::function<void(bool, const QString &, bool)> reply;
};

class SshAuthSession : public QObject
{
    Q_OBJECT
public:
    SshAuthSession(const Config &config, const QString &cacheDirectory, QObject *parent = nullptr);
    ~SshAuthSession() override;
    QProcessEnvironment environment() const;
    void stop();

signals:
    void promptRequested(sw::SshAuthPrompt *prompt);
    void cancelled();

private:
    QLocalServer server;
    QString token;
    QString host;
    QString account;
    QString cacheDirectory;
    QSet<QString> asked;
    QSet<QLocalSocket *> clients;
    void accept(QLocalSocket *socket, const QJsonObject &request);
};
}
