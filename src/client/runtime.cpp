// SPDX-License-Identifier: Apache-2.0
#include "runtime.h"
#include "workbench_version.h"
#include <iostream>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace sw
{
#ifdef Q_OS_WIN
bool needsIndependentRuntime();
void startIndependentRuntime(const QString &executable, const QStringList &arguments);
#endif
namespace
{
void failure(const QString &message)
{
    throw std::runtime_error(message.toUtf8().constData());
}
}

QString siblingExecutable(const QString &name)
{
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath() + "/" + name + ".exe";
#else
    return QCoreApplication::applicationDirPath() + "/" + name;
#endif
}

bool maintenanceInProgress()
{
    return QFileInfo::exists(QCoreApplication::applicationDirPath() + "/.workbench-maintenance");
}

QString defaultSettingsPath()
{
    const auto directory = QCoreApplication::applicationDirPath();
    QFile installation(directory + "/installation.json");
    if (installation.open(QIODevice::ReadOnly)
        && QJsonDocument::fromJson(installation.readAll()).object()["distribution"] == "installed")
    {
        return qEnvironmentVariable("LOCALAPPDATA", QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
            + "/SerialWorkbench/settings.json";
    }
    return directory + "/settings.json";
}

QString instanceName(const QString &settings)
{
    auto path = QDir::cleanPath(QFileInfo(settings).absoluteFilePath());
    const QFileInfo info(path);
    if (info.exists()) { path = info.canonicalFilePath(); }
#ifdef Q_OS_WIN
    path = path.toLower();
#endif
    const auto key = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex().left(24);
    return "SerialWorkbenchRuntime-" + qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER")) + "-" + key;
}

QJsonObject instanceRequest(const QString &settings, const QString &command)
{
    QLocalSocket socket;
    socket.connectToServer(instanceName(settings));
    if (!socket.waitForConnected(200)) { return {}; }
    socket.write(QJsonDocument(QJsonObject {{"command", command}}).toJson(QJsonDocument::Compact) + '\n');
    if (!socket.waitForBytesWritten(1000)) { failure("Cannot contact the running workbench"); }
    QByteArray data;
    QElapsedTimer timer;
    timer.start();
    while (!data.contains('\n') && timer.elapsed() < 5000)
    {
        socket.waitForReadyRead(100);
        data += socket.readAll();
        if (data.size() > 16384) { failure("Invalid instance response"); }
        if (socket.state() == QLocalSocket::UnconnectedState) { break; }
    }
    const auto result = QJsonDocument::fromJson(data).object();
    if (result["application"] != "SerialWorkbench") { failure("The running instance did not respond; no duplicate was started"); }
    if (result.contains("error")) { failure(result["error"].toString()); }
    return result;
}

QJsonObject ensureInstance(const QString &settings, bool allowControl)
{
    if (maintenanceInProgress()) { failure("Serial Workbench is being updated. Reconnect MCP after setup finishes."); }
    auto existing = instanceRequest(settings, "status");
    if (!existing.isEmpty())
    {
        if (allowControl && !existing["allow_ai_control"].toBool())
        {
            failure("An instance is already running with AI control disabled. Enable it in its settings; it was not changed.");
        }
        return existing;
    }
    const auto executable = siblingExecutable("SerialWorkbench");
    if (!QFileInfo::exists(executable)) { failure("SerialWorkbench executable is missing beside this tool"); }
    QStringList arguments {"--headless", "--settings", QFileInfo(settings).absoluteFilePath()};
    if (allowControl) { arguments << "--allow-control"; }
    QProcess process;
    process.setProgram(executable);
    process.setArguments(arguments);
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments)
    {
        // A long-lived GUI subprocess must not keep a CLI caller's pipes open.
        arguments->inheritHandles = false;
        arguments->flags |= CREATE_NO_WINDOW;
        arguments->startupInfo->dwFlags &= ~STARTF_USESTDHANDLES;
    });
#endif
#ifdef Q_OS_WIN
    if (needsIndependentRuntime()) { startIndependentRuntime(executable, arguments); }
    else
#endif
    if (!process.startDetached()) { failure("Cannot start the background workbench"); }
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 15000)
    {
        existing = instanceRequest(settings, "status");
        if (!existing.isEmpty()) { return existing; }
        QThread::msleep(100);
    }
    failure("Background startup timed out. Check the settings path and available local ports.");
    return {};
}

McpClient::McpClient(const QString &url) : endpoint(url)
{
    if (!endpoint.isValid() || (endpoint.scheme() != "http" && endpoint.scheme() != "https")
        || endpoint.host().isEmpty() || !endpoint.userInfo().isEmpty())
    {
        failure("Invalid MCP HTTP URL");
    }
}

QJsonObject McpClient::request(const QJsonObject &rpc)
{
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json, text/event-stream");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    if (!session.isEmpty()) { request.setRawHeader("Mcp-Session-Id", session); }
    auto reply = manager.post(request, QJsonDocument(rpc).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeout.start(20000);
    loop.exec();
    if (reply->hasRawHeader("Mcp-Session-Id")) { session = reply->rawHeader("Mcp-Session-Id"); }
    const auto bytes = reply->readAll();
    const auto error = reply->error();
    const auto message = reply->errorString();
    delete reply;
    if (error != QNetworkReply::NoError) { failure("MCP request failed: " + message); }
    if (!rpc.contains("id")) { return {}; }
    const auto result = QJsonDocument::fromJson(bytes).object();
    if (result.isEmpty() || result["id"] != rpc["id"]) { failure("Invalid MCP JSON response"); }
    return result;
}

void McpClient::initialize()
{
    const auto result = request({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", QJsonObject {{"protocolVersion", "2025-03-26"}, {"capabilities", QJsonObject {}},
            {"clientInfo", QJsonObject {{"name", "SerialWorkbenchCLI"}, {"version", SERIAL_WORKBENCH_VERSION}}}}}});
    if (result.contains("error")) { failure("MCP initialization rejected"); }
    request({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
}

void McpClient::close()
{
    if (session.isEmpty()) { return; }
    QNetworkRequest request(endpoint);
    request.setTransferTimeout(2000);
    request.setRawHeader("Mcp-Session-Id", session);
    auto reply = manager.deleteResource(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    delete reply;
    session.clear();
}

int runStdio(const QString &settings, const QString &url)
{
    if (maintenanceInProgress()) { failure("Serial Workbench is being updated. Reconnect MCP after setup finishes."); }
    // Explicit URLs never launch local processes. Default mode discovers the
    // profile's actual port, including a GUI port changed after installation.
    McpClient client(url.isEmpty() ? ensureInstance(settings)["mcp_url"].toString() : url);
    std::string line;
    while (std::getline(std::cin, line))
    {
        const auto rpc = QJsonDocument::fromJson(QByteArray::fromStdString(line)).object();
        if (rpc.isEmpty()) { continue; }
        QJsonObject result;
        try { result = client.request(rpc); }
        catch (const std::exception &error)
        {
            result = {{"jsonrpc", "2.0"}, {"id", rpc["id"]},
                {"error", QJsonObject {{"code", -32000}, {"message", QString::fromUtf8(error.what())}}}};
        }
        if (rpc.contains("id"))
        {
            std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
        }
    }
    client.close();
    return 0;
}
}
