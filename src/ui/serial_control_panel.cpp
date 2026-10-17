// SPDX-License-Identifier: Apache-2.0
#include "serial_control_panel.h"
#include "control_card.h"
#include "serial_monitor_dialog.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace sw
{
SerialMonitorDialog *SerialControlPanel::receiveMonitor() const
{
    return monitor.data();
}
struct SerialControlPanel::Row
{
    QJsonObject spec;
    ControlCard *card = nullptr;
    QLabel *preview = nullptr;
    QToolButton *previewToggle = nullptr;
    QPushButton *sendButton = nullptr;
    QCheckBox *automatic = nullptr;
    QPointer<Channel> pendingTarget;
    int pendingGeneration = -1;
    bool dirty = false;
    QElapsedTimer lastSent;
};

namespace
{
class CardTitle : public QLabel
{
public:
    explicit CardTitle(const QString &title) : QLabel(title)
    {
        setObjectName("controlCardTitle");
        setMinimumWidth(0);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter,
            fontMetrics().elidedText(text(), Qt::ElideRight, width()));
    }
};

QString groupNameDialog(QWidget *parent, const QString &title, const QString &initial)
{
    QDialog dialog(parent);
    dialog.setObjectName("controlGroupNameDialog");
    dialog.setWindowTitle(title);
    dialog.resize(420, 230);
    dialog.setMinimumSize(340, 200);
    auto body = new QVBoxLayout(&dialog);
    body->setContentsMargins(16, 12, 16, 12);
    body->addWidget(new QLabel("控件组名称"));
    auto name = new QLineEdit(initial);
    name->setObjectName("controlGroupName");
    name->setMaxLength(80);
    body->addWidget(name);
    body->addStretch();
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto confirm = buttons->button(QDialogButtonBox::Ok);
    confirm->setText("确定");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    confirm->setEnabled(!name->text().trimmed().isEmpty());
    body->addWidget(buttons);
    QObject::connect(name, &QLineEdit::textChanged, &dialog, [confirm](const QString &text)
    { confirm->setEnabled(!text.trimmed().isEmpty()); });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    name->selectAll();
    name->setFocus();
    return dialog.exec() == QDialog::Accepted ? name->text().trimmed() : QString();
}
}

static QString typeName(const QString &type)
{
    if (type == "slider") { return "滑块"; }
    if (type == "number") { return "数字输入"; }
    if (type == "switch") { return "开关"; }
    if (type == "button") { return "按钮"; }
    return "下拉框";
}

static QString decimal(double number, int decimals)
{
    return QLocale::c().toString(number, 'f', decimals);
}

static int groupIndex(const QJsonArray &groups, const QString &id)
{
    for (int index = 0; index < groups.size(); ++index)
    {
        if (groups[index].toObject()["id"].toString() == id) { return index; }
    }
    return -1;
}

static QJsonObject newGroup(const QString &name)
{
    return {{"id", QUuid::createUuid().toString(QUuid::Id128)}, {"name", name}, {"controls", QJsonArray()}};
}

SerialControlPanel::SerialControlPanel(QWidget *parent) : QWidget(parent)
{
    setObjectName("serialControlPanel");
    setAttribute(Qt::WA_StyledBackground);
    auto body = new QVBoxLayout(this);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(10);
    monitorFont = Theme::defaultDataFont();
    auto toolbar = new QFrame(this);
    toolbar->setObjectName("controlToolbar");
    toolbar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    auto toolbarBody = new QVBoxLayout(toolbar);
    toolbarBody->setContentsMargins(10, 8, 10, 8);
    toolbarBody->setSpacing(6);
    auto groupRow = new QHBoxLayout();
    auto groupLabel = new QLabel("控件组");
    groupLabel->setObjectName("controlGroupLabel");
    groupRow->addWidget(groupLabel);
    auto groupCombo = new Combo();
    groupCombo->menuPopup = true;
    groupPicker = groupCombo;
    groupPicker->setObjectName("controlGroup");
    groupPicker->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    groupPicker->setMinimumContentsLength(6);
    groupRow->addWidget(groupPicker, 1);
    connect(groupPicker, &QComboBox::activated, this, [this](int index)
    {
        const auto id = groupPicker->itemData(index).toString();
        // Let the popup release its mouse grab before rebuilding the controls.
        QTimer::singleShot(0, this, [this, id] { selectGroup(id); });
    });
    auto create = new QPushButton("新建组");
    create->setObjectName("addControlGroup");
    groupRow->addWidget(create);
    connect(create, &QPushButton::clicked, this, [this] { manageGroup("new"); });
    auto manage = new QPushButton("管理组");
    auto groupMenu = new QMenu(manage);
    for (const auto &entry : QList<QPair<QString, QString>> {{"rename", "重命名"}, {"copy", "复制控件组"}, {"delete", "删除控件组"}})
    {
        groupMenu->addAction(entry.second, this, [this, action = entry.first] { manageGroup(action); });
    }
    manage->setMenu(groupMenu);
    groupRow->addWidget(manage);
    toolbarBody->addLayout(groupRow);

    toolbarTools = new QBoxLayout(QBoxLayout::LeftToRight);
    auto tools = new QHBoxLayout();
    auto placement = new QHBoxLayout();
    auto add = new QPushButton("添加控件");
    add->setObjectName("addSerialControl");
    auto menu = new QMenu(add);
    for (const auto &type : {"slider", "number", "switch", "button", "select"})
    {
        menu->addAction(typeName(type), this, [this, type]
        {
            if (rows.size() >= 64) { status->setText("每组最多 64 个控件"); return; }
            addControl(defaultControl(type));
            edit(int(rows.size()) - 1);
        });
    }
    add->setMenu(menu);
    auto examples = new QPushButton("常用控件组");
    examples->setObjectName("addControlExamples");
    auto presets = new QMenu(examples);
    presets->setToolTipsVisible(true);
    for (const auto &entry : controlGroupPresets())
    {
        const auto preset = entry.toObject();
        auto item = presets->addAction(preset["name"].toString(), this,
            [this, id = preset["id"].toString()] { addPresetGroup(id); });
        item->setObjectName("controlPreset_" + preset["id"].toString());
        item->setToolTip(preset["description"].toString() + "；新建独立组，默认手动发送");
    }
    presets->addSeparator();
    presets->addAction("基础控件示例（添加到当前组）", this, &SerialControlPanel::addExamples);
    examples->setMenu(presets);
    columns = new Combo();
    columns->setObjectName("controlColumns");
    columns->addItem("自由布局", 0);
    columns->addItem("两列排列", 2);
    columns->addItem("单列排列", 1);
    connect(columns, &QComboBox::activated, this, [this]
    {
        rememberGeometry();
        layoutColumns = -1;
        arrange();
        emit changed();
    });
    auto tidy = new QPushButton("整理排列");
    tidy->setObjectName("tidyControls");
    tidy->setToolTip("按当前窗口宽度重新整理位置，保留控件参数");
    connect(tidy, &QPushButton::clicked, this, [this]
    {
        cancelPending();
        for (const auto &row : rows) { row->spec.remove("rect"); }
        layoutColumns = -1;
        arrange();
        rememberGeometry();
        emit changed();
    });
    tools->addWidget(add);
    tools->addWidget(examples);
    monitorToggle = new QPushButton("收发小窗");
    monitorToggle->setObjectName("controlMonitorToggle");
    monitorToggle->setCheckable(true);
    monitorToggle->setToolTip("打开可调整大小的收发小窗，查看当前串口的发送和回显");
    connect(monitorToggle, &QPushButton::toggled, this, &SerialControlPanel::showMonitor);
    tools->addWidget(monitorToggle);
    auto clearAll = new QPushButton("清空全部");
    clearAll->setObjectName("clearAllControls");
    clearAll->setToolTip("清空本页面所有组内的控件，保留控件组和串口连接");
    connect(clearAll, &QPushButton::clicked, this, [this] { clearControls(); });
    tools->addWidget(clearAll);
    tools->addStretch();
    placement->addWidget(columns);
    placement->addWidget(tidy);
    toolbarTools->addLayout(tools, 1);
    toolbarTools->addLayout(placement);
    toolbarBody->addLayout(toolbarTools);
    body->addWidget(toolbar);

    auto content = new QWidget();
    content->setObjectName("controlCanvas");
    content->setAttribute(Qt::WA_StyledBackground);
    cards = new QGridLayout(content);
    cards->setContentsMargins(0, 4, 4, 4);
    cards->setSpacing(8);
    scroll = new QScrollArea();
    scroll->setObjectName("controlScroll");
    scroll->viewport()->setObjectName("controlViewport");
    scroll->viewport()->setAttribute(Qt::WA_StyledBackground);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    auto canvasFrame = new QFrame(this);
    canvasFrame->setObjectName("controlCanvasFrame");
    auto canvasBody = new QVBoxLayout(canvasFrame);
    canvasBody->setContentsMargins(8, 8, 8, 8);
    canvasBody->addWidget(scroll);
    body->addWidget(canvasFrame, 1);
    status = new QLabel("选择串口后可发送；每个控件默认手动，可单独开启自动发送。");
    status->setWordWrap(true);
    status->setTextFormat(Qt::PlainText);
    status->setObjectName("controlStatus");
    body->addWidget(status);
    sendTimer.setSingleShot(true);
    sendTimer.setTimerType(Qt::PreciseTimer);
    connect(&sendTimer, &QTimer::timeout, this, &SerialControlPanel::flushPending);
    saveTimer.setSingleShot(true);
    saveTimer.setInterval(300);
    connect(&saveTimer, &QTimer::timeout, this, &SerialControlPanel::changed);
    arrangeTimer.setSingleShot(true);
    arrangeTimer.setInterval(0);
    connect(&arrangeTimer, &QTimer::timeout, this, &SerialControlPanel::arrange);
    scroll->viewport()->installEventFilter(this);
    restore({});
}

