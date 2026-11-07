// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "controls.h"
#include "log_filter.h"

namespace sw
{
class FilterBox : public SearchBox
{
    Q_OBJECT
public:
    explicit FilterBox(QWidget *parent = nullptr);
    const SearchFilter &criteria() const { return compiled; }
    QJsonObject settings() const { return compiled.settings(); }
    void restore(const QJsonObject &saved);
    void resizeEvent(QResizeEvent *event) override;

signals:
    void criteriaChanged();

private:
    SearchFilter compiled;
    QToolButton *caseButton;
    QToolButton *wordButton;
    QToolButton *regexButton;
    QToolButton *logicButton;
    QList<QToolButton *> options;
    void rebuild();
};
}
