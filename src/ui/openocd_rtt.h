// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "controls.h"
#include "openocd.h"
#include "terminal.h"

namespace sw
{
class OpenOcdRttSettings : public QWidget
{
    Q_OBJECT
public:
    explicit OpenOcdRttSettings(QWidget *parent = nullptr);
    void load(const QJsonObject &config);
    QJsonObject configuration() const;

signals:
    void edited();

private:
    Combo *mode;
    Combo *startup;
    QFormLayout *fields;
    QLabel *managedHint;
    QLineEdit *host;
    QLineEdit *address;
    QLineEdit *size;
    QLineEdit *identifier;
    QSpinBox *port;
    QSpinBox *channel;
    QSpinBox *poll;
    QSpinBox *gdb;
    bool loading = false;
    void updateMode();
};

// Each embedded or floating viewer has its own cursor. Clearing or pausing a
// viewer never consumes the shared service journal used by MCP and other views.
class OpenOcdRttConsole : public QWidget
{
    Q_OBJECT
public:
    explicit OpenOcdRttConsole(OpenOcdService *service, QWidget *parent = nullptr, bool floating = false);
    void setAppearance(const Theme &theme, const QFont &font);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QPointer<OpenOcdService> service;
    QBoxLayout *toolbar;
    FilterBox *filter;
    QStackedWidget *display;
    TerminalView *terminal;
    QPlainTextEdit *matches;
    QPushButton *pause;
    QLabel *status;
    QTimer timer;
    QTimer searchTimer;
    qint64 cursor = 0;
    qint64 clearedThrough = 0;
    int displayedConnection = 0;
    QString displayedJob;
    QString searchSummary;
    QPointer<QDialog> popup;
    void schedule();
    void drain();
    void scheduleSearch();
    void refreshSearch();
    QString displayText(bool selected) const;
    void updateStatus();
    void openWindow();
};
}
