// SPDX-License-Identifier: Apache-2.0
#include "model3d_panel.h"
#include <QtConcurrentRun>
#include <QMimeData>

namespace sw
{
namespace
{
QDoubleSpinBox *numeric(QWidget *parent, double low, double high, int decimals = 4)
{
    auto box = new QDoubleSpinBox(parent);
    box->setRange(low, high);
    box->setDecimals(decimals);
    box->setKeyboardTracking(false);
    box->setMinimumWidth(75);
    return box;
}

QLabel *note(const QString &text)
{
    auto label = new QLabel(text);
    label->setWordWrap(true);
    label->setObjectName("muted");
    return label;
}

QWidget *formPage(QVBoxLayout *&layout)
{
    auto page = new QWidget;
    layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    return page;
}
}

ModelPanel::BindingRow ModelPanel::bindingRow(QGridLayout *layout, int row,
    const QString &title, const QString &initial, const QString &id)
{
    BindingRow result;
    result.field = new Combo;
    result.field->setEditable(true);
    result.field->setInsertPolicy(QComboBox::NoInsert);
    result.field->lineEdit()->setPlaceholderText("留空使用常量");
    result.field->lineEdit()->setMaxLength(256);
    result.field->setEditText(initial);
    result.field->setObjectName(id + "Field");
    result.gain = numeric(this, -1e9, 1e9, 6);
    result.gain->setValue(1);
    result.gain->setPrefix("× ");
    result.gain->setObjectName(id + "Gain");
    result.gain->setToolTip("通道值 × 倍率 + 偏移");
    result.offset = numeric(this, -1e9, 1e9, 6);
    result.offset->setPrefix("+ ");
    result.offset->setObjectName(id + "Offset");
    result.offset->setToolTip("通道值 × 倍率 + 偏移；留空的通道可用偏移设置固定值");
    layout->addWidget(new QLabel(title), row * 2, 0, 1, 2);
    layout->addWidget(result.field, row * 2, 2, 1, 4);
    layout->addWidget(result.gain, row * 2 + 1, 0, 1, 3);
    layout->addWidget(result.offset, row * 2 + 1, 3, 1, 3);
    connect(result.field, &QComboBox::currentTextChanged, this, &ModelPanel::scheduleSave);
    connect(result.gain, &QDoubleSpinBox::valueChanged, this, &ModelPanel::scheduleSave);
    connect(result.offset, &QDoubleSpinBox::valueChanged, this, &ModelPanel::scheduleSave);
    return result;
}

ModelPanel::ModelPanel(QWidget *parent) : QWidget(parent)
{
    setAcceptDrops(true);
    setObjectName("model3dPanel");
    auto body = new QVBoxLayout(this);
    body->setContentsMargins(0, 0, 0, 0);
    auto toolbar = new QHBoxLayout;
    import = new QPushButton("导入模型");
    import->setObjectName("import3dModel");
    import->setToolTip("STL / OBJ / PLY；支持拖放，最大 64 MiB / 50 万三角面");
    toolbar->addWidget(import);
    fileLabel = new QLabel("方向示例 · 导入自己的模型开始");
    fileLabel->setObjectName("muted");
    fileLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    toolbar->addWidget(fileLabel, 1);
    auto fit = new QPushButton("适配视图");
    fit->setObjectName("fit3dModel");
    toolbar->addWidget(fit);
    auto camera = new Combo;
    camera->setObjectName("model3dViewDirection");
    camera->addItem("立体视图", "iso");
    camera->addItem("正视图", "front");
    camera->addItem("右视图", "right");
    camera->addItem("俯视图", "top");
    toolbar->addWidget(camera);
    body->addLayout(toolbar);
    auto split = new Splitter(Qt::Horizontal);
    view = new ModelView;
    split->addWidget(view);
    QVBoxLayout *sideBody;
    auto side = formPage(sideBody);
    side->setMinimumWidth(300);
    side->setMaximumWidth(430);
    auto feedRow = new QHBoxLayout;
    feed = new Combo;
    feed->setObjectName("model3dFeed");
    feed->addItem("波形通道 @wave", "wave");
    feed->addItem("脚本解析结果", "script");
    pause = new QCheckBox("暂停");
    pause->setObjectName("model3dPause");
    pause->setToolTip("只暂停模型更新，后台继续接收");
    feedRow->addWidget(feed, 1);
    feedRow->addWidget(pause);
    sideBody->addLayout(feedRow);
    peer = new Combo;
    peer->setObjectName("model3dPeer");
    peer->addItem("本连接", "default");
    peer->setToolTip("网络有多个对端时选择同一个来源，姿态分量不跨对端混用");
    sideBody->addWidget(peer);
    auto tabs = new QTabWidget;
    tabs->setObjectName("model3dMappingTabs");
    QVBoxLayout *positionBody;
    auto positionPage = formPage(positionBody);
    positionBody->addWidget(note("位置 = 通道值 × 倍率 + 偏移。未绑定时默认为 0。"));
    auto positionGrid = new QGridLayout;
    for (int i = 0; i < 3; ++i)
    {
        position[i] = bindingRow(positionGrid, i, QString("位置 %1").arg(QString("XYZ")[i]), {}, QString("modelPosition%1").arg(i));
    }
    positionBody->addLayout(positionGrid);
    positionBody->addStretch();
    tabs->addTab(positionPage, "位置");
    QVBoxLayout *rotationBody;
    auto rotationPage = formPage(rotationBody);
    mode = new Combo;
    mode->setObjectName("model3dRotationMode");
    mode->addItem("欧拉角", "euler");
    mode->addItem("四元数", "quaternion");
    rotationBody->addWidget(mode);
    rotationPages = new QStackedWidget;
    QVBoxLayout *eulerBody;
    auto eulerPage = formPage(eulerBody);
    auto orderRow = new QHBoxLayout;
    order = new Combo;
    order->setObjectName("model3dEulerOrder");
    order->setMaximumWidth(90);
    for (const auto &value : QStringList {"ZYX", "XYZ", "XZY", "YXZ", "YZX", "ZXY"}) { order->addItem(value); }
    convention = new Combo;
    convention->addItem("内禀 / 随动轴", true);
    convention->addItem("外禀 / 固定轴", false);
    convention->setObjectName("model3dConvention");
    convention->setMinimumWidth(160);
    orderRow->addWidget(order);
    orderRow->addWidget(convention, 1);
    eulerBody->addLayout(orderRow);
    units = new Combo;
    units->addItem("角度 °", false);
    units->addItem("弧度 rad", true);
    units->setObjectName("model3dAngleUnits");
    eulerBody->addWidget(units);
    auto angleGrid = new QGridLayout;
    const QStringList defaults {"roll", "pitch", "yaw"};
    for (int i = 0; i < 3; ++i)
    {
        angles[i] = bindingRow(angleGrid, i, QString("绕 %1").arg(QString("XYZ")[i]), defaults[i], QString("modelAngle%1").arg(i));
    }
    eulerBody->addLayout(angleGrid);
    eulerBody->addWidget(note("默认 ZYX 内禀：Rz(yaw) · Ry(pitch) · Rx(roll)。正向遵循右手定则。"));
    eulerBody->addStretch();
    rotationPages->addWidget(eulerPage);
    QVBoxLayout *quaternionBody;
    auto quaternionPage = formPage(quaternionBody);
    quaternionBody->addWidget(note("分别指定 W / X / Y / Z，无论设备按哪种顺序发送。自动归一化，零四元数不更新。"));
    auto quaternionGrid = new QGridLayout;
    for (int i = 0; i < 4; ++i)
    {
        quaternion[i] = bindingRow(quaternionGrid, i, QString("WXYZ")[i], QString("q") + QString("wxyz")[i], QString("modelQuaternion%1").arg(i));
    }
    quaternionBody->addLayout(quaternionGrid);
    quaternionBody->addStretch();
    rotationPages->addWidget(quaternionPage);
    rotationBody->addWidget(rotationPages);
    tabs->addTab(rotationPage, "姿态");
    QVBoxLayout *axesBody;
    auto axesPage = formPage(axesBody);
    axesBody->addWidget(note("输入坐标轴 → 世界坐标轴。位置与姿态一起转换；三个目标轴不能重复。"));
    auto axisForm = new QFormLayout;
    for (int i = 0; i < 3; ++i)
    {
        axes[i] = new Combo;
        axes[i]->setObjectName(QString("modelAxis%1").arg(i));
        for (int axis = 1; axis <= 3; ++axis)
        {
            axes[i]->addItem(QString("+") + QString("XYZ")[axis - 1], axis);
            axes[i]->addItem(QString("−") + QString("XYZ")[axis - 1], -axis);
        }
        axes[i]->setCurrentIndex(i * 2);
        axisForm->addRow(QString("输入 %1").arg(QString("XYZ")[i]), axes[i]);
        connect(axes[i], &QComboBox::currentIndexChanged, this, &ModelPanel::scheduleSave);
    }
    axesBody->addLayout(axisForm);
    axesBody->addWidget(note("模型初始对齐（°）只影响模型自身，不改变遥测坐标。"));
    auto modelForm = new QFormLayout;
    for (int i = 0; i < 3; ++i)
    {
        alignment[i] = numeric(this, -360, 360, 2);
        alignment[i]->setObjectName(QString("modelAlignment%1").arg(i));
        modelForm->addRow(QString("绕 %1").arg(QString("XYZ")[i]), alignment[i]);
        connect(alignment[i], &QDoubleSpinBox::valueChanged, this, &ModelPanel::scheduleSave);
    }
    scale = numeric(this, 0.000001, 1000000, 6);
    scale->setValue(1);
    scale->setObjectName("model3dScale");
    scale->setToolTip("仅缩放模型尺寸。例如毫米模型转米填 0.001；位置倍率单独设置");
    modelForm->addRow("模型比例", scale);
    centered = new QCheckBox("模型中心作为旋转原点");
    centered->setChecked(true);
    centered->setObjectName("model3dCentered");
    modelForm->addRow(centered);
    axesBody->addLayout(modelForm);
    axesBody->addStretch();
    tabs->addTab(axesPage, "坐标轴");
    sideBody->addWidget(tabs, 1);
    sideBody->addWidget(note("示例：@wave roll=10,pitch=20,yaw=30\n以换行结束。也可绑定脚本 emit 输出的数值字段。"));
    auto sideScroll = scrollPanel(side);
    sideScroll->setMinimumWidth(320);
    sideScroll->setMaximumWidth(460);
    split->addWidget(sideScroll);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);
    split->setSizes({760, 350});
    body->addWidget(split, 1);
    status = new QLabel("等待通道数据 · 可先导入模型和设置绑定");
    status->setWordWrap(true);
    status->setObjectName("model3dStatus");
    body->addWidget(status);
    saveTimer.setInterval(250);
    saveTimer.setSingleShot(true);
    connect(&saveTimer, &QTimer::timeout, this, &ModelPanel::changed);
    connect(view, &ModelView::cameraChanged, this, [this] { if (!restoring) { saveTimer.start(); } });
    connect(fit, &QPushButton::clicked, this, [this] { view->fit(); saveTimer.start(); });
    connect(camera, &QComboBox::activated, this, [this, camera] { view->setView(camera->currentData().toString()); });
    connect(import, &QPushButton::clicked, this, [this]
    {
        const auto path = QFileDialog::getOpenFileName(this, "导入 3D 模型", modelPath,
            "三角网格 (*.stl *.obj *.ply);;STL (*.stl);;Wavefront OBJ (*.obj);;PLY (*.ply)");
        if (!path.isEmpty()) { loadFile(path); }
    });
    for (auto combo : {mode, order, convention, units})
    {
        connect(combo, &QComboBox::currentIndexChanged, this, &ModelPanel::scheduleSave);
    }
    connect(scale, &QDoubleSpinBox::valueChanged, this, &ModelPanel::scheduleSave);
    connect(centered, &QCheckBox::toggled, this, &ModelPanel::scheduleSave);
    connect(pause, &QCheckBox::toggled, this, [this] { scheduleSave(); if (!pause->isChecked()) { lastSample = -1; tick(channel); } });
    connect(feed, &QComboBox::currentIndexChanged, this, [this] { resetSource(channel); scheduleSave(); });
    connect(peer, &QComboBox::activated, this, [this]
    {
        peerKey = peer->currentData().toString();
        resetSource(channel);
        scheduleSave();
    });
    connect(&loader, &QFutureWatcher<ModelLoadResult>::finished, this, [this]
    {
        import->setEnabled(true);
        const auto result = loader.result();
        if (!result.error.isEmpty())
        {
            status->setText("导入失败：" + result.error + "；保留当前模型，可重新选择文件");
            pendingCamera = {};
            emit modelLoaded(false);
            return;
        }
        modelPath = pendingPath;
        view->setMesh(result.mesh);
        applyModelTransform();
        view->fit();
        view->restoreCamera(pendingCamera);
        pendingCamera = {};
        fileLabel->setText(QString("%1 · %2 个三角面").arg(QFileInfo(modelPath).fileName()).arg(result.mesh.vertices.size() / 3));
        fileLabel->setToolTip(modelPath);
        status->setText("模型已载入 · 选择通道数据并设置位置或姿态绑定");
        saveTimer.start();
        emit modelLoaded(true);
    });
}

