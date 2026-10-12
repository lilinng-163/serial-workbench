#include "ssh_profiles.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

namespace sw
{
namespace
{
void collectAliases(const QString &path, QStringList &result, QSet<QString> &visited, int depth)
{
    if (depth > 12)
    {
        return;
    }

    const QFileInfo information(path);
    const QString canonical = information.canonicalFilePath();
    if (canonical.isEmpty() || visited.contains(canonical))
    {
        return;
    }
    visited.insert(canonical);

    QFile file(canonical);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    QTextStream stream(&file);
    while (!stream.atEnd())
    {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
        {
            continue;
        }

        const auto match = QRegularExpression(R"(^(Host|Include)\s+(.+)$)", QRegularExpression::CaseInsensitiveOption).match(line);
        if (!match.hasMatch())
        {
            continue;
        }

        const auto values = QProcess::splitCommand(match.captured(2).section('#', 0, 0).trimmed());
        if (match.captured(1).compare("Host", Qt::CaseInsensitive) == 0)
        {
            for (const QString &alias : values)
            {
                if (!alias.isEmpty() && !alias.startsWith('!') && !alias.contains('*')
                    && !alias.contains('?') && !alias.startsWith('-') && !result.contains(alias))
                {
                    result.append(alias);
                }
            }
            continue;
        }

        for (QString pattern : values)
        {
            if (pattern.startsWith("~/"))
            {
                pattern = QDir::homePath() + pattern.mid(1);
            }
            if (QDir::isRelativePath(pattern))
            {
                pattern = information.dir().filePath(pattern);
            }

            const QFileInfo included(pattern);
            const auto files = included.dir().entryInfoList({included.fileName()}, QDir::Files | QDir::Readable, QDir::Name);
            for (const QFileInfo &entry : files)
            {
                collectAliases(entry.absoluteFilePath(), result, visited, depth + 1);
            }
        }
    }
}
}

QStringList SshProfiles::aliases(const QString &configPath)
{
    QStringList result;
    QSet<QString> visited;
    const QString path = configPath.isEmpty() ? QDir::home().filePath(".ssh/config") : configPath;
    collectAliases(path, result, visited, 0);
    return result;
}
}
