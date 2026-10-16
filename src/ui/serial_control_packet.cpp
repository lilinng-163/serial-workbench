// SPDX-License-Identifier: Apache-2.0
#include "serial_control_panel.h"
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace sw
{
static void invalid(const QString &message)
{
    throw std::runtime_error(message.toUtf8().constData());
}

QJsonObject defaultControl(const QString &type)
{
    return {{"type", type}, {"title", "新控件"}, {"key", "value"},
        {"minimum", 0}, {"maximum", 100}, {"step", 1}, {"decimals", 0}, {"auto_send", false},
        {"value", "0"}, {"options", "低=0\n中=1\n高=2"},
        {"format", "text"}, {"pattern", "SET {key} {value}"},
        {"encoding", "utf-8"}, {"newline", "crlf"}};
}

QByteArray controlPacket(const QJsonObject &spec)
{
    const QString pattern = spec["pattern"].toString();
    const QString value = spec["value"].toString();
    if (pattern.size() > 4096 || value.size() > 1024) { invalid("模板或数值过长"); }
    QByteArray bytes;
    if (spec["format"].toString() == "text")
    {
        const auto codec = QTextCodec::codecForName(spec["encoding"].toString("utf-8").toLatin1());
        if (!codec) { invalid("不支持此文本编码"); }
        // Replace once: substituted values can contain literal braces.
        const QRegularExpression tokens(R"(\{([A-Za-z_][A-Za-z0-9_:]*)\})");
        auto matches = tokens.globalMatch(pattern);
        qsizetype offset = 0;
        QString text;
        while (matches.hasNext())
        {
            const auto match = matches.next();
            text += pattern.mid(offset, match.capturedStart() - offset);
            if (match.captured(1) == "key") { text += spec["key"].toString(); }
            else if (match.captured(1) == "value") { text += value; }
            else { invalid("未知占位符：" + match.captured()); }
            offset = match.capturedEnd();
        }
        text += pattern.mid(offset);
        QTextCodec::ConverterState state(QTextCodec::IgnoreHeader);
        bytes = codec->fromUnicode(text.constData(), int(text.size()), &state);
        if (state.invalidChars) { invalid("当前编码无法表示这些字符"); }
        const QString newline = spec["newline"].toString("crlf");
        if (newline == "crlf") { bytes += "\r\n"; }
        else if (newline == "lf") { bytes += '\n'; }
        else if (newline == "cr") { bytes += '\r'; }
        else if (newline != "none") { invalid("未知行尾格式"); }
    }
    else if (spec["format"].toString() == "hex")
    {
        QString literals;
        const auto flush = [&]
        {
            literals.remove(QRegularExpression("\\s"));
            if (literals.size() % 2 || literals.contains(QRegularExpression("[^0-9a-fA-F]")))
            {
                invalid("HEX 常量需要成对的十六进制数字，例如 AA 55");
            }
            bytes += QByteArray::fromHex(literals.toLatin1());
            literals.clear();
        };
        for (qsizetype pos = 0; pos < pattern.size();)
        {
            if (pattern[pos] != '{') { literals += pattern[pos++]; continue; }
            flush();
            const auto end = pattern.indexOf('}', pos);
            if (end < 0) { invalid("占位符缺少右花括号"); }
            const QString token = pattern.mid(pos + 1, end - pos - 1);
            pos = end + 1;
            if (!token.startsWith("value:")) { invalid("HEX 使用 {value:u8} 等数值占位符"); }
            const QString type = token.mid(6);
            const QRegularExpression integerType(R"(\A([ui])(8|16|32)(le|be)?\z)");
            const auto match = integerType.match(type);
            bool ok = false;
            const double number = QLocale::c().toDouble(value, &ok);
            if (!ok || !std::isfinite(number)) { invalid("HEX 数值必须是有限数字"); }
            quint32 bits = 0;
            int size = 0;
            if (type == "f32le" || type == "f32be")
            {
                const float floating = float(number);
                if (!std::isfinite(floating)) { invalid("数值超出 32 位浮点范围"); }
                std::memcpy(&bits, &floating, sizeof(bits));
                size = 4;
            }
            else if (match.hasMatch())
            {
                const int width = match.captured(2).toInt();
                if (width > 8 && match.captured(3).isEmpty()) { invalid("多字节数值需要指定 le 或 be 字节序"); }
                const bool signedValue = match.captured(1) == "i";
                const double minimum = signedValue ? -std::ldexp(1.0, width - 1) : 0;
                const double maximum = std::ldexp(1.0, signedValue ? width - 1 : width) - 1;
                if (std::trunc(number) != number || number < minimum || number > maximum)
                {
                    invalid("数值超出 " + type + " 的整数范围");
                }
                bits = quint32(qint64(number));
                size = width / 8;
            }
            else { invalid("未知数值类型：" + type); }
            for (int byte = 0; byte < size; ++byte)
            {
                const int shift = type.endsWith("be") ? (size - byte - 1) * 8 : byte * 8;
                bytes += char((bits >> shift) & 0xff);
            }
        }
        flush();
    }
    else { invalid("未知发送格式"); }
    if (bytes.isEmpty() || bytes.size() > 4096) { invalid("报文长度需要为 1～4096 字节"); }
    return bytes;
}
}
