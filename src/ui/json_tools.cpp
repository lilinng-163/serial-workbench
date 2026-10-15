// SPDX-License-Identifier: Apache-2.0
#include "json_tools.h"
#include "controls.h"
#include <functional>

namespace sw
{
namespace
{
class JsonSyntax
{
public:
    const QString &text;
    qsizetype position = 0;
    QString error;

    explicit JsonSyntax(const QString &text) : text(text) {}

    void whitespace()
    {
        while (position < text.size() && QString(" \t\r\n").contains(text[position])) { ++position; }
    }

    bool fail(const QString &message)
    {
        error = message;
        return false;
    }

    bool take(QChar character)
    {
        whitespace();
        if (position >= text.size() || text[position] != character) { return false; }
        ++position;
        return true;
    }

    bool string()
    {
        if (!take('"')) { return fail("字段名和字符串必须使用双引号"); }
        while (position < text.size())
        {
            const auto character = text[position++];
            if (character == '"') { return true; }
            if (character.unicode() < 32) { return fail("字符串中不能直接出现换行或控制字符，请使用转义"); }
            if (character != '\\') { continue; }
            if (position >= text.size()) { return fail("字符串转义未结束"); }
            const auto escaped = text[position++];
            if (escaped == 'u')
            {
                for (int digit = 0; digit < 4; ++digit)
                {
                    if (position >= text.size() || !QString("0123456789abcdefABCDEF").contains(text[position]))
                    {
                        return fail("Unicode 转义需要四位十六进制数字");
                    }
                    ++position;
                }
            }
            else if (!QString("\"\\/bfnrt").contains(escaped)) { return fail("不支持的字符串转义"); }
        }
        return fail("字符串缺少结束双引号");
    }

    bool digits()
    {
        const auto begin = position;
        while (position < text.size() && text[position] >= '0' && text[position] <= '9') { ++position; }
        return position > begin;
    }

    bool value(int depth = 0)
    {
        whitespace();
        if (depth > 128) { return fail("对象或数组嵌套超过 128 层"); }
        if (position >= text.size()) { return fail("缺少 JSON 值"); }
        const auto character = text[position];
        if (character == '"') { return string(); }
        if (character == '{' || character == '[')
        {
            ++position;
            const bool object = character == '{';
            const QChar closing = object ? '}' : ']';
            if (take(closing)) { return true; }
            while (true)
            {
                if (object)
                {
                    if (!string()) { return false; }
                    if (!take(':')) { return fail("字段名后缺少冒号 :"); }
                }
                if (!value(depth + 1)) { return false; }
                if (take(closing)) { return true; }
                if (!take(',')) { return fail(object ? "对象成员间缺少逗号，或缺少 }" : "数组元素间缺少逗号，或缺少 ]"); }
                whitespace();
                if (position < text.size() && text[position] == closing) { return fail("最后一个成员后不能加逗号"); }
            }
        }
        for (const auto &literal : {QString("true"), QString("false"), QString("null")})
        {
            if (text.mid(position, literal.size()) == literal) { position += literal.size(); return true; }
        }
        if (character == '-' || (character >= '0' && character <= '9'))
        {
            if (text[position] == '-') { ++position; }
            if (position < text.size() && text[position] == '0') { ++position; }
            else if (!digits()) { return fail("数字格式不正确"); }
            if (position < text.size() && text[position] == '.')
            {
                ++position;
                if (!digits()) { return fail("小数点后需要数字"); }
            }
            if (position < text.size() && (text[position] == 'e' || text[position] == 'E'))
            {
                ++position;
                if (position < text.size() && (text[position] == '+' || text[position] == '-')) { ++position; }
                if (!digits()) { return fail("指数部分需要数字"); }
            }
            return true;
        }
        return fail("值不合法；字符串使用双引号，布尔值使用 true 或 false");
    }

