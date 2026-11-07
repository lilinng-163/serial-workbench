// SPDX-License-Identifier: Apache-2.0
#include "openocd_dialog.h"
#include "searchable_combo.h"
#include <algorithm>
#include <functional>

namespace sw
{
namespace
{
Combo *choice(const QString &name, const QList<QPair<QString, QString>> &items)
{
    auto combo = new Combo();
    combo->setObjectName(name);
    combo->setMinimumWidth(180);
    for (const auto &item : items) { combo->addItem(item.first, item.second); }
    return combo;
}

QFormLayout *form(QWidget *parent)
{
    auto layout = new QFormLayout(parent);
    layout->setContentsMargins(12, 18, 12, 12);
    layout->setVerticalSpacing(9);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    return layout;
}

QWidget *fileField(QLineEdit *edit, QWidget *parent, bool directory, const QString &title)
{
    auto row = new QWidget();
    auto layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    auto browse = new QPushButton("浏览…");
    browse->setProperty("variant", "outline");
    layout->addWidget(edit, 1);
    layout->addWidget(browse);
    QObject::connect(browse, &QPushButton::clicked, parent, [edit, parent, directory, title]
    {
        const auto path = directory ? QFileDialog::getExistingDirectory(parent, title, edit->text())
            : QFileDialog::getOpenFileName(parent, title, edit->text());
        if (!path.isEmpty() && !edit->isReadOnly()) { edit->setText(QDir::fromNativeSeparators(path)); }
    });
    return row;
}

Combo *configCombo(const QString &name, const QString &kind)
{
    auto combo = new SearchableCombo("搜索" + kind + "名称，例如 ST-Link、STM32 F4…");
    combo->setObjectName(name);
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);
    combo->setMinimumWidth(200);
    combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    combo->lineEdit()->setPlaceholderText("展开搜索，或填写 .cfg 完整路径");
    combo->setCompleter(nullptr);
    return combo;
}

void selectValue(Combo *combo, const QJsonValue &value)
{
    combo->setCurrentIndex(combo->findData(value.toString()));
}

QTabWidget *tabs(const QString &name)
{
    auto widget = new QTabWidget();
    widget->setObjectName(name);
    widget->setProperty("openocdTabs", true);
    widget->tabBar()->setDrawBase(false);
    return widget;
}

QPlainTextEdit *outputText(const QString &name)
{
    auto edit = new QPlainTextEdit();
    edit->setObjectName(name);
    edit->setReadOnly(true);
    edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    return edit;
}
}

