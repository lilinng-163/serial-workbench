// SPDX-License-Identifier: Apache-2.0
#include "openocd.h"
#include "core.h"
#include <cmath>
#include <algorithm>
#include <QTcpServer>
#include <string>
#include <limits>
#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif

namespace sw
{
namespace
{
constexpr qsizetype MaxLogBytes = 256 * 1024;

QJsonObject defaults()
{
    const auto found = QStandardPaths::findExecutable("openocd");
    return {
        {"executable", found.isEmpty() ? "openocd" : found},
        {"scripts_dir", ""},
        {"mode", "target"},
        {"board", ""},
        {"interface", ""},
        {"target", ""},
        {"transport", "default"},
        {"speed_khz", 0},
        {"adapter_serial", ""},
        {"reset_config", "board"},
        {"strategy", "program"},
        {"startup", "default"},
        {"verify", true},
        {"reset_run", true},
        {"timeout_ms", 120000},
        {"images", QJsonArray {}},
        {"rtt_mode", "managed"},
        {"rtt_address", ""},
        {"rtt_size", "0x100"},
        {"rtt_id", "SEGGER RTT"},
        {"rtt_channel", 0},
        {"rtt_poll_ms", 10},
        {"rtt_startup", "attach"},
        {"rtt_host", "127.0.0.1"},
        {"rtt_port", 0},
        {"rtt_gdb_port", 0}
    };
}

void checkChoice(const QJsonObject &object, const QString &key, const QStringList &values)
{
    if (!values.contains(object[key].toString())) { fail("OpenOCD " + key + " 选项无效"); }
}

QJsonObject normalized(const QJsonObject &base, const QJsonObject &patch)
{
    auto result = base;
    for (auto it = patch.begin(); it != patch.end(); ++it)
    {
        if (!base.contains(it.key())) { fail("未知 OpenOCD 设置：" + it.key()); }
        if (it->type() != base[it.key()].type()) { fail("OpenOCD 设置类型不正确：" + it.key()); }
        if (it->isString() && (it->toString().size() > 4096 || it->toString().contains(QChar::Null)))
        {
            fail("OpenOCD 设置文本过长或包含空字符");
        }
        result[it.key()] = *it;
    }
    checkChoice(result, "mode", {"board", "target"});
    checkChoice(result, "transport", {"default", "swd", "jtag", "hla_swd", "hla_jtag", "dapdirect_swd", "dapdirect_jtag"});
    checkChoice(result, "reset_config", {"board", "none", "srst_only", "connect_assert_srst"});
    checkChoice(result, "strategy", {"program", "esp"});
    checkChoice(result, "startup", {"default", "reset_init", "reset_halt", "halt"});
    checkChoice(result, "rtt_mode", {"managed", "external"});
    checkChoice(result, "rtt_startup", {"attach", "resume", "reset_run"});
    const QMap<QString, QPair<int, int>> rttRanges {
        {"rtt_channel", {0, 255}}, {"rtt_poll_ms", {1, 1000}},
        {"rtt_port", {0, 65535}}, {"rtt_gdb_port", {0, 65535}}
    };
    for (auto it = rttRanges.begin(); it != rttRanges.end(); ++it)
    {
        const double value = result[it.key()].toDouble();
        if (!std::isfinite(value) || value != std::floor(value) || value < it->first || value > it->second)
        {
            fail("RTT 参数范围不正确：" + it.key());
        }
    }
    if (result["rtt_id"].toString().isEmpty() || result["rtt_id"].toString().toUtf8().size() > 15
        || result["rtt_id"].toString().contains('\n') || result["rtt_id"].toString().contains('\r'))
    {
        fail("RTT 标识应为 1..15 字节的单行文本");
    }
    for (const auto &key : {"speed_khz", "timeout_ms"})
    {
        const double value = result[key].toDouble();
        const int minimum = QString(key) == "timeout_ms" ? 5000 : 0;
        const int maximum = QString(key) == "timeout_ms" ? 600000 : 100000;
        if (!std::isfinite(value) || value != std::floor(value) || value < minimum || value > maximum)
        {
            fail(QString("OpenOCD %1 范围 %2..%3").arg(key).arg(minimum).arg(maximum));
        }
    }
    const auto images = result["images"].toArray();
    if (images.size() > 16) { fail("OpenOCD 最多支持 16 个固件分段"); }
    for (const auto &value : images)
    {
        if (!value.isObject()) { fail("固件分段必须是对象"); }
        const auto image = value.toObject();
        for (auto it = image.begin(); it != image.end(); ++it)
        {
            if ((it.key() != "path" && it.key() != "address") || !it->isString()
                || it->toString().size() > 4096 || it->toString().contains(QChar::Null))
            {
                fail("固件分段只接受字符串 path 和 address");
            }
        }
    }
    return result;
}

QString executablePath(const QString &value)
{
    const auto path = QStandardPaths::findExecutable(value.trimmed());
    if (path.isEmpty()) { fail("未找到 OpenOCD 可执行文件，请选择 openocd 程序"); }
#ifdef Q_OS_WIN
    if (QFileInfo(path).suffix().compare("exe", Qt::CaseInsensitive) != 0)
    {
        fail("请选择 OpenOCD .exe，不支持批处理或 shell 脚本");
    }
#endif
    return QFileInfo(path).absoluteFilePath();
}

QString scriptsPath(const QString &executable, const QString &configured)
{
    if (!configured.trimmed().isEmpty())
    {
        const QFileInfo info(configured.trimmed());
        if (!info.isDir()) { fail("OpenOCD scripts 目录不存在"); }
        return info.absoluteFilePath();
    }
    const QDir root(QFileInfo(executable).absolutePath() + "/..");
    for (const auto &relative : {"share/openocd/scripts", "openocd/scripts", "scripts"})
    {
        const auto path = root.absoluteFilePath(relative);
        if (QFileInfo(path).isDir()) { return QDir::cleanPath(path); }
    }
    fail("未自动找到 scripts 目录，请选择所用 OpenOCD 发行包的 scripts 文件夹");
}

bool hasNonAscii(const QString &text)
{
    return std::any_of(text.begin(), text.end(), [](QChar character) { return character.unicode() > 127; });
}

QString nativeToolPath(const QString &path)
{
#ifdef Q_OS_WIN
    // Some Windows OpenOCD/Jim builds use ANSI fopen, not UTF-8 paths. An
    // existing 8.3 alias lets script libraries retain their relative includes.
    if (hasNonAscii(path))
    {
        const auto native = QDir::toNativeSeparators(path);
        const auto size = GetShortPathNameW(reinterpret_cast<LPCWSTR>(native.utf16()), nullptr, 0);
        if (size > 0)
        {
            std::wstring shortened(size, L'\0');
            const auto written = GetShortPathNameW(reinterpret_cast<LPCWSTR>(native.utf16()), shortened.data(), size);
            if (written > 0 && written < size)
            {
                const auto result = QString::fromWCharArray(shortened.data(), written);
                if (!hasNonAscii(result)) { return QDir::fromNativeSeparators(result); }
            }
        }
        fail("当前 OpenOCD 不兼容该配置目录的中文路径，且系统没有可用的短路径。请将 OpenOCD / .cfg 放到英文目录；中文固件文件会自动暂存兼容。路径：" + path);
    }
#endif
    return path;
}

// Tcl double-quoted words still substitute $, [] and backslashes. Escape all
// substitution syntax, including newlines; a filename can never become code.
QString tclWord(const QString &value)
{
    QString result = "\"";
    for (const auto c : value)
    {
        if (c == '\n') { result += "\\n"; }
        else if (c == '\r') { result += "\\r"; }
        else if (c == '\t') { result += "\\t"; }
        else
        {
            if (c == '\\' || c == '"' || c == '$' || c == '[' || c == ']' || c == '{' || c == '}') { result += '\\'; }
            result += c;
        }
    }
    return result + '"';
}

QString configPath(const QString &root, const QString &value, const QString &kind)
{
    if (value.trimmed().isEmpty()) { fail("请选择 OpenOCD " + kind + " 配置"); }
    const auto path = QDir::isAbsolutePath(value) ? value : QDir(root).absoluteFilePath(value);
    const QFileInfo info(path);
    if (!info.isFile() || info.suffix().compare("cfg", Qt::CaseInsensitive) != 0)
    {
        fail("配置文件不存在或不是 .cfg：" + path);
    }
    return nativeToolPath(QDir::fromNativeSeparators(info.absoluteFilePath()));
}

QString hexAddress(const QString &text)
{
    const auto trimmed = text.trimmed();
    static const QRegularExpression pattern("^(?:0[xX][0-9a-fA-F]{1,16}|[0-9]{1,20})$");
    bool ok = false;
    const auto number = trimmed.toULongLong(&ok, trimmed.startsWith("0x", Qt::CaseInsensitive) ? 16 : 10);
    if (!pattern.match(trimmed).hasMatch() || !ok) { fail("地址和范围应为 0x 开头的十六进制数或十进制数"); }
    return "0x" + QString::number(number, 16);
}

QStringList initializeCommands(const QString &startup, bool connectionOnly)
{
    const auto reset = startup == "default" ? (connectionOnly ? "halt" : "reset init")
        : startup == "reset_init" ? "reset init" : startup == "reset_halt" ? "reset halt" : "halt";
    return {"init", reset};
}
}

OpenOcdService::OpenOcdService(QObject *parent) : QObject(parent), config(defaults())
{
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments)
    {
        arguments->flags |= CREATE_NO_WINDOW;
    });