    bool check()
    {
        if (!value()) { return false; }
        whitespace();
        return position == text.size() || fail("JSON 结束后存在多余内容");
    }
};

QString cString(const QString &text)
{
    QString result = "\"";
    for (const unsigned char byte : text.toUtf8())
    {
        if (byte == '"' || byte == '\\') { result += '\\'; result += QChar(byte); }
        else if (byte >= 32 && byte < 127) { result += QChar(byte); }
        else { result += "\\" + QString::number(byte, 8).rightJustified(3, '0'); }
    }
    return result + '"';
}

QByteArray jsonBytes(const QJsonValue &value, QJsonDocument::JsonFormat format)
{
    if (value.isObject()) { return QJsonDocument(value.toObject()).toJson(format); }
    if (value.isArray()) { return QJsonDocument(value.toArray()).toJson(format); }
    auto bytes = QJsonDocument(QJsonArray {value}).toJson(QJsonDocument::Compact);
    return bytes.mid(1, bytes.size() - 2);
}

QString jsonError(QJsonParseError::ParseError error)
{
    switch (error)
    {
        case QJsonParseError::UnterminatedObject: return "对象缺少右花括号 }";
        case QJsonParseError::MissingNameSeparator: return "字段名后缺少冒号 :";
        case QJsonParseError::UnterminatedArray: return "数组缺少右方括号 ]";
        case QJsonParseError::MissingValueSeparator: return "成员之间缺少逗号 ,";
        case QJsonParseError::IllegalValue: return "值不合法；字符串和字段名需要双引号";
        case QJsonParseError::TerminationByNumber: return "数字未完整结束";
        case QJsonParseError::IllegalNumber: return "数字格式不正确";
        case QJsonParseError::IllegalEscapeSequence: return "字符串转义不正确";
        case QJsonParseError::IllegalUTF8String: return "字符串编码不正确";
        case QJsonParseError::UnterminatedString: return "字符串缺少结束双引号";
        case QJsonParseError::MissingObject: return "请输入一个 JSON 值";
        case QJsonParseError::DeepNesting: return "对象或数组嵌套过深";
        case QJsonParseError::DocumentTooLarge: return "JSON 内容过大";
        case QJsonParseError::GarbageAtEnd: return "JSON 结束后存在多余内容";
        default: return "JSON 语法不正确";
    }
}

QString typeName(const QJsonValue &value)
{
    if (value.isObject()) { return "Object"; }
    if (value.isArray()) { return "Array"; }
    if (value.isString()) { return "String"; }
    if (value.isBool()) { return "Bool"; }
    if (value.isDouble()) { return "Number"; }
    return "Null";
}

QString creation(const QJsonValue &value)
{
    QString argument;
    if (value.isString()) { argument = cString(value.toString()); }
    if (value.isDouble()) { argument = QLocale::c().toString(value.toDouble(), 'g', 17); }
    if (value.isBool()) { argument = value.toBool() ? "1" : "0"; }
    return "cJSON_Create" + typeName(value) + "(" + argument + ")";
}

class Generator
{
public:
    QStringList parsing;
    QStringList building;
    int next = 0;
    QSet<QString> identifiers {"root", "root_value", "text", "end", "invalid", "failed", "parse_message", "build_message"};

    QString identifier(QString suggested)
    {
        static const QSet<QString> keywords {
            "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum",
            "extern", "float", "for", "goto", "if", "int", "long", "register", "return", "short", "signed", "sizeof",
            "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "inline", "restrict",
            "class", "new", "delete", "this", "true", "false", "nullptr", "bool", "namespace", "template", "typename",
            "public", "private", "protected", "virtual", "operator", "try", "catch", "throw", "using", "friend",
            "explicit", "export", "mutable", "constexpr", "consteval", "constinit", "thread_local", "static_assert",
            "alignas", "alignof", "decltype", "noexcept", "dynamic_cast", "static_cast", "const_cast", "reinterpret_cast",
            "typeid", "wchar_t", "char8_t", "char16_t", "char32_t", "and", "or", "xor", "not", "bitand", "bitor",
            "compl", "and_eq", "or_eq", "xor_eq", "not_eq", "asm", "concept", "requires", "co_await", "co_return",
            "co_yield", "typeof", "typeof_unqual", "NULL", "size_t"
        };
        suggested.replace(QRegularExpression("[^A-Za-z0-9_]+"), "_");
        if (suggested.isEmpty() || suggested == "_") { suggested = "field"; }
        if (suggested[0].isDigit() || suggested.startsWith('_') || suggested.startsWith("cJSON") || suggested.startsWith("CJSON") || keywords.contains(suggested))
        {
            suggested.prepend("field_");
        }
        suggested.replace(QRegularExpression("_+"), "_");
        suggested = suggested.left(48);
        QString name = suggested;
        int suffix = 2;
        while (identifiers.contains(name) || identifiers.contains(name + "_value"))
        {
            name = suggested + "_" + QString::number(suffix++);
        }
        identifiers.insert(name);
        identifiers.insert(name + "_value");
        return name;
    }

