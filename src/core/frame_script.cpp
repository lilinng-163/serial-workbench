// SPDX-License-Identifier: Apache-2.0

#include "frame_script.h"
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <functional>
#include <vector>

namespace sw
{
ScriptError::ScriptError(int position, const QString &message)
    : std::runtime_error(message.toStdString()), line(position), detail(message)
{
}

QString ScriptError::display() const
{
    return line > 0 ? QString("第 %1 行：%2").arg(line).arg(detail) : detail;
}

namespace
{
constexpr int MaximumBuffer = 65536;
constexpr int MaximumSteps = 200000;

struct Token
{
    enum Kind { Symbol, Identifier, Number, Text, End } kind = Symbol;
    QString text;
    int line = 1;
};

QVector<Token> tokenize(const QString &code)
{
    if (code.toUtf8().size() > 65536)
    {
        throw ScriptError(0, "脚本最多 64 KiB");
    }
    QVector<Token> result;
    int at = 0;
    int line = 1;
    while (at < code.size())
    {
        const QChar c = code[at];
        if (c.isSpace())
        {
            line += c == '\n';
            ++at;
            continue;
        }
        if (code.mid(at, 2) == "//")
        {
            while (at < code.size() && code[at] != '\n') { ++at; }
            continue;
        }
        if (code.mid(at, 2) == "/*")
        {
            const int end = code.indexOf("*/", at + 2);
            if (end < 0) { throw ScriptError(line, "注释缺少 */"); }
            line += code.mid(at, end + 2 - at).count('\n');
            at = end + 2;
            continue;
        }
        Token token;
        token.line = line;
        const int start = at;
        if (c.isLetter() || c == '_')
        {
            token.kind = Token::Identifier;
            while (at < code.size() && (code[at].isLetterOrNumber() || code[at] == '_')) { ++at; }
            token.text = code.mid(start, at - start);
        }
        else if (c.isDigit() || (c == '.' && at + 1 < code.size() && code[at + 1].isDigit()))
        {
            token.kind = Token::Number;
            if (code.mid(at, 2).compare("0x", Qt::CaseInsensitive) == 0)
            {
                at += 2;
                while (at < code.size() && QString("0123456789abcdefABCDEF").contains(code[at])) { ++at; }
            }
            else
            {
                while (at < code.size() && (code[at].isDigit() || code[at] == '.')) { ++at; }
                if (at < code.size() && (code[at] == 'e' || code[at] == 'E'))
                {
                    ++at;
                    if (at < code.size() && (code[at] == '+' || code[at] == '-')) { ++at; }
                    while (at < code.size() && code[at].isDigit()) { ++at; }
                }
            }
            token.text = code.mid(start, at - start);
            if (at < code.size() && QString("uUfF").contains(code[at]))
            {
                if (code[at].toLower() == 'f' && !token.text.contains('.') && !token.text.contains('e', Qt::CaseInsensitive)) { token.text += ".0"; }
                ++at;
            }
        }
        else if (c == '"')
        {
            token.kind = Token::Text;
            ++at;
            while (at < code.size() && code[at] != '"')
            {
                QChar next = code[at++];
                if (next == '\n') { throw ScriptError(line, "字符串缺少结束引号"); }
                if (next == '\\')
                {
                    if (at >= code.size()) { throw ScriptError(line, "字符串转义不完整"); }
                    next = code[at++];
                    if (next == 'n') { next = '\n'; }
                    else if (next == 'r') { next = '\r'; }
                    else if (next == 't') { next = '\t'; }
                    else if (next != '\\' && next != '"') { throw ScriptError(line, "不支持的字符串转义"); }
                }
                token.text += next;
            }
            if (at >= code.size()) { throw ScriptError(line, "字符串缺少结束引号"); }
            ++at;
        }
        else
        {
            const QString three = code.mid(at, 3);
            const QString two = code.mid(at, 2);
            if (QStringList {"<<=", ">>="}.contains(three))
            {
                token.text = three;
                at += 3;
            }
            else if (QStringList {"++", "--", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=",
                                  "==", "!=", "<=", ">=", "&&", "||", "<<", ">>"}.contains(two))
            {
                token.text = two;
                at += 2;
            }
            else
            {
                if (!QString("{}()[];,+-*/%&|^~!<>=?:").contains(c))
                {
                    throw ScriptError(line, QString("无法识别的字符：") + c);
                }
                token.text = c;
                ++at;
            }
        }
        result.append(token);
        if (result.size() > 16384) { throw ScriptError(line, "脚本过于复杂"); }
    }
    result.append({Token::End, {}, line});
    return result;
}

bool isType(const QString &text)
{
    static const QStringList types {"int", "float", "double", "bool", "byte", "char", "short", "long",
        "size_t", "uint8_t", "uint16_t", "uint32_t", "int8_t", "int16_t", "int32_t", "unsigned", "signed"};
    return types.contains(text);
}

struct Expr;
struct Statement;
using Expression = std::shared_ptr<const Expr>;
using Block = std::shared_ptr<const Statement>;

struct Expr
{
    enum Kind { Literal, String, Name, Index, Call, Unary, Binary, Assign, Cast, Postfix, Conditional } kind;
    Token token;
    QString operation;
    std::vector<Expression> children;
    int depth = 1;
};

struct Statement
{
    enum Kind { Sequence, Declaration, Evaluate, If, While, For, Break, Continue, Return, Empty } kind;
    int line = 1;
    int identity = 0;
    QString name;
    QString type;
    bool constant = false;
    bool persistent = false;
    Expression first;
    Expression second;
    Block body;
    Block otherwise;
    Block initial;
    std::vector<Block> children;
};

const QMap<QString, QPair<int, int>> &functions()
{
    static const QMap<QString, QPair<int, int>> entries {
        {"buffer_append", {1, 2}}, {"buffer_size", {0, 0}}, {"buffer_drop", {1, 1}},
        {"buffer_clear", {0, 0}}, {"emit", {2, 2}}, {"emit_frame", {0, 0}},
        {"read_u8", {2, 2}}, {"read_i8", {2, 2}},
        {"read_u16_le", {2, 2}}, {"read_u16_be", {2, 2}},
        {"read_i16_le", {2, 2}}, {"read_i16_be", {2, 2}},
        {"read_u32_le", {2, 2}}, {"read_u32_be", {2, 2}},
        {"read_i32_le", {2, 2}}, {"read_i32_be", {2, 2}},
        {"read_f32_le", {2, 2}}, {"read_f32_be", {2, 2}},
        {"sum8", {3, 3}}, {"xor8", {3, 3}}, {"crc16_modbus", {3, 3}},
        {"min", {2, 2}}, {"max", {2, 2}}, {"abs", {1, 1}}};
    return entries;
}

class Parser
{
public:
    explicit Parser(const QString &code) : tokens(tokenize(code)) {}
    QVector<Token> tokens;
    int at = 0;
    int statementDepth = 0;
    int loopDepth = 0;
    QString dataName;
    QString lengthName;