#endif
    initializeRtt();
    process.setProcessChannelMode(QProcess::MergedChannels);
    deadline.setSingleShot(true);
    killTimer.setSingleShot(true);
    connect(&process, &QProcess::readyReadStandardOutput, this, &OpenOcdService::readOutput);
    connect(&process, &QProcess::finished, this, &OpenOcdService::finish);
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError processError)
    {
        if (processError == QProcess::FailedToStart && running)
        {
            error = "无法启动 OpenOCD：" + process.errorString();
            finish(-1, QProcess::CrashExit);
        }
    });
    connect(&deadline, &QTimer::timeout, this, [this] { stop("timed_out"); });
    connect(&killTimer, &QTimer::timeout, this, [this]
    {
        if (process.state() != QProcess::NotRunning) { process.kill(); }
    });
}

OpenOcdService::~OpenOcdService()
{
    closeRtt();
    disconnect(&process, nullptr, this, nullptr);
    if (process.state() != QProcess::NotRunning)
    {
        process.kill();
        process.waitForFinished(1500);
    }
}

void OpenOcdService::restore(const QJsonObject &saved)
{
    if (busy()) { fail("OpenOCD 运行期间不能恢复配置"); }
    config = normalized(defaults(), saved);
    ++configRevision;
    emit configurationChanged();
}

