#pragma once
#include "terminal.h"
#include "mcp.h"

namespace sw
{
class Window;

class Instrument : public QWidget
{
    Q_OBJECT
public:
    Instrument(const QString &kind, QWidget *parent = nullptr);
    QString kind;
    QCheckBox *freeze = nullptr;
    QSpinBox *points = nullptr;
    WavePlot *plot = nullptr;
    QTreeWidget *table = nullptr;
    QPlainTextEdit *detail = nullptr;
    Combo *parser = nullptr;
    QLabel *info = nullptr;
    QLineEdit *filter = nullptr;
    QJsonObject cursors;
    void tick(Channel *channel);
    QJsonObject settings() const;
};

class Pane : public QWidget
{
    Q_OBJECT
public:
    explicit Pane(Window *window);
    Window *window;
    QPointer<Channel> channel;
    Config draft;
    qint64 cursor = 0;
    qint64 viewStart = 0;
    QTabWidget *pages;
    QWidget *monitorPage;
    QToolButton *controls;
    QStackedWidget *display;
    LogView *log;
    TerminalView *terminal;
    QPlainTextEdit *payload;
    QWidget *sender;
    Splitter *splitter;
    QList<int> senderSizes {600, 150};
    Combo *encoding;
    Combo *newline;
    Combo *peer;
    QCheckBox *sendHex;
    QCheckBox *repeat;
    QSpinBox *interval;
    QTimer repeatTimer;
    QByteArray repeatedPayload;
    QString repeatedPeer;
    QPushButton *sendButton;
    IconButton *pause;
    QLineEdit *filter;
    bool terminalMode = false;
    QList<Instrument *> instruments;
    void setChannel(Channel *channel);
    void tick();
    void sendCurrent();
    void setTerminal(bool enabled);
    void clearView();
    void replay();
    void exportLog();
    void templateMenu();
    void applyAppearance();
    QJsonObject settings() const;
    void restore(const QJsonObject &settings);
    Instrument *addInstrument(const QString &kind, bool activate = true);
    void closeInstrument(int index);
    QMenu *controlMenu();
    bool eventFilter(QObject *object, QEvent *event) override;
};

class Window : public QMainWindow
{
    Q_OBJECT
public:
    Window(Engine *engine, McpServer *mcp, const QString &settingsPath);
    Engine *engine;
    McpServer *mcp;
    Theme theme;
    QFont dataFont {"Cascadia Mono", 11};
    QString settingsPath;
    QJsonObject saved;
    QJsonObject viewSettings;
    QJsonArray templates;
    bool syncing = false;
    bool closing = false;
    QFrame *sidebar;
    QFrame *rail;
    ConnectionBar *tabs;
    QWidget *tabHeader;
    QStackedWidget *stack;
    IconButton *plus;
    IconButton *gear;
    Combo *protocol;
    Combo *port;
    Combo *baud;
    QLineEdit *connectionName;
    Combo *bits;
    Combo *parity;
    Combo *stopbits;
    Combo *flow;
    QCheckBox *dtr;
    QCheckBox *rts;
    QStackedWidget *configStack;
    QLineEdit *host;
    QSpinBox *remotePort;
    QLineEdit *bindHost;
    QSpinBox *bindPort;
    QLabel *remoteLabel;
    QLabel *localLabel;
    QLabel *serialInfo;
    QPushButton *connectButton;
    QLabel *status;
    QLabel *mcpStatus;
    QTimer timer;
    QElapsedTimer settingsRefresh;
    QDialog *settingsDialog = nullptr;
    QTreeWidget *mcpConnections = nullptr;
    QTreeWidget *auditTable = nullptr;
    QLabel *mcpSummary = nullptr;
    QList<Pane *> panes;
    Pane *active = nullptr;

    Pane *newConnection();
    void activate(int index);
    void closeTab(int index);
    void adjustTabs();
    void syncConnections();
    void updateStatus();
    void connectionAction();
    void applyBaud();
    Config form() const;
    void loadForm(const Config &config);
    void refreshPorts();
    void protocolChanged();
    void advancedSettings();
    void openSettings();
    void configureClient(int index, QLabel *result);
    void saveSettings();
    void applyTheme(bool dark);
    void setDataFont(const QFont &font);
    void error(const QString &message);
    void waveHelp();
    void tabMenu(const QPoint &point);
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
};
}
