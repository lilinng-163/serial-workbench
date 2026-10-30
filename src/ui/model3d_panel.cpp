// SPDX-License-Identifier: Apache-2.0
#include "model3d_panel.h"
#include <QtConcurrentRun>
#include <QMimeData>
#include <QColorDialog>
#include <QSaveFile>

namespace sw
{
namespace
{
const QString unboundLabel = QStringLiteral("未绑定（使用偏移常量）");
class CompactDoubleSpinBox : public QDoubleSpinBox
{
public:
    using QDoubleSpinBox::QDoubleSpinBox;

protected:
    QString textFromValue(double value) const override
    {
        auto numberLocale = locale();
        numberLocale.setNumberOptions(QLocale::OmitGroupSeparator);
        auto text = numberLocale.toString(value, 'f', decimals());
        if (text.contains(numberLocale.decimalPoint()))
        {
            while (text.endsWith(numberLocale.zeroDigit())) { text.chop(numberLocale.zeroDigit().size()); }
            if (text.endsWith(numberLocale.decimalPoint())) { text.chop(numberLocale.decimalPoint().size()); }
        }
        return text;
    }
};

QDoubleSpinBox *numeric(QWidget *parent, double low, double high, int decimals = 4)
{
    auto box = new CompactDoubleSpinBox(parent);
    box->setRange(low, high);
    box->setDecimals(decimals);
    box->setKeyboardTracking(false);
    box->setMinimumWidth(box->fontMetrics().horizontalAdvance("-1000000000.000001") + 60);
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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

std::pair<ModelMapping, ModelMapping> separateMapping(const ModelMapping &mapping)
{
    auto position = mapping;
    position.mode = "euler";
    position.angles = {};
    auto rotation = mapping;
    rotation.position = {};
    return {position, rotation};
}
}

ModelPanel::BindingRow ModelPanel::bindingRow(QGridLayout *layout, int row,
    const QString &title, const QString &initial, const QString &id)
{
    BindingRow result;
    result.field = new Combo;
    result.field->setEditable(true);
    result.field->setInsertPolicy(QComboBox::NoInsert);
    result.field->addItem(unboundLabel);
    result.field->lineEdit()->setPlaceholderText("留空使用常量");
    result.field->lineEdit()->setMaxLength(256);
    result.field->setEditText(initial.isEmpty() ? unboundLabel : initial);
    result.field->setObjectName(id + "Field");
    result.gain = numeric(this, -1e9, 1e9, 6);
    result.gain->setValue(1);
    result.gain->setObjectName(id + "Gain");
    result.gain->setToolTip("通道值 × 倍率 + 偏移");
    result.offset = numeric(this, -1e9, 1e9, 6);
    result.offset->setObjectName(id + "Offset");
    result.offset->setToolTip("通道值 × 倍率 + 偏移；留空的通道可用偏移设置固定值");
    layout->setColumnStretch(1, 1);
    layout->addWidget(new QLabel(title), row * 3, 0);
    layout->addWidget(result.field, row * 3, 1);
    layout->addWidget(new QLabel("倍率"), row * 3 + 1, 0);
    layout->addWidget(result.gain, row * 3 + 1, 1);
    layout->addWidget(new QLabel("偏移"), row * 3 + 2, 0);
    layout->addWidget(result.offset, row * 3 + 2, 1);
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
    side->setMinimumWidth(380);
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
    stream = new Combo;
    stream->setObjectName("model3dStream");
    stream->setEditable(true);
    stream->setInsertPolicy(QComboBox::NoInsert);
    stream->lineEdit()->setMaxLength(32);
    stream->lineEdit()->setPlaceholderText("未命名 @wave · 或输入流名，如 imu");
    stream->addItem("");
    stream->setToolTip("只接收所选连接、对端和流名的数据。留空对应 @wave；imu 对应 @wave[imu]");
    auto streamForm = new QFormLayout;
    streamForm->addRow("数据流", stream);
    sideBody->addLayout(streamForm);
    auto tabs = new ModelTabs;
    mappingTabs = tabs;
    tabs->setObjectName("model3dMappingTabs");
    tabs->setDocumentMode(true);
    QVBoxLayout *positionBody;
    auto positionPage = formPage(positionBody);
    positionBody->addWidget(note("手动平移 · 世界坐标\n始终生效，并叠加到通道位置上；无需等待姿态数据。"));
    auto manualForm = new QFormLayout;
    for (int i = 0; i < 3; ++i)
    {
        manualPosition[i] = numeric(this, -1e9, 1e9, 6);
        manualPosition[i]->setObjectName(QString("modelManualPosition%1").arg(i));
        manualForm->addRow(QString("XYZ")[i], manualPosition[i]);
        connect(manualPosition[i], &QDoubleSpinBox::valueChanged, this, &ModelPanel::scheduleSave);
    }
    positionBody->addLayout(manualForm);
    positionBody->addWidget(note("通道位置 = 数值 × 倍率 + 偏移。字段留空时使用偏移常量，缺失字段时保留上次位置。"));
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
    auto orderRow = new QFormLayout;
    order = new Combo;
    order->setObjectName("model3dEulerOrder");
    for (const auto &value : QStringList {"ZYX", "XYZ", "XZY", "YXZ", "YZX", "ZXY"}) { order->addItem(value); }
    convention = new Combo;
    convention->addItem("内禀 / 随动轴", true);
    convention->addItem("外禀 / 固定轴", false);
    convention->setObjectName("model3dConvention");
    convention->setMinimumWidth(160);
    // Preserve old configurations and MCP semantics without exposing confusing selectors.
    delete orderRow;
    order->setParent(this);
    convention->setParent(this);
    order->hide();
    convention->hide();
    rotationConvention = note("姿态约定：ZYX 内禀；字段名来自所选数据流。");
    eulerBody->addWidget(rotationConvention);
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
    eulerBody->addWidget(note("绕 X / Y / Z 分别选择设备发送的字段名。正向遵循右手定则；报文字段排列顺序不影响绑定。"));
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
    axesBody->addStretch();
    tabs->addTab(axesPage, "坐标轴");
    QVBoxLayout *modelBody;
    auto modelPage = formPage(modelBody);
    followTheme = new QCheckBox("模型颜色跟随主题");
    followTheme->setObjectName("model3dFollowTheme");
    followTheme->setChecked(true);
    modelBody->addWidget(followTheme);
    auto colorRow = new QHBoxLayout;
    colorHex = new QLineEdit(customColor.name());
    colorHex->setObjectName("model3dColor");
    colorHex->setPlaceholderText("#RRGGBB");
    colorHex->setMaxLength(7);
    colorHex->setEnabled(false);
    auto chooseColor = new QPushButton("选色");
    chooseColor->setObjectName("model3dChooseColor");
    colorRow->addWidget(colorHex, 1);
    colorRow->addWidget(chooseColor);
    modelBody->addLayout(colorRow);
    modelBody->addWidget(note("自定义颜色支持 #RGB / #RRGGBB，显示时保留立体光照。"));
    modelBody->addWidget(note("模型初始对齐（°）只影响模型自身，不改变遥测坐标。"));
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
    modelBody->addLayout(modelForm);
    modelBody->addStretch();
    tabs->addTab(modelPage, "模型");
    QVBoxLayout *cameraBody;
    auto cameraPage = formPage(cameraBody);
    cameraBody->addWidget(note("相机只改变观察视角，不改变模型姿态或串口绑定。拖动、滚轮和适配后数值实时更新。"));
    auto cameraForm = new QFormLayout;
    const QStringList cameraLabels {"方位角 °", "仰角 °", "观察距离", "观察中心 X", "观察中心 Y", "观察中心 Z"};
    for (int i = 0; i < 6; ++i)
    {
        cameraFields[i] = numeric(this, i == 2 ? .001 : i < 2 ? (i == 0 ? -360 : -89.9) : -1e12,
            i < 2 ? (i == 0 ? 360 : 89.9) : 1e12, 6);
        cameraFields[i]->setObjectName(QString("modelCamera%1").arg(i));
        cameraForm->addRow(cameraLabels[i], cameraFields[i]);
        connect(cameraFields[i], &QDoubleSpinBox::valueChanged, this, [this]
        {
            if (restoring) { return; }
            view->restoreCamera({{"yaw", cameraFields[0]->value()}, {"elevation", cameraFields[1]->value()},
                {"distance", cameraFields[2]->value()}, {"target", QJsonArray {cameraFields[3]->value(),
                    cameraFields[4]->value(), cameraFields[5]->value()}}});
            saveTimer.start();
        });
    }
    cameraBody->addLayout(cameraForm);
    antialiasing = new Combo;
    antialiasing->setObjectName("modelAntialiasing");
    for (int samples : {0, 2, 4, 8})
    {
        antialiasing->addItem(samples ? QString("%1× MSAA").arg(samples) : "关闭", samples);
    }
    antialiasing->setCurrentIndex(2);
    cameraForm->addRow("抗锯齿", antialiasing);
    cameraBody->addWidget(note("抗锯齿质量受显卡支持限制；MCP 模型状态报告请求值与实际生效值。"));
    auto qualityStatus = note("实际抗锯齿：等待渲染");
    qualityStatus->setObjectName("modelAntialiasingStatus");
    cameraBody->addWidget(qualityStatus);
    connect(view, &ModelView::renderQualityChanged, qualityStatus, [qualityStatus](int samples)
    {
        qualityStatus->setText(samples ? QString("实际抗锯齿：%1× MSAA").arg(samples) : "实际抗锯齿：关闭 / 显卡不支持");
    }, Qt::QueuedConnection);
    connect(antialiasing, &QComboBox::currentIndexChanged, this, [this]
    {
        view->setAntialiasing(antialiasing->currentData().toInt());
        if (!restoring) { saveTimer.start(); }
    });
    cameraBody->addStretch();
    tabs->addTab(cameraPage, "视角");
    auto configurationRow = new QHBoxLayout;
    auto exportConfig = new QPushButton("导出配置");
    exportConfig->setObjectName("exportModelConfiguration");
    auto importConfig = new QPushButton("导入配置");
    importConfig->setObjectName("importModelConfiguration");
    configurationRow->addWidget(exportConfig);
    configurationRow->addWidget(importConfig);
    sideBody->addLayout(configurationRow);
    connect(exportConfig, &QPushButton::clicked, this, [this]
    {
        const auto path = QFileDialog::getSaveFileName(this, "导出模型配置", "model3d.json", "JSON (*.json)");
        if (path.isEmpty()) { return; }
        QSaveFile file(path);
        const auto bytes = QJsonDocument(exportConfiguration()).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        {
            QMessageBox::warning(this, "导出失败", file.errorString());
        }
    });
    connect(importConfig, &QPushButton::clicked, this, [this]
    {
        const auto path = QFileDialog::getOpenFileName(this, "导入模型配置", {}, "JSON (*.json)");
        if (path.isEmpty()) { return; }
        try
        {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) { fail("配置不可读或超过 1 MiB"); }
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) { fail("配置必须为有效 JSON 对象"); }
            importConfiguration(document.object());
        }
        catch (const std::exception &error) { QMessageBox::warning(this, "导入失败", QString::fromUtf8(error.what())); }
    });
    sideBody->addWidget(tabs, 1);
    sideBody->addWidget(note("分流示例：@wave[imu] roll=10,pitch=20,yaw=30\n选择 imu，只驱动此模型。旧 @wave 对应空流名；报文以换行结束。"));
    auto sideScroll = scrollPanel(side);
    sideScroll->setMinimumWidth(400);
    split->addWidget(sideScroll);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);
    split->setSizes({720, 420});
    body->addWidget(split, 1);
    status = new QLabel("等待通道数据 · 可先导入模型和设置绑定");
    status->setWordWrap(true);
    status->setObjectName("model3dStatus");
    body->addWidget(status);
    saveTimer.setInterval(250);
    saveTimer.setSingleShot(true);
    connect(&saveTimer, &QTimer::timeout, this, &ModelPanel::changed);
    cameraFieldsTimer.setInterval(33);
    cameraFieldsTimer.setSingleShot(true);
    connect(&cameraFieldsTimer, &QTimer::timeout, this, &ModelPanel::syncCameraFields);
    connect(view, &ModelView::cameraChanged, this, [this]
    {
        // Keep high-rate mouse input out of six spin-box layouts. During a
        // drag the fields stay live at 30 Hz; release and API changes flush now.
        if (view->isNavigating() && !restoring)
        {
            if (!cameraFieldsTimer.isActive()) { cameraFieldsTimer.start(); }
        }
        else
        {
            cameraFieldsTimer.stop();
            syncCameraFields();
        }
        if (!restoring) { saveTimer.start(); }
    });
    syncCameraFields();
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
    connect(followTheme, &QCheckBox::toggled, this, [this]
    {
        colorHex->setEnabled(!followTheme->isChecked());
        scheduleSave();
    });
    connect(colorHex, &QLineEdit::editingFinished, this, [this]
    {
        const auto text = colorHex->text().trimmed();
        static const QRegularExpression pattern("^#(?:[0-9a-fA-F]{3}|[0-9a-fA-F]{6})$");
        if (!pattern.match(text).hasMatch())
        {
            colorHex->setText(customColor.name());
            status->setText("颜色需为 #RGB 或 #RRGGBB，已保留原色");
            return;
        }
        customColor = QColor(text);
        colorHex->setText(customColor.name());
        scheduleSave();
    });
    connect(chooseColor, &QPushButton::clicked, this, [this]
    {
        const auto selected = QColorDialog::getColor(view->modelColor(), this, "模型颜色");
        if (!selected.isValid()) { return; }
        customColor = selected;
        colorHex->setText(selected.name());
        followTheme->setChecked(false);
        scheduleSave();
    });
    connect(pause, &QCheckBox::toggled, this, [this]
    {
        if (restoring) { return; }
        scheduleSave();
        if (!pause->isChecked()) { lastSample = -1; tick(channel); }
    });
    connect(feed, &QComboBox::currentIndexChanged, this, [this]
    {
        stream->setEnabled(feed->currentData() == "wave");
        if (restoring) { return; }
        resetSource(channel);
        scheduleSave();
    });
    connect(stream, &QComboBox::currentTextChanged, this, [this]
    {
        if (restoring) { return; }
        streamKey = stream->currentText().trimmed();
        resetSource(channel);
        scheduleSave();
    });
    connect(peer, &QComboBox::activated, this, [this]
    {
        peerKey = peer->currentData().toString();
        resetSource(channel);
        scheduleSave();
    });
    connect(&loader, &QFutureWatcher<ModelLoadResult>::finished, this, [this]
    {
        importPending = false;
        import->setEnabled(true);
        const auto result = loader.result();
        loadError = result.error;
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
            const auto text = rows[i].field->currentText().trimmed();
            bindings[i] = {text == unboundLabel ? QString() : text, rows[i].gain->value(), rows[i].offset->value()};
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
    view->setModelColor(followTheme->isChecked() ? QColor() : customColor);
}

void ModelPanel::applyMapping(const QStringList &incomingErrors)
{
    const auto [positionMapping, rotationMapping] = separateMapping(mapping());
    const auto positionPose = evaluateModelPose(positionMapping, positionValues);
    const auto rotationPose = evaluateModelPose(rotationMapping, rotationValues);
    if (positionPose.valid()) { telemetryPosition = positionPose.position; }
    if (rotationPose.valid()) { telemetryRotation = rotationPose.matrix; }

    ModelPose pose;
    pose.position = telemetryPosition + QVector3D(float(manualPosition[0]->value()),
        float(manualPosition[1]->value()), float(manualPosition[2]->value()));
    pose.matrix.translate(pose.position);
    pose.matrix *= telemetryRotation;
    view->setPose(pose);

    QStringList errors = incomingErrors;
    if (!positionPose.valid()) { errors.append("位置：" + positionPose.error); }
    if (!rotationPose.valid()) { errors.append("姿态：" + rotationPose.error); }
    errors.removeDuplicates();
    mappingErrors = errors;
    const auto source = (peerKey == "default" ? "本连接" : peerKey)
        + (feed->currentData() == "wave" ? (streamKey.isEmpty() ? " / 未命名 @wave" : " / " + streamKey) : " / 脚本");
    auto message = QString("%1 · X %2   Y %3   Z %4 · %5")
        .arg(pause->isChecked() ? "显示已暂停" : "姿态已更新")
        .arg(pose.position.x(), 0, 'g', 6).arg(pose.position.y(), 0, 'g', 6).arg(pose.position.z(), 0, 'g', 6)
        .arg(source);
    if (!errors.isEmpty()) { message += "\n" + errors.join("；") + " · 保留对应的上一有效值，手动平移仍生效"; }
    status->setText(message);
}

void ModelPanel::refreshFieldNames(const QJsonObject &values)
{
    const auto current = modelNumericFields(values);
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
            row.field->addItem(unboundLabel);
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
    positionSample = -1;
    rotationSample = -1;
    floorSample = -1;
    latestValues = {};
    positionValues = {};
    rotationValues = {};
    telemetryPosition = {};
    telemetryRotation.setToIdentity();
    fields.clear();
    applyMapping();
    status->setText("等待所选来源的完整数据帧");
    {
        const QSignalBlocker blocker(stream);
        stream->clear();
        stream->addItem("");
        if (!streamKey.isEmpty()) { stream->addItem(streamKey); }
        stream->setEditText(streamKey);
    }
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
    if (!channel->sources.contains(peerKey)) { return; }
    const auto source = channel->sources.value(peerKey);
    const bool script = feed->currentData() == "script";
    if (!script)
    {
        auto names = source->waveStreams.values();
        if (!names.contains("")) { names.append(""); }
        if (!names.contains(streamKey)) { names.append(streamKey); }
        names.sort();
        QStringList currentNames;
        for (int i = 0; i < stream->count(); ++i) { currentNames.append(stream->itemText(i)); }
        if (names != currentNames)
        {
            const QSignalBlocker streamBlocker(stream);
            stream->clear();
            stream->addItems(names);
            stream->setEditText(streamKey);
        }
    }
    if (pause->isChecked()) { return; }
    const auto &records = script ? source->scriptRecords : source->samples;
    if (records.empty()) { return; }
    const auto indexKey = script ? "seq" : "index";
    const auto valuesKey = script ? "value" : "values";
    const qint64 sequence = records.back()[indexKey].toInteger();
    if (sequence == lastSample || sequence <= floorSample) { return; }

    // Select the newest complete position and rotation separately, but never
    // assemble rotation components across frames or across named streams.
    const auto [positionMapping, rotationMapping] = separateMapping(mapping());
    const auto after = qMax(lastSample, floorSample);
    bool found = false;
    bool foundPosition = false;
    bool foundRotation = false;
    QString positionError;
    QString rotationError;
    for (auto record = records.rbegin(); record != records.rend(); ++record)
    {
        if ((*record)[indexKey].toInteger() <= after) { break; }
        if (!script && (*record)["stream"].toString() != streamKey) { continue; }
        const auto values = (*record)[valuesKey].toObject();
        if (!found)
        {
            latestValues = values;
            found = true;
        }
        if (!foundPosition)
        {
            const auto pose = evaluateModelPose(positionMapping, values);
            if (pose.valid())
            {
                positionValues = values;
                positionSample = (*record)[indexKey].toInteger();
                foundPosition = true;
                positionError.clear();
            }
            else if (positionError.isEmpty()) { positionError = pose.error; }
        }
        if (!foundRotation)
        {
            const auto pose = evaluateModelPose(rotationMapping, values);
            if (pose.valid())
            {
                rotationValues = values;
                rotationSample = (*record)[indexKey].toInteger();
                foundRotation = true;
                rotationError.clear();
            }
            else if (rotationError.isEmpty()) { rotationError = pose.error; }
        }
        if (foundPosition && foundRotation) { break; }
    }
    lastSample = sequence;
    if (found)
    {
        refreshFieldNames(latestValues);
        if (foundPosition) { refreshFieldNames(positionValues); }
        if (foundRotation) { refreshFieldNames(rotationValues); }
        QStringList errors;
        if (!positionError.isEmpty()) { errors.append("位置：" + positionError); }
        if (!rotationError.isEmpty()) { errors.append("姿态：" + rotationError); }
        applyMapping(errors);
    }
}

void ModelPanel::clearDisplay()
{
    positionSample = -1;
    rotationSample = -1;
    floorSample = lastSample;
    if (channel && channel->sources.contains(peerKey))
    {
        const auto source = channel->sources.value(peerKey);
        floorSample = feed->currentData() == "script" ? source->scriptCount : source->count;
    }
    latestValues = {};
    positionValues = {};
    rotationValues = {};
    telemetryPosition = {};
    telemetryRotation.setToIdentity();
    applyMapping();
    status->setText("已复位显示，等待新数据；共享历史未清除");
}

void ModelPanel::loadFile(const QString &path)
{
    if (loading()) { return; }
    importPending = true;
    pendingPath = QFileInfo(path).absoluteFilePath();
    loadError.clear();
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
    mappingTabs->setTheme(theme);
}

QJsonObject ModelPanel::settings() const
{
    return {{"file", modelPath}, {"mapping", mapping().settings()}, {"feed", feed->currentData().toString()},
        {"peer", peerKey}, {"stream", streamKey}, {"paused", pause->isChecked()},
        {"scale", scale->value()}, {"centered", centered->isChecked()},
        {"manual_position", QJsonArray {manualPosition[0]->value(), manualPosition[1]->value(), manualPosition[2]->value()}},
        {"follow_theme", followTheme->isChecked()}, {"color", customColor.name()},
        {"alignment", QJsonArray {alignment[0]->value(), alignment[1]->value(), alignment[2]->value()}},
        {"camera", view->cameraSettings()}, {"antialiasing", view->antialiasing()}};
}

void ModelPanel::restore(const QJsonObject &settings, bool reload)
{
    const bool sourceChanged = feed->currentData().toString() != settings["feed"].toString("wave")
        || peerKey != settings["peer"].toString("default") || streamKey != settings["stream"].toString();
    restoring = true;
    const auto config = ModelMapping::restore(settings["mapping"].toObject());
    const bool mappingChanged = mapping().settings() != config.settings();
    const auto write = [](auto &rows, const auto &bindings)
    {
        for (int i = 0; i < int(rows.size()); ++i)
        {
            rows[i].field->setEditText(bindings[i].field.isEmpty() ? unboundLabel : bindings[i].field);
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
    rotationConvention->setText(QString("姿态约定：%1 %2；字段名来自所选数据流。")
        .arg(config.order, config.intrinsic ? "内禀" : "外禀"));
    units->setCurrentIndex(config.radians ? 1 : 0);
    rotationPages->setCurrentIndex(mode->currentIndex());
    feed->setCurrentIndex(settings["feed"] == "script" ? 1 : 0);
    streamKey = settings["stream"].toString();
    peerKey = settings["peer"].toString("default");
    if (peer->findData(peerKey) < 0) { peer->addItem(peerKey, peerKey); }
    peer->setCurrentIndex(peer->findData(peerKey));
    for (int i = 0; i < 3; ++i)
    {
        axes[i]->setCurrentIndex(axes[i]->findData(config.axes[i]));
        const auto initial = settings["alignment"].toArray();
        alignment[i]->setValue(i < initial.size() ? initial[i].toDouble() : 0);
        const auto manual = settings["manual_position"].toArray();
        manualPosition[i]->setValue(i < manual.size() ? manual[i].toDouble() : 0);
    }
    scale->setValue(settings["scale"].toDouble(1));
    centered->setChecked(settings["centered"].toBool(true));
    const QColor savedColor(settings["color"].toString());
    if (savedColor.isValid()) { customColor = savedColor; }
    colorHex->setText(customColor.name());
    followTheme->setChecked(settings["follow_theme"].toBool(true));
    colorHex->setEnabled(!followTheme->isChecked());
    pause->setChecked(settings["paused"].toBool());
    if (reload)
    {
        modelPath = settings["file"].toString();
        pendingCamera = settings["camera"].toObject();
    }
    applyModelTransform();
    if (reload || sourceChanged) { resetSource(channel); }
    else { applyMapping(mappingChanged ? QStringList() : mappingErrors); }
    stream->setEditText(streamKey);
    stream->setEnabled(feed->currentData() == "wave");
    view->restoreCamera(settings["camera"].toObject());
    antialiasing->setCurrentIndex(antialiasing->findData(settings["antialiasing"].toInt(4)));
    if (reload && !modelPath.isEmpty()) { loadFile(modelPath); }
    restoring = false;
    saveTimer.stop();
}

void ModelPanel::syncCameraFields()
{
    const auto camera = view->cameraSettings();
    const auto target = camera["target"].toArray();
    for (int i = 0; i < 6; ++i)
    {
        const QSignalBlocker blocker(cameraFields[i]);
        cameraFields[i]->setValue(i < 3 ? camera[QStringList {"yaw", "elevation", "distance"}[i]].toDouble()
            : target[i - 3].toDouble());
    }
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
