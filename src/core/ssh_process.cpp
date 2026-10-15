// SPDX-License-Identifier: Apache-2.0
#include "ssh_process.h"

#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#ifdef Q_OS_WIN
#include <windows.h>
#include <cstring>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#endif

namespace sw
{
struct SshProcess::Impl
{
    QString error;
#ifdef Q_OS_WIN
    using CreateConsole = HRESULT (WINAPI *)(COORD, HANDLE, HANDLE, DWORD, HANDLE *);
    using ResizeConsole = HRESULT (WINAPI *)(HANDLE, COORD);
    using CloseConsole = void (WINAPI *)(HANDLE);
    CreateConsole createConsole = nullptr;
    ResizeConsole resizeConsole = nullptr;
    CloseConsole closeConsole = nullptr;
    HANDLE console = nullptr;
    HANDLE process = nullptr;
    HANDLE input = nullptr;
    HANDLE output = nullptr;
    std::thread reader;
    std::thread writer;
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<QByteArray> pending;
    qint64 pendingBytes = 0;
    bool stopping = false;
#else
    QProcess *process = nullptr;
#endif
};

SshProcess::SshProcess(QObject *parent) : QObject(parent), impl(std::make_unique<Impl>())
{
#ifdef Q_OS_WIN
    poll.setInterval(20);
    connect(&poll, &QTimer::timeout, this, [this]
    {
        if (!impl->process || WaitForSingleObject(impl->process, 0) != WAIT_OBJECT_0)
        {
            return;
        }
        DWORD code = 0;
        GetExitCodeProcess(impl->process, &code);
        stop();
        // Deliver all queued output before the completion notification.
        QMetaObject::invokeMethod(this, [this, code] { emit finished(int(code)); }, Qt::QueuedConnection);
    });
#endif
}

SshProcess::~SshProcess()
{
    stop();
}

#ifdef Q_OS_WIN
namespace
{
template<typename Function>
Function procedure(HMODULE module, const char *name)
{
    const auto address = GetProcAddress(module, name);
    Function result = nullptr;
    static_assert(sizeof(result) == sizeof(address));
    std::memcpy(&result, &address, sizeof(result));
    return result;
}

QString quoteArgument(const QString &value)
{
    QString result = "\"";
    int slashes = 0;
    for (const QChar character : value)
    {
        if (character == '\\')
        {
            ++slashes;
            continue;
        }
        result += QString(slashes * (character == '"' ? 2 : 1), '\\');
        slashes = 0;
        if (character == '"') { result += '\\'; }
        result += character;
    }
    return result + QString(slashes * 2, '\\') + '"';
}
}
#endif

bool SshProcess::start(const QString &program, const QStringList &arguments, const QSize &size,
    QProcessEnvironment environment)
{
    stop();
    impl->error.clear();
#ifdef Q_OS_WIN
    const auto kernel = GetModuleHandleW(L"kernel32.dll");
    impl->createConsole = procedure<Impl::CreateConsole>(kernel, "CreatePseudoConsole");
    impl->resizeConsole = procedure<Impl::ResizeConsole>(kernel, "ResizePseudoConsole");
    impl->closeConsole = procedure<Impl::CloseConsole>(kernel, "ClosePseudoConsole");
    if (!impl->createConsole || !impl->resizeConsole || !impl->closeConsole)
    {
        impl->error = "SSH 交互终端需要 Windows 10 1809 或更新版本";
        return false;
    }
    HANDLE inputRead = nullptr;
    HANDLE outputWrite = nullptr;
    if (!CreatePipe(&inputRead, &impl->input, nullptr, 0)
        || !CreatePipe(&impl->output, &outputWrite, nullptr, 0))
    {
        if (inputRead) { CloseHandle(inputRead); }
        if (outputWrite) { CloseHandle(outputWrite); }
        impl->error = "无法创建 SSH 终端管道";
        stop();
        return false;
    }

    COORD dimensions {SHORT(qBound(2, size.width(), 500)), SHORT(qBound(1, size.height(), 200))};
    const HRESULT created = impl->createConsole(dimensions, inputRead, outputWrite, 0, &impl->console);
    if (FAILED(created))
    {
        CloseHandle(inputRead);
        CloseHandle(outputWrite);
        impl->error = "无法创建 Windows 伪终端（需要 Windows 10 1809 或更新版本）";
        stop();
        return false;
    }

    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    QByteArray attributes(int(bytes), '\0');
    STARTUPINFOEXW startup {};
    startup.StartupInfo.cb = sizeof(startup);
    // NULL handles ask ConPTY to supply its console instead of duplicating the
    // parent's redirected stdout/stdin (including QtTest and GUI launchers).
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    const bool initialized = InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &bytes);
    constexpr DWORD_PTR PseudoConsoleAttribute = 0x00020016;
    const bool attached = initialized && UpdateProcThreadAttribute(startup.lpAttributeList, 0,
        PseudoConsoleAttribute, impl->console, sizeof(impl->console), nullptr, nullptr);

