// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QtCore>

namespace sw
{
struct AgentEnvironment
{
    QString home;
    QString appData;
    QString localAppData;
    QMap<QString, QString> variables;
    QStringList searchPaths;
    bool inspectWindowsApps = false;
    static AgentEnvironment current();
    QString variable(const QString &name, const QString &fallback) const;
    QString executable(const QString &name) const;
};

struct AgentClient
{
    QString id;
    QString name;
    QString logo;
    QString executable;
    QString configPath;
    QString format;
    QStringList section;
    bool modernOpenCode = false;
    bool installed() const { return !executable.isEmpty(); }
};

class AgentConfig
{
public:
    static QList<AgentClient> discover(const AgentEnvironment &environment = AgentEnvironment::current());
    static QJsonObject entry(const AgentClient &agent, const QString &url, const QString &bridge);
    static QString updated(const AgentClient &agent, const QString &original, const QString &url, const QString &bridge);
    static QString configure(const AgentClient &agent, const QString &url, const QString &bridge);
    static QString check(const AgentClient &agent, const QString &url, const QString &bridge);
};
}
