// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QJsonObject>
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

SearchFilter patchedSearchFilter(const SearchFilter &current, const QJsonObject &patch);
}
