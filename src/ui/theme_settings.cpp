// SPDX-License-Identifier: Apache-2.0
#include "window.h"

namespace sw
{
namespace
{
class ThemeColorInput : public QLineEdit
{
public:
    using QLineEdit::QLineEdit;

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        {
            emit editingFinished();
            event->accept();
            return;
        }
        QLineEdit::keyPressEvent(event);
    }
};

QIcon colorSwatch(const QColor &primary, const QColor &secondary, bool split)
{
    QPixmap swatch(20, 20);
    swatch.fill(Qt::transparent);
    QPainter painter(&swatch);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath shape;
    shape.addRoundedRect(QRectF(1, 1, 18, 18), 5, 5);
    painter.setClipPath(shape);
    if (split)
    {
        painter.fillRect(QRectF(1, 1, 9, 18), primary);
        painter.fillRect(QRectF(10, 1, 9, 18), secondary);
    }
    else
    {
        painter.fillRect(QRectF(1, 1, 18, 18), primary);
    }
    painter.setClipping(false);
    painter.setPen(QPen(primary.darker(150), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(1, 1, 18, 18), 5, 5);
    painter.end();
    return QIcon(swatch);
}
}

void Window::setThemeColor(const QColor &color)
{
    auto root = rootWindow();
    const QColor selected = color.isValid() ? QColor(color.name(QColor::HexRgb)) : QColor();
    if (root->themeColor == selected && (selected.isValid() || !root->themeContrast.isValid()))
    {
        syncThemeColorControls();
        return;
    }

    root->themeColor = selected;
    if (!selected.isValid()) { root->themeContrast = QColor(); }
    root->applyTheme(root->theme.dark);
    root->saveSettings();
}

void Window::setThemeContrast(const QColor &color)
{
    auto root = rootWindow();
    const QColor selected = color.isValid() ? QColor(color.name(QColor::HexRgb)) : QColor();
    if (root->themeContrast == selected)
    {
        syncThemeColorControls();
        return;
    }

    root->themeContrast = selected;
    root->applyTheme(root->theme.dark);
    root->saveSettings();
}

void Window::syncThemeColorControls()
{
    if (!settingsDialog)
    {
        return;
    }

    if (auto choice = settingsDialog->findChild<QComboBox *>("themeChoice"))
    {
        const QSignalBlocker blocker(choice);
        choice->setCurrentIndex(theme.dark ? 1 : 0);
    }
    const QColor mainColor = themeColor.isValid() ? themeColor : theme.background;
    if (auto input = settingsDialog->findChild<QLineEdit *>("themeColorHex"))
    {
        const QSignalBlocker blocker(input);
        input->setText(mainColor.name(QColor::HexRgb).toUpper());
    }
    if (auto input = settingsDialog->findChild<QLineEdit *>("themeContrastHex"))
    {
        const QSignalBlocker blocker(input);
        input->setText(theme.accent.name(QColor::HexRgb).toUpper());
        input->setEnabled(true);
    }
    if (auto hint = settingsDialog->findChild<QLabel *>("themeColorHint"))
    {
        hint->setText("主色调整背景和面板，撞色可独立设置。按钮保留所选撞色，文字、图标和边框自动调整明暗，保持浅深主题清晰可读。");
        hint->setStyleSheet(QString("color: %1;").arg(theme.muted.name()));
    }
    if (auto choose = settingsDialog->findChild<QPushButton *>("themeColorChoose"))
    {
        choose->setIcon(colorSwatch(mainColor, theme.accent, true));
        choose->setIconSize(QSize(20, 20));
        choose->setToolTip("左侧为主色（背景/面板），右侧为撞色（按钮/链接/选中）");
    }
    if (auto choose = settingsDialog->findChild<QPushButton *>("themeContrastChoose"))
    {
        choose->setIcon(colorSwatch(theme.accent, theme.accent, false));
        choose->setIconSize(QSize(20, 20));
        choose->setToolTip("当前撞色（按钮/链接/选中）");
        choose->setEnabled(true);
    }
    if (auto apply = settingsDialog->findChild<QPushButton *>("themeContrastApply"))
    {
        apply->setEnabled(true);
    }
    if (auto reset = settingsDialog->findChild<QPushButton *>("themeContrastReset"))
    {
        reset->setEnabled(themeContrast.isValid());
    }
    if (auto label = settingsDialog->findChild<QLabel *>("themeContrastLabel"))
    {
        label->setEnabled(true);
    }
    if (auto sources = settingsDialog->findChild<QLabel *>("aboutSources"))
    {
        // Scroll-area viewports otherwise keep the system Link color.
        auto palette = sources->palette();
        palette.setColor(QPalette::Link, theme.accentText);
        palette.setColor(QPalette::LinkVisited, theme.accentText);
        sources->setPalette(palette);
    }
}

QWidget *Window::themeColorSettingsPanel()
{
    auto panel = new QWidget();
    panel->setObjectName("themeColorPanel");
    auto body = new QVBoxLayout(panel);
    body->setContentsMargins(0, 12, 0, 12);
    body->setSpacing(10);
    auto title = new QLabel("主题色");
    title->setObjectName("section");
    body->addWidget(title);

    auto primaryLabel = new QLabel("主色（背景/面板）");
    primaryLabel->setObjectName("themePrimaryLabel");
    primaryLabel->setProperty("role", "muted");
    body->addWidget(primaryLabel);
    auto row = new QHBoxLayout();
    row->setSpacing(8);
    auto choose = new QPushButton("选色盘…");
    choose->setObjectName("themeColorChoose");
    choose->setProperty("variant", "outline");
    const QColor mainColor = themeColor.isValid() ? themeColor : theme.background;
    auto input = new ThemeColorInput(mainColor.name(QColor::HexRgb).toUpper());
    input->setObjectName("themeColorHex");
    input->setAccessibleName("主题色十六进制值");
    input->setPlaceholderText("#RRGGBB");
    input->setMaxLength(32);
    input->setMinimumWidth(100);
    input->setClearButtonEnabled(true);
    auto apply = new QPushButton("应用");
    apply->setObjectName("themeColorApply");
    apply->setProperty("variant", "outline");
    apply->setAutoDefault(false);
    choose->setAutoDefault(false);
    row->addWidget(choose);
    row->addWidget(input, 1);
    row->addWidget(apply);
    body->addLayout(row);

    auto contrastLabel = new QLabel("撞色（按钮/链接/选中）");
    contrastLabel->setObjectName("themeContrastLabel");
    contrastLabel->setProperty("role", "muted");
    body->addWidget(contrastLabel);
    auto contrastRow = new QHBoxLayout();
    contrastRow->setSpacing(8);
    auto contrastChoose = new QPushButton("选色盘…");
    contrastChoose->setObjectName("themeContrastChoose");
    contrastChoose->setProperty("variant", "outline");
    contrastChoose->setAutoDefault(false);
    auto contrastInput = new ThemeColorInput(theme.accent.name(QColor::HexRgb).toUpper());
    contrastInput->setObjectName("themeContrastHex");
    contrastInput->setAccessibleName("撞色十六进制值");
    contrastInput->setPlaceholderText("#RRGGBB");
    contrastInput->setMaxLength(32);
    contrastInput->setMinimumWidth(100);
    contrastInput->setClearButtonEnabled(true);
    auto contrastApply = new QPushButton("应用");
    contrastApply->setObjectName("themeContrastApply");
    contrastApply->setProperty("variant", "outline");
    contrastApply->setAutoDefault(false);
    contrastRow->addWidget(contrastChoose);
    contrastRow->addWidget(contrastInput, 1);
    contrastRow->addWidget(contrastApply);
    body->addLayout(contrastRow);

    auto hint = new QLabel();
    hint->setObjectName("themeColorHint");
    hint->setProperty("role", "muted");
    hint->setWordWrap(true);
    body->addWidget(hint);
    auto resetRow = new QHBoxLayout();
    resetRow->setSpacing(8);
    auto reset = new QPushButton("恢复默认主题色/撞色");
    reset->setObjectName("themeColorReset");
    reset->setProperty("variant", "outline");
    reset->setAutoDefault(false);
    auto contrastReset = new QPushButton("恢复默认撞色");
    contrastReset->setObjectName("themeContrastReset");
    contrastReset->setProperty("variant", "outline");
    contrastReset->setAutoDefault(false);
    resetRow->addWidget(reset);
    resetRow->addWidget(contrastReset);
    resetRow->addStretch(1);
    body->addLayout(resetRow);

    const auto applyInput = [this, input, hint]
    {
        const auto color = Theme::parseColor(input->text());
        if (!color.isValid())
        {
            hint->setText("颜色格式无效，请输入 #RRGGBB（如 #4080E0）或 #RGB（如 #48E）。");
            hint->setStyleSheet(QString("color: %1;").arg(theme.red.name()));
            return;
        }
        setThemeColor(color);
    };
    const auto applyContrast = [this, contrastInput, hint]
    {
        const auto color = Theme::parseColor(contrastInput->text());
        if (!color.isValid())
        {
            hint->setText("撞色格式无效，请输入 #RRGGBB 或 #RGB。");
            hint->setStyleSheet(QString("color: %1;").arg(theme.red.name()));
            return;
        }
        setThemeContrast(color);
    };
    connect(input, &QLineEdit::editingFinished, panel, applyInput);
    connect(apply, &QPushButton::clicked, panel, applyInput);
    connect(contrastInput, &QLineEdit::editingFinished, panel, applyContrast);
    connect(contrastApply, &QPushButton::clicked, panel, applyContrast);
    connect(reset, &QPushButton::clicked, this, [this] { setThemeColor({}); });
    connect(contrastReset, &QPushButton::clicked, this, [this] { setThemeContrast({}); });
    connect(choose, &QPushButton::clicked, this, [this]
    {
        QTranslator translations;
        if (translations.load(":/translations/qtbase_zh_CN.qm"))
        {
            qApp->installTranslator(&translations);
        }
        QColorDialog dialog(themeColor.isValid() ? themeColor : theme.background, settingsDialog);
        dialog.setObjectName("themeColorDialog");
        dialog.setWindowTitle("选择主色");
        dialog.setOption(QColorDialog::DontUseNativeDialog);
        dialog.setStyleSheet(theme.stylesheet());
        dialog.setPalette(settingsDialog->palette());
        dialog.setFont(Theme::interfaceFont());
        dialog.setMinimumSize(560, 480);
        if (dialog.exec() == QDialog::Accepted)
        {
            setThemeColor(dialog.selectedColor());
        }
    });
    connect(contrastChoose, &QPushButton::clicked, this, [this]
    {
        QTranslator translations;
        if (translations.load(":/translations/qtbase_zh_CN.qm"))
        {
            qApp->installTranslator(&translations);
        }
        QColorDialog dialog(theme.accent, settingsDialog);
        dialog.setObjectName("themeContrastDialog");
        dialog.setWindowTitle("选择撞色");
        dialog.setOption(QColorDialog::DontUseNativeDialog);
        dialog.setStyleSheet(theme.stylesheet());
        dialog.setPalette(settingsDialog->palette());
        dialog.setFont(Theme::interfaceFont());
        dialog.setMinimumSize(560, 480);
        if (dialog.exec() == QDialog::Accepted)
        {
            setThemeContrast(dialog.selectedColor());
        }
    });
    syncThemeColorControls();
    return panel;
}
}