SerialControlPanel::~SerialControlPanel() = default;

void SerialControlPanel::setTheme(const Theme &theme)
{
    appearance = theme;
    if (monitor) { monitor->setAppearance(theme, monitorFont); }
    for (auto button : findChildren<IconButton *>())
    {
        button->theme = theme;
        button->update();
    }
}

void SerialControlPanel::setDataFont(const QFont &font)
{
    monitorFont = font;
    if (monitor) { monitor->setAppearance(appearance, font); }
}

void SerialControlPanel::showMonitor(bool visible)
{
    if (!visible)
    {
        if (monitor) { monitor->hide(); }
        return;
    }
    if (!monitor)
    {
        monitor = new SerialMonitorDialog(this);
        monitor->setAppearance(appearance, monitorFont);
        monitor->restore(monitorSettings);
        monitor->setChannel(target);
        connect(monitor, &SerialMonitorDialog::zoomRequested, this, &SerialControlPanel::zoomRequested);
        connect(monitor, &SerialMonitorDialog::configurationChanged, this, [this] { saveTimer.start(); });
        connect(monitor, &SerialMonitorDialog::windowHidden, this, [this]
        {
            const QSignalBlocker blocker(monitorToggle);
            monitorToggle->setChecked(false);
        });
    }
    monitor->show();
    monitor->raise();
    monitor->activateWindow();
}

void SerialControlPanel::cancelPending()
{
    sendTimer.stop();
    for (const auto &row : rows)
    {
        row->dirty = false;
        row->pendingTarget.clear();
        row->lastSent.invalidate();
    }
}

void SerialControlPanel::hideEvent(QHideEvent *event)
{
    cancelPending();
    QWidget::hideEvent(event);
}

bool SerialControlPanel::eventFilter(QObject *object, QEvent *event)
{
    if (object == scroll->viewport() && event->type() == QEvent::Resize)
    {
        arrangeTimer.start();
    }
    return QWidget::eventFilter(object, event);
}

void SerialControlPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (toolbarTools)
    {
        toolbarTools->setDirection(width() < 780 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    }
    arrangeTimer.start();
}

void SerialControlPanel::clearCards()
{
    while (auto item = cards->takeAt(0)) { delete item; }
    for (const auto &row : rows)
    {
        delete row->card;
        row->card = nullptr;
    }
    delete empty;
    empty = nullptr;
    layoutColumns = -1;
    scroll->widget()->setMinimumSize(0, 0);
}

void SerialControlPanel::setChannel(Channel *channel)
{
    if (channel && channel->config.kind != "serial") { channel = nullptr; }
    const int nextGeneration = channel ? channel->generation : -1;
    if (target != channel || generation != nextGeneration || !channel || channel->state != "connected")
    {
        cancelPending();
        if (target != channel || generation != nextGeneration)
        {
            status->setText(channel ? "控制目标：" + channel->name : "选择已建立的串口连接");
        }
        target = channel;
        generation = nextGeneration;
    }
    if (monitor) { monitor->setChannel(target); }
    const bool ready = target && target->state == "connected" && !target->transferOwner;
    if (!ready) { cancelPending(); }
    for (const auto &row : rows)
    {
        row->sendButton->setEnabled(ready && row->preview->property("valid").toBool());
    }
}

