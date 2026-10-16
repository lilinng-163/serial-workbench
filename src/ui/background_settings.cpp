// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "window_chrome.h"

namespace sw
{
namespace
{
void syncControls(Window *host, bool receive)
{
    if (!host->settingsDialog) { return; }
    const QString prefix = receive ? "receiveBackground" : "windowBackground";
    const auto &spec = receive ? host->receiveBackground : host->windowBackground;
    const auto &image = receive ? host->receiveBackgroundImage : host->windowBackgroundImage;
    if (auto label = host->settingsDialog->findChild<QLabel *>(prefix + "File"))
    {
        label->setText(spec.file.isEmpty() ? "未设置图片" : spec.name + (image.isNull() ? "（图片不可用）" : ""));
        label->setToolTip(QDir(QFileInfo(host->settingsPath).absolutePath()).filePath(spec.file));
    }
    if (auto mode = host->settingsDialog->findChild<QComboBox *>(prefix + "Mode"))
    {
        const QSignalBlocker block(mode);
        mode->setCurrentIndex(qMax(0, mode->findData(spec.fit)));
    }
    if (auto opacity = host->settingsDialog->findChild<QSlider *>(prefix + "Opacity"))
    {
        const QSignalBlocker block(opacity);
        opacity->setValue(spec.opacity);
    }
    if (auto value = host->settingsDialog->findChild<QLabel *>(prefix + "Percent"))
    {
        value->setText(QString::number(spec.opacity) + "%");
    }
    for (bool horizontal : {true, false})
    {
        const QString suffix = horizontal ? "PositionX" : "PositionY";
        const int position = horizontal ? spec.positionX : spec.positionY;
        if (auto slider = host->settingsDialog->findChild<QSlider *>(prefix + suffix))
        {
            const QSignalBlocker block(slider);
            slider->setValue(position);
        }
        if (auto value = host->settingsDialog->findChild<QLabel *>(prefix + suffix + "Value"))
        {
            value->setText(QString::number(position) + "%");
        }
    }
}
}

QString Window::workspaceStyle() const
{
    QString style = theme.stylesheet();
    if (windowBackgroundImage.isNull()) { return style; }
    style += R"(
        QWidget#root QFrame#sidebar, QWidget#root QWidget#workspace,
        QWidget#root QWidget#page, QWidget#root QWidget#tabHeader,
        QWidget#root QWidget#monitorToolbar, QWidget#root QWidget#sender,
        QWidget#root QStackedWidget { background: transparent; }
        QWidget#root QFrame#rail, QWidget#root QWidget#pixelBrand { background: %1; }
    )";
    style = style.arg(QString("rgba(%1,%2,%3,200)").arg(theme.rail.red()).arg(theme.rail.green()).arg(theme.rail.blue()));
    return style;
}

bool Window::importBackground(bool receive, const QString &path, QString *error)
{
    auto root = rootWindow();
    const QString relative = BackgroundImage::importFile(path, root->settingsPath, error);
    if (relative.isEmpty()) { return false; }
    auto &spec = receive ? root->receiveBackground : root->windowBackground;
    auto &image = receive ? root->receiveBackgroundImage : root->windowBackgroundImage;
    const auto loaded = BackgroundImage::read(QDir(QFileInfo(root->settingsPath).absolutePath()).filePath(relative), error);
    if (loaded.isNull()) { return false; }
    spec.file = relative;
    spec.name = QFileInfo(path).fileName();
    image = loaded;
    root->applyBackgrounds();
    root->saveSettings();
    return true;
}

void Window::applyBackgrounds()
{
    if (primary) { rootWindow()->applyBackgrounds(); return; }
    updateWindowChrome(theme, windowBackground, windowBackgroundImage);
    for (auto host : hosts)
    {
        if (!host) { continue; }
        host->windowBackground = windowBackground;
        host->receiveBackground = receiveBackground;
        host->windowBackgroundImage = windowBackgroundImage;
        host->receiveBackgroundImage = receiveBackgroundImage;
        const auto style = host->workspaceStyle();
        if (host->styleSheet() != style) { host->setStyleSheet(style); }
        if (host->backgroundSurface)
        {
            host->backgroundSurface->base = host->theme.background;
            host->backgroundSurface->background.set(windowBackground, windowBackgroundImage);
            host->backgroundSurface->update();
        }
        for (auto pane : host->panes)
        {
            pane->applyAppearance();
            if (pane->log) { pane->log->viewport()->update(); }
        }
        syncControls(host, false);
        syncControls(host, true);
    }
}

