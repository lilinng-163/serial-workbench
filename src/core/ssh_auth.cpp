// SPDX-License-Identifier: Apache-2.0
#include "ssh_auth.h"
#include "core.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace sw
{
namespace
{
QString hash(const QString &value)
{
    return QString::fromLatin1(QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString cachePath(const QString &root, const QString &host)
{
    return QDir(root).filePath(hash(host.toLower()) + ".json");
}

QJsonObject readCache(const QString &path)
{
    QFile file(path);
    if (!file.exists()) { return {}; }
    if (!file.open(QIODevice::ReadOnly)) { fail("无法读取 SSH 密码缓存"); }
    return QJsonDocument::fromJson(file.readAll()).object();
}

void saveCache(const QString &path, const QJsonObject &object)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { fail("无法写入 SSH 密码缓存"); }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(QJsonDocument(object).toJson(QJsonDocument::Compact)) < 0 || !file.commit())
    {
        fail("无法保存 SSH 密码缓存");
    }
}

QByteArray protect(const QByteArray &input, const QByteArray &entropy, bool encrypt)
{
#ifdef Q_OS_WIN
    DATA_BLOB source {DWORD(input.size()), reinterpret_cast<BYTE *>(const_cast<char *>(input.constData()))};
    DATA_BLOB salt {DWORD(entropy.size()), reinterpret_cast<BYTE *>(const_cast<char *>(entropy.constData()))};
    DATA_BLOB output {};
    const bool ok = encrypt
        ? CryptProtectData(&source, L"Serial Workbench SSH", &salt, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)
        : CryptUnprotectData(&source, nullptr, &salt, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output);
    if (!ok)
    {
        if (encrypt) { fail("Windows 无法加密 SSH 密码，密码未保存"); }
        return {};
    }
    QByteArray result(reinterpret_cast<const char *>(output.pbData), int(output.cbData));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return result;
#else
    Q_UNUSED(input)
    Q_UNUSED(entropy)
    if (encrypt) { fail("此平台暂不支持保存 SSH 密码"); }
    return {};
#endif
}

bool hostMatches(const QByteArray &entry, const QStringList &identifiers)
{
    for (const auto &identifier : identifiers)
    {
        if (entry.startsWith("|1|"))
        {
            const auto parts = entry.split('|');
            if (parts.size() == 4 && QMessageAuthenticationCode::hash(identifier.toUtf8(),
                    QByteArray::fromBase64(parts[2]), QCryptographicHash::Sha1) == QByteArray::fromBase64(parts[3]))
            {
                return true;
            }
        }
        else if (QString::fromUtf8(entry).compare(identifier, Qt::CaseInsensitive) == 0)
        {
            return true;
        }
    }
    return false;
}
}

QString SshCredentialCache::directory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/SerialWorkbench/ssh";
}

QString SshCredentialCache::read(const QString &root, const QString &host, const QString &key)
{
    const auto encoded = readCache(cachePath(root, host))[hash(key)].toString().toLatin1();
    if (encoded.isEmpty()) { return {}; }
    return QString::fromUtf8(protect(QByteArray::fromBase64(encoded), hash(host.toLower() + key).toUtf8(), false));
}

void SshCredentialCache::write(const QString &root, const QString &host, const QString &key, const QString &secret)
{
    auto plain = secret.toUtf8();
    const auto encrypted = protect(plain, hash(host.toLower() + key).toUtf8(), true);
    plain.fill('\0');
    if (!QDir().mkpath(root)) { fail("无法创建 SSH 密码缓存目录"); }
    const auto path = cachePath(root, host);
    QLockFile lock(path + ".lock");
    if (!lock.tryLock(500)) { fail("SSH 密码缓存正在使用，请重试"); }
    auto object = readCache(path);
    object[hash(key)] = QString::fromLatin1(encrypted.toBase64());
    saveCache(path, object);
}

void SshCredentialCache::remove(const QString &root, const QString &host, const QString &key)
{
    const auto path = cachePath(root, host);
    if (!QFile::exists(path)) { return; }
    QLockFile lock(path + ".lock");
    if (!lock.tryLock(500)) { fail("SSH 密码缓存正在使用，请重试"); }
    if (key.isEmpty())
    {
        if (!QFile::remove(path)) { fail("无法清除 SSH 密码缓存"); }
    }
    else
    {
        auto object = readCache(path);
        object.remove(hash(key));
        saveCache(path, object);
    }
}

