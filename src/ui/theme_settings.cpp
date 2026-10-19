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
}

void Window::setThemeColor(const QColor &color)
{
    auto root = rootWindow();
    const QColor selected = color.isValid() ? QColor(color.name(QColor::HexRgb)) : QColor();
    if (root->themeColor == selected)
    {
        syncThemeColorControls();
        return;
    }

    root->themeColor = selected;
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
    if (auto input = settingsDialog->findChild<QLineEdit *>("themeColorHex"))
    {
        const QSignalBlocker blocker(input);
        input->setText(theme.accent.name(QColor::HexRgb).toUpper());
    }
    if (auto hint = settingsDialog->findChild<QLabel *>("themeColorHint"))
    {
        hint->setText("支持 #RRGGBB 或 #RGB；应用后立即保存，并同步浅色、深色及各窗口。");
        hint->setStyleSheet(QString("color: %1;").arg(theme.muted.name()));
    }
    if (auto choose = settingsDialog->findChild<QPushButton *>("themeColorChoose"))
    {
        QPixmap swatch(20, 20);
        swatch.fill(Qt::transparent);
        QPainter painter(&swatch);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(theme.edge, 1));
        painter.setBrush(theme.accent);
        painter.drawRoundedRect(QRectF(1, 1, 18, 18), 5, 5);
        painter.end();
        choose->setIcon(QIcon(swatch));
    }
    if (auto sources = settingsDialog->findChild<QLabel *>("aboutSources"))
    {
        // Scroll-area viewports otherwise keep the system Link color.
        auto palette = sources->palette();
        palette.setColor(QPalette::Link, theme.accent);
        palette.setColor(QPalette::LinkVisited, theme.accent);
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

    auto row = new QHBoxLayout();
    row->setSpacing(8);
    auto choose = new QPushButton("选色盘…");
    choose->setObjectName("themeColorChoose");
    choose->setProperty("variant", "outline");
    auto input = new ThemeColorInput(theme.accent.name(QColor::HexRgb).toUpper());
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

    auto hint = new QLabel();
    hint->setObjectName("themeColorHint");
    hint->setProperty("role", "muted");
    hint->setWordWrap(true);
    body->addWidget(hint);
    auto reset = new QPushButton("恢复默认主题色");
    reset->setObjectName("themeColorReset");
    reset->setProperty("variant", "outline");
    reset->setAutoDefault(false);
    body->addWidget(reset, 0, Qt::AlignLeft);

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
    connect(input, &QLineEdit::editingFinished, panel, applyInput);
    connect(apply, &QPushButton::clicked, panel, applyInput);
    connect(reset, &QPushButton::clicked, this, [this] { setThemeColor({}); });
    connect(choose, &QPushButton::clicked, this, [this]
    {
        QTranslator translations;
        if (translations.load(":/translations/qtbase_zh_CN.qm"))
        {
            qApp->installTranslator(&translations);
        }
        QColorDialog dialog(theme.accent, settingsDialog);
        dialog.setObjectName("themeColorDialog");
        dialog.setWindowTitle("选择主题色");
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
    syncThemeColorControls();
    return panel;
}
}