OpenOcdDialog::OpenOcdDialog(OpenOcdService *owner, QWidget *parent, bool embedded) : QDialog(parent), service(owner), embeddedMode(embedded)
{
    if (embedded) { setWindowFlags(Qt::Widget); }
    setObjectName("openOcdDialog");
    setWindowTitle("OpenOCD");
    setWindowIcon(QIcon(":/brand/lightning.svg"));
    if (embedded)
    {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    }
    else
    {
        setAttribute(Qt::WA_DeleteOnClose);
        resize(1200, 850);
        setMinimumSize(780, 620);
    }
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(10);
    auto heading = new QLabel("OpenOCD");
    heading->setObjectName("themeTitle");
    auto headingFont = font();
    headingFont.setPointSize(16);
    headingFont.setWeight(QFont::DemiBold);
    heading->setFont(headingFont);
    auto header = new QHBoxLayout();
    header->addWidget(heading);
    header->addStretch();
    auto subtitle = new QLabel("下拉框可搜索 · 日志可拖动调整");
    subtitle->setToolTip("选择开发板，或组合调试器与芯片配置。设置与 AI / MCP 共用，启动时不会自动连接设备。");
    subtitle->setWordWrap(false);
    subtitle->setObjectName("muted");
    header->addWidget(subtitle);
    if (!embedded) { layout->addLayout(header); }
    else
    {
        delete header;
        delete heading;
        delete subtitle;
    }
    workspaceTabs = tabs("openocdWorkspaceTabs");
    workspaceTabs->tabBar()->setObjectName("openocdWorkspaceBar");
    layout->addWidget(workspaceTabs, 1);
    flashPage = new QWidget();
    auto flashLayout = new QVBoxLayout(flashPage);
    flashLayout->setContentsMargins(8, 8, 8, 6);
    flashLayout->setSpacing(8);
    workspaceTabs->addTab(flashPage, "固件烧录");

    content = new QStackedWidget();
    flashLayout->addWidget(content, 1);
    split = new QSplitter(Qt::Horizontal);
    split->setObjectName("openocdSplit");
    split->setHandleWidth(10);
    split->setChildrenCollapsible(false);
    content->addWidget(split);
    compactTabs = tabs("openocdCompactTabs");
    content->addWidget(compactTabs);
    configurationTabs = tabs("openocdConfigurationTabs");
    configurationTabs->setMinimumWidth(360);
    configurationTabs->installEventFilter(this);
    split->addWidget(configurationTabs);
    auto editor = new QWidget();
    auto body = new QVBoxLayout(editor);
    body->setContentsMargins(0, 0, 8, 0);
    body->setSpacing(14);
    auto tool = new QGroupBox("工具与配置库");
    auto toolForm = form(tool);
    executable = new QLineEdit();
    executable->setObjectName("openocdExecutable");
    scripts = new QLineEdit();
    scripts->setObjectName("openocdScripts");
    scripts->setPlaceholderText("留空时从 OpenOCD 安装目录自动寻找");
    toolForm->addRow("OpenOCD", fileField(executable, this, false, "选择 OpenOCD 可执行文件"));
    toolForm->addRow("Scripts 目录", fileField(scripts, this, true, "选择 OpenOCD scripts 目录"));
    auto fileActions = new QHBoxLayout();
    auto importConfig = new QPushButton("导入配置…");
    importConfig->setObjectName("openocdImport");
    importConfig->setProperty("variant", "outline");
    auto exportConfig = new QPushButton("导出配置…");
    exportConfig->setObjectName("openocdExport");
    exportConfig->setProperty("variant", "outline");
    auto rescan = new QPushButton("重新扫描");
    rescan->setProperty("variant", "outline");
    for (auto button : {importConfig, exportConfig, rescan}) { button->setAutoDefault(false); }
    fileActions->addWidget(importConfig);
    fileActions->addWidget(exportConfig);
    fileActions->addStretch();
    fileActions->addWidget(rescan);
    body->addLayout(fileActions);
    catalogHint = new QLabel();
    catalogHint->setWordWrap(true);
    catalogHint->setObjectName("muted");
    toolForm->addRow(catalogHint);

    columns = new QBoxLayout(QBoxLayout::LeftToRight);
    columns->setSpacing(14);
    auto device = new QGroupBox("目标与连接");
    deviceForm = form(device);
    mode = choice("openocdMode", {{"调试器 + 芯片", "target"}, {"开发板配置", "board"}});
    board = configCombo("openocdBoard", "开发板");
    adapter = configCombo("openocdInterface", "调试器");
    target = configCombo("openocdTarget", "芯片");
    transport = choice("openocdTransport", {{"配置默认", "default"}, {"SWD", "swd"}, {"JTAG", "jtag"},
        {"HLA SWD", "hla_swd"}, {"HLA JTAG", "hla_jtag"}, {"DAP direct SWD", "dapdirect_swd"}, {"DAP direct JTAG", "dapdirect_jtag"}});
    speed = new QSpinBox();
    speed->setObjectName("openocdSpeed");
    speed->setRange(0, 100000);
    speed->setSuffix(" kHz");
    speed->setSpecialValueText("配置默认");
    adapterSerial = new QLineEdit();
    adapterSerial->setObjectName("openocdAdapterSerial");
    adapterSerial->setPlaceholderText("单板可留空；并行时每页使用不同序列号");
    deviceForm->addRow("配置方式", mode);
    deviceForm->addRow("开发板", board);
    deviceForm->addRow("调试器", adapter);
    deviceForm->addRow("芯片", target);
    deviceForm->addRow("传输方式", transport);
    deviceForm->addRow("探针序列号", adapterSerial);
    columns->addWidget(device, 1);

    auto flow = new QGroupBox("初始化与烧录流程");
    flowForm = form(flow);
    strategy = choice("openocdStrategy", {{"通用 · program", "program"}, {"Espressif · program_esp", "esp"}});
    startup = choice("openocdStartup", {{"推荐流程（自动初始化）", "default"}, {"init → reset init", "reset_init"},
        {"init → reset halt", "reset_halt"}, {"init → halt（不复位）", "halt"}});
    resetConfig = choice("openocdResetConfig", {{"沿用开发板 / 芯片配置", "board"}, {"无复位线 · none", "none"},
        {"SRST 复位线 · srst_only", "srst_only"}, {"复位下连接 · SRST", "connect_assert_srst"}});
    resetConfig->setToolTip("复位下连接需要实际接好 SRST，并且目标、探针均支持。默认沿用配置文件，不猜测接线。");
    timeout = new QSpinBox();
    timeout->setRange(5, 600);
    timeout->setSuffix(" s");
    verify = new QCheckBox("写入后校验");
    verify->setObjectName("openocdVerify");
    resetRun = new QCheckBox("完成后复位运行");
    flowForm->addRow("烧录流程", strategy);
    flowForm->addRow("初始化", startup);
    flowForm->addRow("复位接线", resetConfig);
    flowForm->addRow("任务超时", timeout);
    flowHint = new QLabel();
    flowHint->setWordWrap(true);
    flowHint->setObjectName("muted");
    flowForm->addRow(flowHint);
    body->addLayout(columns);
    advanced = new QPushButton("初始化与高级设置…");
    advanced->setObjectName("openocdAdvanced");
    advanced->setProperty("variant", "outline");
    advanced->setAutoDefault(false);
    deviceForm->addRow(advanced);

    auto firmware = new QGroupBox("固件");
    auto firmwareLayout = new QVBoxLayout(firmware);
    firmwareLayout->setContentsMargins(12, 18, 12, 12);
    images = new QTableWidget(0, 2);
    images->setObjectName("openocdImages");
    images->setHorizontalHeaderLabels({"文件路径", "地址（BIN 必填）"});
    images->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    images->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    images->setColumnWidth(1, 155);
    images->setTextElideMode(Qt::ElideMiddle);
    images->verticalHeader()->hide();
    images->setSelectionBehavior(QAbstractItemView::SelectRows);
    images->setMinimumHeight(100);
    images->setMaximumHeight(130);
    auto filesRow = new QHBoxLayout();
    auto add = new QPushButton("选择固件…");
    auto remove = new QPushButton("移除选中");
    add->setProperty("variant", "outline");
    remove->setProperty("variant", "outline");
    auto fileHint = new QLabel("ELF / HEX / S19 使用文件内地址；BIN 填 0x 地址。ESP 支持多分段。");
    fileHint->setWordWrap(true);
    fileHint->setObjectName("muted");
    filesRow->addWidget(add);
    filesRow->addWidget(remove);
    filesRow->addStretch();
    firmwareLayout->addLayout(filesRow);
    firmwareLayout->addWidget(images);
    firmwareLayout->addWidget(fileHint);
    auto verifyRow = new QHBoxLayout();
    verifyRow->addWidget(verify);
    verifyRow->addWidget(resetRun);
    verifyRow->addStretch();
    firmwareLayout->addLayout(verifyRow);
    columns->addWidget(firmware, 1);
    body->addStretch();
    configurationTabs->addTab(scrollPanel(editor, "openocdScroll"), "目标与固件");
    auto advancedPage = new QWidget();
    auto advancedLayout = new QBoxLayout(QBoxLayout::LeftToRight, advancedPage);
    advancedColumns = advancedLayout;
    advancedLayout->setContentsMargins(0, 0, 8, 0);
    advancedLayout->setSpacing(14);
    advancedLayout->addWidget(flow, 1, Qt::AlignTop);
    auto adapterOptions = new QGroupBox("调试连接");
    auto adapterOptionsForm = form(adapterOptions);
    adapterOptionsForm->addRow("调试时钟", speed);
    auto defaultHint = new QLabel("调试时钟为 0 时沿用配置文件。多板并行时，请在目标页填写不同的调试器序列号。");
    defaultHint->setWordWrap(true);
    defaultHint->setObjectName("muted");
    adapterOptionsForm->addRow(defaultHint);
    advancedLayout->addWidget(adapterOptions, 1, Qt::AlignTop);
    configurationTabs->addTab(scrollPanel(advancedPage, "openocdAdvancedScroll"), "初始化与高级");
    auto toolPage = new QWidget();
    auto toolLayout = new QVBoxLayout(toolPage);
    toolLayout->setContentsMargins(0, 0, 8, 0);
    toolLayout->addWidget(tool);
    auto toolHint = new QLabel("更换 OpenOCD 或 Scripts 目录后，可重新扫描配置库。工具路径与烧录参数共享给 GUI / MCP。");
    toolHint->setObjectName("muted");
    toolHint->setWordWrap(true);
    toolLayout->addWidget(toolHint);
    toolLayout->addStretch();
    configurationTabs->addTab(scrollPanel(toolPage, "openocdToolScroll"), "OpenOCD 工具");

    configurationEditors = {mode, board, adapter, target, transport, flow, adapterOptions, firmware, importConfig};
    for (auto browse : tool->findChildren<QPushButton *>()) { configurationEditors.append(browse); }

    output = new QWidget();
    output->setObjectName("openocdOutputPane");
    output->setMinimumWidth(300);
    auto outputLayout = new QVBoxLayout(output);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(6);
    resultTabs = tabs("openocdResultTabs");
    logs = outputText("openocdLogs");
    logs->setMinimumHeight(100);
    logs->setMaximumBlockCount(1500);
    plan = outputText("openocdPlan");
    resultTabs->addTab(logs, "运行日志");
    resultTabs->addTab(plan, "执行预览");
    outputLayout->addWidget(resultTabs, 1);
    split->addWidget(output);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    split->setSizes({560, 440});
    split->handle(1)->setToolTip("左右拖动，调整配置区和日志区的宽度");

    status = new QLabel("尚未启动");
    status->setObjectName("openocdStatus");
    status->setWordWrap(true);
    layout->addWidget(status);
    progress = new QProgressBar();
    progress->setRange(0, 0);
    progress->setTextVisible(false);
    progress->setFixedHeight(4);
    progress->hide();
    layout->addWidget(progress);
    auto actions = new QHBoxLayout();
    version = new QPushButton("检查版本");
    version->setObjectName("openocdVersion");
    connectTarget = new QPushButton("连接检查");
    connectTarget->setObjectName("openocdConnect");
    connectTarget->setToolTip("初始化并暂停目标，不写闪存；显式选择 reset 时也会复位设备。");
    flash = new QPushButton("开始烧录");
    flash->setObjectName("openocdFlash");
    flash->setProperty("primary", true);
    flash->setAutoDefault(false);
    cancelJob = new QPushButton("取消任务");
    cancelJob->setObjectName("openocdCancel");
    for (auto button : {version, connectTarget, flash, cancelJob}) { button->setAutoDefault(false); }
    for (auto button : {version, connectTarget, cancelJob}) { button->setProperty("variant", "outline"); }
    actions->addWidget(version);
    actions->addWidget(connectTarget);
    actions->addStretch();
    actions->addWidget(cancelJob);
    actions->addWidget(flash);
    flashLayout->addLayout(actions);

    rttPage = createRttPage();
    workspaceTabs->addTab(rttPage, "RTT 日志");

    connect(rttSettings, &OpenOcdRttSettings::edited, this, &OpenOcdDialog::commit);
    connect(workspaceTabs, &QTabWidget::currentChanged, this, [this]
    {
        if (layoutReady) { refreshPlan(); }
    });
    connect(rttAction, &QPushButton::clicked, this, [this]
    {
        if (service->busy())
        {
            const auto state = service->state(0, 1);
            if (state["operation"] == "rtt") { cancelRun(state["job_id"].toString()); }
            return;
        }
        if (commit()) { run("rtt"); }
    });
    connect(rescan, &QPushButton::clicked, this, &OpenOcdDialog::refreshCatalog);
    connect(importConfig, &QPushButton::clicked, this, [this]
    {
        const auto revision = service->revision();
        const auto path = QFileDialog::getOpenFileName(this, "导入 OpenOCD 配置", {}, "OpenOCD 配置 (*.json)");
        if (path.isEmpty()) { return; }
        try { loadConfiguration(path, revision); }
        catch (const std::exception &exception) { status->setText(QString::fromUtf8(exception.what())); }
    });
    connect(exportConfig, &QPushButton::clicked, this, [this]
    {
        auto path = QFileDialog::getSaveFileName(this, "导出 OpenOCD 配置", "OpenOCD.json", "OpenOCD 配置 (*.json)");
        if (path.isEmpty()) { return; }
        if (QFileInfo(path).suffix().isEmpty()) { path += ".json"; }
        try { saveConfiguration(path); }
        catch (const std::exception &exception) { status->setText(QString::fromUtf8(exception.what())); }
    });
    connect(advanced, &QPushButton::clicked, this, [this] { configurationTabs->setCurrentIndex(1); });
    for (auto edit : {executable, scripts, adapterSerial}) { connect(edit, &QLineEdit::textChanged, this, &OpenOcdDialog::commit); }
    connect(executable, &QLineEdit::editingFinished, this, &OpenOcdDialog::refreshCatalog);
    connect(scripts, &QLineEdit::editingFinished, this, &OpenOcdDialog::refreshCatalog);
    for (auto combo : {mode, transport, startup, resetConfig}) { connect(combo, &Combo::currentIndexChanged, this, &OpenOcdDialog::commit); }
    for (auto combo : {board, adapter, target}) { connect(combo, &Combo::currentTextChanged, this, &OpenOcdDialog::commit); }
    connect(strategy, &Combo::currentIndexChanged, this, [this]
    {
        if (syncing) { return; }
        if (strategy->currentData() == "esp")
        {
            const QSignalBlocker blocker(startup);
            startup->setCurrentIndex(0);
        }
        commit();
    });
    for (auto spin : {speed, timeout}) { connect(spin, &QSpinBox::valueChanged, this, &OpenOcdDialog::commit); }
    for (auto check : {verify, resetRun}) { connect(check, &QCheckBox::toggled, this, &OpenOcdDialog::commit); }
    connect(images, &QTableWidget::cellChanged, this, &OpenOcdDialog::commit);
    connect(add, &QPushButton::clicked, this, [this]
    {
        const auto files = QFileDialog::getOpenFileNames(this, "选择固件", {}, "固件 (*.elf *.hex *.ihex *.s19 *.srec *.bin)");
        if (files.isEmpty() || service->busy()) { return; }
        syncing = true;
        if (strategy->currentData() != "esp") { images->setRowCount(0); }
        for (const auto &file : files)
        {
            if (images->rowCount() >= 16) { break; }
            const int row = images->rowCount();
            images->insertRow(row);
            images->setItem(row, 0, new QTableWidgetItem(QDir::fromNativeSeparators(file)));
            images->setItem(row, 1, new QTableWidgetItem());
            if (strategy->currentData() != "esp") { break; }
        }
        syncing = false;
        commit();
    });
    connect(remove, &QPushButton::clicked, this, [this]
    {
        QSet<int> selected;
        for (auto item : images->selectedItems()) { selected.insert(item->row()); }
        auto rows = selected.values();
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        syncing = true;
        for (const int row : rows) { images->removeRow(row); }
        syncing = false;
        commit();
    });
    connect(version, &QPushButton::clicked, this, [this] { run("version"); });
    connect(connectTarget, &QPushButton::clicked, this, [this] { run("connect"); });
    connect(flash, &QPushButton::clicked, this, [this] { run("flash"); });
    connect(cancelJob, &QPushButton::clicked, this, [this]
    {
        const auto state = service->state(0, 1);
        if (state["operation"] != "rtt") { cancelRun(state["job_id"].toString()); }
    });
    connect(service, &OpenOcdService::configurationChanged, this, &OpenOcdDialog::load);
    connect(service, &OpenOcdService::stateChanged, this, &OpenOcdDialog::refreshState);
    clock.setInterval(500);
    connect(&clock, &QTimer::timeout, this, &OpenOcdDialog::refreshState);
    layoutTimer.setSingleShot(true);
    layoutTimer.setInterval(0);
    connect(&layoutTimer, &QTimer::timeout, this, &OpenOcdDialog::arrangeContent);
    load();
    layoutReady = true;
    arrangeContent();
}

