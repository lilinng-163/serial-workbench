// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "agent_config.h"
#include "mcp.h"
#include <QtWidgets>

namespace sw
{
class AgentPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AgentPanel(McpServer *server, QWidget *parent = nullptr,
        std::function<AgentEnvironment()> environment = AgentEnvironment::current);
    void refresh();
    void configureAll();
    void configureSkills();
    void checkAgent(int index, bool configure);
signals:
    void configurationUpdated();
private:
    struct Card
    {
        QLabel *state;
        QLabel *result;
        QPushButton *configure;
        QPushButton *check;
    };
    std::function<AgentEnvironment()> environment;
    McpServer *server;
    QList<AgentClient> agents;
    QList<Card> cards;
    QLabel *summary;
    QPushButton *updateAll;
    QCheckBox *autoStart;
    bool busy = false;
    void prepareProbe(const std::function<void(const QString &)> &finished);
    void probe(const std::function<void(const QString &)> &finished);
};
}
