// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "terminal.h"
#include "mcp.h"
#include "update_service.h"

namespace sw
{
class Window;
class UpdateDialog;

class Instrument : public QWidget
{
    Q_OBJECT
public:
    Instrument(const QString &kind, QWidget *parent = nullptr);
    QString kind;
    QPointer<Window> helpWindow;
    QCheckBox *freeze = nullptr;
    QSpinBox *points = nullptr;
    QCheckBox *waveFollow = nullptr;
    QCheckBox *waveXAxis = nullptr;
    QCheckBox *waveYAxis = nullptr;
    WavePlot *plot = nullptr;
    QTreeWidget *table = nullptr;
    QPlainTextEdit *detail = nullptr;
    Combo *parser = nullptr;
    Combo *source = nullptr;
    QLabel *info = nullptr;
    QLineEdit *filter = nullptr;
    QJsonObject cursors;
    QPointer<Channel> waveChannel;
    QTimer frameTimer;
    void tick(Channel *channel);
    QJsonObject settings() const;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
};

class Pane : public QWidget
{
    Q_OBJECT
public:
    explicit Pane(Window *window, const QString &instrumentKind = {});
    Window *window;
    QPointer<Channel> channel;
    Config draft;
    QJsonObject portMetadata;
    qint64 cursor = 0;
    qint64 viewStart = 0;
    Instrument *instrument = nullptr;
    QString sourceKey;
    QString pageTitle;
    QWidget *monitorPage = nullptr;
    IconButton *modeButton = nullptr;
    IconButton *timestamps = nullptr;
    IconButton *hexDisplay = nullptr;
    IconButton *wrapLines = nullptr;
    IconButton *cursorShape = nullptr;
    IconButton *clipboardButton = nullptr;
    QStackedWidget *display = nullptr;
    LogView *log = nullptr;
    TerminalView *terminal = nullptr;
    QPlainTextEdit *payload = nullptr;
    QWidget *sender = nullptr;
    Splitter *splitter = nullptr;
    Splitter *receiveSplitter = nullptr;
    QFrame *scriptPanel = nullptr;
    QTreeWidget *scriptTable = nullptr;
    QLabel *scriptSummary = nullptr;
    IconButton *scriptCollapse = nullptr;
    QJsonObject scriptCursors;
    qint64 scriptViewStart = 0;
    bool scriptCollapsed = false;
    int scriptPanelHeight = 164;
    QList<int> senderSizes {600, 176};
    Combo *encoding = nullptr;
    Combo *newline = nullptr;
    Combo *peer = nullptr;
    QCheckBox *sendHex = nullptr;
    IconButton *repeat = nullptr;
    QSpinBox *interval = nullptr;
    QTimer repeatTimer;
    QByteArray repeatedPayload;
    QString repeatedPeer;
    QPushButton *sendButton = nullptr;
    IconButton *pause = nullptr;
    IconButton *follow = nullptr;
    IconButton *exportButton = nullptr;
    IconButton *transferButton = nullptr;
    QLineEdit *filter = nullptr;
    bool terminalMode = false;
    QString scriptCode = FrameScriptProgram::example();
    bool scriptRequested = false;
    QPointer<QDialog> scriptDialog;
    bool supportsScript() const;
    void applyScript(const QString &code, bool enabled);
    void openScriptEditor();
    void buildScriptResults();
    void refreshScriptResults();
    void clearScriptResults();
    void resizeScriptResults();
    void setChannel(Channel *channel);
    void tick();
    void sendCurrent();
    void setTerminal(bool enabled);
    void clearView();
    void replay();
    void exportLog();
    void templateMenu();
    void clipboardMenu();
    void applyAppearance();
    QJsonObject settings() const;
    void restore(const QJsonObject &settings);
    bool isInstrument() const { return instrument != nullptr; }
    void refreshSource();
    void enableParser();
    bool eventFilter(QObject *object, QEvent *event) override;
};

class Window : public QMainWindow
{
    Q_OBJECT
public:
    Window(Engine *engine, McpServer *mcp, const QString &settingsPath, Window *primary = nullptr);
    Window *primary;
    QList<QPointer<Window>> hosts;
    Window *rootWindow();
    Pane *findPane(Channel *channel);
    Window *detachConnection(int index, const QPoint &position, bool systemMove = false);
    void transferPane(Pane *pane, Window *target);
    void attachPane(Pane *pane);
    Engine *engine;
    McpServer *mcp;
    Theme theme;
    QFont dataFont;
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
    IconButton *parameterToggle;
    QWidget *parameterGrip = nullptr;
    int parameterWidth = 246;
    bool parametersVisible = true;
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
    Combo *sshTarget;
    QLineEdit *sshUser;
    QLineEdit *sshPort;
    QLineEdit *sshIdentity;
    QLabel *connectionHint;
    QLabel *remoteLabel;
    QLabel *localLabel;
    QLabel *serialInfo;
    QLabel *portDetails;
    StatusLed *connectionLed;
    QLabel *connectionState;
    QPushButton *connectButton;
    QWidget *scriptControls;
    QCheckBox *scriptToggle;
    IconButton *scriptEdit;
    QLabel *scriptHint;
    QLabel *status;
    QLabel *mcpStatus;
    QPushButton *updateButton = nullptr;
    QProgressBar *updateProgress = nullptr;
    UpdateService *updater = nullptr;
    QPointer<UpdateDialog> updateDialog;
    QTimer timer;
    QTimer portTimer;
    QTimer sshConfigTimer;
    QString sshConfigFingerprint;
    QJsonArray lastPorts;
    bool portsLoaded = false;
    std::function<QJsonArray()> portProvider = [] { return Engine::ports(); };
    QElapsedTimer settingsRefresh;
    QDialog *settingsDialog = nullptr;
    QTreeWidget *mcpConnections = nullptr;
    QTreeWidget *auditTable = nullptr;
    QLabel *mcpSummary = nullptr;
    QList<Pane *> panes;
    Pane *active = nullptr;
    QPointer<Pane> parameterPane;
    QJsonObject parameterConfig;

    Pane *newConnection();
    Pane *newInstrument(const QString &kind, Channel *source = nullptr, bool activate = true, const QString &sourceKey = {});
    QMenu *newPageMenu();
    void restoreInstruments(const QJsonObject &settings, const QString &sourceKey);

    void activate(int index);
    void closeTab(int index);
    void adjustTabs();
    void syncConnections();
    void updateStatus();
    void syncParameterPane(bool force = false);
    void connectionAction();
    void applyBaud();
    Config form() const;
    void loadForm(const Config &config);
    void refreshPorts();
    void refreshSshProfiles();
    bool holdsSerialPort(const QString &name) const;
    void protocolChanged();
    void openSettings();
    void configureClient(int index, QLabel *result);
    void saveSettings();
    void applyTheme(bool dark);
    void applyLocalTheme(bool dark);
    void arrangeParameters();
    void resizeParameters(int width);
    void setDataFont(const QFont &font);
    void error(const QString &message);
    void waveHelp();
    void tabMenu(const QPoint &point);
    void beginTabRename(int index);
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool nativeEvent(const QByteArray &type, void *message, qintptr *result) override;
};
}