QWidget *Window::backgroundSettingsPanel()
{
    auto panel = new QWidget();
    auto body = new QVBoxLayout(panel);
    body->setContentsMargins(0, 12, 0, 0);
    body->setSpacing(16);
    for (bool receive : {false, true})
    {
        const QString prefix = receive ? "receiveBackground" : "windowBackground";
        const auto &spec = receive ? rootWindow()->receiveBackground : rootWindow()->windowBackground;
        auto heading = new QLabel(receive ? "收发显示区背景" : "主界面背景（含左侧参数栏）");
        heading->setObjectName("section");
        body->addWidget(heading);
        auto row = new QHBoxLayout();
        auto choose = new QPushButton("选择图片…");
        choose->setObjectName(prefix + "Choose");
        choose->setProperty("variant", "outline");
        auto reset = new QPushButton("恢复默认");
        reset->setObjectName(prefix + "Reset");
        auto center = new QPushButton("图片居中");
        center->setObjectName(prefix + "PositionReset");
        row->addWidget(choose);
        row->addWidget(center);
        row->addWidget(reset);
        row->addStretch();
        body->addLayout(row);
        auto file = new QLabel(spec.file.isEmpty() ? "未设置图片" : spec.name);
        file->setObjectName(prefix + "File");
        file->setWordWrap(true);
        body->addWidget(file);
        auto form = new QFormLayout();
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        auto mode = new Combo();
        mode->setObjectName(prefix + "Mode");
        mode->addItem("填满（保持比例裁剪）", "cover");
        mode->addItem("完整显示（保持比例）", "contain");
        mode->addItem("平铺", "tile");
        mode->setCurrentIndex(qMax(0, mode->findData(spec.fit)));
        form->addRow("显示方式", mode);
        auto opacityRow = new QHBoxLayout();
        auto opacity = new QSlider(Qt::Horizontal);
        opacity->setObjectName(prefix + "Opacity");
        opacity->setRange(0, 100);
        opacity->setValue(spec.opacity);
        opacity->setAccessibleName(receive ? "收发区背景不透明度" : "主界面背景不透明度");
        auto percent = new QLabel(QString::number(spec.opacity) + "%");
        percent->setObjectName(prefix + "Percent");
        percent->setMinimumWidth(44);
        opacityRow->addWidget(opacity, 1);
        opacityRow->addWidget(percent);
        form->addRow("不透明度", opacityRow);
        const auto positionControl = [&](bool horizontal)
        {
            const QString suffix = horizontal ? "PositionX" : "PositionY";
            const int position = horizontal ? spec.positionX : spec.positionY;
            auto row = new QHBoxLayout();
            auto slider = new QSlider(Qt::Horizontal);
            slider->setObjectName(prefix + suffix);
            slider->setRange(0, 100);
            slider->setValue(position);
            slider->setAccessibleName((receive ? QString("收发区背景") : QString("主界面背景"))
                + (horizontal ? "水平位置" : "垂直位置"));
            slider->setToolTip(horizontal ? "0% 靠左，50% 居中，100% 靠右" : "0% 靠上，50% 居中，100% 靠下");
            auto value = new QLabel(QString::number(position) + "%");
            value->setObjectName(prefix + suffix + "Value");
            value->setMinimumWidth(44);
            row->addWidget(slider, 1);
            row->addWidget(value);
            form->addRow(horizontal ? "水平位置" : "垂直位置", row);
            return slider;
        };
        auto positionX = positionControl(true);
        auto positionY = positionControl(false);
        body->addLayout(form);
        connect(choose, &QPushButton::clicked, this, [this, receive]
        {
            const auto path = QFileDialog::getOpenFileName(settingsDialog, "选择背景图片", {},
                "常见图片 (*.jpg *.jpeg *.png *.bmp *.webp *.gif *.tif *.tiff);;全部文件 (*)");
            if (path.isEmpty()) { return; }
            QString message;
            if (!importBackground(receive, path, &message))
            {
                QMessageBox::warning(settingsDialog, "无法设置背景", message);
            }
        });
        connect(reset, &QPushButton::clicked, this, [this, receive]
        {
            auto root = rootWindow();
            (receive ? root->receiveBackground : root->windowBackground) = {};
            (receive ? root->receiveBackgroundImage : root->windowBackgroundImage) = {};
            root->applyBackgrounds();
            root->saveSettings();
        });
        connect(center, &QPushButton::clicked, this, [this, receive]
        {
            auto root = rootWindow();
            auto &setting = receive ? root->receiveBackground : root->windowBackground;
            setting.positionX = 50;
            setting.positionY = 50;
            root->applyBackgrounds();
            root->saveSettings();
        });
        const auto changed = [this, receive, mode, opacity, positionX, positionY]
        {
            auto root = rootWindow();
            auto &setting = receive ? root->receiveBackground : root->windowBackground;
            setting.fit = mode->currentData().toString();
            setting.opacity = opacity->value();
            setting.positionX = positionX->value();
            setting.positionY = positionY->value();
            root->applyBackgrounds();
            root->saveSettings();
        };
        connect(mode, &QComboBox::currentIndexChanged, this, changed);
        connect(opacity, &QSlider::valueChanged, this, changed);
        connect(positionX, &QSlider::valueChanged, this, changed);
        connect(positionY, &QSlider::valueChanged, this, changed);
    }
    auto hint = new QLabel("支持 JPG、PNG、BMP、WebP、GIF、TIFF；GIF 使用首帧。图片会保存到配置目录，降低不透明度可让文字更清晰。\n"
        "位置 0% 靠左或靠上，50% 居中，100% 靠右或靠下。填满时调整裁剪位置，完整显示时调整留白位置，平铺时移动平铺起点；与区域刚好贴合的方向无需移动。");
    hint->setWordWrap(true);
    body->addWidget(hint);
    return panel;
}
}
