// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QtWidgets>

namespace sw
{
struct JsonAssistance
{
    bool valid = false;
    QString formatted;
    QString compact;
    QString parseCode;
    QString buildCode;
    QString message;
    int errorPosition = -1;
};

JsonAssistance analyzeJson(const QString &text);
QDialog *showJsonAssistant(QWidget *parent, const QString &initial = {});
}