    const Token &peek(int offset = 0) const { return tokens[qMin(at + offset, int(tokens.size()) - 1)]; }
    Token take() { return tokens[at++]; }
    bool accept(const QString &text)
    {
        if (peek().text != text) { return false; }
        ++at;
        return true;
    }
    void expect(const QString &text)
    {
        if (!accept(text)) { throw ScriptError(peek().line, "应为 " + text); }
    }
    QString identifier()
    {
        if (peek().kind != Token::Identifier) { throw ScriptError(peek().line, "需要变量名"); }
        if (isType(peek().text) || QStringList {"void", "main", "const", "static", "if", "else", "for", "while", "break", "continue", "return", "true", "false"}.contains(peek().text))
        {
            throw ScriptError(peek().line, "变量名不能使用关键字");
        }
        return take().text;
    }
    QString type()
    {
        if (!isType(peek().text)) { throw ScriptError(peek().line, "不支持的类型"); }
        auto text = take().text;
        if (text == "unsigned" || text == "signed")
        {
            const bool positive = text == "unsigned";
            text = isType(peek().text) ? take().text : "int";
            if (text == "char") { return positive ? "uint8_t" : "int8_t"; }
            if (text == "short") { accept("int"); return positive ? "uint16_t" : "int16_t"; }
            if (text == "long") { accept("int"); }
            else if (text != "int") { throw ScriptError(peek().line, "signed / unsigned 仅支持 char、short、int 或 long"); }
            return positive ? "uint32_t" : "int32_t";
        }
        return text;
    }
    Block parse()
    {
        expect("void");
        expect("main");
        expect("(");
        accept("const");
        const auto bytesType = type();
        if (!QStringList {"uint8_t", "byte", "char"}.contains(bytesType))
        {
            throw ScriptError(peek().line, "main 第一个参数应为字节数组");
        }
        const bool pointer = accept("*");
        dataName = identifier();
        if (!pointer) { expect("["); expect("]"); }
        expect(",");
        const auto countType = type();
        if (countType == "float" || countType == "double")
        {
            throw ScriptError(peek().line, "main 长度参数应为整数");
        }
        lengthName = identifier();
        if (dataName == lengthName || dataName == "buffer" || lengthName == "buffer")
        {
            throw ScriptError(peek().line, "main 参数名重复或使用了保留名称 buffer");
        }
        expect(")");
        if (peek().text != "{") { throw ScriptError(peek().line, "main 缺少函数体"); }
        auto body = statement();
        if (peek().kind != Token::End) { throw ScriptError(peek().line, "第一版仅支持一个固定的 main 函数"); }
        return body;
    }
    int precedence(const QString &op) const
    {
        if (QStringList {"=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="}.contains(op)) { return 1; }
        if (op == "?") { return 2; }
        if (op == "||") { return 3; }
        if (op == "&&") { return 4; }
        if (op == "|") { return 5; }
        if (op == "^") { return 6; }
        if (op == "&") { return 7; }
        if (op == "==" || op == "!=") { return 8; }
        if (QStringList {"<", ">", "<=", ">="}.contains(op)) { return 9; }
        if (op == "<<" || op == ">>") { return 10; }
        if (op == "+" || op == "-") { return 11; }
        if (op == "*" || op == "/" || op == "%") { return 12; }
        return 0;
    }
    Expression node(Expr::Kind kind, const Token &token, const QString &op, std::vector<Expression> children = {})
    {
        auto result = std::make_shared<Expr>();
        result->kind = kind;
        result->token = token;
        result->operation = op;
        result->children = std::move(children);
        for (const auto &child : result->children) { result->depth = qMax(result->depth, child->depth + 1); }
        if (result->depth > 64) { throw ScriptError(token.line, "表达式嵌套超过 64 层"); }
        return result;
    }
    Expression expression(int minimum = 1, int nesting = 0)
    {
        if (nesting > 64) { throw ScriptError(peek().line, "表达式嵌套超过 64 层"); }
        const auto token = take();
        Expression left;
        if (token.kind == Token::Number)
        {
            bool ok = false;
            const double value = token.text.startsWith("0x", Qt::CaseInsensitive)
                ? double(token.text.mid(2).toULongLong(&ok, 16)) : token.text.toDouble(&ok);
            const bool integral = token.text.startsWith("0x", Qt::CaseInsensitive)
                || (!token.text.contains('.') && !token.text.contains('e', Qt::CaseInsensitive));
            if (!ok || !std::isfinite(value) || (integral && std::abs(value) > 9007199254740991.0))
            {
                throw ScriptError(token.line, "数值字面量无效或超出精确数值范围");
            }
            left = node(Expr::Literal, token, token.text);
        }
        else if (token.kind == Token::Text) { left = node(Expr::String, token, token.text); }
        else if (token.text == "(")
        {
            if (isType(peek().text))
            {
                const auto castType = type();
                expect(")");
                left = node(Expr::Cast, token, castType, {expression(13, nesting + 1)});
            }
            else
            {
                left = expression(1, nesting + 1);
                expect(")");
            }
        }
        else if (QStringList {"+", "-", "!", "~", "++", "--"}.contains(token.text))
        {
            left = node(Expr::Unary, token, token.text, {expression(13, nesting + 1)});
        }
        else if (token.kind == Token::Identifier)
        {
            if (accept("("))
            {
                std::vector<Expression> args;
                if (!accept(")"))
                {
                    do { args.push_back(expression(1, nesting + 1)); } while (accept(","));
                    expect(")");
                }
                if (!functions().contains(token.text)) { throw ScriptError(token.line, "未知函数：" + token.text); }
                const auto arity = functions()[token.text];
                if (int(args.size()) < arity.first || int(args.size()) > arity.second)
                {
                    throw ScriptError(token.line, "函数参数个数不正确：" + token.text);
                }
                left = node(Expr::Call, token, token.text, std::move(args));
            }
            else { left = node(Expr::Name, token, token.text); }
        }
        else { throw ScriptError(token.line, "需要表达式"); }

        while (true)
        {
            if (accept("["))
            {
                auto index = expression(1, nesting + 1);
                expect("]");
                left = node(Expr::Index, token, {}, {left, index});
                continue;
            }
            if (peek().text == "++" || peek().text == "--")
            {
                auto op = take();
                if (left->kind != Expr::Name) { throw ScriptError(op.line, "自增 / 自减仅支持变量"); }
                left = node(Expr::Postfix, op, op.text, {left});
                continue;
            }
            const auto op = peek();
            const int level = precedence(op.text);
            if (level == 0 || level < minimum) { break; }
            take();
            if (op.text == "?")
            {
                auto yes = expression(1, nesting + 1);
                expect(":");
                left = node(Expr::Conditional, op, {}, {left, yes, expression(2, nesting + 1)});
            }
            else
            {
                if (level == 1 && left->kind != Expr::Name) { throw ScriptError(op.line, "字节数组只读，只能给变量赋值"); }
                auto right = expression(level == 1 ? level : level + 1, nesting + 1);
                left = node(level == 1 ? Expr::Assign : Expr::Binary, op, op.text, {left, right});
            }
        }
        return left;
    }
    Block simple(bool semicolon)
    {
        auto result = std::make_shared<Statement>();
        result->line = peek().line;
        result->identity = at;
        result->persistent = accept("static");
        result->constant = accept("const");
        if (isType(peek().text))
        {
            result->kind = Statement::Declaration;
            result->type = type();
            result->name = identifier();
            if (result->name == dataName || result->name == lengthName || result->name == "buffer")
            {
                throw ScriptError(result->line, "不能覆盖 main 参数或 buffer");
            }
            if (accept("=")) { result->first = expression(); }
        }
        else
        {
            if (result->persistent || result->constant) { throw ScriptError(result->line, "需要变量类型"); }
            result->kind = Statement::Evaluate;
            result->first = expression();
        }
        if (semicolon) { expect(";"); }
        return result;
    }
    Block statement()
    {
        if (++statementDepth > 64) { throw ScriptError(peek().line, "语句嵌套超过 64 层"); }
        auto result = std::make_shared<Statement>();
        result->line = peek().line;
        if (accept("{"))
        {
            result->kind = Statement::Sequence;
            while (peek().text != "}")
            {
                if (peek().kind == Token::End) { throw ScriptError(peek().line, "缺少 }"); }
                result->children.push_back(statement());
            }
            expect("}");
        }
        else if (accept("if"))
        {
            result->kind = Statement::If;
            expect("(");
            result->first = expression();
            expect(")");
            result->body = statement();
            if (accept("else")) { result->otherwise = statement(); }
        }
        else if (accept("while"))
        {
            result->kind = Statement::While;
            expect("(");
            result->first = expression();
            expect(")");
            ++loopDepth;
            result->body = statement();
            --loopDepth;
        }
        else if (accept("for"))
        {
            result->kind = Statement::For;
            expect("(");
            if (!accept(";")) { result->initial = simple(true); }
            if (!accept(";")) { result->first = expression(); expect(";"); }
            if (!accept(")")) { result->second = expression(); expect(")"); }
            ++loopDepth;
            result->body = statement();
            --loopDepth;
        }
        else if (peek().text == "break" || peek().text == "continue")
        {
            if (loopDepth == 0) { throw ScriptError(peek().line, "break / continue 只能用于循环"); }
            result->kind = take().text == "break" ? Statement::Break : Statement::Continue;
            expect(";");
        }
        else if (accept("return"))
        {
            result->kind = Statement::Return;
            expect(";");
        }
        else if (accept(";")) { result->kind = Statement::Empty; }
        else
        {
            --statementDepth;
            return simple(true);
        }
        --statementDepth;
        return result;
    }
};

struct Value
{
    enum Kind { Number, Text, Array } kind = Number;
    double number = 0;
    bool integral = true;
    QString text;
    bool buffered = false;
    static Value numeric(double value, bool integer = true)
    {
        Value result;
        result.number = value;
        result.integral = integer;
        return result;
    }
};

struct Variable
{
    Value value;
    QString type;
    bool constant = false;
    QString persistentKey;
};

double numeric(const Value &value, int line)
{
    if (value.kind != Value::Number || !std::isfinite(value.number))
    {
        throw ScriptError(line, "需要有限的数值");
    }
    return value.number;
}

quint32 bits(double number)
{
    double value = std::fmod(std::trunc(number), 4294967296.0);
    if (value < 0) { value += 4294967296.0; }
    return quint32(value);
}

Value converted(const Value &value, const QString &type, int line)
{
    const double number = numeric(value, line);
    if (type == "double") { return Value::numeric(number, false); }
    if (type == "float")
    {
        const float rounded = static_cast<float>(number);
        if (!std::isfinite(rounded)) { throw ScriptError(line, "float 数值溢出"); }
        return Value::numeric(rounded, false);
    }
    if (type == "bool") { return Value::numeric(number != 0); }
    const quint32 raw = bits(number);
    if (type == "byte" || type == "uint8_t") { return Value::numeric(raw & 255); }
    if (type == "char" || type == "int8_t") { return Value::numeric(raw & 128 ? int(raw & 255) - 256 : int(raw & 255)); }
    if (type == "uint16_t") { return Value::numeric(raw & 65535); }
    if (type == "short" || type == "int16_t") { return Value::numeric(raw & 32768 ? int(raw & 65535) - 65536 : int(raw & 65535)); }
    if (type == "uint32_t" || type == "size_t") { return Value::numeric(raw); }
    return Value::numeric(raw >= 0x80000000u ? double(raw) - 4294967296.0 : double(raw));
}
}

struct FrameScriptProgram::Impl
{
    QString dataName;
    QString lengthName;
    Block body;
};

FrameScriptProgram::FrameScriptProgram(std::shared_ptr<const Impl> value) : syntax(std::move(value)) {}

std::shared_ptr<const FrameScriptProgram> FrameScriptProgram::compile(const QString &code)
{
    Parser parser(code);
    auto syntax = std::make_shared<Impl>();
    syntax->body = parser.parse();
    syntax->dataName = parser.dataName;
    syntax->lengthName = parser.lengthName;
    return std::shared_ptr<const FrameScriptProgram>(new FrameScriptProgram(syntax));
}

QString FrameScriptProgram::example()
{
    return QString::fromUtf8(R"(// 帧：AA 55 | int16 温度 / 100 | uint16 电压 / 1000 | SUM8 | 0D
// 示例：AA 55 C4 09 E4 0C BC 0D -> 温度 25，电压 3.3
void main(const uint8_t data[], int length)
{
    buffer_append(data, length);

    while (buffer_size() >= 8)
    {
        if (buffer[0] != 0xAA || buffer[1] != 0x55)
        {
            buffer_drop(1);
            continue;
        }

        if (buffer[7] != 0x0D
            || sum8(buffer, 0, 6) != buffer[6])
        {
            buffer_drop(1);
            continue;
        }

        emit("温度", read_i16_le(buffer, 2) / 100.0);
        emit("电压", read_u16_le(buffer, 4) / 1000.0);
        buffer_drop(8);
    }
}
)");
}

struct FrameScriptRuntime::Impl
{
    std::shared_ptr<const FrameScriptProgram::Impl> program;
    QByteArray input;
    QByteArray buffer;
    QMap<QString, Variable> persistent;
    std::vector<QMap<QString, Variable>> scopes;
    QJsonObject fields;
    QJsonArray output;
    QElapsedTimer timer;
    int steps = 0;
    enum Flow { Normal, Break, Continue, Return };