void OpenOcdService::configure(const QJsonObject &patch, qint64 expectedRevision)
{
    if (busy()) { fail("OpenOCD 正在运行，请完成或取消后再修改配置"); }
    if (expectedRevision != configRevision) { fail("OpenOCD 配置已更改，请重新读取 revision"); }
    const auto next = normalized(config, patch);
    if (next == config) { return; }
    config = next;
    ++configRevision;
    emit configurationChanged();
}

QJsonObject OpenOcdService::exportConfiguration() const
{
    return {
        {"format", "serial-workbench.openocd"},
        {"version", 1},
        {"title", title},
        {"configuration", config}
    };
}

void OpenOcdService::importConfiguration(const QJsonObject &document, qint64 expectedRevision)
{
    if (busy()) { fail("OpenOCD 正在运行，请完成或取消后再导入配置"); }
    if (expectedRevision != configRevision) { fail("OpenOCD 配置已更改，请重新读取 revision"); }

    for (auto field = document.begin(); field != document.end(); ++field)
    {
        if (!QStringList {"format", "version", "title", "configuration"}.contains(field.key()))
        {
            fail("未知 OpenOCD 配置文件字段：" + field.key());
        }
    }
    if (document["format"] != "serial-workbench.openocd" || document["version"] != 1
        || !document["configuration"].isObject())
    {
        fail("不支持的 OpenOCD 配置格式或版本");
    }

    auto nextTitle = title;
    if (document.contains("title"))
    {
        const auto value = document["title"];
        if (!value.isString() || value.toString().trimmed().isEmpty() || value.toString().size() > 200
            || value.toString().contains(QChar::Null))
        {
            fail("OpenOCD 配置名称应为 1..200 个字符");
        }
        nextTitle = value.toString().trimmed();
    }

    // Import replaces the configuration. Missing fields use defaults rather
    // than inheriting another board's firmware or probe settings.
    const auto next = normalized(defaults(), document["configuration"].toObject());
    if (next == config && nextTitle == title) { return; }

    config = next;
    title = nextTitle;
    ++configRevision;
    emit configurationChanged();
}