ModelMapping ModelPanel::mapping() const
{
    ModelMapping result;
    const auto read = [](const auto &rows, auto &bindings)
    {
        for (int i = 0; i < int(rows.size()); ++i)
        {
            bindings[i] = {rows[i].field->currentText().trimmed(), rows[i].gain->value(), rows[i].offset->value()};
        }
    };
    read(position, result.position);
    read(angles, result.angles);
    read(quaternion, result.quaternion);
    result.mode = mode->currentData().toString();
    result.order = order->currentText();
    result.intrinsic = convention->currentData().toBool();
    result.radians = units->currentData().toBool();
    for (int i = 0; i < 3; ++i) { result.axes[i] = axes[i]->currentData().toInt(); }
    return result;
}

void ModelPanel::scheduleSave()
{
    if (restoring) { return; }
    rotationPages->setCurrentIndex(mode->currentIndex());
    applyModelTransform();
    applyMapping();
    saveTimer.start();
}

void ModelPanel::applyModelTransform()
{
    view->setModelTransform(scale->value(), centered->isChecked(),
        {float(alignment[0]->value()), float(alignment[1]->value()), float(alignment[2]->value())});
}

void ModelPanel::applyMapping()
{
    const auto pose = evaluateModelPose(mapping(), latestValues);
    if (!pose.valid())
    {
        status->setText(pose.error + " · 保留上一有效姿态");
        return;
    }
    view->setPose(pose);
    status->setText(QString("%1 · X %2   Y %3   Z %4 · %5")
        .arg(pause->isChecked() ? "显示已暂停" : "姿态已更新")
        .arg(pose.position.x(), 0, 'g', 6).arg(pose.position.y(), 0, 'g', 6).arg(pose.position.z(), 0, 'g', 6)
        .arg(peerKey == "default" ? "本连接" : peerKey));
}

