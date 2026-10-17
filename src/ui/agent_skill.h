// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "agent_config.h"

namespace sw
{
class AgentSkill
{
public:
    static QJsonArray targets(const AgentEnvironment &environment = AgentEnvironment::current());
    static void writeDetection(const QString &path, const AgentEnvironment &environment = AgentEnvironment::current());
    static QJsonObject install(const QString &agent, const AgentEnvironment &environment = AgentEnvironment::current());
};
}
