// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "controls.h"
#include "openocd.h"

namespace sw
{
class OpenOcdDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OpenOcdDialog(OpenOcdService *service, QWidget *parent = nullptr, bool embedded = false);
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
    QList<int> configurationSizes;
    QWidget *editor;
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
    QPushButton *version;
    QPushButton *connectTarget;
    QPushButton *flash;
    QPushButton *cancelJob;
    QProgressBar *progress;
    QTimer clock;

    void load();
    void commit();
    void refreshCatalog();
    void refreshState();
    void refreshPlan();
    void run(const QString &operation);
    void arrangeContent();
};
}