void ModelPanel::refreshFieldNames()
{
    const auto current = modelNumericFields(latestValues);
    bool added = false;
    for (const auto &name : current)
    {
        if (!fields.contains(name) && fields.size() < 256) { fields.append(name); added = true; }
    }
    if (!added) { return; }
    fields.sort();
    const auto refresh = [&](auto &rows)
    {
        for (auto &row : rows)
        {
            const QSignalBlocker blocker(row.field);
            const auto text = row.field->currentText();
            row.field->clear();
            row.field->addItem("");
            row.field->addItems(fields);
            row.field->setEditText(text);
        }
    };
    refresh(position);
    refresh(angles);
    refresh(quaternion);
}

void ModelPanel::resetSource(Channel *value)
{
    channel = value;
    generation = value ? value->generation : -1;
    lastSample = -1;
    floorSample = -1;
    latestValues = {};
    fields.clear();
    view->setPose({});
    status->setText("等待所选来源的完整数据帧");
    for (auto *rows : {&position, &angles})
    {
        for (auto &row : *rows)
        {
            const QSignalBlocker blocker(row.field);
            const auto text = row.field->currentText();
            row.field->clear();
            row.field->setEditText(text);
        }
    }
    for (auto &row : quaternion)
    {
        const QSignalBlocker blocker(row.field);
        const auto text = row.field->currentText();
        row.field->clear();
        row.field->setEditText(text);
    }
}