    const auto executable = QStandardPaths::findExecutable(program);
    QStringList command {quoteArgument(QDir::toNativeSeparators(executable.isEmpty() ? program : executable))};
    for (const auto &argument : arguments) { command.append(quoteArgument(argument)); }
    std::wstring commandLine = command.join(' ').toStdWString();
    environment.insert("TERM", "xterm-256color");
    auto environmentEntries = environment.toStringList();
    environmentEntries.sort(Qt::CaseInsensitive);
    QString environmentBlock = environmentEntries.join(QChar('\0')) + QChar('\0') + QChar('\0');
    PROCESS_INFORMATION processInfo {};
    const bool started = attached && CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, environmentBlock.data(), nullptr,
        &startup.StartupInfo, &processInfo);
    const DWORD lastError = GetLastError();
    if (initialized) { DeleteProcThreadAttributeList(startup.lpAttributeList); }
    CloseHandle(inputRead);
    CloseHandle(outputWrite);
    if (!started)
    {
        impl->error = QString("无法启动 SSH 终端，Windows 错误 %1").arg(lastError);
        stop();
        return false;
    }

    CloseHandle(processInfo.hThread);
    impl->process = processInfo.hProcess;
    impl->stopping = false;
    impl->reader = std::thread([this]
    {
        char buffer[16384];
        DWORD count = 0;
        while (ReadFile(impl->output, buffer, sizeof(buffer), &count, nullptr) && count)
        {
            const QByteArray data(buffer, int(count));
            QMetaObject::invokeMethod(this, [this, data] { emit output(data); }, Qt::QueuedConnection);
        }
    });
    impl->writer = std::thread([this]
    {
        for (;;)
        {
            QByteArray data;
            {
                std::unique_lock<std::mutex> lock(impl->mutex);
                impl->wake.wait(lock, [this] { return impl->stopping || !impl->pending.empty(); });
                if (impl->stopping) { return; }
                data = std::move(impl->pending.front());
                impl->pending.pop_front();
                impl->pendingBytes -= data.size();
            }
            qsizetype offset = 0;
            while (offset < data.size())
            {
                DWORD count = 0;
                if (!WriteFile(impl->input, data.constData() + offset, DWORD(data.size() - offset), &count, nullptr) || !count)
                {
                    return;
                }
                offset += count;
            }
        }
    });
    poll.start();
    return true;
#else
    impl->process = new QProcess(this);
    environment.insert("TERM", "xterm-256color");
    environment.insert("COLUMNS", QString::number(size.width()));
    environment.insert("LINES", QString::number(size.height()));
    impl->process->setProcessEnvironment(environment);
    impl->process->setProcessChannelMode(QProcess::MergedChannels);
    connect(impl->process, &QProcess::readyRead, this, [this] { emit output(impl->process->readAll()); });
    connect(impl->process, &QProcess::errorOccurred, this, [this] { emit failed(impl->process->errorString()); });
    connect(impl->process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [this](int code, QProcess::ExitStatus) { emit finished(code); });
    impl->process->start(program, arguments);
    return true;
#endif
}

qint64 SshProcess::write(const QByteArray &data)
{
#ifdef Q_OS_WIN
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (!impl->input || impl->stopping || impl->pendingBytes + data.size() > 1024 * 1024)
    {
        return -1;
    }
    impl->pending.push_back(data);
    impl->pendingBytes += data.size();
    impl->wake.notify_one();
    return data.size();
#else
    return impl->process ? impl->process->write(data) : -1;
#endif
}

void SshProcess::resize(const QSize &size)
{
#ifdef Q_OS_WIN
    if (impl->console)
    {
        impl->resizeConsole(impl->console,
            {SHORT(qBound(2, size.width(), 500)), SHORT(qBound(1, size.height(), 200))});
    }
#else
    Q_UNUSED(size)
#endif
}

void SshProcess::stop()
{
    poll.stop();
#ifdef Q_OS_WIN
    {
        std::lock_guard<std::mutex> lock(impl->mutex);
        impl->stopping = true;
        impl->pending.clear();
        impl->pendingBytes = 0;
    }
    impl->wake.notify_all();
    if (impl->process)
    {
        if (WaitForSingleObject(impl->process, 0) != WAIT_OBJECT_0) { TerminateProcess(impl->process, 0); }
        CloseHandle(impl->process);
        impl->process = nullptr;
    }
    // Keep the reader draining while ClosePseudoConsole writes its final frame.
    if (impl->console) { impl->closeConsole(impl->console); }
    impl->console = nullptr;
    if (impl->writer.joinable()) { impl->writer.join(); }
    if (impl->reader.joinable()) { impl->reader.join(); }
    if (impl->input) { CloseHandle(impl->input); }
    if (impl->output) { CloseHandle(impl->output); }
    impl->input = impl->output = nullptr;
#else
    if (impl->process)
    {
        impl->process->disconnect(this);
        impl->process->kill();
        impl->process->waitForFinished(300);
        delete impl->process;
        impl->process = nullptr;
    }
#endif
}

QString SshProcess::errorString() const
{
    return impl->error;
}
}
