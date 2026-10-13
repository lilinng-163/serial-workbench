// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QStringList>

namespace sw
{
class SshProfiles
{
public:
    static QStringList aliases(const QString &configPath = {});
    static QString fingerprint(const QString &configPath = {});
};
}