SshHostRecords SshHostCache::resolve(const Config &config, const QString &program)
{
    config.validate();
    QStringList arguments {"-G"};
    if (!config.sshUser.isEmpty()) { arguments << "-l" << config.sshUser; }
    if (config.sshPort > 0) { arguments << "-p" << QString::number(config.sshPort); }
    if (!config.sshIdentity.isEmpty()) { arguments << "-i" << config.sshIdentity; }
    arguments << config.sshHost;
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForFinished(3000) || process.exitCode() != 0)
    {
        process.kill();
        process.waitForFinished(300);
        fail("无法读取 OpenSSH 的有效配置，请检查主机名称和 SSH 配置文件");
    }
    return parse(process.readAllStandardOutput(), config.sshHost);
}

SshHostRecords SshHostCache::parse(const QByteArray &configuration, const QString &originalHost)
{
    SshHostRecords result;
    result.hostname = originalHost.section('@', -1).toLower();
    QString alias;
    QString user;
    for (const auto &line : QString::fromUtf8(configuration).split('\n'))
    {
        const auto key = line.section(' ', 0, 0).toLower();
        const auto value = line.section(' ', 1).trimmed();
        if (key == "hostname") { result.hostname = value.toLower(); }
        else if (key == "port") { result.port = value.toInt(); }
        else if (key == "user") { user = value; }
        else if (key == "hostkeyalias" && value != "none") { alias = value; }
        else if (key == "userknownhostsfile")
        {
            // ssh -G emits unquoted Windows paths even when they contain spaces.
            // Each absolute path (or home/token prefix) begins another filename.
            for (const auto &part : QProcess::splitCommand(value))
            {
                if (result.files.isEmpty() || part == "none" || QDir::isAbsolutePath(part)
                    || part.startsWith("~/") || part.startsWith("~\\") || part.startsWith('%'))
                {
                    result.files << part;
                }
                else
                {
                    result.files.last() += ' ' + part;
                }
            }
        }
    }
    if (result.hostname.isEmpty() || result.port < 1 || result.port > 65535)
    {
        fail("OpenSSH 主机配置无效，未清除缓存");
    }
    // HostKeyAlias replaces the entire lookup name, including the port syntax.
    result.identifiers << (!alias.isEmpty() ? alias : result.port == 22 ? result.hostname
        : QString("[%1]:%2").arg(result.hostname).arg(result.port));
    QStringList files;
    for (auto path : result.files)
    {
        if (path == "none") { continue; }
        path.replace("%d", QDir::homePath());
        path.replace("%h", result.hostname);
        path.replace("%p", QString::number(result.port));
        path.replace("%n", originalHost);
        path.replace("%r", user);
        if (path.startsWith("~/") || path.startsWith("~\\")) { path = QDir::homePath() + path.mid(1); }
        if (path.contains('%') || path.contains("${") || QDir::isRelativePath(path))
        {
            fail("known_hosts 路径包含尚未解析的变量，未清除缓存");
        }
        files << QDir::cleanPath(path);
    }
    result.files = files;
    result.files.removeDuplicates();
    return result;
}

int SshHostCache::remove(const QString &path, const QStringList &identifiers)
{
    QFile original(path);
    if (!original.exists()) { return 0; }
    if (!original.open(QIODevice::ReadOnly)) { fail("无法读取主机指纹记录"); }
    const auto contents = original.readAll();
    const auto permissions = original.permissions();
    original.close();
    QByteArray updated;
    int removed = 0;
    const QRegularExpression fields(R"(^(\s*(?:@\S+\s+)?)(\S+)(\s+.*)$)");
    for (const auto &line : contents.split('\n'))
    {
        QByteArray next = line;
        const auto match = fields.match(QString::fromUtf8(line));
        if (match.hasMatch() && !line.trimmed().startsWith('#'))
        {
            const auto hosts = match.captured(2).toUtf8().split(',');
            QList<QByteArray> keep;
            for (const auto &entry : hosts)
            {
                if (hostMatches(entry, identifiers)) { ++removed; }
                else { keep << entry; }
            }
            if (keep.isEmpty()) { continue; }
            if (keep.size() != hosts.size())
            {
                next = match.captured(1).toUtf8() + keep.join(',') + match.captured(3).toUtf8();
            }
        }
        updated += next + '\n';
    }
    if (!removed) { return 0; }
    if (!contents.endsWith('\n') && updated.endsWith('\n')) { updated.chop(1); }
    else if (contents.endsWith('\n') && updated.endsWith("\n\n")) { updated.chop(1); }
    const auto backup = path + ".workbench-" + QUuid::createUuid().toString(QUuid::Id128) + ".bak";
    if (!original.open(QIODevice::ReadOnly) || original.readAll() != contents)
    {
        fail("主机指纹文件已被其他程序修改，请重新清除缓存");
    }
    original.close();
    if (!QFile::copy(path, backup)) { fail("无法备份主机指纹记录，未清除"); }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { fail("无法更新主机指纹记录"); }
    file.setPermissions(permissions);
    if (file.write(updated) != updated.size() || !file.commit()) { fail("无法保存主机指纹记录"); }
    return removed;
}