QJsonObject SerialControlPanel::settings() const
{
    auto savedGroups = groups;
    const int index = groupIndex(savedGroups, activeGroup);
    if (index >= 0)
    {
        auto group = savedGroups[index].toObject();
        QJsonArray controls;
        for (const auto &row : rows)
        {
            auto spec = row->spec;
            if (layoutColumns == 0 && (canvasInitialized || spec.contains("rect")))
            {
                const auto rect = row->card->geometry();
                spec["rect"] = QJsonObject {{"x", rect.x()}, {"y", rect.y()}, {"width", rect.width()}, {"height", rect.height()}};
                spec["layer"] = scroll->widget()->children().indexOf(row->card);
            }
            controls.append(spec);
        }
        group["controls"] = controls;
        savedGroups[index] = group;
    }
    return {{"groups", savedGroups}, {"active_group", activeGroup}, {"columns", columns->currentData().toInt()}, {"layout_version", 2},
        {"monitor", monitor ? monitor->settings() : monitorSettings}};
}

void SerialControlPanel::restore(const QJsonObject &saved)
{
    cancelPending();
    clearCards();
    rows.clear();
    groups = {};
    monitorSettings = saved["monitor"].toObject();
    canvasInitialized = false;
    canvasReferenceWidth = 0;
    QSet<QString> ids;
    auto incoming = saved["groups"].toArray();
    if (incoming.isEmpty())
    {
        auto group = newGroup("默认组");
        auto controls = saved["controls"].toArray();
        for (int index = 0; index < controls.size(); ++index)
        {
            auto spec = controls[index].toObject();
            if (!spec.contains("auto_send")) { spec["auto_send"] = saved["auto_send"].toBool(); }
            controls[index] = spec;
        }
        group["controls"] = controls;
        incoming.append(group);
    }
    for (const auto &entry : incoming)
    {
        if (groups.size() >= 32) { break; }
        auto group = entry.toObject();
        auto id = group["id"].toString();
        if (id.isEmpty() || ids.contains(id)) { id = QUuid::createUuid().toString(QUuid::Id128); }
        ids.insert(id);
        group["id"] = id;
        group["name"] = group["name"].toString("控件组").left(80);
        QJsonArray controls;
        for (const auto &control : group["controls"].toArray())
        {
            if (controls.size() >= 64) { break; }
            const auto spec = control.toObject();
            if (!QStringList {"slider", "number", "switch", "button", "select"}.contains(spec["type"].toString())) { continue; }
            auto merged = defaultControl(spec["type"].toString());
            for (auto field = spec.begin(); field != spec.end(); ++field) { merged[field.key()] = field.value(); }
            controls.append(merged);
        }
        group["controls"] = controls;
        groups.append(group);
    }
    int selected = groupIndex(groups, saved["active_group"].toString());
    if (selected < 0) { selected = 0; }
    activeGroup = groups[selected].toObject()["id"].toString();
    const QSignalBlocker blockGroups(groupPicker);
    groupPicker->clear();
    for (const auto &entry : groups)
    {
        const auto group = entry.toObject();
        groupPicker->addItem(group["name"].toString(), group["id"].toString());
    }
    groupPicker->setCurrentIndex(selected);
    const int mode = saved["layout_version"].toInt() >= 2 ? saved["columns"].toInt() : 0;
    columns->setCurrentIndex(qMax(0, columns->findData(mode)));
    for (const auto &entry : groups[selected].toObject()["controls"].toArray())
    {
        auto row = std::make_unique<Row>();
        row->spec = entry.toObject();
        rows.push_back(std::move(row));
    }
    rebuild();
}

QString SerialControlPanel::addGroup(const QString &name)
{
    if (groups.size() >= 32) { status->setText("每页最多 32 个控件组"); return {}; }
    auto saved = settings();
    auto list = saved["groups"].toArray();
    const auto group = newGroup(name.trimmed().isEmpty() ? "新控件组" : name.trimmed().left(80));
    list.append(group);
    saved["groups"] = list;
    saved["active_group"] = group["id"];
    restore(saved);
    emit changed();
    return activeGroup;
}

bool SerialControlPanel::selectGroup(const QString &id)
{
    if (groupIndex(groups, id) < 0) { return false; }
    if (id == activeGroup) { return true; }
    auto saved = settings();
    saved["active_group"] = id;
    restore(saved);
    emit changed();
    return true;
}

void SerialControlPanel::manageGroup(const QString &action)
{
    cancelPending();
    auto saved = settings();
    auto list = saved["groups"].toArray();
    const int index = groupIndex(list, activeGroup);
    auto group = list[index].toObject();
    if (action == "new" || action == "rename" || action == "copy")
    {
        const QString name = groupNameDialog(this, action == "rename" ? "重命名控件组" : "新建控件组",
            action == "new" ? "" : group["name"].toString() + (action == "copy" ? " 副本" : ""));
        if (name.isEmpty()) { return; }
        if (action == "new") { addGroup(name); return; }
        if (action == "copy")
        {
            if (list.size() >= 32) { status->setText("每页最多 32 个控件组"); return; }
            group["id"] = QUuid::createUuid().toString(QUuid::Id128);
            group["name"] = name.left(80);
            list.append(group);
            saved["active_group"] = group["id"];
        }
        else { group["name"] = name.left(80); list[index] = group; }
    }
    else
    {
        if (list.size() == 1) { status->setText("请至少保留一个控件组；可移除组内控件。"); return; }
        QDialog dialog(this);
        dialog.setObjectName("deleteControlGroupDialog");
        dialog.setWindowTitle("删除控件组");
        dialog.resize(460, 250);
        dialog.setMinimumSize(340, 230);
        auto body = new QVBoxLayout(&dialog);
        body->setContentsMargins(16, 12, 16, 12);
        auto summary = new QLabel("删除“" + group["name"].toString() + "”及其中的控件？");
        summary->setTextFormat(Qt::PlainText);
        summary->setWordWrap(true);
        body->addWidget(summary);
        auto hint = new QLabel("删除后无法撤销。串口连接和收发记录会保留。");
        hint->setWordWrap(true);
        body->addWidget(hint);
        body->addStretch();
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        auto confirm = buttons->button(QDialogButtonBox::Ok);
        confirm->setText("删除控件组");
        confirm->setAutoDefault(false);
        auto cancel = buttons->button(QDialogButtonBox::Cancel);
        cancel->setText("取消");
        cancel->setDefault(true);
        body->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) { return; }
        list.removeAt(index);
        saved["active_group"] = list[qMin(index, int(list.size()) - 1)].toObject()["id"];
    }
    saved["groups"] = list;
    restore(saved);
    emit changed();
}

