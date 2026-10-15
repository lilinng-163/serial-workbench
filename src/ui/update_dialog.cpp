// SPDX-License-Identifier: Apache-2.0
#include "update_dialog.h"

#include <QDateTime>
#include <QDesktopServices>

namespace sw
{
UpdateDialog::UpdateDialog(UpdateService *updater, QWidget *parent)
    : QDialog(parent), service(updater)
{
    setObjectName("updateDialog");
    setWindowTitle("Serial Workbench 更新");
    setWindowIcon(QIcon(":/brand/lightning.svg"));
    setWindowFlag(Qt::WindowMinMaxButtonsHint);
    setModal(false);
    resize(660, 520);
    setMinimumSize(440, 320);
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(14);

    heading = new QLabel();
    heading->setObjectName("updateHeading");
    QFont titleFont = font();
    titleFont.setPointSize(16);
    titleFont.setWeight(QFont::DemiBold);
    heading->setFont(titleFont);
    heading->setStyleSheet("font-size: 16pt; font-weight: 600;");
    heading->setWordWrap(true);
    details = new QLabel();
    details->setWordWrap(true);
    layout->addWidget(heading);
    layout->addWidget(details);
    notes = new QTextBrowser();
    notes->setObjectName("updateNotes");
    notes->setOpenLinks(false);
    connect(notes, &QTextBrowser::anchorClicked, this, [](const QUrl &url)
    {
        if (url.scheme() == "https") { QDesktopServices::openUrl(url); }
    });
    layout->addWidget(notes, 1);

    progress = new QProgressBar();
    progress->setObjectName("updateDialogProgress");
    progress->setRange(0, 100);
    layout->addWidget(progress);
    status = new QLabel();
    status->setWordWrap(true);
    layout->addWidget(status);
    auto buttons = new QHBoxLayout();
    background = new QPushButton("稍后");
    background->setObjectName("updateBackground");
    connect(background, &QPushButton::clicked, this, &QWidget::hide);
    cancel = new QPushButton("取消下载");
    connect(cancel, &QPushButton::clicked, service, &UpdateService::cancelDownload);
    action = new QPushButton("下载更新");
    action->setObjectName("updateAction");
    buttons->addWidget(background);
    buttons->addStretch();
    buttons->addWidget(cancel);
    buttons->addWidget(action);
    layout->addLayout(buttons);

    connect(action, &QPushButton::clicked, this, [this]
    {
        const auto path = service->downloadedInstaller();
        if (!path.isEmpty())
        {
            emit installRequested(path);
            return;
        }
        service->download();
        refresh();
    });
    connect(service, &UpdateService::updateAvailable, this, [this] { refresh(); });
    connect(service, &UpdateService::downloadProgress, this, [this](qint64 received, qint64 total)
    {
        refresh();
        status->setText(QString("正在下载 %1 / %2 MiB，可以最小化此窗口继续使用工作台。")
            .arg(received / 1048576.0, 0, 'f', 1).arg(total / 1048576.0, 0, 'f', 1));
    });
    connect(service, &UpdateService::downloadFailed, this, [this](const QString &message)
    {
        refresh();
        status->setText(message);
    });
    connect(service, &UpdateService::downloadReady, this, [this] { refresh(); });
    refresh();
}

void UpdateDialog::refresh()
{
    const auto &release = service->latestRelease();
    heading->setText(release ? QString("发现新版本 v%1").arg(release->version) : "等待更新信息");
    if (release)
    {
        const auto date = QDateTime::fromString(release->publishedAt, Qt::ISODate).toLocalTime().toString("yyyy-MM-dd");
        details->setText(QString("当前版本 v%1  ·  Windows x64  ·  %2 MiB%3")
            .arg(QCoreApplication::applicationVersion()).arg(release->size / 1048576.0, 0, 'f', 1)
            .arg(date.isEmpty() ? QString() : "  ·  " + date));
        const auto text = release->notes.isEmpty() ? QString("此版本未提供更新说明。") : release->notes;
        if (notes->property("releaseNotes").toString() != text)
        {
            notes->setProperty("releaseNotes", text);
            notes->document()->setMarkdown(text,
                QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub) | QTextDocument::MarkdownNoHTML);
        }
    }
    const bool downloading = service->isDownloading();
    const bool ready = !service->downloadedInstaller().isEmpty();
    progress->setVisible(downloading || ready);
    progress->setValue(ready ? 100 : release && release->size > 0 ? int(service->downloadedBytes() * 100 / release->size) : 0);
    cancel->setVisible(downloading);
    action->setEnabled(bool(release) && !downloading);
    action->setText(ready ? "关闭工作台并安装" : downloading ? "正在下载" : "下载更新");
    background->setText(downloading ? "后台下载" : ready ? "稍后安装" : "稍后");
    status->setText(ready ? "下载完成，安装包校验通过。可以继续使用工作台，准备好后再安装。"
        : downloading ? "下载正在后台进行，可最小化或关闭此窗口。" : "下载期间可继续使用串口、SSH 和 MCP。安装器会沿用原安装目录。");
}

void UpdateDialog::reject()
{
    hide();
}
}
