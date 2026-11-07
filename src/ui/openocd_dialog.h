// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "controls.h"
#include "openocd.h"
#include "openocd_rtt.h"

namespace sw
{
class OpenOcdDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OpenOcdDialog(OpenOcdService *service, QWidget *parent = nullptr, bool embedded = false);
    void setAppearance(const Theme &theme, const QFont &font);
    void accept() override;
    void reject() override;
    void saveConfiguration(const QString &path) const;
    void loadConfiguration(const QString &path, qint64 expectedRevision);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    OpenOcdService *service;
    bool syncing = false;
    qint64 logCursor = 0;
    QString displayedJob;
    bool rttWasActive = false;
    QTabWidget *workspaceTabs;
    QWidget *flashPage;
    QWidget *rttPage;
    QSplitter *split;
    QStackedWidget *content;
    QTabWidget *compactTabs;
    QWidget *output;
    QBoxLayout *columns;
    QBoxLayout *advancedColumns;
    bool compact = false;
    bool arranging = false;
    bool layoutReady = false;
    bool embeddedMode = false;
    QTimer layoutTimer;
    QTabWidget *configurationTabs;
    QTabWidget *resultTabs;
    QTabWidget *rttResultTabs;
    QList<int> configurationSizes;
    QList<QWidget *> configurationEditors;
    QFormLayout *deviceForm;
    QFormLayout *flowForm;
    QPushButton *advanced;
    QLineEdit *executable;
    QLineEdit *scripts;
    Combo *mode;
    Combo *board;
    Combo *adapter;
    Combo *target;
    Combo *transport;
    Combo *strategy;
    Combo *startup;
    Combo *resetConfig;
    QSpinBox *speed;
    QSpinBox *timeout;
    QLineEdit *adapterSerial;
    QCheckBox *verify;
    QCheckBox *resetRun;
    QTableWidget *images;
    QLabel *catalogHint;
    QLabel *flowHint;
    QLabel *status;
    QPlainTextEdit *logs;
    QPlainTextEdit *plan;
    QPlainTextEdit *rttLogs;
    QPlainTextEdit *rttPlan;
    QPushButton *version;
    QPushButton *connectTarget;
    QPushButton *flash;
    QPushButton *cancelJob;
    QProgressBar *progress;
    QTimer clock;
    OpenOcdRttSettings *rttSettings;
    QDialog *rttSettingsDialog;
    OpenOcdRttConsole *rttConsole;
    QPushButton *rttAction;

    QWidget *createRttPage();
    void load();
    bool commit();
    void refreshCatalog();
    void refreshState();
    void refreshPlan();
    void run(const QString &operation);
    void cancelRun(const QString &jobId);
    void arrangeContent();
};
}