QWidget *OpenOcdDialog::createRttPage()
{
    auto page = new QWidget();
    auto layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 6);
    layout->setSpacing(8);

    rttSettings = new OpenOcdRttSettings();
    auto settings = scrollPanel(rttSettings, "openocdRttScroll");
    rttSettingsDialog = new QDialog(this);
    rttSettingsDialog->setObjectName("openocdRttSettingsDialog");
    rttSettingsDialog->setWindowTitle("RTT 连接设置");
    rttSettingsDialog->setWindowIcon(windowIcon());
    rttSettingsDialog->setMinimumSize(420, 360);
    rttSettingsDialog->resize(570, 670);
    auto settingsLayout = new QVBoxLayout(rttSettingsDialog);
    settingsLayout->addWidget(settings, 1);
    auto done = new QPushButton("关闭");
    done->setAutoDefault(false);
    settingsLayout->addWidget(done, 0, Qt::AlignRight);
    connect(done, &QPushButton::clicked, rttSettingsDialog, &QDialog::close);

    rttResultTabs = tabs("openocdRttResultTabs");
    rttConsole = new OpenOcdRttConsole(service);
    rttLogs = outputText("openocdRttLogs");
    rttLogs->setMaximumBlockCount(1500);
    rttPlan = outputText("openocdRttPlan");
    rttResultTabs->addTab(rttConsole, "RTT 终端");
    rttResultTabs->addTab(rttLogs, "连接日志");
    rttResultTabs->addTab(rttPlan, "执行预览");

    layout->addWidget(rttResultTabs, 1);

    auto actions = new QHBoxLayout();
    auto configure = new QPushButton("连接设置…");
    configure->setObjectName("openocdRttSettingsAction");
    configure->setProperty("variant", "outline");
    configure->setAutoDefault(false);
    actions->addWidget(configure);
    actions->addStretch();
    connect(configure, &QPushButton::clicked, this, [this]
    {
        rttSettingsDialog->show();
        rttSettingsDialog->raise();
        rttSettingsDialog->activateWindow();
    });
    rttAction = new QPushButton("启动 RTT");
    rttAction->setObjectName("openocdRttAction");
    rttAction->setAutoDefault(false);
    rttAction->setProperty("primary", true);
    rttAction->setMinimumWidth(128);
    actions->addWidget(rttAction);
    layout->addLayout(actions);
    return page;
}