    void step(int line)
    {
        if (++steps > MaximumSteps || ((steps & 127) == 0 && timer.elapsed() > 100))
        {
            throw ScriptError(line, "执行超限，检查循环是否推进了数据或退出条件");
        }
    }
    Variable &variable(const QString &name, int line)
    {
        for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope)
        {
            auto found = scope->find(name);
            if (found != scope->end()) { return found.value(); }
        }
        throw ScriptError(line, "未定义的变量：" + name);
    }
    const QByteArray &bytes(const Value &value, int line) const
    {
        if (value.kind != Value::Array) { throw ScriptError(line, "需要 data 或 buffer 字节数组"); }
        return value.buffered ? buffer : input;
    }
    int integer(const Value &value, int line) const
    {
        const double number = numeric(value, line);
        if (number < 0 || number > 2147483647 || number != std::floor(number))
        {
            throw ScriptError(line, "索引、长度或位移必须为非负整数");
        }
        return int(number);
    }
    void range(const QByteArray &array, int start, int count, int line) const
    {
        if (start > array.size() || count > array.size() - start)
        {
            throw ScriptError(line, "字节数组访问越界");
        }
    }
    void flush(int line)
    {
        if (fields.isEmpty()) { return; }
        if (output.size() >= 1024) { throw ScriptError(line, "单次调用输出超过 1024 帧"); }
        output.append(fields);
        fields = {};
    }
    Value store(const Expression &target, const Value &value, int line)
    {
        if (target->kind != Expr::Name) { throw ScriptError(line, "只能修改局部变量"); }
        auto &entry = variable(target->operation, line);
        if (entry.constant) { throw ScriptError(line, "不能修改只读变量"); }
        entry.value = converted(value, entry.type, line);
        if (!entry.persistentKey.isEmpty()) { persistent[entry.persistentKey] = entry; }
        return entry.value;
    }
    Value binary(const QString &op, const Value &a, const Value &b, int line)
    {
        const double left = numeric(a, line);
        const double right = numeric(b, line);
        const bool integral = a.integral && b.integral;
        double result = 0;
        if (op == "+") { result = left + right; }
        else if (op == "-") { result = left - right; }
        else if (op == "*") { result = left * right; }
        else if (op == "/" || op == "%")
        {
            if (right == 0) { throw ScriptError(line, "除数不能为零"); }
            result = op == "%" ? std::fmod(left, right) : integral ? std::trunc(left / right) : left / right;
        }
        else if (op == "&") { result = bits(left) & bits(right); }
        else if (op == "|") { result = bits(left) | bits(right); }
        else if (op == "^") { result = bits(left) ^ bits(right); }
        else if (op == "<<" || op == ">>")
        {
            const int shift = integer(b, line);
            if (shift > 31) { throw ScriptError(line, "位移范围为 0..31"); }
            result = op == "<<" ? bits(left) << shift : bits(left) >> shift;
        }
        else if (op == "==") { return Value::numeric(left == right); }
        else if (op == "!=") { return Value::numeric(left != right); }
        else if (op == "<") { return Value::numeric(left < right); }
        else if (op == ">") { return Value::numeric(left > right); }
        else if (op == "<=") { return Value::numeric(left <= right); }
        else if (op == ">=") { return Value::numeric(left >= right); }
        else if (op == "&&") { return Value::numeric(left != 0 && right != 0); }
        else if (op == "||") { return Value::numeric(left != 0 || right != 0); }
        else { throw ScriptError(line, "不支持的运算符：" + op); }
        if (!std::isfinite(result)) { throw ScriptError(line, "计算结果溢出"); }
        if (integral && std::abs(result) > 9007199254740991.0)
        {
            throw ScriptError(line, "整数运算超出精确数值范围，请分步处理或转为浮点数");
        }
        return Value::numeric(result, integral);
    }
    Value call(const Expr &node)
    {
        std::vector<Value> args;
        for (const auto &child : node.children) { args.push_back(evaluate(child)); }
        const auto name = node.operation;
        const int line = node.token.line;
        if (name == "buffer_size") { return Value::numeric(buffer.size()); }
        if (name == "buffer_append")
        {
            const auto &array = bytes(args[0], line);
            const int count = args.size() == 2 ? integer(args[1], line) : int(array.size());
            range(array, 0, count, line);
            if (count > MaximumBuffer - buffer.size()) { throw ScriptError(line, "接收缓存超过 64 KiB，请及时消费完整帧"); }
            const QByteArray copy = array.left(count);
            buffer.append(copy);
        }
        else if (name == "buffer_drop")
        {
            const int count = integer(args[0], line);
            range(buffer, 0, count, line);
            flush(line);
            buffer.remove(0, count);
        }
        else if (name == "buffer_clear") { buffer.clear(); }
        else if (name == "emit_frame") { flush(line); }
        else if (name == "emit")
        {
            if (args[0].kind != Value::Text || args[0].text.isEmpty() || args[0].text.size() > 64)
            {
                throw ScriptError(line, "输出字段名应为 1..64 个字符的字符串");
            }
            if (fields.size() >= 16 && !fields.contains(args[0].text))
            {
                throw ScriptError(line, "每帧最多输出 16 个字段");
            }
            fields[args[0].text] = numeric(args[1], line);
        }
        else if (name == "min" || name == "max" || name == "abs")
        {
            const double a = numeric(args[0], line);
            const double b = args.size() > 1 ? numeric(args[1], line) : 0;
            return Value::numeric(name == "abs" ? std::abs(a) : name == "min" ? qMin(a, b) : qMax(a, b),
                                  args[0].integral && (args.size() < 2 || args[1].integral));
        }
        else if (name.startsWith("read_"))
        {
            const auto &array = bytes(args[0], line);
            const int offset = integer(args[1], line);
            const int count = name.contains("32") ? 4 : name.contains("16") ? 2 : 1;
            range(array, offset, count, line);
            quint32 value = 0;
            const bool big = name.endsWith("_be");
            for (int i = 0; i < count; ++i)
            {
                value |= quint32(quint8(array[offset + i])) << ((big ? count - 1 - i : i) * 8);
            }
            if (name.contains("f32"))
            {
                float number;
                std::memcpy(&number, &value, sizeof(number));
                if (!std::isfinite(number)) { throw ScriptError(line, "浮点字段为 NaN 或无穷值"); }
                return Value::numeric(number, false);
            }
            const bool signedValue = name.startsWith("read_i");
            const double number = signedValue && (value & (quint32(1) << (count * 8 - 1)))
                ? double(value) - std::pow(2.0, count * 8) : double(value);
            return Value::numeric(number);
        }
        else if (name == "sum8" || name == "xor8" || name == "crc16_modbus")
        {
            const auto &array = bytes(args[0], line);
            const int start = integer(args[1], line);
            const int count = integer(args[2], line);
            range(array, start, count, line);
            quint32 value = name == "crc16_modbus" ? 0xffff : 0;
            for (int i = 0; i < count; ++i)
            {
                step(line);
                if (name == "sum8") { value = (value + quint8(array[start + i])) & 255; }
                else if (name == "xor8") { value ^= quint8(array[start + i]); }
                else
                {
                    value ^= quint8(array[start + i]);
                    for (int bit = 0; bit < 8; ++bit) { value = value & 1 ? (value >> 1) ^ 0xa001 : value >> 1; }
                }
            }
            return Value::numeric(value);
        }
        return {};
    }
    Value evaluate(const Expression &expression)
    {
        const auto &node = *expression;
        const int line = node.token.line;
        step(line);
        if (node.kind == Expr::Literal)
        {
            bool ok = false;
            const bool hex = node.operation.startsWith("0x", Qt::CaseInsensitive);
            const double number = hex ? double(node.operation.mid(2).toULongLong(&ok, 16)) : node.operation.toDouble(&ok);
            if (!ok || !std::isfinite(number)) { throw ScriptError(line, "数值字面量无效"); }
            return Value::numeric(number, hex || (!node.operation.contains('.') && !node.operation.contains('e', Qt::CaseInsensitive)));
        }
        if (node.kind == Expr::String)
        {
            Value value;
            value.kind = Value::Text;
            value.text = node.operation;
            return value;
        }
        if (node.kind == Expr::Name)
        {
            if (node.operation == program->dataName || node.operation == "buffer")
            {
                Value value;
                value.kind = Value::Array;
                value.buffered = node.operation == "buffer";
                return value;
            }
            if (node.operation == program->lengthName) { return Value::numeric(input.size()); }
            if (node.operation == "true" || node.operation == "false") { return Value::numeric(node.operation == "true"); }
            return variable(node.operation, line).value;
        }
        if (node.kind == Expr::Index)
        {
            const auto arrayValue = evaluate(node.children[0]);
            const int index = integer(evaluate(node.children[1]), line);
            const auto &array = bytes(arrayValue, line);
            range(array, index, 1, line);
            return Value::numeric(quint8(array[index]));
        }
        if (node.kind == Expr::Call) { return call(node); }
        if (node.kind == Expr::Cast) { return converted(evaluate(node.children[0]), node.operation, line); }
        if (node.kind == Expr::Conditional)
        {
            return evaluate(node.children[numeric(evaluate(node.children[0]), line) != 0 ? 1 : 2]);
        }
        const auto left = evaluate(node.children[0]);
        if (node.kind == Expr::Unary || node.kind == Expr::Postfix)
        {
            const double number = numeric(left, line);
            if (node.operation == "++" || node.operation == "--")
            {
                const auto next = store(node.children[0], Value::numeric(number + (node.operation == "++" ? 1 : -1), left.integral), line);
                return node.kind == Expr::Postfix ? left : next;
            }
            if (node.operation == "!") { return Value::numeric(number == 0); }
            if (node.operation == "~") { return Value::numeric(~bits(number)); }
            return Value::numeric(node.operation == "-" ? -number : number, left.integral);
        }
        if (node.kind == Expr::Binary && node.operation == "&&" && numeric(left, line) == 0) { return Value::numeric(0); }
        if (node.kind == Expr::Binary && node.operation == "||" && numeric(left, line) != 0) { return Value::numeric(1); }
        const auto right = evaluate(node.children[1]);
        if (node.kind == Expr::Assign)
        {
            return store(node.children[0], node.operation == "=" ? right
                : binary(node.operation.left(node.operation.size() - 1), left, right, line), line);
        }
        return binary(node.operation, left, right, line);
    }
    Flow execute(const Block &block)
    {
        if (!block) { return Normal; }
        const auto &node = *block;
        step(node.line);
        if (node.kind == Statement::Sequence)
        {
            scopes.emplace_back();
            Flow flow = Normal;
            for (const auto &child : node.children)
            {
                flow = execute(child);
                if (flow != Normal) { break; }
            }
            scopes.pop_back();
            return flow;
        }
        if (node.kind == Statement::Declaration)
        {
            if (scopes.back().contains(node.name)) { throw ScriptError(node.line, "同一作用域重复声明变量：" + node.name); }
            if (scopes.back().size() >= 256 || persistent.size() >= 256) { throw ScriptError(node.line, "变量数量超限"); }
            Variable entry;
            entry.type = node.type;
            entry.constant = node.constant;
            if (node.persistent) { entry.persistentKey = QString::number(node.identity) + ":" + node.name; }
            if (node.persistent && persistent.contains(entry.persistentKey)) { entry = persistent[entry.persistentKey]; }
            else
            {
                entry.value = converted(node.first ? evaluate(node.first) : Value(), node.type, node.line);
                if (node.persistent) { persistent[entry.persistentKey] = entry; }
            }
            scopes.back()[node.name] = entry;
        }
        else if (node.kind == Statement::Evaluate) { evaluate(node.first); }
        else if (node.kind == Statement::If)
        {
            return execute(numeric(evaluate(node.first), node.line) != 0 ? node.body : node.otherwise);
        }
        else if (node.kind == Statement::While || node.kind == Statement::For)
        {
            scopes.emplace_back();
            execute(node.initial);
            Flow flow = Normal;
            while (!node.first || numeric(evaluate(node.first), node.line) != 0)
            {
                step(node.line);
                flow = execute(node.body);
                if (flow == Break || flow == Return) { break; }
                if (node.second) { evaluate(node.second); }
            }
            scopes.pop_back();
            if (flow == Return) { return Return; }
        }
        else if (node.kind == Statement::Break) { return Break; }
        else if (node.kind == Statement::Continue) { return Continue; }
        else if (node.kind == Statement::Return) { return Return; }
        return Normal;
    }
};

FrameScriptRuntime::FrameScriptRuntime(std::shared_ptr<const FrameScriptProgram> program)
    : state(std::make_unique<Impl>())
{
    if (!program) { throw ScriptError(0, "脚本未编译"); }
    state->program = program->syntax;
}

FrameScriptRuntime::~FrameScriptRuntime() = default;

QJsonArray FrameScriptRuntime::feed(const QByteArray &data)
{
    state->input = data;
    state->steps = 0;
    state->fields = {};
    state->output = {};
    state->scopes.clear();
    state->scopes.emplace_back();
    state->timer.start();
    state->execute(state->program->body);
    state->flush(1);
    return state->output;
}

int FrameScriptRuntime::bufferedBytes() const
{
    return int(state->buffer.size());
}
}
