#pragma once

#include <QStringList>

namespace sw
{
class SshProfiles
{
public:
    static QStringList aliases(const QString &configPath = {});
};
}
