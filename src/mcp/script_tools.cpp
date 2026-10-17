// SPDX-License-Identifier: Apache-2.0
#include "mcp.h"

namespace sw
{
namespace
{
QString revision(Channel *channel)
{
    const auto value = channel->scriptCode.toUtf8() + (channel->scriptEnabled ? "1" : "0");
    return QString::fromLatin1(QCryptographicHash::hash(value, QCryptographicHash::Sha256).toHex());
}

QJsonObject describeScript(Channel *channel)
{
    return {{"channel_id", channel->id}, {"code", channel->scriptCode},
        {"enabled", channel->scriptEnabled}, {"error", channel->scriptError},
        {"revision", revision(channel)}, {"frames", channel->scriptFrames}};
}

QString sourceCode(const QJsonObject &arguments)
{
    if (!arguments["code"].isString()) { fail("code 必须是字符串"); }
    const auto code = arguments["code"].toString();
    if (code.toUtf8().size() > 65536) { fail("脚本源码最多 64 KiB"); }
    return code;
}
}

QJsonObject McpServer::scriptTool(const QString &name, const QJsonObject &a)
{
    if (name == "validate_frame_script")
    {
        const auto code = sourceCode(a);
        try
        {
            const auto program = FrameScriptProgram::compile(code);
            FrameScriptRuntime runtime(program);
            QJsonArray frames;
            const auto chunks = a["sample_chunks"].toArray();
            if (a.contains("sample_chunks") && !a["sample_chunks"].isArray())
            {
                fail("sample_chunks 必须是 HEX 字符串数组");
            }
            if (chunks.size() > 128) { fail("试解析最多 128 个数据块"); }
            int bytes = 0;
            QElapsedTimer elapsed;
            elapsed.start();
            for (const auto &chunk : chunks)
            {
                if (elapsed.elapsed() > 500) { throw ScriptError(0, "试解析总耗时超限，请减少测试数据块"); }
                if (!chunk.isString()) { fail("每个试解析数据块必须是 HEX 字符串"); }
                if (chunk.toString().size() > 196608) { fail("试解析输入过长"); }
                const auto data = Engine::encode(chunk.toString(), true, "utf-8", "none");
                bytes += data.size();
                if (bytes > 65536) { fail("试解析输入合计最多 64 KiB"); }
                for (const auto &frame : runtime.feed(data))
                {
                    if (frames.size() >= 1024) { fail("试解析最多返回 1024 帧"); }
                    frames.append(frame);
                }
            }
            return {{"valid", true}, {"frames", frames}, {"buffered_bytes", runtime.bufferedBytes()},
                {"scope", "offline_test"}, {"changes_live_script", false}};
        }
        catch (const ScriptError &error)
        {
            return {{"valid", false}, {"error", error.display()}, {"line", error.line},
                {"scope", "offline_test"}, {"changes_live_script", false}};
        }
    }

    auto channel = accessibleChannel(a["channel_id"].toString());
    if (channel->config.kind != "serial") { fail("帧脚本仅适用于串口连接"); }
    if (name == "get_frame_script") { return describeScript(channel); }
    if (!engine->allowControl) { fail("AI 控制未开启，不能修改或启用脚本"); }
    if (channel->transferOwner) { fail("文件传输期间不能修改帧脚本"); }
    if (!a["enabled"].isBool()) { fail("必须明确指定 enabled 为 true 或 false"); }
    if (a["expected_revision"].toString().isEmpty()
        || a["expected_revision"].toString() != revision(channel))
    {
        fail("脚本已变化或未提供 expected_revision；先 get_frame_script，再基于最新 revision 修改");
    }
    const auto code = sourceCode(a);
    FrameScriptProgram::compile(code);
    if (scriptHandler) { scriptHandler(channel, code, a["enabled"].toBool()); }
    else { channel->configureScript(code, a["enabled"].toBool()); }
    return describeScript(channel);
}
}