void ModelPanel::tick(Channel *value)
{
    if (channel != value)
    {
        resetSource(value);
    }
    else if (value && generation != value->generation)
    {
        // Reconnection retains shared history. Wait for a new sample instead of
        // presenting a pose from the previous device session as current data.
        resetSource(value);
        clearDisplay();
    }
    if (!channel) { return; }
    const QSignalBlocker blocker(peer);
    QStringList keys {"default"};
    for (auto it = channel->sources.begin(); it != channel->sources.end(); ++it)
    {
        if (!keys.contains(it.key())) { keys.append(it.key()); }
    }
    if (!keys.contains(peerKey)) { keys.append(peerKey); }
    QStringList current;
    for (int i = 0; i < peer->count(); ++i) { current.append(peer->itemData(i).toString()); }
    if (current != keys)
    {
        peer->clear();
        for (const auto &key : keys) { peer->addItem(key == "default" ? "本连接" : key, key); }
        peer->setCurrentIndex(peer->findData(peerKey));
    }
    if (pause->isChecked() || !channel->sources.contains(peerKey)) { return; }
    const auto source = channel->sources.value(peerKey);
    const bool script = feed->currentData() == "script";
    const auto &records = script ? source->scriptRecords : source->samples;
    if (records.empty()) { return; }
    const auto &last = records.back();
    const qint64 sequence = last[script ? "seq" : "index"].toInteger();
    if (sequence == lastSample || sequence <= floorSample) { return; }
    lastSample = sequence;
    latestValues = last[script ? "value" : "values"].toObject();
    refreshFieldNames();
    applyMapping();
}

