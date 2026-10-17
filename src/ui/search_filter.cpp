// SPDX-License-Identifier: Apache-2.0
#include "search_filter.h"

namespace sw
{
void SearchFilter::compile()
{
    error.clear();
    groups.clear();
    if (text.isEmpty())
    {
        return;
    }

    QList<QStringList> terms;
    if (!logical)
    {
        terms.append(QStringList {text});
    }
    else
    {
        terms.append(QStringList());
        QString term;
        bool quoted = false;
        const auto finishTerm = [&]()
        {
            const auto value = term.trimmed();
            if (value.isEmpty())
            {
                error = "逻辑运算符两侧需要填写内容";
                return false;
            }
            terms.last().append(value);
            term.clear();
            return true;
        };

        for (int at = 0; at < text.size(); ++at)
        {
            const auto c = text[at];
            if (c == '\\' && at + 1 < text.size())
            {
                // Preserve regex escapes; quote escaping is handled by the lexer.
                const auto next = text[at + 1];
                if (next == '"')
                {
                    term += next;
                    ++at;
                    continue;
                }
                term += c;
                term += next;
                ++at;
                continue;
            }
            if (c == '"')
            {
                quoted = !quoted;
                continue;
            }
            const auto pair = text.mid(at, 2);
            if (!quoted && (pair == "&&" || pair == "||"))
            {
                if (!finishTerm()) { return; }
                if (pair == "||") { terms.append(QStringList()); }
                ++at;
                continue;
            }
            term += c;
        }
        if (quoted)
        {
            error = "双引号没有闭合";
            return;
        }
        if (!finishTerm()) { return; }
    }

    auto flags = QRegularExpression::UseUnicodePropertiesOption;
    QRegularExpression::PatternOptions patternOptions = flags;
    if (!caseSensitive) { patternOptions |= QRegularExpression::CaseInsensitiveOption; }
    for (const auto &conjunction : terms)
    {
        QList<QRegularExpression> group;
        for (const auto &term : conjunction)
        {
            QString pattern = regularExpression ? term : QRegularExpression::escape(term);
            if (wholeWord)
            {
                pattern = "(?<![\\p{L}\\p{N}_])(?:" + pattern + ")(?![\\p{L}\\p{N}_])";
            }
            QRegularExpression expression(pattern, patternOptions);
            if (!expression.isValid())
            {
                error = QString("正则表达式无效：%1").arg(expression.errorString());
                groups.clear();
                return;
            }
            group.append(expression);
        }
        groups.append(group);
    }
}

bool SearchFilter::matches(const QString &line) const
{
    // A partially typed expression never hides the entire log.
    if (text.isEmpty() || !error.isEmpty()) { return true; }
    for (const auto &group : groups)
    {
        bool matched = true;
        for (const auto &expression : group)
        {
            if (!expression.match(line).hasMatch())
            {
                matched = false;
                break;
            }
        }
        if (matched) { return true; }
    }
    return false;
}

QJsonObject SearchFilter::settings() const
{
    return {{"text", text}, {"case_sensitive", caseSensitive}, {"whole_word", wholeWord},
        {"regex", regularExpression}, {"logical", logical}};
}

void SearchFilter::restore(const QJsonObject &saved)
{
    text = saved["text"].toString();
    caseSensitive = saved["case_sensitive"].toBool();
    wholeWord = saved["whole_word"].toBool();
    regularExpression = saved["regex"].toBool();
    logical = saved["logical"].toBool();
    compile();
}

FilterBox::FilterBox(QWidget *parent) : SearchBox(parent)
{
    setMinimumWidth(270);
    setMaximumWidth(480);
    setMaxLength(2048);
    setTextMargins(25, 0, 134, 0);
    const auto addOption = [this](const QString &text, const QString &name, const QString &tip)
    {
        auto option = new QToolButton(this);
        option->setText(text);
        option->setObjectName(name);
        option->setProperty("searchOption", true);
        option->setCheckable(true);
        option->setFixedSize(25, 26);
        option->setToolTip(tip);
        option->setAccessibleName(tip);
        option->setFocusPolicy(Qt::StrongFocus);
        option->setCursor(Qt::PointingHandCursor);
        connect(option, &QAbstractButton::toggled, this, &FilterBox::rebuild);
        options.append(option);
        return option;
    };
    caseButton = addOption("Aa", "filterCase", "区分大小写");
    wordButton = addOption("ab", "filterWord", "全词匹配（字母、数字、下划线为词内字符）");
    auto wordFont = wordButton->font();
    wordFont.setUnderline(true);
    wordButton->setFont(wordFont);
    regexButton = addOption(".*", "filterRegex", "使用正则表达式");
    logicButton = addOption("&&&&", "filterLogic", "逻辑匹配：&& 同时包含，|| 任一包含；&& 优先");
    connect(this, &QLineEdit::textChanged, this, &FilterBox::rebuild);
    rebuild();
}

void FilterBox::resizeEvent(QResizeEvent *event)
{
    SearchBox::resizeEvent(event);
    int left = clearButton->x() - options.size() * 26 - 2;
    for (auto option : options)
    {
        option->move(left, (height() - option->height()) / 2);
        left += 26;
    }
}

void FilterBox::rebuild()
{
    compiled.text = text();
    compiled.caseSensitive = caseButton->isChecked();
    compiled.wholeWord = wordButton->isChecked();
    compiled.regularExpression = regexButton->isChecked();
    compiled.logical = logicButton->isChecked();
    compiled.compile();
    const bool invalid = !compiled.error.isEmpty();
    if (property("invalidFilter").toBool() != invalid)
    {
        setProperty("invalidFilter", invalid);
        style()->unpolish(this);
        style()->polish(this);
    }
    const QString help = "按行筛选；Aa 区分大小写，ab 全词，.* 正则，&& 逻辑匹配。\n"
        "逻辑示例：温度 && 报警 || 故障；双引号内不拆分运算符，例如 \"A||B\"。\n"
        "正则和逻辑可组合；正则项内含 && 或 || 时请用双引号包住。";
    setToolTip(invalid ? compiled.error + "\n" + help : help);
    setAccessibleDescription(invalid ? compiled.error : help);
    update();
    emit criteriaChanged();
}

void FilterBox::restore(const QJsonObject &saved)
{
    QSignalBlocker block(this);
    for (auto option : options) { option->blockSignals(true); }
    setText(saved["text"].toString());
    clearButton->setVisible(!text().isEmpty());
    caseButton->setChecked(saved["case_sensitive"].toBool());
    wordButton->setChecked(saved["whole_word"].toBool());
    regexButton->setChecked(saved["regex"].toBool());
    logicButton->setChecked(saved["logical"].toBool());
    for (auto option : options) { option->blockSignals(false); }
    rebuild();
    // Emit only once, after the complete restored configuration is compiled.
    block.unblock();
    emit criteriaChanged();
}
}
