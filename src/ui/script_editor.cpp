// SPDX-License-Identifier: Apache-2.0

#include "window.h"
#include "usage_guide.h"
#include <QSyntaxHighlighter>
#include <QTextBlock>

namespace sw
{
namespace
{
class CodeEdit : public QPlainTextEdit
{
public:
    QWidget *gutter = new QWidget(this);
    QSyntaxHighlighter *highlighter = nullptr;

    explicit CodeEdit(QWidget *parent = nullptr) : QPlainTextEdit(parent)
    {
        gutter->installEventFilter(this);
        connect(this, &QPlainTextEdit::blockCountChanged, this, [this] { updateMargin(); });
        connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect &rect, int dy)
        {
            if (dy) { gutter->scroll(0, dy); }
            else { gutter->update(0, rect.y(), gutter->width(), rect.height()); }
            if (rect.contains(viewport()->rect())) { updateMargin(); }
        });
        updateMargin();
    }
    int marginWidth() const
    {
        return 16 + fontMetrics().horizontalAdvance('9') * QString::number(blockCount()).size();
    }
    void updateMargin()
    {
        setViewportMargins(marginWidth(), 0, 0, 0);
        const auto box = contentsRect();
        gutter->setGeometry(box.left(), box.top(), marginWidth(), box.height());
    }
    void resizeEvent(QResizeEvent *event) override
    {
        QPlainTextEdit::resizeEvent(event);
        updateMargin();
    }
    void changeEvent(QEvent *event) override
    {
        QPlainTextEdit::changeEvent(event);
        if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        {
            updateMargin();
            if (highlighter) { highlighter->rehighlight(); }
        }
    }
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (object != gutter || event->type() != QEvent::Paint) { return QPlainTextEdit::eventFilter(object, event); }
        QPainter painter(gutter);
        painter.fillRect(gutter->rect(), palette().color(QPalette::Base));
        auto color = palette().color(QPalette::Text);
        color.setAlpha(130);
        painter.setPen(color);
        painter.setFont(font());
        auto block = firstVisibleBlock();
        int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
        while (block.isValid() && top <= gutter->height())
        {
            const int height = qRound(blockBoundingRect(block).height());
            if (block.isVisible() && top + height >= 0)
            {
                painter.drawText(0, top, gutter->width() - 8, fontMetrics().height(),
                                 Qt::AlignRight, QString::number(block.blockNumber() + 1));
            }
            top += height;
            block = block.next();
        }
        return true;
    }
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Tab && !event->modifiers())
        {
            insertPlainText("    ");
            return;
        }
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && !event->modifiers())
        {
            const auto before = textCursor().block().text().left(textCursor().positionInBlock());
            int count = 0;
            while (count < before.size() && before[count].isSpace()) { ++count; }
            insertPlainText("\n" + before.left(count) + (before.trimmed().endsWith('{') ? "    " : ""));
            return;
        }
        QPlainTextEdit::keyPressEvent(event);
    }
    void showError(int line)
    {
        if (line < 1) { return; }
        auto block = document()->findBlockByLineNumber(qMin(line - 1, blockCount() - 1));
        if (!block.isValid()) { return; }
        QTextEdit::ExtraSelection selection;
        selection.cursor = QTextCursor(block);
        selection.format.setBackground(QColor(205, 90, 65, 55));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        setExtraSelections({selection});
        setTextCursor(QTextCursor(block));
        centerCursor();
    }
};

