// SPDX-License-Identifier: Apache-2.0
#include "log_filter.h"
#include "core.h"

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
            const auto pattern = regularExpression ? term : QRegularExpression::escape(term);
            QRegularExpression expression(pattern, patternOptions);
            // Validate the user's regex before adding boundaries. An unclosed
            // bracket in the query can otherwise match a bracket in the suffix.
            if (wholeWord && (!regularExpression || expression.isValid()))
            {
                expression.setPattern("(?<![\\p{L}\\p{N}_])(?:" + pattern + ")(?![\\p{L}\\p{N}_])");
            }
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

SearchFilter patchedSearchFilter(const SearchFilter &current, const QJsonObject &patch)
{
    auto next = current.settings();
    for (auto field = patch.begin(); field != patch.end(); ++field)
    {
        if (!next.contains(field.key())) { fail("未知的搜索参数：" + field.key()); }
        if (field.key() == "text" ? !field->isString() : !field->isBool())
        {
            fail("搜索参数类型错误：" + field.key());
        }
        next[field.key()] = field.value();
    }
    if (next["text"].toString().size() > 2048) { fail("搜索内容最多 2048 字符"); }
    SearchFilter result;
    result.restore(next);
    if (!result.error.isEmpty()) { fail(result.error); }
    return result;
}
}
