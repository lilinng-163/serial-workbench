// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "controls.h"
#include <QRegularExpression>

namespace sw
{
// OR groups of AND terms. Compiled once when the query/options change.
class SearchFilter
{
public:
    QString text;
    bool caseSensitive = false;
    bool wholeWord = false;
    bool regularExpression = false;
    bool logical = false;
    QString error;

    void compile();
    bool matches(const QString &line) const;
    QJsonObject settings() const;
    void restore(const QJsonObject &saved);

private:
    QList<QList<QRegularExpression>> groups;
};

class FilterBox : public SearchBox
{
    Q_OBJECT
public:
    explicit FilterBox(QWidget *parent = nullptr);
    const SearchFilter &criteria() const { return compiled; }
    QJsonObject settings() const { return compiled.settings(); }
    void restore(const QJsonObject &saved);
    void resizeEvent(QResizeEvent *event) override;

signals:
    void criteriaChanged();

private:
    SearchFilter compiled;
    QToolButton *caseButton;
    QToolButton *wordButton;
    QToolButton *regexButton;
    QToolButton *logicButton;
    QList<QToolButton *> options;
    void rebuild();
};
}