void OpenOcdDialog::setAppearance(const Theme &theme, const QFont &font)
{
    rttConsole->setAppearance(theme, font);
    rttSettingsDialog->setPalette(theme.palette());
    rttSettingsDialog->setStyleSheet(theme.stylesheet());
}

void OpenOcdDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    if (layoutReady) { layoutTimer.start(); }
}

void OpenOcdDialog::accept()
{
    if (!embeddedMode) { QDialog::accept(); }
}

void OpenOcdDialog::reject()
{
    if (!embeddedMode) { QDialog::reject(); }
}

void OpenOcdDialog::saveConfiguration(const QString &path) const
{
    QSaveFile file(path);
    const auto data = QJsonDocument(service->exportConfiguration()).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
    {
        fail("无法导出 OpenOCD 配置：" + file.errorString());
    }
}

void OpenOcdDialog::loadConfiguration(const QString &path, qint64 expectedRevision)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { fail("无法读取 OpenOCD 配置：" + file.errorString()); }
    constexpr qint64 maximumBytes = 1024 * 1024;
    if (file.size() > maximumBytes) { fail("OpenOCD 配置文件不能超过 1 MiB"); }
    const auto data = file.read(maximumBytes + 1);
    if (data.size() > maximumBytes || file.error() != QFileDevice::NoError) { fail("无法完整读取 OpenOCD 配置"); }

    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) { fail("OpenOCD 配置应为有效的 JSON 对象"); }
    service->importConfiguration(document.object(), expectedRevision);
}