class CodeColors : public QSyntaxHighlighter
{
public:
    CodeEdit *editor;
    explicit CodeColors(CodeEdit *owner) : QSyntaxHighlighter(owner->document()), editor(owner) {}
    void highlightBlock(const QString &text) override
    {
        const bool dark = editor->palette().color(QPalette::Text).lightness() > 128;
        const auto mark = [&](const QString &pattern, const QColor &color)
        {
            auto matches = QRegularExpression(pattern).globalMatch(text);
            QTextCharFormat format;
            format.setForeground(color);
            while (matches.hasNext())
            {
                const auto match = matches.next();
                setFormat(match.capturedStart(), match.capturedLength(), format);
            }
        };
        mark(R"(\b(void|const|static|int|float|double|bool|byte|char|short|long|unsigned|signed|size_t|u?int(8|16|32)_t|if|else|while|for|break|continue|return|true|false)\b)", dark ? QColor("#e9bb78") : QColor("#945326"));
        mark(R"(\b(0[xX][0-9a-fA-F]+|\d+(\.\d+)?([eE][+-]?\d+)?)\b)", dark ? QColor("#9fc9b5") : QColor("#28766c"));
        mark(R"(\b[a-zA-Z_]\w*(?=\s*\())", dark ? QColor("#b6cddb") : QColor("#3c6582"));
        mark(R"("(\\.|[^"\\])*")", dark ? QColor("#b6ce9b") : QColor("#587744"));
        mark(R"(//.*$)", dark ? QColor("#91a18b") : QColor("#758371"));
        setCurrentBlockState(0);
        int start = previousBlockState() == 1 ? 0 : text.indexOf("/*");
        while (start >= 0)
        {
            const int end = text.indexOf("*/", start + (previousBlockState() == 1 && start == 0 ? 0 : 2));
            const int count = end < 0 ? int(text.size()) - start : end + 2 - start;
            setFormat(start, count, dark ? QColor("#91a18b") : QColor("#758371"));
            if (end < 0) { setCurrentBlockState(1); break; }
            start = text.indexOf("/*", end + 2);
        }
    }
};

QPushButton *action(const QString &text, const QString &name = "outline")
{
    auto result = new QPushButton(text);
    result->setObjectName(name);
    result->setAutoDefault(false);
    result->setDefault(false);
    return result;
}

class ScriptEditor : public QDialog
{
public:
    QPointer<Pane> owner;
    CodeEdit *code;
    QPlainTextEdit *hex;
    QTreeWidget *results;
    QLabel *status;
    QCheckBox *enable;
    QPushButton *run;
    FrameScriptProcessor *testProcessor;
    int remaining = 0;
    int frames = 0;

    explicit ScriptEditor(Pane *pane, bool nonModal) : QDialog(pane->window), owner(pane)
    {
        setObjectName("scriptEditor");
        setWindowTitle("自定义帧解析 · C 风格脚本");
        setWindowModality(nonModal ? Qt::NonModal : Qt::WindowModal);
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowFlag(Qt::WindowMaximizeButtonHint);
        setSizeGripEnabled(true);
        setMinimumSize(560, 440);
        resize(1080, 760);
        auto body = new QVBoxLayout(this);
        body->setContentsMargins(20, 16, 20, 16);
        body->setSpacing(12);
        auto heading = new QLabel("自定义帧解析");
        heading->setObjectName("section");
        body->addWidget(heading);
        auto tools = new QHBoxLayout();
        auto example = action("载入示例");
        auto load = action("导入脚本");
        auto save = action("导出脚本");
        auto check = action("语法检查");
        auto ai = action("AI 提示词");
        ai->setObjectName("scriptAiPrompt");
        check->setObjectName("checkScript");
        tools->addWidget(example);
        tools->addWidget(load);
        tools->addWidget(save);
        tools->addWidget(ai);
        tools->addStretch();
        tools->addWidget(check);
        body->addLayout(tools);

        auto split = new Splitter(Qt::Horizontal);
        code = new CodeEdit();
        code->setObjectName("scriptCode");
        code->setFont(pane->window->dataFont);
        code->setStyleSheet(QString("font-family: '%1'; font-size: %2pt;")
            .arg(pane->window->dataFont.families().join("', '")).arg(pane->window->dataFont.pointSizeF()));
        code->setLineWrapMode(QPlainTextEdit::NoWrap);
        code->setPlainText(pane->scriptCode);
        code->highlighter = new CodeColors(code);
        split->addWidget(code);
        auto tabs = new QTabWidget();
        auto test = new QWidget();
        auto testBody = new QVBoxLayout(test);
        testBody->setContentsMargins(8, 10, 8, 8);
        auto tip = new QLabel("每行 HEX 模拟一次接收，可测试半帧、多帧和噪声。");
        tip->setWordWrap(true);
        testBody->addWidget(tip);
        hex = new QPlainTextEdit("AA 55 C4\n09 E4 0C BC 0D\nAA 55 C4 09 E4 0C BC 0D");
        hex->setObjectName("scriptTestHex");
        hex->setMaximumHeight(140);
        testBody->addWidget(hex);
        run = action("试解析");
        run->setObjectName("testScript");
        testBody->addWidget(run);
        results = new QTreeWidget();
        results->setObjectName("scriptTestResults");
        results->setRootIsDecorated(false);
        results->setHeaderLabels({"帧", "字段值"});
        results->setColumnWidth(0, 45);
        results->header()->setStretchLastSection(true);
        results->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        testBody->addWidget(results, 1);
        tabs->addTab(test, "试解析");
        auto help = new QTextBrowser();
        help->setHtml(
            "<h3>固定入口</h3><pre>void main(const uint8_t data[], int length)</pre>"
            "<p>每次收到一批字节调用一次。data 和 buffer 为只读数组，可用 data[i] / buffer[i] 取值。"
            "接收批次可能包含半帧或多帧；缓存和 static 数值变量跨调用保留。</p>"
            "<h3>缓存与输出</h3><p><b>buffer_append(data, length)</b> 追加字节<br>"
            "<b>buffer_size()</b> 缓存长度<br><b>buffer_drop(n)</b> 消费字节，并提交本帧 emit 字段<br>"
            "<b>buffer_clear()</b> 清空缓存<br><b>emit(\"名称\", 数值)</b> 输出字段<br>"
            "<b>emit_frame()</b> 提交一帧，main 返回时也会提交尚未提交的字段。"
            "结果显示在收发页的脚本解析分区；波形与其他控件独立解析设备发送的指定格式文本。</p>"
            "<h3>读取与校验</h3><p><b>read_u8 / read_i8(array, offset)</b><br>"
            "<b>read_u16 / read_i16 / read_u32 / read_i32 / read_f32</b> 加 <b>_le</b> 或 <b>_be</b>，参数为数组与偏移。<br>"
            "<b>sum8 / xor8 / crc16_modbus(array, start, count)</b><br><b>min / max / abs</b></p>"
            "<h3>C 风格语法</h3><p>支持数值变量、const、static、类型转换、if / else、for、while、"
            "break、continue、return、算术与 32 位无符号位运算。整数除法向零截断。"
            "第一版只提供 main 与内置函数，不支持头文件、结构体、动态内存或用户函数。</p>"
            "<h3>执行范围</h3><p>仅用于串口会话，每个串口独立缓存。重新启用、应用代码或重连时重置状态。"
            "缓存最多 64 KiB，每次最多 20 万执行步、约 100ms、1024 帧，每帧最多 16 个字段。"
            "错误会显示行号并停用解析，连接和原始报文继续保留。试解析独立运行，不向硬件发送数据。</p>");
        tabs->addTab(help, "函数说明");
        split->addWidget(tabs);
        split->setStretchFactor(0, 3);
        split->setStretchFactor(1, 2);
        split->setSizes({620, 400});
        body->addWidget(split, 1);
        status = new QLabel("固定 main(data, length) 入口 · 应用后从新收到的数据开始");
        status->setObjectName("scriptEditorStatus");
        status->setWordWrap(true);
        status->setTextInteractionFlags(Qt::TextSelectableByMouse);
        body->addWidget(status);
        auto footer = new QHBoxLayout();
        enable = new QCheckBox("应用后启用脚本");
        enable->setObjectName("scriptEnabled");
        enable->setChecked(pane->channel ? pane->channel->scriptEnabled : pane->scriptRequested);
        footer->addWidget(enable);
        footer->addStretch();
        auto cancel = action("取消");
        auto apply = action("保存并关闭", "primary");
        apply->setProperty("action", "applyScript");
        footer->addWidget(cancel);
        footer->addWidget(apply);
        body->addLayout(footer);
        makeDialogScrollable(this, QSize(560, 320));

        testProcessor = new FrameScriptProcessor(this);
        connect(testProcessor, &FrameScriptProcessor::samplesReady, this,
                [this](const QString &, const QJsonArray &samples, qint64)
        {
            for (const auto &sample : samples)
            {
                if (frames >= 4096) { break; }
                auto item = new QTreeWidgetItem(results, {QString::number(++frames),
                    QString::fromUtf8(QJsonDocument(sample.toObject()).toJson(QJsonDocument::Compact))});
                item->setToolTip(1, item->text(1));
            }
        });
        connect(testProcessor, &FrameScriptProcessor::batchFinished, this, [this](int buffered)
        {
            if (remaining > 0 && --remaining == 0)
            {
                run->setEnabled(true);
                status->setText(QString("试解析完成：%1 帧，缓存剩余 %2 字节").arg(frames).arg(buffered));
            }
        });
        connect(testProcessor, &FrameScriptProcessor::failed, this, [this](const QString &message)
        {
            testProcessor->configure(nullptr);
            remaining = 0;
            run->setEnabled(true);
            showFailure(message);
        });
        connect(run, &QPushButton::clicked, this, [this] { testInput(); });
        connect(check, &QPushButton::clicked, this, [this]
        {
            if (compile()) { status->setText("语法检查通过，可使用右侧 HEX 样本验证解析逻辑。"); }
        });
        connect(ai, &QPushButton::clicked, this, [this]
        {
            if (!owner) { return; }
            const auto config = owner->channel ? owner->channel->config : owner->draft;
            const QJsonObject context {{"channel_id", owner->channel ? owner->channel->id : QString()},
                {"protocol", config.kind}, {"port", config.port}, {"mcp_url", owner->window->mcp->url()}};
            auto dialog = new QDialog(this);
            dialog->setObjectName("scriptAiPromptDialog");
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setWindowTitle("AI 提示词 · 串口帧脚本");
            dialog->setStyleSheet(owner->window->theme.stylesheet());
            dialog->resize(780, 600);
            auto layout = new QVBoxLayout(dialog);
            auto hint = new QLabel("复制后补充帧协议，再交给 AI。内容包含脚本约束、示例、当前代码和 HEX 样本；不会发送给 AI 或修改当前脚本。");
            hint->setWordWrap(true);
            layout->addWidget(hint);
            auto preview = new QPlainTextEdit(UsageGuide::scriptPrompt(code->toPlainText(), hex->toPlainText(), context));
            preview->setObjectName("scriptAiPromptText");
            preview->setFont(owner->window->dataFont);
            preview->setReadOnly(true);
            layout->addWidget(preview, 1);
            auto buttons = new QHBoxLayout();
            auto copy = action("复制提示词", "primary");
            copy->setObjectName("copyScriptAiPrompt");
            auto close = action("关闭");
            buttons->addStretch();
            buttons->addWidget(copy);
            buttons->addWidget(close);
            layout->addLayout(buttons);
            connect(copy, &QPushButton::clicked, dialog, [preview, copy]
            {
                QApplication::clipboard()->setText(preview->toPlainText());
                copy->setText("已复制");
            });
            connect(close, &QPushButton::clicked, dialog, &QDialog::accept);
            makeDialogScrollable(dialog, QSize(480, 300));
            dialog->setWindowModality(Qt::WindowModal);
            dialog->open();
        });
        connect(example, &QPushButton::clicked, this, [this]
        {
            auto cursor = code->textCursor();
            cursor.select(QTextCursor::Document);
            cursor.insertText(FrameScriptProgram::example());
        });
        connect(load, &QPushButton::clicked, this, [this]
        {
            const auto path = QFileDialog::getOpenFileName(this, "导入解析脚本", {}, "解析脚本 (*.c *.txt);;全部文件 (*)");
            if (path.isEmpty()) { return; }
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly) || file.size() > 65536)
            {
                status->setText("无法读取脚本，文件应小于 64 KiB。");
                return;
            }
            auto cursor = code->textCursor();
            cursor.select(QTextCursor::Document);
            cursor.insertText(QString::fromUtf8(file.readAll()));
        });
        connect(save, &QPushButton::clicked, this, [this]
        {
            const auto path = QFileDialog::getSaveFileName(this, "导出解析脚本", "frame-parser.c", "解析脚本 (*.c);;文本文件 (*.txt)");
            if (path.isEmpty()) { return; }
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly)) { status->setText(file.errorString()); return; }
            file.write(code->toPlainText().toUtf8());
            status->setText(file.commit() ? "脚本已导出" : file.errorString());
        });
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(apply, &QPushButton::clicked, this, [this]
        {
            if (!owner) { reject(); return; }
            try
            {
                owner->applyScript(code->toPlainText(), enable->isChecked());
                accept();
            }
            catch (const ScriptError &failure) { showFailure(failure.display(), failure.line); }
        });
        connect(code, &QPlainTextEdit::textChanged, this, [this] { code->setExtraSelections({}); });
        connect(pane, &QObject::destroyed, this, &QDialog::reject);
    }
    void showFailure(const QString &message, int line = 0)
    {
        status->setText(message);
        if (!line)
        {
            const auto match = QRegularExpression("第 (\\d+) 行").match(message);
            if (match.hasMatch()) { line = match.captured(1).toInt(); }
        }
        code->showError(line);
    }
    std::shared_ptr<const FrameScriptProgram> compile()
    {
        try { return FrameScriptProgram::compile(code->toPlainText()); }
        catch (const ScriptError &failure) { showFailure(failure.display(), failure.line); return nullptr; }
    }
    void testInput()
    {
        auto program = compile();
        if (!program) { return; }
        try
        {
            QList<QByteArray> chunks;
            qsizetype total = 0;
            for (const auto &line : hex->toPlainText().split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts))
            {
                auto chunk = Engine::encode(line, true, "utf-8", "none");
                total += chunk.size();
                chunks.append(chunk);
            }
            if (chunks.isEmpty() || chunks.size() > 64 || total > 65536)
            {
                status->setText("请输入 1..64 行 HEX，总计不超过 64 KiB。");
                return;
            }
            testProcessor->configure(program);
            results->clear();
            frames = 0;
            remaining = chunks.size();
            run->setEnabled(false);
            status->setText("正在试解析…");
            for (const auto &chunk : chunks) { testProcessor->submit(chunk); }
        }
        catch (const std::exception &failure) { status->setText(QString::fromUtf8(failure.what())); }
    }
};
}

bool Pane::supportsScript() const
{
    return !isInstrument() && (channel ? channel->config.kind : draft.kind) == "serial";
}

void Pane::applyScript(const QString &code, bool enabled)
{
    if (!supportsScript()) { throw ScriptError(0, "脚本解析仅适用于串口会话"); }
    if (channel) { channel->configureScript(code, enabled); }
    else if (enabled) { FrameScriptProgram::compile(code); }
    scriptCode = code;
    scriptRequested = enabled;
    window->saveSettings();
    window->updateStatus();
    refreshScriptResults();
}

void Pane::openScriptEditor(bool nonModal)
{
    if (!supportsScript()) { return; }
    if (scriptDialog)
    {
        scriptDialog->show();
        scriptDialog->raise();
        scriptDialog->activateWindow();
        return;
    }
    scriptDialog = new ScriptEditor(this, nonModal);
    scriptDialog->show();
}
}