QJsonObject OpenOcdService::catalog(const QString &executable, const QString &scriptsDirectory) const
{
    QJsonObject result {{"boards", QJsonArray {}}, {"interfaces", QJsonArray {}}, {"targets", QJsonArray {}},
        {"truncated", false}, {"error", ""}};
    try
    {
        const auto program = executablePath(executable.isEmpty() ? config["executable"].toString() : executable);
        result["executable"] = program;
        const auto root = scriptsPath(program, scriptsDirectory.isEmpty() ? config["scripts_dir"].toString() : scriptsDirectory);
        result["scripts_dir"] = root;
        int count = 0;
        for (const auto &group : {"board", "interface", "target"})
        {
            QStringList names;
            QDirIterator files(QDir(root).filePath(group), {"*.cfg"}, QDir::Files, QDirIterator::Subdirectories);
            while (files.hasNext())
            {
                const auto file = files.next();
                if (++count > 4096)
                {
                    result["truncated"] = true;
                    break;
                }
                names.append(QDir(root).relativeFilePath(file));
            }
            names.sort(Qt::CaseInsensitive);
            result[QString(group) + "s"] = QJsonArray::fromStringList(names);
        }
    }
    catch (const std::exception &exception)
    {
        result["error"] = QString::fromUtf8(exception.what());
    }
    return result;
}