void ModelPanel::clearDisplay()
{
    floorSample = lastSample;
    if (channel && channel->sources.contains(peerKey))
    {
        const auto source = channel->sources.value(peerKey);
        floorSample = feed->currentData() == "script" ? source->scriptCount : source->count;
    }
    latestValues = {};
    view->setPose({});
    status->setText("已复位显示，等待新数据；共享历史未清除");
}

void ModelPanel::loadFile(const QString &path)
{
    if (loader.isRunning()) { return; }
    pendingPath = QFileInfo(path).absoluteFilePath();
    import->setEnabled(false);
    status->setText("正在后台读取模型…");
    loader.setFuture(QtConcurrent::run([path = pendingPath]
    {
        ModelLoadResult result;
        try { result.mesh = loadModelMesh(path); }
        catch (const std::exception &error) { result.error = QString::fromUtf8(error.what()); }
        return result;
    }));
}

void ModelPanel::setTheme(const Theme &theme)
{
    view->setTheme(theme);
}

QJsonObject ModelPanel::settings() const
{
    return {{"file", modelPath}, {"mapping", mapping().settings()}, {"feed", feed->currentData().toString()},
        {"peer", peerKey}, {"paused", pause->isChecked()}, {"scale", scale->value()}, {"centered", centered->isChecked()},
        {"alignment", QJsonArray {alignment[0]->value(), alignment[1]->value(), alignment[2]->value()}},
        {"camera", view->cameraSettings()}};
}

void ModelPanel::restore(const QJsonObject &settings)
{
    restoring = true;
    const auto config = ModelMapping::restore(settings["mapping"].toObject());
    const auto write = [](auto &rows, const auto &bindings)
    {
        for (int i = 0; i < int(rows.size()); ++i)
        {
            rows[i].field->setEditText(bindings[i].field);
            rows[i].gain->setValue(bindings[i].gain);
            rows[i].offset->setValue(bindings[i].offset);
        }
    };
    write(position, config.position);
    write(angles, config.angles);
    write(quaternion, config.quaternion);
    mode->setCurrentIndex(mode->findData(config.mode));
    order->setCurrentText(config.order);
    convention->setCurrentIndex(config.intrinsic ? 0 : 1);
    units->setCurrentIndex(config.radians ? 1 : 0);
    rotationPages->setCurrentIndex(mode->currentIndex());
    feed->setCurrentIndex(settings["feed"] == "script" ? 1 : 0);
    peerKey = settings["peer"].toString("default");
    if (peer->findData(peerKey) < 0) { peer->addItem(peerKey, peerKey); }
    peer->setCurrentIndex(peer->findData(peerKey));
    for (int i = 0; i < 3; ++i)
    {
        axes[i]->setCurrentIndex(axes[i]->findData(config.axes[i]));
        const auto initial = settings["alignment"].toArray();
        alignment[i]->setValue(i < initial.size() ? initial[i].toDouble() : 0);
    }
    scale->setValue(settings["scale"].toDouble(1));
    centered->setChecked(settings["centered"].toBool(true));
    pause->setChecked(settings["paused"].toBool());
    modelPath = settings["file"].toString();
    pendingCamera = settings["camera"].toObject();
    applyModelTransform();
    view->restoreCamera(pendingCamera);
    if (!modelPath.isEmpty()) { loadFile(modelPath); }
    restoring = false;
    saveTimer.stop();
}

void ModelPanel::dragEnterEvent(QDragEnterEvent *event)
{
    const auto urls = event->mimeData()->urls();
    if (urls.size() == 1 && urls.first().isLocalFile()
        && QStringList {"stl", "obj", "ply"}.contains(QFileInfo(urls.first().toLocalFile()).suffix().toLower()))
    {
        event->acceptProposedAction();
    }
}

void ModelPanel::dropEvent(QDropEvent *event)
{
    const auto urls = event->mimeData()->urls();
    if (urls.size() == 1 && urls.first().isLocalFile())
    {
        loadFile(urls.first().toLocalFile());
        event->acceptProposedAction();
    }
}
}
