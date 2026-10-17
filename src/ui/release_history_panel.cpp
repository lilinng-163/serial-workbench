// SPDX-License-Identifier: Apache-2.0
#include "release_history_panel.h"

#include <QDateTime>
#include <QDesktopServices>

namespace sw
{
ReleaseHistoryPanel::ReleaseHistoryPanel(QWidget *parent, QNetworkAccessManager *transport) : QWidget(parent)
{
    setObjectName("releaseHistoryPanel");
    service = new UpdateService(this, transport);
    auto body = new QVBoxLayout(this);
    body->setContentsMargins(20, 20, 20, 16);
    body->setSpacing(10);
    auto heading = new QLabel("版本回溯");
    heading->setStyleSheet("font-weight: 600; font-size: 15px;");
    body->addWidget(heading);
    auto hint = new QLabel("查看最近 3 个正式版本的特性，下载对应的 Windows x64 安装包。");
    hint->setWordWrap(true);
    body->addWidget(hint);
    auto tools = new QHBoxLayout();
    versions = new Combo();
    versions->menuPopup = true;
    versions->setObjectName("releaseHistoryVersions");
    tools->addWidget(versions, 1);
    refresh = new QPushButton("刷新列表");
    refresh->setObjectName("releaseHistoryRefresh");
    tools->addWidget(refresh);
    body->addLayout(tools);
    details = new QLabel();
    details->setObjectName("releaseHistoryDetails");
    details->setTextFormat(Qt::PlainText);
    details->setWordWrap(true);
    body->addWidget(details);
    notes = new QTextBrowser();
    notes->setObjectName("releaseHistoryNotes");
    notes->setMinimumHeight(80);
    notes->setOpenLinks(false);
    connect(notes, &QTextBrowser::anchorClicked, this, [](const QUrl &url)
    {
        if (url.scheme() == "https") { QDesktopServices::openUrl(url); }
    });
    body->addWidget(notes, 1);
    status = new QLabel("已载入随软件附带的发布记录，可联网刷新。");
    status->setObjectName("releaseHistoryStatus");
    status->setWordWrap(true);
    body->addWidget(status);
    progress = new QProgressBar();
    progress->setObjectName("releaseHistoryProgress");
    progress->setRange(0, 100);
    body->addWidget(progress);
    auto actions = new QHBoxLayout();
    cancel = new QPushButton("取消下载");
    cancel->setObjectName("releaseHistoryCancel");
    actions->addWidget(cancel);
    actions->addStretch();
    download = new QPushButton("下载安装包");
    download->setObjectName("releaseHistoryDownload");
    actions->addWidget(download);
    body->addLayout(actions);
    connect(versions, &QComboBox::currentIndexChanged, this, [this]
    {
        render();
        status->setText("所选版本的安装包来自 GitHub；下载期间可继续使用工作台。");
    });
    connect(refresh, &QPushButton::clicked, this, &ReleaseHistoryPanel::refreshHistory);
    connect(cancel, &QPushButton::clicked, service, &UpdateService::cancelDownload);
    connect(download, &QPushButton::clicked, this, [this]
    {
        const int index = versions->currentIndex();
        if (index < 0 || index >= service->recentReleases().size()) { return; }
        const auto candidate = service->recentReleases()[index];
        const auto selected = service->latestRelease();
        if (selected && selected->version == candidate.version && !service->downloadedInstaller().isEmpty())
        {
            emit installRequested(service->downloadedInstaller());
            return;
        }
        service->downloadRelease(candidate);
        render();
    });
    connect(service, &UpdateService::historyChanged, this, [this]
    {
        updateChoices();
        status->setText("已从 GitHub 更新最近 3 个正式版本。");
    });
    connect(service, &UpdateService::historyFailed, this, [this](const QString &message)
    {
        render();
        status->setText(message);
    });
    connect(service, &UpdateService::downloadProgress, this, [this](qint64 received, qint64 total)
    {
        render();
        status->setText(QString("正在下载 %1 / %2 MiB；可离开设置继续使用工作台。")
            .arg(received / 1048576.0, 0, 'f', 1).arg(total / 1048576.0, 0, 'f', 1));
    });
    connect(service, &UpdateService::downloadReady, this, [this]
    {
        render();
        status->setText("安装包大小及 SHA-256 校验通过，准备好后点击安装。安装器会保留本地设置。");
    });
    connect(service, &UpdateService::downloadFailed, this, [this](const QString &message)
    {
        render();
        status->setText(message);
    });
    updateChoices();
}

void ReleaseHistoryPanel::refreshHistory()
{
    if (service->isCheckingHistory() || service->isDownloading()) { return; }
    service->checkHistory();
    status->setText("正在刷新，现有发布记录仍可查看。");
    render();
}

void ReleaseHistoryPanel::updateChoices()
{
    const auto previous = versions->currentData().toString();
    const QSignalBlocker blocker(versions);
    versions->clear();
    for (const auto &release : service->recentReleases())
    {
        versions->addItem("v" + release.version, release.version);
    }
    versions->setCurrentIndex(qMax(0, versions->findData(previous)));
    render();
}

void ReleaseHistoryPanel::render()
{
    const int index = versions->currentIndex();
    const bool valid = index >= 0 && index < service->recentReleases().size();
    bool ready = false;
    if (valid)
    {
        const auto &release = service->recentReleases()[index];
        const auto date = QDateTime::fromString(release.publishedAt, Qt::ISODate).toLocalTime().toString("yyyy-MM-dd");
        details->setText(QString("%1  ·  %2  ·  %3 MiB").arg(release.title, date)
            .arg(release.size / 1048576.0, 0, 'f', 1));
        if (!notes->property("releaseNotes").isValid() || notes->property("releaseNotes").toString() != release.notes)
        {
            notes->setProperty("releaseNotes", release.notes);
            notes->document()->setMarkdown(release.notes.isEmpty() ? "此版本未提供特性说明。" : release.notes,
                QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub) | QTextDocument::MarkdownNoHTML);
            notes->moveCursor(QTextCursor::Start);
            notes->verticalScrollBar()->setValue(0);
        }
        ready = service->latestRelease() && service->latestRelease()->version == release.version
            && !service->downloadedInstaller().isEmpty();
    }
    const bool downloading = service->isDownloading();
    versions->setEnabled(!downloading);
    refresh->setEnabled(!service->isCheckingHistory() && !downloading);
    download->setEnabled(valid && !downloading && !service->isCheckingHistory());
    download->setText(ready ? "关闭工作台并安装" : downloading ? "正在下载" : "下载安装包");
    cancel->setVisible(downloading);
    progress->setVisible(downloading || ready);
    const auto &selected = service->latestRelease();
    progress->setValue(ready ? 100 : downloading && selected && selected->size > 0
        ? int(service->downloadedBytes() * 100 / selected->size) : 0);
}
}