QJsonObject OpenOcdService::preview(const QString &requested) const
{
    if (!QStringList {"version", "connect", "flash", "rtt"}.contains(requested)) { fail("OpenOCD operation 应为 version、connect、flash 或 rtt"); }
    if (requested == "rtt" && config["rtt_mode"] == "external")
    {
        if (config["rtt_host"].toString().trimmed().isEmpty() || config["rtt_port"].toInt() == 0)
        {
            fail("接入已有 RTT 服务需要填写主机和端口（1..65535）");
        }
        return {{"view_id", viewId}, {"operation", requested}, {"revision", configRevision},
            {"writes_flash", false}, {"touches_hardware", false}, {"external", true},
            {"script", "只连接已有 RTT TCP 服务，不启动 OpenOCD、不发送数据、不复位目标。"}};
    }
    const auto program = executablePath(config["executable"].toString());
    QJsonObject result {{"view_id", viewId}, {"executable", program}, {"operation", requested}, {"revision", configRevision},
        {"writes_flash", requested == "flash"}, {"touches_hardware", requested != "version"}};
    if (requested == "version")
    {
        result["arguments"] = QJsonArray {"--version"};
        result["script"] = "";
        return result;
    }
    const auto root = scriptsPath(program, config["scripts_dir"].toString());
    const bool rtt = requested == "rtt";
    const int gdbPort = rtt ? config["rtt_gdb_port"].toInt() : 0;
    QStringList setup {"bindto 127.0.0.1", "gdb_port " + (gdbPort ? QString::number(gdbPort) : "disabled"),
        "tcl_port disabled", "telnet_port disabled"};
    const auto source = [&](const QString &kind)
    {
        return "source " + tclWord(configPath(root, config[kind].toString(), kind));
    };
    if (config["mode"] == "board") { setup.append(source("board")); }
    else
    {
        setup.append(source("interface"));
        if (config["transport"] != "default") { setup.append("transport select " + config["transport"].toString()); }
        setup.append(source("target"));
    }
    if (config["speed_khz"].toInt() > 0) { setup.append("adapter speed " + QString::number(config["speed_khz"].toInt())); }
    if (!config["adapter_serial"].toString().trimmed().isEmpty())
    {
        setup.append("adapter serial " + tclWord(config["adapter_serial"].toString().trimmed()));
    }
    const auto wiring = config["reset_config"].toString();
    if (wiring != "board")
    {
        setup.append("reset_config " + (wiring == "connect_assert_srst" ? "srst_only srst_nogate connect_assert_srst" : wiring));
    }
    QStringList commands;
    QJsonArray stagedImages;
    const auto startup = config["startup"].toString();
    if (rtt)
    {
        const auto address = hexAddress(config["rtt_address"].toString());
        const auto size = hexAddress(config["rtt_size"].toString());
        const auto length = size.mid(2).toULongLong(nullptr, 16);
        const auto first = address.mid(2).toULongLong(nullptr, 16);
        if (length == 0 || length > 16 * 1024 * 1024 || first > std::numeric_limits<quint64>::max() - length)
        {
            fail("RTT 搜索范围应为 1 字节到 16 MiB，且不能超出地址空间");
        }
        const int port = config["rtt_port"].toInt();
        if (port && port == gdbPort) { fail("RTT 与 GDB 端口不能相同"); }
        commands.append("init");
        if (config["rtt_startup"] == "reset_run") { commands.append({"reset run", "sleep 500"}); }
        else if (config["rtt_startup"] == "resume") { commands.append("resume"); }
        commands.append("rtt setup " + address + " " + size + " " + tclWord(config["rtt_id"].toString()));
        commands.append("rtt polling_interval " + QString::number(config["rtt_poll_ms"].toInt()));
        commands.append("rtt start");
        commands.append("rtt server start " + (port ? QString::number(port) : QString("@RTT_PORT@"))
            + " " + QString::number(config["rtt_channel"].toInt()));
        result["persistent"] = true;
        result["gdb_port"] = gdbPort;
    }
    else if (requested == "connect") { commands = initializeCommands(startup, true); }
    else
    {
        const auto images = config["images"].toArray();
        const bool esp = config["strategy"] == "esp";
        if (images.isEmpty()) { fail("请添加要烧录的固件"); }
        if (!esp && images.size() != 1) { fail("通用烧录一次使用一个 ELF / HEX / S19 / BIN；多个 BIN 分段请使用 Espressif 流程"); }
        if (esp && startup != "default") { fail("Espressif 流程由 program_esp 处理初始化，请选择推荐流程"); }
        if (esp && config["mode"] == "target"
            && (config["transport"] == "swd" || config["transport"] == "hla_swd" || config["transport"] == "dapdirect_swd"))
        {
            fail("Espressif 流程使用 JTAG，请选择配置默认或 JTAG");
        }
        if (esp)
        {
            commands.append("if {[llength [info commands program_esp]] == 0} {error \"Selected OpenOCD has no program_esp; use Espressif OpenOCD and matching configs\"}");
        }
        else if (startup != "default") { commands = initializeCommands(startup, false); }
        for (qsizetype index = 0; index < images.size(); ++index)
        {
            const auto image = images[index].toObject();
            const QFileInfo file(image["path"].toString());
            if (image["path"].toString().isEmpty() || !file.isFile() || file.size() <= 0)
            {
                fail("固件文件不存在或为空：" + image["path"].toString());
            }
            const auto suffix = file.suffix().toLower();
            if (!QStringList {"elf", "hex", "ihex", "s19", "srec", "bin"}.contains(suffix))
            {
                fail("固件支持 ELF、HEX、S19 / SREC 和 BIN");
            }
            if (esp && suffix != "bin") { fail("Espressif program_esp 需要 BIN 文件"); }
            const bool binary = suffix == "bin";
            const auto addressText = image["address"].toString().trimmed();
            if (binary && addressText.isEmpty()) { fail("BIN 文件必须明确填写烧录地址，不会猜测芯片地址"); }
            if (!binary && !addressText.isEmpty()) { fail("ELF / HEX / S19 使用文件内地址，请将地址栏留空"); }
            const auto address = binary ? hexAddress(addressText) : QString();
            auto imagePath = QDir::fromNativeSeparators(file.absoluteFilePath());
#ifdef Q_OS_WIN
            if (hasNonAscii(imagePath))
            {
                const auto staged = QString("firmware-%1.%2").arg(index).arg(suffix);
                stagedImages.append(QJsonObject {{"source", imagePath}, {"path", staged}});
                imagePath = staged;
            }
#endif
            const auto path = tclWord(imagePath);
            const bool verify = config["verify"].toBool();
            const bool reset = config["reset_run"].toBool() && index + 1 == images.size();
            if (esp)
            {
                commands.append("program_esp " + path + " " + address + (verify ? " verify" : "") + (reset ? " reset" : ""));
            }
            else if (startup == "default")
            {
                // program already calls init + reset init. Never duplicate them.
                commands.append("program " + path + (verify ? " verify" : "") + (reset ? " reset" : "")
                    + (binary ? " " + address : ""));
            }
            else
            {
                const auto type = binary ? "bin" : suffix == "elf" ? "elf" : suffix == "hex" || suffix == "ihex" ? "ihex" : "s19";
                const auto imageArgs = path + " " + (binary ? address : "0") + " " + type;
                commands.append("flash write_image erase " + imageArgs);
                if (verify) { commands.append("verify_image " + imageArgs); }
                if (reset) { commands.append("reset run"); }
            }
        }
    }
    // catch also covers config hooks and missing vendor commands. Only a marker
    // after every requested operation plus a zero exit code counts as success.
    const auto script = "if {[catch {\n    " + (setup + commands).join("\n    ")
        + "\n} sw_error]} {\n    echo $sw_error\n    shutdown error\n}\n"
        + "echo SW_OPENOCD_COMPLETE\n" + (rtt ? QString() : QString("shutdown\n"));
    result["scripts_dir"] = root;
    result["arguments"] = QJsonArray {"-s", nativeToolPath(root), "-f", "<generated-job.cfg>"};
    result["staged_images"] = stagedImages;
    result["script"] = script;
    result["steps"] = QJsonArray::fromStringList(commands);
    return result;
}