    void line(QStringList &output, int depth, const QString &text)
    {
        output.append(QString(depth * 4, ' ') + text);
    }

    void node(const QJsonValue &value, const QString &name, int depth)
    {
        line(parsing, depth, "if (!cJSON_Is" + typeName(value) + "(" + name + ")) { goto invalid; }");
        if (value.isString())
        {
            line(parsing, depth, "const char *" + name + "_value = cJSON_GetStringValue(" + name + ");");
            line(parsing, depth, "(void)" + name + "_value; /* 在释放 root 前使用或复制字符串。 */");
        }
        if (value.isDouble() || value.isBool())
        {
            const QString getter = value.isDouble() ? name + "->valuedouble" : "cJSON_IsTrue(" + name + ")";
            line(parsing, depth, (value.isDouble() ? "double " : "int ") + name + "_value = " + getter + ";");
            line(parsing, depth, "(void)" + name + "_value; /* 在这里使用解析得到的值。 */");
        }
        if (value.isObject())
        {
            const auto object = value.toObject();
            for (auto field = object.begin(); field != object.end(); ++field)
            {
                child(field.value(), name == "root" ? field.key() : name + "_" + field.key(), "cJSON_GetObjectItemCaseSensitive(" + name + ", " + cString(field.key()) + ")",
                    "cJSON_AddItemToObject(" + name + ", " + cString(field.key()) + ", ", depth);
            }
        }
        if (value.isArray())
        {
            const auto array = value.toArray();
            if (!array.isEmpty())
            {
                line(parsing, depth, "if (cJSON_GetArraySize(" + name + ") < " + QString::number(array.size()) + ") { goto invalid; }");
            }
            for (qsizetype index = 0; index < array.size(); ++index)
            {
                child(array[index], (name == "root" ? "item" : name) + "_" + QString::number(index), "cJSON_GetArrayItem(" + name + ", " + QString::number(index) + ")",
                    "cJSON_AddItemToArray(" + name + ", ", depth);
            }
        }
    }