bool OpenOcdDialog::eventFilter(QObject *object, QEvent *event)
{
    if (object == configurationTabs && event->type() == QEvent::Resize && layoutReady && !arranging) { layoutTimer.start(); }
    return QDialog::eventFilter(object, event);
}

void OpenOcdDialog::arrangeContent()
{
    if (!layoutReady || arranging) { return; }
    const QScopedValueRollback<bool> guard(arranging, true);
    const bool nextCompact = width() < 1000 || height() < 640;
    if (nextCompact != compact)
    {
        if (nextCompact)
        {
            configurationSizes = split->sizes();
            configurationTabs->setMinimumWidth(0);
            output->setMinimumWidth(0);
            compactTabs->addTab(configurationTabs, "配置");
            compactTabs->addTab(output, "运行输出");
            const auto state = service->state(0, 1);
            const bool flashBusy = state["running"].toBool() && state["operation"] != "rtt";
            compactTabs->setCurrentWidget(flashBusy ? output : static_cast<QWidget *>(configurationTabs));
            content->setCurrentWidget(compactTabs);
        }
        else
        {
            while (compactTabs->count() > 0) { compactTabs->removeTab(0); }
            configurationTabs->setMinimumWidth(360);
            output->setMinimumWidth(300);
            split->addWidget(configurationTabs);
            split->addWidget(output);
            content->setCurrentWidget(split);
            // Removing a tab explicitly hides its page. Reparenting it into a
            // splitter preserves that state, so show both after the transfer.
            configurationTabs->show();
            output->show();
            split->setSizes(configurationSizes.isEmpty() ? QList<int> {560, 440} : configurationSizes);
        }
        compact = nextCompact;
    }
    const auto direction = configurationTabs->width() < 680 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
    columns->setDirection(direction);
    advancedColumns->setDirection(direction);

}