void SerialControlPanel::addControl(const QJsonObject &spec)
{
    if (rows.size() >= 64) { status->setText("每组最多 64 个控件"); return; }
    auto saved = settings();
    auto list = saved["groups"].toArray();
    const int index = groupIndex(list, activeGroup);
    auto group = list[index].toObject();
    auto controls = group["controls"].toArray();
    auto added = spec;
    if (!added.contains("layer")) { added["layer"] = int(controls.size()) + 1; }
    controls.append(added);
    group["controls"] = controls;
    list[index] = group;
    saved["groups"] = list;
    restore(saved);
    emit changed();
}

void SerialControlPanel::addExamples()
{
    auto saved = settings();
    auto list = saved["groups"].toArray();
    const int current = groupIndex(list, activeGroup);
    auto group = list[current].toObject();
    auto controls = group["controls"].toArray();
    const QStringList types {"slider", "number", "switch", "button", "select"};
    const QStringList titles {"目标转速", "目标温度", "使能输出", "保存参数", "工作模式"};
    const QStringList keys {"speed", "temperature", "enable", "save", "mode"};
    for (int index = 0; index < types.size() && controls.size() < 64; ++index)
    {
        auto spec = defaultControl(types[index]);
        spec["title"] = titles[index];
        spec["key"] = keys[index];
        if (index == 0) { spec["maximum"] = 3000; spec["step"] = 10; spec["value"] = "1200"; }
        if (index == 1) { spec["decimals"] = 1; spec["step"] = 0.5; spec["value"] = "25.0"; }
        if (index == 3) { spec["value"] = "1"; }
        spec["layer"] = int(controls.size()) + 1;
        controls.append(spec);
    }
    group["controls"] = controls;
    list[current] = group;
    saved["groups"] = list;
    restore(saved);
    emit changed();
    status->setText("已向当前组添加示例；拖动标题移动，拖动边缘缩放，点击垃圾桶删除。");
}

bool SerialControlPanel::addPresetGroup(const QString &presetId)
{
    if (groups.size() >= 32) { status->setText("每页最多 32 个控件组"); return false; }
    for (const auto &entry : controlGroupPresets())
    {
        const auto preset = entry.toObject();
        if (preset["id"].toString() != presetId) { continue; }
        cancelPending();
        auto saved = settings();
        auto list = saved["groups"].toArray();
        const QString base = preset["name"].toString();
        QString name = base;
        int suffix = 2;
        const auto nameExists = [&list](const QString &candidate)
        {
            for (const auto &group : list)
            { if (group.toObject()["name"].toString() == candidate) { return true; } }
            return false;
        };
        while (nameExists(name)) { name = base + QString("（%1）").arg(suffix++); }
        auto group = newGroup(name);
        group["controls"] = preset["controls"];
        list.append(group);
        saved["groups"] = list;
        saved["active_group"] = group["id"];
        restore(saved);
        emit changed();
        status->setText("已添加“" + name + "”；默认手动发送，使用 SET {key} {value} + CRLF，请按设备协议编辑模板。");
        return true;
    }
    return false;
}

void SerialControlPanel::clearControls()
{
    cancelPending();
    const auto saved = settings();
    const auto list = saved["groups"].toArray();
    int count = 0;
    for (const auto &entry : list)
    {
        count += entry.toObject()["controls"].toArray().size();
    }
    if (!count) { status->setText("所选范围内没有控件，无需清空。"); return; }
    QDialog dialog(this);
    dialog.setObjectName("clearControlsDialog");
    dialog.setWindowTitle("清空全部控件");
    dialog.resize(460, 250);
    dialog.setMinimumSize(340, 230);
    auto body = new QVBoxLayout(&dialog);
    body->setContentsMargins(16, 12, 16, 12);
    auto summary = new QLabel(QString("清空本页面 %1 个控件组中的全部 %2 个控件？").arg(list.size()).arg(count));
    summary->setObjectName("clearControlsSummary");
    summary->setTextFormat(Qt::PlainText);
    summary->setWordWrap(true);
    body->addWidget(summary);
    auto hint = new QLabel("控件组、串口连接和收发记录会保留。清空后无法撤销。此操作不会发送数据。");
    hint->setWordWrap(true);
    body->addWidget(hint);
    body->addStretch();
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto confirm = buttons->button(QDialogButtonBox::Ok);
    confirm->setObjectName("confirmClearControls");
    confirm->setText("清空控件");
    confirm->setAutoDefault(false);
    auto cancel = buttons->button(QDialogButtonBox::Cancel);
    cancel->setText("取消");
    cancel->setDefault(true);
    body->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) { return; }
    auto result = settings();
    auto cleared = result["groups"].toArray();
    for (int index = 0; index < cleared.size(); ++index)
    {
        auto group = cleared[index].toObject();
        group["controls"] = QJsonArray();
        cleared[index] = group;
    }
    result["groups"] = cleared;
    restore(result);
    emit changed();
    status->setText(QString("已清空 %1 个控件，控件组和连接已保留。").arg(count));
}

void SerialControlPanel::reorder(int index, int destination)
{
    cancelPending();
    auto saved = settings();
    auto list = saved["groups"].toArray();
    const int current = groupIndex(list, activeGroup);
    auto group = list[current].toObject();
    auto controls = group["controls"].toArray();
    if (index < 0 || index >= controls.size()) { return; }
    const auto entry = controls.takeAt(index);
    if (destination >= 0 && destination <= controls.size()) { controls.insert(destination, entry); }
    group["controls"] = controls;
    list[current] = group;
    saved["groups"] = list;
    restore(saved);
    emit changed();
}

void SerialControlPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, &SerialControlPanel::initializeCanvas);
}

void SerialControlPanel::initializeCanvas()
{
    if (!isVisible()) { return; }
    arrange();
    if (layoutColumns == 0) { rememberGeometry(); }
}

