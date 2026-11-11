// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QtCore>

namespace sw
{
class UsageGuide
{
public:
    static QMap<QString, QByteArray> files();
    static QJsonObject manifest();
    static QString text(const QString &topic);
    static QJsonObject read(const QString &topic = "overview");
};
}