void OpenOcdDialog::load()
{
    if (syncing) { return; }
    syncing = true;
    const auto config = service->configuration();
    rttSettings->load(config);
    executable->setText(config["executable"].toString());
    scripts->setText(config["scripts_dir"].toString());
    selectValue(mode, config["mode"]);
    board->setEditText(config["board"].toString());
    adapter->setEditText(config["interface"].toString());
    target->setEditText(config["target"].toString());
    selectValue(transport, config["transport"]);
    selectValue(strategy, config["strategy"]);
    selectValue(startup, config["startup"]);
    selectValue(resetConfig, config["reset_config"]);
    speed->setValue(config["speed_khz"].toInt());
    timeout->setValue(config["timeout_ms"].toInt() / 1000);
    adapterSerial->setText(config["adapter_serial"].toString());
    verify->setChecked(config["verify"].toBool());
    resetRun->setChecked(config["reset_run"].toBool());
    images->setRowCount(0);
    for (const auto &value : config["images"].toArray())
    {
        const auto image = value.toObject();
        const int row = images->rowCount();
        images->insertRow(row);
        images->setItem(row, 0, new QTableWidgetItem(image["path"].toString()));
        images->setItem(row, 1, new QTableWidgetItem(image["address"].toString()));
    }
    syncing = false;
    refreshCatalog();
    refreshState();
    refreshPlan();
}