void OpenOcdService::appendLog(const QString &text)
{
    for (qsizetype offset = 0; offset < text.size(); offset += 8192)
    {
        const auto part = text.mid(offset, 8192);
        records.append(QJsonObject {{"seq", ++sequence}, {"time", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}, {"text", part}});
        logBytes += part.toUtf8().size();
        while (!records.isEmpty() && (logBytes > MaxLogBytes || records.size() > 1000))
        {
            logBytes -= records.takeFirst()["text"].toString().toUtf8().size();
        }
    }
}

void OpenOcdService::readOutput()
{
    if (!process.isOpen()) { return; }
    const QString text = decoder.decode(process.readAllStandardOutput());
    if (text.isEmpty()) { return; }
    appendLog(text);
    markerPending += text;
    qsizetype newline = 0;
    while ((newline = markerPending.indexOf('\n')) >= 0)
    {
        if (markerPending.left(newline).trimmed() == marker)
        {
            completedMarker = true;
            if (operation == "rtt" && rttStatus == "starting") { connectRtt(); }
        }
        markerPending.remove(0, newline + 1);
    }
    markerPending = markerPending.right(8192);
    emit stateChanged();
}

QJsonObject OpenOcdService::state(qint64 after, int limit) const
{
    if (after < 0 || after > sequence || limit < 1 || limit > 200) { fail("OpenOCD after 应为 0..latest_cursor，limit 应为 1..200"); }
    QJsonArray page;
    qint64 cursor = records.isEmpty() ? sequence : after;
    for (const auto &record : records)
    {
        if (record["seq"].toInteger() <= after) { continue; }
        page.append(record);
        cursor = record["seq"].toInteger();
        if (page.size() >= limit) { break; }
    }
    return {{"view_id", viewId}, {"title", title}, {"configuration", config}, {"revision", configRevision}, {"job_id", jobId},
        {"status", status}, {"running", running}, {"operation", operation}, {"error", error}, {"rtt", readRtt(rttSequence, 1)},
        {"exit_code", exitCode < 0 ? QJsonValue() : QJsonValue(exitCode)},
        {"elapsed_ms", running ? elapsed.elapsed() : durationMs}, {"records", page},
        {"next_cursor", cursor}, {"latest_cursor", sequence},
        {"gap", records.isEmpty() ? after < sequence : after + 1 < records.first()["seq"].toInteger()}, {"has_more", cursor < sequence}};
}