void SshAuthPrompt::answer(const QString &value, bool remember)
{
    if (!reply) { return; }
    auto complete = std::move(reply);
    complete(true, value, remember);
    emit finished();
}

void SshAuthPrompt::cancel()
{
    if (!reply) { return; }
    auto complete = std::move(reply);
    complete(false, {}, false);
    emit finished();
}

SshAuthSession::SshAuthSession(const Config &config, const QString &root, QObject *parent)
    : QObject(parent), token(QUuid::createUuid().toString(QUuid::Id128)),
      host(config.sshHost), account(config.key()), cacheDirectory(root)
{
    server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server.listen("SerialWorkbench-ssh-" + token)) { fail("无法创建 SSH 认证通道"); }
    connect(&server, &QLocalServer::newConnection, this, [this]
    {
        while (server.hasPendingConnections())
        {
            auto socket = server.nextPendingConnection();
            clients.insert(socket);
            auto buffer = std::make_shared<QByteArray>();
            auto handled = std::make_shared<bool>(false);
            connect(socket, &QLocalSocket::disconnected, this, [this, socket]
            {
                clients.remove(socket);
                socket->deleteLater();
            });
            QTimer::singleShot(300000, socket, [socket] { socket->abort(); });
            connect(socket, &QLocalSocket::readyRead, this, [this, socket, buffer, handled]
            {
                if (*handled) { return; }
                *buffer += socket->readAll();
                if (buffer->size() > 16384) { socket->abort(); return; }
                if (!buffer->contains('\n')) { return; }
                *handled = true;
                accept(socket, QJsonDocument::fromJson(buffer->left(buffer->indexOf('\n'))).object());
            });
        }
    });
}

SshAuthSession::~SshAuthSession()
{
    stop();
}

QProcessEnvironment SshAuthSession::environment() const
{
    auto result = QProcessEnvironment::systemEnvironment();
    const auto helper = QCoreApplication::applicationDirPath() + "/SerialWorkbenchSSHAskpass.exe";
    if (!QFile::exists(helper)) { fail("缺少 SSH 认证助手，请重新安装完整安装包"); }
    result.insert("SSH_ASKPASS", QDir::toNativeSeparators(helper));
    result.insert("SSH_ASKPASS_REQUIRE", "force");
    result.insert("SW_SSH_AUTH_SERVER", server.serverName());
    result.insert("SW_SSH_AUTH_TOKEN", token);
    return result;
}

void SshAuthSession::stop()
{
    server.close();
    for (auto socket : clients.values()) { socket->abort(); }
    clients.clear();
}

void SshAuthSession::accept(QLocalSocket *socket, const QJsonObject &request)
{
    if (request["token"].toString() != token) { socket->abort(); return; }
    const auto message = request["prompt"].toString();
    const auto hint = request["hint"].toString();
    if (hint == "none")
    {
        socket->write("{\"accepted\":true,\"answer\":\"\"}\n");
        socket->disconnectFromServer();
        return;
    }
    auto prompt = new SshAuthPrompt(socket);
    prompt->host = host;
    prompt->message = message;
    prompt->confirmation = hint == "confirm" || message.contains("yes/no", Qt::CaseInsensitive);
    prompt->canRemember = !prompt->confirmation
        && (message.contains("password", Qt::CaseInsensitive) || message.contains("passphrase", Qt::CaseInsensitive));
    const auto key = account + '\n' + message;
    prompt->retry = asked.contains(key);
    asked.insert(key);
    prompt->reply = [this, socket, prompt, key](bool accepted, const QString &answer, bool remember)
    {
        if (accepted && remember && prompt->canRemember)
        {
            try { SshCredentialCache::write(cacheDirectory, host, key, answer); }
            catch (const std::exception &error) { prompt->cacheError = QString::fromUtf8(error.what()); }
        }
        const QJsonObject response {{"accepted", accepted}, {"answer", answer}};
        socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n');
        socket->disconnectFromServer();
        if (!accepted) { emit cancelled(); }
    };
    connect(socket, &QLocalSocket::disconnected, prompt, &SshAuthPrompt::finished);
    try
    {
        if (prompt->canRemember && prompt->retry)
        {
            SshCredentialCache::remove(cacheDirectory, host, key);
        }
        else if (prompt->canRemember)
        {
            const auto cached = SshCredentialCache::read(cacheDirectory, host, key);
            if (!cached.isEmpty()) { prompt->answer(cached); return; }
        }
    }
    catch (const std::exception &) { /* A cache failure must still allow a fresh login. */ }
    emit promptRequested(prompt);
}
}
