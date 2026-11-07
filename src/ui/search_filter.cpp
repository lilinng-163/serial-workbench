// SPDX-License-Identifier: Apache-2.0
#include "search_filter.h"

namespace sw
{
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