void SerialControlPanel::rememberGeometry()
{
    if (layoutColumns != 0) { return; }
    canvasInitialized = true;
    canvasReferenceWidth = qMax(340, scroll->viewport()->width() - 8);
    for (const auto &row : rows)
    {
        const auto rect = row->card->geometry();
        row->spec["rect"] = QJsonObject {{"x", rect.x()}, {"y", rect.y()}, {"width", rect.width()}, {"height", rect.height()}};
        row->spec["layer"] = scroll->widget()->children().indexOf(row->card);
    }
}

void SerialControlPanel::updateCanvasExtent()
{
    if (layoutColumns != 0) { return; }
    QSize extent(0, 0);
    for (const auto &row : rows)
    {
        extent.setWidth(qMax(extent.width(), row->card->geometry().right() + 9));
        extent.setHeight(qMax(extent.height(), row->card->geometry().bottom() + 9));
    }
    scroll->widget()->setMinimumSize(extent);
}

void SerialControlPanel::arrange()
{
    const int requested = columns->currentData().toInt();
    const int count = requested == 0 ? 0 : (requested == 2 && width() >= 700 ? 2 : 1);
    const bool changed = layoutColumns != count;
    layoutColumns = count;
    if (changed)
    {
        while (auto item = cards->takeAt(0)) { delete item; }
        for (int row = 0; row < cards->rowCount(); ++row) { cards->setRowStretch(row, 0); }
        for (int column = 0; column < 2; ++column) { cards->setColumnStretch(column, column < count ? 1 : 0); }
    }
    for (const auto &row : rows) { row->card->setFreeLayout(count == 0); }
    if (count == 0)
    {
        const int available = qMax(340, scroll->viewport()->width() - 8);
        // Keep a stable baseline so repeated resizing does not accumulate rounding.
        // A restored canvas first keeps its saved geometry; live resizing scales it.
        const qreal scale = canvasInitialized && canvasReferenceWidth > 0
            ? qreal(available) / canvasReferenceWidth : 1.0;
        const int initialColumns = available >= 700 ? 2 : 1;
        const int cardWidth = qMax(340, (available - 8 * (initialColumns - 1)) / initialColumns);
        for (int index = 0; index < int(rows.size()); ++index)
        {
            auto row = rows[size_t(index)].get();
            const auto saved = row->spec["rect"].toObject();
            QRect rect(index % initialColumns * (cardWidth + 8), index / initialColumns * 148, cardWidth, 140);
            if (!saved.isEmpty())
            {
                rect = QRect(qBound(0, qRound(saved["x"].toInt() * scale), 16000),
                    qBound(0, saved["y"].toInt(), 16000),
                    qBound(320, qRound(saved["width"].toInt(380) * scale), 4000),
                    qBound(132, saved["height"].toInt(140), 4000));
            }
            rect.setSize(rect.size().expandedTo(row->card->minimumSize()));
            rect.moveLeft(qMin(rect.left(), 16000 - rect.width()));
            rect.moveTop(qMin(rect.top(), 16000 - rect.height()));
            row->card->setGeometry(rect);
        }
        if (empty) { empty->setGeometry(8, 8, qMax(300, available - 16), 64); }
        updateCanvasExtent();
    }
    else if (changed)
    {
        scroll->widget()->setMinimumSize(0, 0);
        if (empty) { cards->addWidget(empty, 0, 0, 1, count); }
        for (int index = 0; index < int(rows.size()); ++index)
        {
            cards->addWidget(rows[size_t(index)]->card, index / count, index % count);
        }
        cards->setRowStretch((int(rows.size()) + count - 1) / count + 1, 1);
    }
}

