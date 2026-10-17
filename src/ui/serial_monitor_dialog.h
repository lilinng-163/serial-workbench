// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "terminal.h"

namespace sw
{
class SerialMonitorDialog : public QDialog
{
    Q_OBJECT
public:
    QString searchViewId = QUuid::createUuid().toString(QUuid::Id128);
    QString channelId() const { return boundId; }
    FilterBox *searchBox() const { return filter; }
    LogView *logView() const { return log; }
    explicit SerialMonitorDialog(QWidget *parent = nullptr);
    void setChannel(Channel *channel);
    void setAppearance(const Theme &theme, const QFont &font);
    QJsonObject settings() const;
    void restore(const QJsonObject &settings);
    void refresh();

signals:
    void configurationChanged();
    void windowHidden();
    void zoomRequested(int steps);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private:
    QPointer<Channel> target;
    QString boundId;
    qint64 cursor = 0;
    qint64 viewStart = 0;
    Theme appearance;
    QFont dataFont;
    LogView *log = nullptr;
    FilterBox *filter = nullptr;
    QLabel *source = nullptr;
    QLabel *connection = nullptr;
    QLabel *statistics = nullptr;
    QLabel *message = nullptr;
    IconButton *pause = nullptr;
    IconButton *hex = nullptr;
    IconButton *timestamps = nullptr;
    IconButton *follow = nullptr;
    QComboBox *encoding = nullptr;
    QComboBox *newline = nullptr;
    QLineEdit *payload = nullptr;
    QPushButton *sendButton = nullptr;
    QTimer timer;
    QTimer saveTimer;
    void replay();
    void clearView();
    void sendCurrent();
};
}