bool OpenOcdDialog::commit()
{
    if (syncing || service->busy()) { return false; }
    QJsonArray files;
    for (int row = 0; row < images->rowCount(); ++row)
    {
        files.append(QJsonObject {{"path", images->item(row, 0) ? images->item(row, 0)->text() : ""},
            {"address", images->item(row, 1) ? images->item(row, 1)->text() : ""}});
    }
    QJsonObject config {
        {"executable", executable->text()}, {"scripts_dir", scripts->text()}, {"mode", mode->currentData().toString()},
        {"board", board->currentText()}, {"interface", adapter->currentText()}, {"target", target->currentText()},
        {"transport", transport->currentData().toString()}, {"strategy", strategy->currentData().toString()}, {"startup", startup->currentData().toString()},
        {"reset_config", resetConfig->currentData().toString()}, {"speed_khz", speed->value()}, {"timeout_ms", timeout->value() * 1000},
        {"adapter_serial", adapterSerial->text()}, {"verify", verify->isChecked()}, {"reset_run", resetRun->isChecked()}, {"images", files}
    };
    const auto rtt = rttSettings->configuration();
    for (auto field = rtt.begin(); field != rtt.end(); ++field) { config[field.key()] = *field; }
    syncing = true;
    try { service->configure(config, service->revision()); }
    catch (const std::exception &exception)
    {
        syncing = false;
        status->setText(QString::fromUtf8(exception.what()));
        return false;
    }
    syncing = false;
    refreshState();
    refreshPlan();
    return true;
}

void OpenOcdDialog::refreshCatalog()
{
    const QScopedValueRollback<bool> guard(syncing, true);
    const auto catalog = service->catalog();
    const auto fill = [&](Combo *combo, const QString &key)
    {
        combo->hidePopup();
        const auto selected = combo->currentText();
        combo->clear();
        combo->addItem("");
        for (const auto &value : catalog[key].toArray()) { combo->addItem(value.toString()); }
        combo->setEditText(selected);
    };
    fill(board, "boards");
    fill(adapter, "interfaces");
    fill(target, "targets");
    const auto catalogError = catalog["error"].toString();
    catalogHint->setText(catalogError.isEmpty()
        ? QString("%1 个开发板 · %2 个调试器 · %3 个芯片配置%4 · 下拉框内可搜索")
            .arg(catalog["boards"].toArray().size()).arg(catalog["interfaces"].toArray().size()).arg(catalog["targets"].toArray().size())
            .arg(catalog["truncated"].toBool() ? "（列表已截断，可填写完整路径）" : "")
        : catalogError);
    if (!catalogError.isEmpty() && !service->busy() && workspaceTabs->currentWidget() == flashPage
        && service->configuration()["rtt_mode"] != "external")
    {
        configurationTabs->setCurrentIndex(2);
    }
}

void OpenOcdDialog::refreshPlan()
{
    const int visibleFiles = qBound(1, images->rowCount(), 3);
    const int fileHeight = images->horizontalHeader()->sizeHint().height()
        + visibleFiles * images->verticalHeader()->defaultSectionSize() + 4;
    images->setFixedHeight(qMax(76, fileHeight));
    const bool boardMode = mode->currentData() == "board";
    deviceForm->setRowVisible(board, boardMode);
    deviceForm->setRowVisible(adapter, !boardMode);
    deviceForm->setRowVisible(target, !boardMode);
    deviceForm->setRowVisible(transport, !boardMode);
    const bool esp = strategy->currentData() == "esp";
    startup->setEnabled(!esp);
    flowHint->setText(esp
        ? "使用匹配的 Espressif OpenOCD。program_esp 负责初始化；ESP32 使用 JTAG，不支持 SWD。分段地址请按工程填写。"
        : startup->currentData() == "default"
            ? "program 已包含 init → reset init。reset init 会执行板级初始化钩子；无需重复添加。"
            : startup->currentData() == "reset_init" ? "先初始化，复位暂停并执行板级钩子，再擦写固件。"
            : startup->currentData() == "reset_halt" ? "先初始化，复位后暂停，再擦写固件；不会执行 reset-init 钩子。"
            : "先初始化并暂停，不执行复位。仅适用于已经完成所需板级初始化的目标。");
    const bool showingRtt = workspaceTabs->currentWidget() == rttPage;
    auto previewText = showingRtt ? rttPlan : plan;
    try
    {
        const auto preview = service->preview(showingRtt ? "rtt" : "flash");
        const auto program = preview["executable"].toString();
        const auto script = preview["script"].toString();
        previewText->setPlainText(program.isEmpty() ? script : program + "\n\n" + script);
    }
    catch (const std::exception &exception)
    {
        previewText->setPlainText("完善配置后将在这里显示执行步骤。\n\n" + QString::fromUtf8(exception.what()));
    }
}

