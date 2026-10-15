// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "update_service.h"
#include <QtWidgets>

namespace sw
{
class UpdateDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UpdateDialog(UpdateService *service, QWidget *parent = nullptr);
    void refresh();
    void reject() override;

signals:
    void installRequested(const QString &path);

private:
    UpdateService *service;
    QLabel *heading;
    QLabel *details;
    QTextBrowser *notes;
    QProgressBar *progress;
    QLabel *status;
    QPushButton *action;
    QPushButton *cancel;
    QPushButton *background;
};
}
