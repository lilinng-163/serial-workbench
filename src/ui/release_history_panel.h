// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "controls.h"
#include "update_service.h"

namespace sw
{
class ReleaseHistoryPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ReleaseHistoryPanel(QWidget *parent = nullptr, QNetworkAccessManager *transport = nullptr);
    void refreshHistory();

signals:
    void installRequested(const QString &path);

private:
    UpdateService *service;
    Combo *versions;
    QLabel *details;
    QTextBrowser *notes;
    QLabel *status;
    QProgressBar *progress;
    QPushButton *refresh;
    QPushButton *download;
    QPushButton *cancel;
    void updateChoices();
    void render();
};
}