    void child(const QJsonValue &value, const QString &suggested, const QString &lookup, const QString &add, int depth)
    {
        ++next;
        const QString name = identifier(suggested);
        line(parsing, depth, "{");
        line(parsing, depth + 1, "const cJSON *" + name + " = " + lookup + ";");
        line(building, depth, "{");
        line(building, depth + 1, "cJSON *" + name + " = " + creation(value) + ";");
        line(building, depth + 1, "if (" + name + " == NULL) { goto failed; }");
        line(building, depth + 1, "if (!" + add + name + "))");
        line(building, depth + 1, "{");
        line(building, depth + 2, "cJSON_Delete(" + name + ");");
        line(building, depth + 2, "goto failed;");
        line(building, depth + 1, "}");
        node(value, name, depth + 1);
        line(parsing, depth, "}");
        line(building, depth, "}");
    }
};

bool canGenerate(const QJsonValue &value, int depth, int &nodes)
{
    if (depth > 32 || ++nodes > 512) { return false; }
    if (value.isString() && value.toString().contains(QChar(0))) { return false; }
    if (value.isObject())
    {
        const auto object = value.toObject();
        for (auto entry = object.begin(); entry != object.end(); ++entry)
        {
            if (entry.key().contains(QChar(0)) || !canGenerate(entry.value(), depth + 1, nodes)) { return false; }
        }
    }
    if (value.isArray())
    {
        for (const auto &entry : value.toArray())
        {
            if (!canGenerate(entry, depth + 1, nodes)) { return false; }
        }
    }
    return true;
}

class JsonHighlighter : public QSyntaxHighlighter
{
public:
    explicit JsonHighlighter(QTextDocument *document) : QSyntaxHighlighter(document) {}
    void highlightBlock(const QString &text) override
    {
        static const QRegularExpression tokens(R"("(?:\\.|[^"\\])*"|-?\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b|\b(?:true|false|null)\b)");
        auto matches = tokens.globalMatch(text);
        while (matches.hasNext())
        {
            const auto match = matches.next();
            const auto token = match.captured();
            QTextCharFormat format;
            if (token.startsWith('"'))
            {
                auto next = match.capturedEnd();
                while (next < text.size() && text[next].isSpace()) { ++next; }
                const bool key = next < text.size() && text[next] == ':';
                format.setForeground(QColor(key ? "#518ca7" : "#8c9861"));
                if (key) { format.setFontWeight(QFont::DemiBold); }
            }
            else { format.setForeground(QColor("#c68c49")); }
            setFormat(match.capturedStart(), match.capturedLength(), format);
        }
    }
};

QPlainTextEdit *codeEditor(const QString &name, bool readOnly, bool json)
{
    auto editor = new QPlainTextEdit();
    editor->setObjectName(name);
    editor->setReadOnly(readOnly);
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->setFont(Theme::defaultDataFont());
    editor->setStyleSheet("font-family: 'Consolas', 'Microsoft YaHei UI'; font-size: 12pt;");
    editor->setTabStopDistance(QFontMetricsF(editor->font()).horizontalAdvance(' ') * 4);
    if (json) { new JsonHighlighter(editor->document()); }
    return editor;
}

class CJsonReference : public QTextBrowser
{
public:
    QString content;
    QString currentStyle;

    void refreshColors()
    {
        if (content.isEmpty()) { return; }
        const auto color = palette().color(QPalette::Base).lightness() < 128 ? "#edaa50" : "#477255";
        const QString style = QString("body { font-size: 14px; } code { font-family: Consolas; font-size: 14px; } "
            "h3 { font-size: 17px; } a { color: %1; text-decoration: underline; }").arg(color);
        if (currentStyle == style) { return; }
        currentStyle = style;
        document()->setDefaultStyleSheet(style);
        setHtml(content);
    }

    void setReferenceHtml(const QString &html)
    {
        content = html;
        ensurePolished();
        refreshColors();
    }

    void changeEvent(QEvent *event) override
    {
        QTextBrowser::changeEvent(event);
        if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        {
            refreshColors();
        }
    }
};

class JsonAssistant : public QDialog
{
public:
    QPlainTextEdit *input;
    QPlainTextEdit *formatted;
    QPlainTextEdit *parse;
    QPlainTextEdit *build;
    QLabel *status;
    QTabWidget *tabs;
    QTimer debounce;
    JsonAssistance analysis;

    JsonAssistant(QWidget *parent, const QString &initial) : QDialog(parent)
    {
        setObjectName("jsonAssistantDialog");
        setWindowTitle("JSON 助手");
        setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
        setMinimumSize(760, 480);
        resize(1100, 720);
        auto body = new QVBoxLayout(this);
        body->setContentsMargins(18, 16, 18, 16);
        auto tools = new QHBoxLayout();
        auto paste = new QPushButton("粘贴 JSON");
        paste->setObjectName("pasteJson");
        auto expand = new QPushButton("展开输入");
        expand->setObjectName("expandJson");
        auto compact = new QPushButton("压缩输入");
        compact->setObjectName("compactJson");
        auto example = new QPushButton("示例");
        auto copy = new QPushButton("复制当前结果");
        copy->setObjectName("copyJsonResult");
        copy->setProperty("variant", "outline");
        for (auto button : {paste, expand, compact, example}) { tools->addWidget(button); }
        tools->addStretch();
        tools->addWidget(copy);
        body->addLayout(tools);
        tabs = new QTabWidget();
        tabs->setObjectName("jsonAssistantTabs");
        body->addWidget(tabs, 1);
        auto editPage = new QWidget();
        auto editBody = new QVBoxLayout(editPage);
        auto split = new Splitter(Qt::Horizontal);
        auto column = [&](const QString &title, QPlainTextEdit *editor)
        {
            auto widget = new QWidget();
            auto layout = new QVBoxLayout(widget);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->addWidget(new QLabel(title));
            layout->addWidget(editor, 1);
            split->addWidget(widget);
        };
        input = codeEditor("jsonInput", false, true);
        input->setPlaceholderText("在这里粘贴单行或多行 JSON，右侧会自动展开。\n字符串与字段名用双引号；成员之间用逗号；末尾不加逗号。");
        formatted = codeEditor("jsonFormatted", true, true);
        column("输入 JSON", input);
        column("自动格式化", formatted);
        split->setSizes({500, 500});
        editBody->addWidget(split);
        tabs->addTab(editPage, "校验与格式化");
        parse = codeEditor("cjsonParseCode", true, false);
        build = codeEditor("cjsonBuildCode", true, false);
        tabs->addTab(parse, "cJSON 解析");
        tabs->addTab(build, "cJSON 组装");
        auto reference = new CJsonReference();
        reference->setObjectName("cjsonReference");
        reference->setOpenExternalLinks(true);
        reference->setReferenceHtml(R"(<p>本工具依据开源项目 cJSON 的函数生成 JSON 解析和组装示例。</p>
<h3>解析：从 JSON 读取数据</h3>
<p><code>cJSON_ParseWithOpts(text, &amp;end, 1)</code>：把完整 JSON 文本解析为树，并检查尾部。<br>
<code>cJSON_GetObjectItemCaseSensitive(root, "humi")</code>：区分大小写，查找字段 humi。<br>
<code>cJSON_IsNumber(item)</code>、<code>cJSON_IsString(item)</code>、<code>cJSON_IsBool(item)</code>：读取前先检查类型。<br>
<code>item-&gt;valuedouble</code>：读取数字，包含整数和小数。<br>
<code>cJSON_GetStringValue(item)</code>：读取字符串；<code>cJSON_IsTrue(item)</code>：读取布尔值。<br>
<code>cJSON_GetArraySize(array)</code>：获取数组长度；<code>cJSON_GetArrayItem(array, index)</code>：按索引读取元素。</p>
<h3>组装：把数据写成 JSON</h3>
<p><code>cJSON_CreateObject()</code>、<code>cJSON_CreateArray()</code>：创建对象或数组。<br>
<code>cJSON_AddNumberToObject(root, "humi", 65.5)</code>：添加数字字段。<br>
<code>cJSON_AddStringToObject(root, "name", "sensor")</code>：添加字符串字段。<br>
<code>cJSON_AddBoolToObject(root, "online", 1)</code>：添加布尔字段。<br>
<code>cJSON_AddItemToObject(parent, "key", item)</code>、<code>cJSON_AddItemToArray(parent, item)</code>：添加已创建的子节点。<br>
<code>cJSON_Print(root)</code>：输出多行 JSON；<code>cJSON_PrintUnformatted(root)</code>：输出紧凑 JSON。</p>
<h3>释放内存</h3>
<p><code>cJSON_Delete(root)</code>：递归释放解析树或组装树，已添加的子节点会一并释放。<br>
<code>cJSON_free(text)</code>：释放打印函数返回的字符串。树中的字符串需要在释放树前使用或复制。</p>
<p><a href="https://github.com/DaveGamble/cJSON">查看 cJSON 开源项目</a></p>)");
        tabs->addTab(reference, "常用函数");
        status = new QLabel();
        status->setObjectName("jsonStatus");
        status->setWordWrap(true);
        body->addWidget(status);
        auto hint = new QLabel("解析和组装代码会随有效 JSON 自动更新，可复制后在自己的项目中使用。");
        hint->setWordWrap(true);
        body->addWidget(hint);
        debounce.setSingleShot(true);
        debounce.setInterval(160);
        connect(input, &QPlainTextEdit::textChanged, &debounce, qOverload<>(&QTimer::start));
        connect(&debounce, &QTimer::timeout, this, [this] { refresh(); });
        connect(paste, &QPushButton::clicked, input, [this] { input->setPlainText(QApplication::clipboard()->text()); });
        connect(example, &QPushButton::clicked, input, [this]
        {
            input->setPlainText(R"({"device":"sensor","temperature":25.5,"online":true,"samples":[1,2,3],"config":{"period_ms":20}})");
        });
        connect(expand, &QPushButton::clicked, input, [this]
        {
            refresh();
            if (analysis.valid) { input->setPlainText(analysis.formatted); }
        });
        connect(compact, &QPushButton::clicked, input, [this]
        {
            refresh();
            if (analysis.valid) { input->setPlainText(analysis.compact); }
        });
        connect(copy, &QPushButton::clicked, this, [this, reference]
        {
            refresh();
            const QString text = tabs->currentIndex() == 1 ? analysis.parseCode : tabs->currentIndex() == 2 ? analysis.buildCode
                : tabs->currentIndex() == 3 ? reference->toPlainText() : analysis.formatted;
            if (!text.isEmpty()) { QApplication::clipboard()->setText(text); }
        });
        makeDialogScrollable(this, QSize(560, 320));
        input->setPlainText(initial);
        refresh();
    }

    void refresh()
    {
        analysis = analyzeJson(input->toPlainText());
        formatted->setPlainText(analysis.formatted);
        parse->setPlainText(analysis.parseCode);
        build->setPlainText(analysis.buildCode);
        status->setText(analysis.message);
        QList<QTextEdit::ExtraSelection> selections;
        if (analysis.errorPosition >= 0)
        {
            QTextEdit::ExtraSelection selection;
            selection.cursor = input->textCursor();
            selection.cursor.setPosition(analysis.errorPosition);
            selection.cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            selection.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
            selection.format.setUnderlineColor(QColor("#d96650"));
            selections.append(selection);
        }
        input->setExtraSelections(selections);
    }
};
}

JsonAssistance analyzeJson(const QString &text)
{
    JsonAssistance result;
    if (text.trimmed().isEmpty())
    {
        result.message = "粘贴 JSON 后自动校验并展开；支持对象、数组及单个值。";
        return result;
    }
    const auto bytes = text.toUtf8();
    if (bytes.size() > 1024 * 1024)
    {
        result.message = "输入超过 1 MB，请缩小内容后重试。";
        return result;
    }
    QJsonParseError error;
    JsonSyntax syntax(text);
    if (!syntax.check())
    {
        result.errorPosition = int(syntax.position);
        const auto prefix = text.left(syntax.position);
        result.message = QString("第 %1 行，第 %2 列：%3").arg(prefix.count('\n') + 1)
            .arg(prefix.size() - prefix.lastIndexOf('\n')).arg(syntax.error);
        return result;
    }
    const auto document = QJsonDocument::fromJson("[" + bytes + "]", &error);
    if (error.error != QJsonParseError::NoError || document.array().size() != 1)
    {
        const auto prefix = QString::fromUtf8(bytes.left(qBound(0, error.offset - 1, int(bytes.size()))));
        result.errorPosition = int(prefix.size());
        const int line = prefix.count('\n') + 1;
        const int column = int(prefix.size()) - int(prefix.lastIndexOf('\n'));
        result.message = QString("第 %1 行，第 %2 列：%3").arg(line).arg(column)
            .arg(error.error == QJsonParseError::NoError ? "只允许一个 JSON 值" : jsonError(error.error));
        return result;
    }
    const auto value = document.array().first();
    result.valid = true;
    result.formatted = QString::fromUtf8(jsonBytes(value, QJsonDocument::Indented));
    result.compact = QString::fromUtf8(jsonBytes(value, QJsonDocument::Compact));
    result.message = "JSON 语法正确；已更新格式化结果与 cJSON 示例。";
    int nodes = 0;
    if (!canGenerate(value, 0, nodes))
    {
        result.message = "JSON 语法正确；代码生成支持最多 512 个值、32 层嵌套，且不支持字符串中的空字符。";
        return result;
    }
    Generator generator;
    generator.line(generator.parsing, 1, "{");
    generator.line(generator.building, 1, "{");
    generator.node(value, "root", 2);
    generator.line(generator.parsing, 1, "}");
    generator.line(generator.building, 1, "}");
    result.parseCode = "/* 根据 JSON 示例检查结构；0 表示成功，-1 表示语法或结构不符。 */\n"
        "#include <stddef.h>\n#include \"cJSON.h\"\n\nint parse_message(const char *text)\n{\n"
        "    const char *end = NULL;\n    cJSON *root;\n    if (text == NULL) { return -1; }\n"
        "    root = cJSON_ParseWithOpts(text, &end, 1);\n    if (root == NULL) { return -1; }\n"
        + generator.parsing.join('\n')
        + "\n    cJSON_Delete(root);\n    return 0;\n\ninvalid:\n    cJSON_Delete(root);\n    return -1;\n}\n";
    result.buildCode = "/* 使用示例值组装 JSON；返回值由调用方使用 cJSON_free() 释放。 */\n"
        "#include <stddef.h>\n#include \"cJSON.h\"\n\nchar *build_message(void)\n{\n"
        "    cJSON *root = " + creation(value) + ";\n    char *text = NULL;\n    if (root == NULL) { return NULL; }\n"
        + generator.building.join('\n')
        + "\n    text = cJSON_PrintUnformatted(root);\n    cJSON_Delete(root);\n    return text;\n\n"
        "failed:\n    cJSON_Delete(root);\n    return NULL;\n}\n";
    // Scalar or empty containers have no fallible child additions.
    if (generator.next == 0)
    {
        result.buildCode.remove("\nfailed:\n    cJSON_Delete(root);\n    return NULL;\n");
    }
    return result;
}

QDialog *showJsonAssistant(QWidget *parent, const QString &initial)
{
    auto existing = parent->findChild<QDialog *>("jsonAssistantDialog", Qt::FindDirectChildrenOnly);
    auto dialog = existing ? existing : new JsonAssistant(parent, initial);
    dialog->showNormal();
    dialog->raise();
    dialog->activateWindow();
    return dialog;
}
}