void OpenOcdService::start(const QString &requested, qint64 expectedRevision)
{
    if (busy()) { fail("OpenOCD 已有运行中的任务"); }
    if (expectedRevision != configRevision) { fail("OpenOCD 配置已更改，请重新预览并使用当前 revision"); }
    const auto plan = preview(requested);
    const bool external = plan["external"].toBool();
    if (requested != "version" && !external)
    {
        if (auto engine = qobject_cast<Engine *>(parent())) { engine->checkOpenOcdProbe(this); }
    }
    QTcpServer reservation;
    int port = config["rtt_port"].toInt();
    if (requested == "rtt" && !external)
    {
        if (!reservation.listen(QHostAddress::LocalHost, quint16(port))) { fail("RTT 端口被占用，请选择其他端口或接入已有服务"); }
        port = reservation.serverPort();
        if (port == config["rtt_gdb_port"].toInt()) { fail("RTT 自动端口与 GDB 冲突，请重试或指定其他端口"); }
    }
    workspace = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/sw-openocd-XXXXXX");
    if (!workspace->isValid()) { fail("无法创建 OpenOCD 临时工作目录"); }
    QStringList arguments;
    for (const auto &value : plan["arguments"].toArray()) { arguments.append(value.toString()); }
    const auto nextJobId = QUuid::createUuid().toString(QUuid::Id128);
    marker = "SW_OPENOCD_COMPLETE_" + nextJobId;
    if (requested != "version" && !external)
    {
        for (const auto &value : plan["staged_images"].toArray())
        {
            const auto image = value.toObject();
            if (!QFile::copy(image["source"].toString(), workspace->filePath(image["path"].toString())))
            {
                fail("无法暂存中文路径的固件，请检查临时目录空间与文件权限：" + image["source"].toString());
            }
        }
        QFile script(workspace->filePath("job.cfg"));
        const auto bytes = plan["script"].toString().replace("echo SW_OPENOCD_COMPLETE\n", "echo " + marker + "\n")
            .replace("@RTT_PORT@", QString::number(port)).toUtf8();
        if (!script.open(QIODevice::WriteOnly) || script.write(bytes) != bytes.size()) { fail("无法写入 OpenOCD 任务配置"); }
        script.close();
        // Relative ASCII name also works when the Windows temp parent is Unicode.
        arguments[arguments.size() - 1] = "job.cfg";
    }
    jobId = nextJobId;
    operation = requested;
    status = "running";
    error.clear();
    exitCode = -1;
    durationMs = 0;
    records.clear();
    logBytes = 0;
    // Sequence stays monotonic across jobs. Readers may retain independent cursors.
    completedMarker = false;
    markerPending.clear();
    decoder.resetState();
    elapsed.start();
    running = true;
    if (requested == "rtt") { resetRtt(port); }
    if (external)
    {
        deadline.start(config["timeout_ms"].toInt());
        connectRtt();
        emit stateChanged();
        return;
    }
    process.setProgram(plan["executable"].toString());
    process.setArguments(arguments);
    process.setWorkingDirectory(workspace->path());
    deadline.start(requested == "version" ? qMin(10000, config["timeout_ms"].toInt()) : config["timeout_ms"].toInt());
    reservation.close();
    process.start(QIODevice::ReadOnly);
    emit stateChanged();
}

void OpenOcdService::finish(int code, QProcess::ExitStatus exitStatus)
{
    if (!running) { return; }
    readOutput();
    if (markerPending.trimmed() == marker) { completedMarker = true; }
    deadline.stop();
    killTimer.stop();
    if (operation == "rtt") { closeRtt(); }
    durationMs = elapsed.elapsed();
    running = false;
    exitCode = code;
    if (status == "cancelling")
    {
        status = "cancelled";
        error = operation == "rtt" ? "RTT 查看已停止" : "任务已取消；目标可能已被暂停或部分写入，请核对设备后重新操作";
    }
    else if (status == "timed_out") { error = "OpenOCD 运行超时，已结束本次进程；请检查日志和设备状态"; }
    else if (operation == "rtt" || exitStatus != QProcess::NormalExit || code != 0 || (operation != "version" && !completedMarker))
    {
        status = "failed";
        if (error.isEmpty()) { error = code == 0 ? "OpenOCD 未完成全部步骤，请查看日志" : "OpenOCD 执行失败，请查看日志和退出码"; }
    }
    else { status = "succeeded"; }
    workspace.reset();
    emit stateChanged();
}

void OpenOcdService::stop(const QString &reason)
{
    if (!running || status == "cancelling" || status == "timed_out") { return; }
    status = reason;
    deadline.stop();
    if (operation == "rtt")
    {
        closeRtt();
        if (config["rtt_mode"] == "external")
        {
            finish(0, QProcess::NormalExit);
            return;
        }
    }
    process.terminate();
    if (process.state() != QProcess::NotRunning) { killTimer.start(800); }
    emit stateChanged();
}

void OpenOcdService::cancel(const QString &expectedJobId)
{
    if (!running || expectedJobId != jobId) { fail("OpenOCD job_id 不匹配或任务已结束"); }
    stop("cancelling");
}
}