void SerialControlPanel::rebuild()
{
    cancelPending();
    clearCards();
    if (rows.empty())
    {
        empty = new QLabel("当前组还没有控件，点击“添加控件”或选择“常用控件组”。", scroll->widget());
        empty->setWordWrap(true);
    }
    for (int index = 0; index < int(rows.size()); ++index)
    {
        auto row = rows[size_t(index)].get();
        row->card = new ControlCard(scroll->widget());
        row->card->setObjectName("serialControlCard");
        row->card->setAttribute(Qt::WA_StyledBackground);
        row->card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto body = new QVBoxLayout(row->card);
        body->setContentsMargins(10, 6, 10, 18);
        body->setSpacing(4);
        auto header = new QWidget(row->card);
        header->setMinimumHeight(26);
        auto heading = new QHBoxLayout(header);
        heading->setContentsMargins(0, 0, 0, 0);
        heading->setSpacing(4);
        auto title = new CardTitle(row->spec["title"].toString());
        title->setTextFormat(Qt::PlainText);
        title->setToolTip(row->spec["title"].toString() + " · " + typeName(row->spec["type"].toString()) + " · " + row->spec["key"].toString());
        title->setWordWrap(false);
        title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        heading->addWidget(title, 1);
        auto badge = new QLabel(typeName(row->spec["type"].toString()));
        badge->setObjectName("controlKindBadge");
        heading->addWidget(badge);
        auto editButton = new QPushButton("编辑");
        editButton->setObjectName("editControl");
        heading->addWidget(editButton);
        connect(editButton, &QPushButton::clicked, this, [this, index] { edit(index); });
        auto more = new QToolButton();
        more->setObjectName("controlLayerMenu");
        more->setText("图层");
        more->setAccessibleName("调整控件图层");
        more->setToolTip("调整控件重叠时的上下层级");
        auto menu = new QMenu(more);
        menu->addAction("移至最上层", this, [this, row]
        {
            cancelPending();
            row->card->raise();
            rememberGeometry();
            emit changed();
        });
        menu->addAction("移至最下层", this, [this, row]
        {
            cancelPending();
            row->card->lower();
            rememberGeometry();
            emit changed();
        });
        more->setMenu(menu);
        more->setPopupMode(QToolButton::InstantPopup);
        heading->addWidget(more);
        auto remove = new IconButton("trash", "删除此控件", row->card);
        remove->setObjectName("deleteControl");
        remove->theme = appearance;
        remove->setFixedSize(26, 26);
        heading->addWidget(remove);
        connect(remove, &QAbstractButton::clicked, this, [this, index]
        {
            cancelPending();
            const auto expectedGroup = activeGroup;
            QTimer::singleShot(0, this, [this, index, expectedGroup]
            {
                if (activeGroup == expectedGroup) { reorder(index, -1); }
            });
        });
        row->card->setDragHandle(header, title);
        connect(row->card, &ControlCard::interactionStarted, this, [this, row]
        {
            cancelPending();
            rememberGeometry();
            row->card->raise();
        });
        connect(row->card, &ControlCard::geometryEdited, this, [this]
        {
            rememberGeometry();
            updateCanvasExtent();
            saveTimer.start();
        });
        body->addWidget(header);
        auto valuesHost = new QWidget(row->card);
        valuesHost->setMinimumHeight(34);
        auto values = new QHBoxLayout(valuesHost);
        values->setContentsMargins(0, 0, 0, 0);
        const QString type = row->spec["type"].toString();
        if (type == "slider" || type == "number")
        {
            const int decimals = qBound(0, row->spec["decimals"].toInt(), 6);
            const double minimum = qBound(-1e9, row->spec["minimum"].toDouble(), 1e9);
            const double maximum = qBound(minimum, row->spec["maximum"].toDouble(100), 1e9);
            const double step = qMax(std::pow(10.0, -decimals), row->spec["step"].toDouble(1));
            auto number = new QDoubleSpinBox();
            number->setObjectName("controlValue");
            number->setLocale(QLocale::c());
            number->setAlignment(Qt::AlignRight);
            number->setDecimals(decimals);
            number->setRange(minimum, maximum);
            number->setSingleStep(step);
            number->setKeyboardTracking(false);
            number->setValue(row->spec["value"].toString().toDouble());
            row->spec["value"] = decimal(number->value(), decimals);
            if (type == "slider")
            {
                auto slider = new QSlider(Qt::Horizontal);
                slider->setObjectName("controlSlider");
                // QSlider has an int coordinate; scale it independently of numeric precision.
                const double steps = (maximum - minimum) / step;
                const int ticks = int(qBound(1.0, std::ceil(steps), 1000000.0));
                slider->setRange(0, ticks);
                const auto position = [minimum, maximum, ticks](double value)
                { return maximum > minimum ? qRound((value - minimum) / (maximum - minimum) * ticks) : 0; };
                slider->setValue(position(number->value()));
                connect(slider, &QSlider::valueChanged, number, [number, minimum, maximum, step, ticks](int value)
                {
                    const double raw = minimum + (maximum - minimum) * double(value) / ticks;
                    number->setValue(value == ticks ? maximum : qBound(minimum, minimum + std::round((raw - minimum) / step) * step, maximum));
                });
                connect(number, &QDoubleSpinBox::valueChanged, slider, [slider, position](double value)
                {
                    const QSignalBlocker blocker(slider);
                    slider->setValue(position(value));
                });
                values->addWidget(slider, 1);
                number->setMaximumWidth(decimals > 3 ? 144 : 118);
            }

            values->addWidget(number, type == "number" ? 1 : 0);
            connect(number, &QDoubleSpinBox::valueChanged, this, [this, row, decimals](double value)
            { valueChanged(row, decimal(value, decimals)); });
        }
        else if (type == "switch")
        {
            auto toggle = new QCheckBox("开启");
            toggle->setObjectName("controlSwitch");
            toggle->setChecked(row->spec["value"].toString() == "1");
            row->spec["value"] = toggle->isChecked() ? "1" : "0";
            values->addWidget(toggle);
            values->addStretch();
            connect(toggle, &QCheckBox::toggled, this, [this, row](bool checked)
            { valueChanged(row, checked ? "1" : "0"); });
        }
        else if (type == "select")
        {
            auto choices = new QComboBox();
            choices->setObjectName("controlSelect");
            for (const auto &line : row->spec["options"].toString().split('\n', Qt::SkipEmptyParts))
            {
                const auto separator = line.indexOf('=');
                if (separator < 1) { continue; }
                choices->addItem(line.left(separator).trimmed(), line.mid(separator + 1).trimmed());
            }
            choices->setCurrentIndex(qMax(0, choices->findData(row->spec["value"].toString())));
            row->spec["value"] = choices->currentData().toString();
            values->addWidget(choices, 1);
            connect(choices, &QComboBox::activated, this, [this, row, choices]
            { valueChanged(row, choices->currentData().toString()); });
        }
        else
        {
            auto value = new QLabel("固定值：" + row->spec["value"].toString());
            value->setTextFormat(Qt::PlainText);
            value->setWordWrap(true);
            values->addWidget(value, 1);
        }
        row->sendButton = new QPushButton(type == "button" ? "执行动作" : "发送");
        row->sendButton->setObjectName("sendControl");
        values->addWidget(row->sendButton);
        connect(row->sendButton, &QPushButton::clicked, this, [this, row] { row->dirty = false; send(row); });
        body->addWidget(valuesHost);
        auto footerHost = new QWidget(row->card);
        footerHost->setMinimumHeight(26);
        auto footer = new QHBoxLayout(footerHost);
        footer->setContentsMargins(0, 0, 0, 0);
        row->automatic = new QCheckBox("自动发送", row->card);
        row->automatic->setObjectName("controlAutoSend");
        row->automatic->setChecked(row->spec["auto_send"].toBool());
        row->automatic->setToolTip("调节时立即提交首个值，此后最多每 50 毫秒发送最新值；切换开关本身不发送。");
        row->automatic->setVisible(type != "button");
        footer->addWidget(row->automatic);
        connect(row->automatic, &QCheckBox::toggled, this, [this, row](bool enabled)
        {
            row->dirty = false;
            row->lastSent.invalidate();
            row->spec["auto_send"] = enabled;
            emit changed();
        });
        footer->addStretch();
        row->previewToggle = new QToolButton();
        row->previewToggle->setText("报文预览");
        row->previewToggle->setObjectName("controlPreviewToggle");
        row->previewToggle->setCheckable(true);
        footer->addWidget(row->previewToggle);
        body->addWidget(footerHost);
        row->preview = new QLabel();
        row->preview->setObjectName("controlPreview");
        row->preview->setTextFormat(Qt::PlainText);
        row->preview->setWordWrap(true);
        row->preview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        row->preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
        body->addWidget(row->preview);
        body->addStretch(1);
        connect(row->previewToggle, &QToolButton::toggled, this, [this, row]
        {
            updatePreview(row);
            row->card->fitContents();
            rememberGeometry();
            updateCanvasExtent();
        });
        updatePreview(row);
        row->card->fitContents();
    }
    arrange();
    std::vector<Row *> layers;
    for (const auto &row : rows) { layers.push_back(row.get()); }
    std::stable_sort(layers.begin(), layers.end(), [](const Row *left, const Row *right)
    { return left->spec["layer"].toInt() < right->spec["layer"].toInt(); });
    for (auto row : layers)
    {
        row->card->raise();
        row->card->show();
    }
    setChannel(target);
    if (isVisible()) { QTimer::singleShot(0, this, &SerialControlPanel::initializeCanvas); }
}

