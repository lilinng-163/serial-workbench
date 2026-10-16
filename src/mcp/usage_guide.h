// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QtCore>

namespace sw
{
class UsageGuide
{
public:
    static QString text(const QString &topic);
    static QJsonObject read(const QString &topic = "overview");
    static QString scriptPrompt(const QString &code, const QString &hex, const QJsonObject &connection);
};
}
