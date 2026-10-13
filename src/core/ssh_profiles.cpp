#include "ssh_profiles.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>

namespace sw
{
namespace
{
void collectConfig(const QString &path, QStringList *result, QSet<QString> &visited,
    QCryptographicHash *fingerprint, int depth)
{
    if (depth > 12)
    {
        return;
    }

    const QFileInfo information(path);
    const QString canonical = information.canonicalFilePath();
    if (canonical.isEmpty())
    {
        if (fingerprint)
        {
            fingerprint->addData(QDir::cleanPath(information.absoluteFilePath()).toUtf8());
            fingerprint->addData("missing");
        }
        return;
    }
    if (visited.contains(canonical))
    {
        return;
    }
    visited.insert(canonical);

    QFile file(canonical);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (fingerprint)
        {
            fingerprint->addData(canonical.toUtf8());
            fingerprint->addData("unreadable");
        }
        return;
    }

    const QByteArray contents = file.readAll();
    if (fingerprint)
    {
        fingerprint->addData(canonical.toUtf8());
        fingerprint->addData(contents);
    }

    const auto lines = QString::fromUtf8(contents).split('\n');
    for (const QString &lineValue : lines)
    {
        const QString line = lineValue.trimmed();
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
                    && !alias.contains('?') && !alias.startsWith('-') && result && !result->contains(alias))
                {
                    result->append(alias);
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
            if (fingerprint)
            {
                fingerprint->addData(pattern.toUtf8());
                for (const QFileInfo &entry : files)
                {
                    fingerprint->addData(entry.fileName().toUtf8());
                }
            }
            for (const QFileInfo &entry : files)
            {
                collectConfig(entry.absoluteFilePath(), result, visited, fingerprint, depth + 1);
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
    collectConfig(path, &result, visited, nullptr, 0);
    return result;
}

QString SshProfiles::fingerprint(const QString &configPath)
{
    QSet<QString> visited;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const QString path = configPath.isEmpty() ? QDir::home().filePath(".ssh/config") : configPath;
    collectConfig(path, nullptr, visited, &hash, 0);
    return QString::fromLatin1(hash.result().toHex());
}
}