void SerialControlPanel::updatePreview(Row *row)
{
    try
    {
        const auto packet = controlPacket(row->spec);
        QString readable;
        if (row->spec["format"] == "text")
        {
            const auto codec = QTextCodec::codecForName(row->spec["encoding"].toString().toLatin1());
            readable = "文本：" + codec->toUnicode(packet).replace('\r', "\\r").replace('\n', "\\n") + "\n";
        }
        row->preview->setText(readable + QString("字节（%1）：%2").arg(packet.size()).arg(QString::fromLatin1(packet.toHex(' ').toUpper())));
        row->preview->setProperty("valid", true);
        row->preview->setVisible(row->previewToggle->isChecked());
    }
    catch (const std::exception &failure)
    {
        row->preview->setText("格式错误：" + QString::fromUtf8(failure.what()));
        row->preview->setProperty("valid", false);
        row->preview->show();
    }
    row->sendButton->setEnabled(target && target->state == "connected" && !target->transferOwner
        && row->preview->property("valid").toBool());
}

void SerialControlPanel::valueChanged(Row *row, const QString &value)
{
    row->spec["value"] = value;
    updatePreview(row);
    if (row->automatic->isChecked() && isVisible() && row->sendButton->isEnabled())
    {
        row->pendingTarget = target;
        row->pendingGeneration = target->generation;
        row->dirty = true;
        if (!sendTimer.isActive()) { sendTimer.start(0); }
    }
    // Defer disk persistence while dragging; explicit close still snapshots current values.
    saveTimer.start();
}

void SerialControlPanel::flushPending()
{
    int next = 50;
    bool pending = false;
    for (const auto &item : rows)
    {
        auto row = item.get();
        if (!row->dirty) { continue; }
        if (!isVisible() || !row->automatic->isChecked() || !target || target != row->pendingTarget
            || target->generation != row->pendingGeneration || target->state != "connected" || target->transferOwner)
        {
            row->dirty = false;
            continue;
        }
        const qint64 elapsed = row->lastSent.isValid() ? row->lastSent.elapsed() : 50;
        const bool congested = target->serial && target->serial->bytesToWrite() > qBound(32, target->config.baudrate / 300, 512);
        if (elapsed >= 50 && !congested)
        {
            row->dirty = false;
            send(row);
            row->lastSent.start();
        }
        else
        {
            pending = true;
            next = qMin(next, congested ? 10 : qMax(1, int(50 - elapsed)));
        }
    }
    if (pending) { sendTimer.start(next); }
}

void SerialControlPanel::send(Row *row)
{
    try
    {
        if (!target || target->config.kind != "serial" || target->state != "connected")
        {
            status->setText("发送失败：请先建立并选择串口连接");
            return;
        }
        const auto packet = controlPacket(row->spec);
        target->send(packet, {}, "control-panel");
        status->setText(QString("已向 %1 发送 %2 字节 · %3").arg(target->name).arg(packet.size()).arg(row->spec["title"].toString()));
    }
    catch (const std::exception &failure) { status->setText("发送失败：" + QString::fromUtf8(failure.what())); }
}