void OpenOcdDialog::refreshState()
{
    auto state = service->state(logCursor, 200);
    const auto job = state["job_id"].toString();
    const auto operation = state["operation"].toString();
    const bool isRtt = operation == "rtt";
    auto jobLogs = isRtt ? rttLogs : logs;
    if (displayedJob != job)
    {
        displayedJob = job;
        jobLogs->clear();
        if (isRtt)
        {
            workspaceTabs->setCurrentWidget(rttPage);
            rttResultTabs->setCurrentWidget(rttConsole);
        }
        else
        {
            workspaceTabs->setCurrentWidget(flashPage);
            resultTabs->setCurrentWidget(logs);
            if (compact) { compactTabs->setCurrentWidget(output); }
        }
        logCursor = 0;
        state = service->state(logCursor, 200);
    }
    auto scroll = jobLogs->verticalScrollBar();
    const bool follow = scroll->value() >= scroll->maximum() - 2;
    auto cursor = jobLogs->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (state["gap"].toBool() && logCursor > 0) { cursor.insertText("\n… 较早日志已超出保留范围 …\n"); }
    for (const auto &value : state["records"].toArray()) { cursor.insertText(value.toObject()["text"].toString()); }
    logCursor = state["next_cursor"].toInteger();
    if (follow) { scroll->setValue(scroll->maximum()); }
    const bool busy = state["running"].toBool();
    const bool stoppable = busy && state["status"] == "running";
    for (auto editor : configurationEditors) { editor->setEnabled(!busy); }
    for (auto edit : {executable, scripts, adapterSerial}) { edit->setReadOnly(busy); }
    rttSettings->setEnabled(!busy);
    version->setEnabled(!busy);
    connectTarget->setEnabled(!busy);
    flash->setEnabled(!busy);
    flash->setToolTip(busy && isRtt ? "停止 RTT 后可以烧录" : QString());
    cancelJob->setVisible(busy && !isRtt);
    cancelJob->setEnabled(stoppable && !isRtt);
    rttAction->setEnabled(!busy || (isRtt && stoppable));
    rttAction->setText(busy && isRtt ? (stoppable ? "停止 RTT" : "正在停止 RTT…") : "启动 RTT");
    const auto rtt = state["rtt"].toObject();
    const auto rttPhase = rtt["status"].toString();
    const auto jobStatus = state["status"].toString();
    const auto received = rtt["received_bytes"].toInteger();
    progress->setVisible(busy && !(isRtt && rttPhase == "connected"));
    if (rttWasActive && isRtt && !busy && received == 0 && (jobStatus == "failed" || jobStatus == "timed_out")
        && workspaceTabs->currentWidget() == rttPage && rttResultTabs->currentWidget() == rttConsole)
    {
        rttResultTabs->setCurrentWidget(rttLogs);
    }
    rttWasActive = busy && isRtt;
    if (busy && !clock.isActive()) { clock.start(); }
    if (!busy) { clock.stop(); }
    const QMap<QString, QString> titles {{"idle", "尚未启动"}, {"running", "正在执行"}, {"succeeded", "执行完成"},
        {"failed", "执行失败"}, {"cancelled", "已取消"}, {"cancelling", "正在结束进程"}, {"timed_out", "运行超时"}};
    QString text = titles.value(jobStatus);
    if (stoppable && isRtt)
    {
        if (rttPhase == "connected") { text = rtt["connection_bytes"].toInteger() ? "RTT 正在接收" : "RTT 已连接，等待固件输出"; }
        else if (rttPhase == "connecting" || rttPhase == "reconnecting")
        {
            text = (rttPhase == "reconnecting" ? "正在重连 RTT " : "正在连接 RTT ")
                + QString("%1:%2").arg(rtt["host"].toString()).arg(rtt["port"].toInt());
            const int attempts = rtt["connect_attempts"].toInt();
            if (attempts > 1) { text += QString(" · 第 %1 次尝试").arg(attempts); }
        }
        else { text = "正在启动 OpenOCD 并查找 RTT 控制块"; }
        const auto config = service->configuration();
        if (rttPhase == "connected" && config["rtt_mode"] == "managed" && config["rtt_gdb_port"].toInt() > 0)
        {
            text += QString(" · GDB 127.0.0.1:%1").arg(config["rtt_gdb_port"].toInt());
        }
    }
    else if (stoppable && operation == "flash") { text = "正在烧录"; }
    else if (stoppable && operation == "connect") { text = "正在检查连接"; }
    else if (stoppable && operation == "version") { text = "正在检查版本"; }
    if (!job.isEmpty()) { text += QString(" · %1 s").arg(state["elapsed_ms"].toInteger() / 1000.0, 0, 'f', 1); }
    if (!state["error"].toString().isEmpty()) { text += " · " + state["error"].toString(); }
    status->setText(text);
    if (state["has_more"].toBool()) { QTimer::singleShot(0, this, &OpenOcdDialog::refreshState); }
}

void OpenOcdDialog::run(const QString &operation)
{
    try { service->start(operation, service->revision()); }
    catch (const std::exception &exception) { status->setText(QString::fromUtf8(exception.what())); }
}

void OpenOcdDialog::cancelRun(const QString &jobId)
{
    try { service->cancel(jobId); }
    catch (const std::exception &exception) { status->setText(QString::fromUtf8(exception.what())); }
}
}