void SerialControlPanel::edit(int index)
{
    cancelPending();
    auto row = rows[size_t(index)].get();
    const auto original = row->spec;
    QDialog dialog(this);
    dialog.setObjectName("serialControlEditor");
    dialog.setWindowTitle("控件与发送格式");
    dialog.resize(620, 620);
    auto body = new QVBoxLayout(&dialog);
    auto content = new QWidget();
    content->setObjectName("controlEditorContent");
    content->setAttribute(Qt::WA_StyledBackground);
    auto form = new QFormLayout(content);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto title = new QLineEdit(original["title"].toString());
    auto key = new QLineEdit(original["key"].toString());
    title->setMaxLength(80);
    key->setMaxLength(64);
    form->addRow("名称", title);
    form->addRow("参数名 {key}", key);
    auto group = new QComboBox();
    group->setObjectName("controlEditorGroup");
    for (const auto &entry : groups)
    {
        const auto item = entry.toObject();
        group->addItem(item["name"].toString(), item["id"].toString());
    }
    group->setCurrentIndex(group->findData(activeGroup));
    form->addRow("所属控件组", group);
    const QString type = original["type"].toString();
    const auto spin = [&](const QString &field, const QString &label)
    {
        auto control = new QDoubleSpinBox();
        control->setObjectName("controlEditor_" + field);
        control->setLocale(QLocale::c());
        control->setDecimals(6);
        control->setRange(-1e9, 1e9);
        control->setValue(original[field].toDouble());
        form->addRow(label, control);
        control->setVisible(type == "slider" || type == "number");
        form->labelForField(control)->setVisible(control->isVisibleTo(content));
        return control;
    };
    auto minimum = spin("minimum", "最小值");
    auto maximum = spin("maximum", "最大值");
    auto step = spin("step", "调节步长");
    auto decimals = new QSpinBox();
    decimals->setObjectName("controlEditorDecimals");
    decimals->setRange(0, 6);
    decimals->setValue(original["decimals"].toInt());
    if (type == "slider" || type == "number") { form->addRow("小数位数", decimals); }
    else { decimals->setParent(content); decimals->hide(); }
    auto fixedValue = new QLineEdit(original["value"].toString());
    if (type == "button") { form->addRow("按钮发送值", fixedValue); }
    else { fixedValue->setParent(content); fixedValue->hide(); }
    auto options = new QPlainTextEdit(original["options"].toString());
    options->setMaximumHeight(100);
    if (type == "select") { form->addRow("选项（每行 名称=值）", options); }
    else { options->setParent(content); options->hide(); }
    auto format = new QComboBox();
    format->setObjectName("controlFormat");
    format->addItem("文本模板", "text");
    format->addItem("HEX 字节模板", "hex");
    format->setCurrentIndex(qMax(0, format->findData(original["format"].toString())));
    form->addRow("发送格式", format);
    auto pattern = new QPlainTextEdit(original["pattern"].toString());
    pattern->setObjectName("controlPattern");
    pattern->setMaximumHeight(110);
    form->addRow("报文模板", pattern);
    auto encoding = new QComboBox();
    encoding->addItems({"utf-8", "GBK", "ASCII"});
    encoding->setCurrentText(original["encoding"].toString());
    form->addRow("文本编码", encoding);
    auto newline = new QComboBox();
    newline->setObjectName("controlNewline");
    newline->addItem("无行尾", "none");
    newline->addItem("换行 LF", "lf");
    newline->addItem("回车 CR", "cr");
    newline->addItem("回车换行 CRLF", "crlf");
    newline->setCurrentIndex(qMax(0, newline->findData(original["newline"].toString())));
    form->addRow("文本行尾", newline);
    auto autoSend = new QCheckBox("调节时自动发送");
    autoSend->setChecked(original["auto_send"].toBool());
    if (type != "button") { form->addRow("发送方式", autoSend); }
    else { autoSend->setParent(content); autoSend->hide(); }
    auto help = new QLabel("文本：SET {key} {value}，也可填写 JSON，如 {\"speed\":{value}}。\n"
        "HEX：AA 55 {value:u16le} 0D 0A。支持 u8/i8、u16/i16、u32/i32、f32；多字节需加 le（低字节在前）或 be（高字节在前）。HEX 不附加文本行尾。\n"
        "自动发送只响应调节操作；动作按钮始终点击才发送。设备端需自行实现对应协议。");
    help->setWordWrap(true);
    form->addRow(help);
    auto preview = new QLabel();
    preview->setWordWrap(true);
    preview->setTextFormat(Qt::PlainText);
    form->addRow("当前值预览", preview);
    auto scroll = new QScrollArea();
    scroll->setObjectName("controlEditorScroll");
    scroll->viewport()->setObjectName("controlEditorViewport");
    scroll->viewport()->setAttribute(Qt::WA_StyledBackground);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    body->addWidget(scroll, 1);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText("保存");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    auto validation = new QLabel();
    validation->setObjectName("controlValidation");
    validation->setTextFormat(Qt::PlainText);
    validation->setWordWrap(true);
    body->addWidget(validation);
    body->addWidget(buttons);
    dialog.setMinimumSize(340, 280);
    const auto collect = [&]
    {
        auto spec = original;
        spec["auto_send"] = type != "button" && autoSend->isChecked();
        spec["title"] = title->text().trimmed();
        spec["key"] = key->text();
        spec["minimum"] = minimum->value();
        spec["maximum"] = maximum->value();
        spec["step"] = step->value();
        spec["decimals"] = decimals->value();
        spec["options"] = options->toPlainText();
        spec["format"] = format->currentData().toString();
        spec["pattern"] = pattern->toPlainText();
        spec["encoding"] = encoding->currentText();
        spec["newline"] = newline->currentData().toString();
        if (type == "button") { spec["value"] = fixedValue->text(); }
        if (type == "slider" || type == "number")
        {
            spec["value"] = decimal(qBound(minimum->value(), original["value"].toString().toDouble(),
                qMax(minimum->value(), maximum->value())), decimals->value());
        }
        if (type == "select")
        {
            const auto lines = options->toPlainText().split('\n', Qt::SkipEmptyParts);
            bool found = false;
            QString first;
            for (const auto &line : lines)
            {
                const int separator = line.indexOf('=');
                if (separator < 1) { continue; }
                const auto value = line.mid(separator + 1).trimmed();
                if (first.isNull()) { first = value; }
                if (value == spec["value"].toString()) { found = true; }
            }
            if (!found) { spec["value"] = first; }
        }
        return spec;
    };
    const auto refresh = [&]
    {
        encoding->setEnabled(format->currentData() == "text");
        newline->setEnabled(format->currentData() == "text");
        bool valid = true;
        try
        {
            if ((type == "slider" || type == "number") && (maximum->value() <= minimum->value()
                || step->value() + 1e-12 < std::pow(10.0, -decimals->value())))
            { throw std::runtime_error("最大值需大于最小值，步长需大于零且与小数位数一致"); }
            const int destination = groupIndex(groups, group->currentData().toString());
            if (group->currentData().toString() != activeGroup
                && groups[destination].toObject()["controls"].toArray().size() >= 64)
            { throw std::runtime_error("目标组已满，请选择其他控件组"); }
            if (type == "select")
            {
                const auto lines = options->toPlainText().split('\n', Qt::SkipEmptyParts);
                if (lines.isEmpty()) { throw std::runtime_error("至少填写一个下拉选项"); }
                for (const auto &line : lines)
                {
                    if (line.indexOf('=') < 1) { throw std::runtime_error("选项格式为 名称=值，每行一项"); }
                }
            }
            preview->setText(QString::fromLatin1(controlPacket(collect()).toHex(' ').toUpper()));
        }
        catch (const std::exception &failure) { valid = false; preview->setText(QString::fromUtf8(failure.what())); }
        validation->setText(valid ? QString() : preview->text());
        validation->setVisible(!valid);
        buttons->button(QDialogButtonBox::Save)->setEnabled(valid);
    };
    for (auto control : {title, key, fixedValue}) { connect(control, &QLineEdit::textChanged, &dialog, refresh); }
    for (auto control : {minimum, maximum, step}) { connect(control, &QDoubleSpinBox::valueChanged, &dialog, refresh); }
    connect(decimals, &QSpinBox::valueChanged, &dialog, refresh);
    connect(options, &QPlainTextEdit::textChanged, &dialog, refresh);
    connect(pattern, &QPlainTextEdit::textChanged, &dialog, refresh);
    connect(group, &QComboBox::currentIndexChanged, &dialog, refresh);
    connect(autoSend, &QCheckBox::toggled, &dialog, refresh);
    for (auto control : {format, encoding, newline}) { connect(control, &QComboBox::currentIndexChanged, &dialog, refresh); }
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    refresh();
    if (dialog.exec() == QDialog::Accepted)
    {
        const auto spec = collect();
        const QString destination = group->currentData().toString();
        if (destination == activeGroup)
        {
            row->spec = spec;
            rebuild();
        }
        else
        {
            auto saved = settings();
            auto list = saved["groups"].toArray();
            const int from = groupIndex(list, activeGroup);
            const int to = groupIndex(list, destination);
            auto sourceGroup = list[from].toObject();
            auto destinationGroup = list[to].toObject();
            auto sourceControls = sourceGroup["controls"].toArray();
            auto destinationControls = destinationGroup["controls"].toArray();
            if (destinationControls.size() >= 64)
            {
                status->setText("目标组已有 64 个控件，未移动；请调整后重试。");
                return;
            }
            sourceControls.removeAt(index);
            destinationControls.append(spec);
            sourceGroup["controls"] = sourceControls;
            destinationGroup["controls"] = destinationControls;
            list[from] = sourceGroup;
            list[to] = destinationGroup;
            saved["groups"] = list;
            restore(saved);
        }
        emit changed();
    }
}
}
