// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "window_chrome.h"
#include "release_history_panel.h"
#include <QtTest>
#include <QNetworkReply>
#include <cstring>

namespace
{
class FixtureReply : public QNetworkReply
{
public:
    FixtureReply(const QNetworkRequest &request, QByteArray bytes, int status, QObject *parent)
        : QNetworkReply(parent), bytes(std::move(bytes))
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        open(QIODevice::ReadOnly);
        QTimer::singleShot(20, this, [this]
        {
            if (finished) { return; }
            emit readyRead();
            if (finished) { return; }
            finished = true;
            setFinished(true);
            emit QNetworkReply::finished();
        });
    }

    void abort() override
    {
        if (finished) { return; }
        finished = true;
        setError(QNetworkReply::OperationCanceledError, "Cancelled");
        setFinished(true);
        emit QNetworkReply::finished();
    }

    qint64 bytesAvailable() const override
    {
        return bytes.size() - offset + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char *target, qint64 maximum) override
    {
        const qint64 count = qMin(maximum, qint64(bytes.size()) - offset);
        if (!count) { return -1; }
        std::memcpy(target, bytes.constData() + offset, size_t(count));
        offset += count;
        return count;
    }

private:
    QByteArray bytes;
    qint64 offset = 0;
    bool finished = false;
};

class FixtureNetwork : public QNetworkAccessManager
{
public:
    QByteArray response;
    int status = 200;
    QList<QUrl> requests;

protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override
    {
        requests.append(request.url());
        return new FixtureReply(request, response, status, this);
    }
};

const QByteArray InstallerBytes("offline installer fixture; never executed");

QJsonObject release(const QString &version)
{
    const auto name = "SerialWorkbench-" + version + "-windows-x64-setup.exe";
    const auto url = "https://github.com/lilinng-163/serial-workbench/releases/download/v" + version + "/" + name;
    return {{"tag_name", "v" + version}, {"name", "特性 " + version}, {"body", "- 搜索与脚本改进\n- 控件优化"},
        {"published_at", "2026-10-17T00:00:00Z"}, {"draft", false}, {"prerelease", false},
        {"assets", QJsonArray {QJsonObject {{"name", name}, {"state", "uploaded"}, {"browser_download_url", url},
            {"digest", "sha256:" + QString::fromLatin1(QCryptographicHash::hash(InstallerBytes, QCryptographicHash::Sha256).toHex())},
            {"size", InstallerBytes.size()}}}}};
}

QByteArray feed()
{
    return QJsonDocument(QJsonArray {release("99.99.991234"), release("99.98.991234"), release("99.97.991234")}).toJson();
}

void capture(QWidget *widget, const QString &name)
{
    const auto directory = qEnvironmentVariable("SW_HISTORY_CAPTURE_DIR");
    if (directory.isEmpty()) { return; }
    QDir().mkpath(directory);
    QVERIFY(widget->grab().save(directory + "/" + name + ".png"));
}
}

class ReleaseHistoryTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QApplication::setStyle("Fusion");
        QApplication::setFont(sw::Theme::interfaceFont());
        QCoreApplication::setApplicationVersion(sw::Version);
        sw::installWindowChrome();
    }

    void stableHistoryIsVerifiedSortedUniqueAndLimited()
    {
        auto draft = release("99.0.0");
        draft["draft"] = true;
        auto preview = release("98.0.0");
        preview["prerelease"] = true;
        auto unverified = release("97.0.0");
        auto assets = unverified["assets"].toArray();
        auto asset = assets[0].toObject();
        asset["digest"] = "";
        assets[0] = asset;
        unverified["assets"] = assets;
        const auto result = sw::UpdateService::parseHistory(QJsonDocument(QJsonArray {
            release("0.9.0"), release("0.10.0"), draft, preview, unverified,
            release("0.8.0"), release("0.11.0"), release("0.10.0")}).toJson());
        QCOMPARE(result.size(), 3);
        QCOMPARE(result[0].version, QString("0.11.0"));
        QCOMPARE(result[1].version, QString("0.10.0"));
        QCOMPARE(result[2].version, QString("0.9.0"));
        QVERIFY(sw::UpdateService::parseHistory("{}").isEmpty());
        QVERIFY(sw::UpdateService::parseHistory("[broken").isEmpty());
        QVERIFY(!sw::UpdateService::parseRelease(QJsonDocument(release("0.11.0")).toJson(), "0.12.0"));
        auto spoofed = release("0.11.0");
        assets = spoofed["assets"].toArray();
        asset = assets[0].toObject();
        asset["browser_download_url"] = asset["browser_download_url"].toString().replace("github.com", "github.com:444");
        assets[0] = asset;
        spoofed["assets"] = assets;
        QVERIFY(sw::UpdateService::parseHistory(QJsonDocument(QJsonArray {spoofed}).toJson()).isEmpty());
    }

    void bundledHistoryWorksOffline()
    {
        const auto releases = sw::UpdateService::bundledHistory();
        QCOMPARE(releases.size(), 3);
        for (const auto &entry : releases)
        {
            QVERIFY(!entry.notes.isEmpty());
            QVERIFY(!entry.title.isEmpty());
            QCOMPARE(entry.sha256.size(), 64);
        }
        FixtureNetwork network;
        sw::ReleaseHistoryPanel panel(nullptr, &network);
        panel.resize(760, 560);
        panel.show();
        auto versions = panel.findChild<QComboBox *>("releaseHistoryVersions");
        QCOMPARE(versions->count(), 3);
        auto notes = panel.findChild<QTextBrowser *>("releaseHistoryNotes");
        for (int i = 0; i < 3; ++i)
        {
            versions->setCurrentIndex(i);
            QVERIFY(!notes->toPlainText().isEmpty());
            QCOMPARE(notes->verticalScrollBar()->value(), 0);
            QVERIFY(panel.findChild<QPushButton *>("releaseHistoryDownload")->isEnabled());
        }
        QVERIFY(network.requests.isEmpty());
    }

    void refreshFailureKeepsHistoryAndCanRetry()
    {
        FixtureNetwork network;
        network.status = 503;
        sw::ReleaseHistoryPanel panel(nullptr, &network);
        auto service = panel.findChild<sw::UpdateService *>();
        QSignalSpy failed(service, &sw::UpdateService::historyFailed);
        QSignalSpy changed(service, &sw::UpdateService::historyChanged);
        panel.refreshHistory();
        panel.refreshHistory();
        QCOMPARE(network.requests.size(), 1);
        QVERIFY(!panel.findChild<QPushButton *>("releaseHistoryRefresh")->isEnabled());
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(service->recentReleases().size(), 3);
        QVERIFY(panel.findChild<QLabel *>("releaseHistoryStatus")->text().contains("保留"));
        network.status = 200;
        network.response = feed();
        panel.refreshHistory();
        QTRY_COMPARE(changed.size(), 1);
        QCOMPARE(network.requests.last().host(), QString("api.github.com"));
        QCOMPARE(service->recentReleases()[0].version, QString("99.99.991234"));
        QVERIFY(panel.findChild<QPushButton *>("releaseHistoryRefresh")->isEnabled());
    }

    void verifiedDownloadRequestsInstallOnlyAfterClick()
    {
        FixtureNetwork network;
        network.response = feed();
        sw::ReleaseHistoryPanel panel(nullptr, &network);
        auto service = panel.findChild<sw::UpdateService *>();
        panel.refreshHistory();
        QTRY_VERIFY(!service->isCheckingHistory());
        QSignalSpy ready(service, &sw::UpdateService::downloadReady);
        QSignalSpy install(&panel, &sw::ReleaseHistoryPanel::installRequested);
        network.response = InstallerBytes;
        auto download = panel.findChild<QPushButton *>("releaseHistoryDownload");
        download->click();
        QVERIFY(service->isDownloading());
        QVERIFY(!panel.findChild<QComboBox *>("releaseHistoryVersions")->isEnabled());
        QVERIFY(!download->isEnabled());
        panel.refreshHistory();
        QCOMPARE(network.requests.size(), 2);
        QTRY_COMPARE(ready.size(), 1);
        QCOMPARE(install.size(), 0);
        const auto path = service->downloadedInstaller();
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), InstallerBytes);
        file.close();
        QVERIFY(download->text().contains("安装"));
        download->click();
        QCOMPARE(install.size(), 1);
        QCOMPARE(install[0][0].toString(), path);
        QVERIFY(QFile::remove(path));
    }

    void corruptedOrCancelledDownloadDoesNotInstall()
    {
        FixtureNetwork network;
        network.response = feed();
        sw::ReleaseHistoryPanel panel(nullptr, &network);
        auto service = panel.findChild<sw::UpdateService *>();
        panel.refreshHistory();
        QTRY_VERIFY(!service->isCheckingHistory());
        QSignalSpy failed(service, &sw::UpdateService::downloadFailed);
        QSignalSpy install(&panel, &sw::ReleaseHistoryPanel::installRequested);
        auto download = panel.findChild<QPushButton *>("releaseHistoryDownload");
        network.response = QByteArray(InstallerBytes.size(), 'x');
        download->click();
        QTRY_COMPARE(failed.size(), 1);
        QVERIFY(service->downloadedInstaller().isEmpty());
        QVERIFY(!download->text().contains("并安装"));
        network.response = InstallerBytes;
        download->click();
        panel.findChild<QPushButton *>("releaseHistoryCancel")->click();
        QTRY_COMPARE(failed.size(), 2);
        QVERIFY(service->downloadedInstaller().isEmpty());
        QCOMPARE(install.size(), 0);
        auto forged = service->recentReleases().first();
        forged.url = QUrl("https://example.org/installer.exe");
        QVERIFY(!service->downloadRelease(forged));
        QTest::qWait(40);
        QCOMPARE(failed.size(), 2);
    }

    void liveHistoryAndVerifiedInstaller()
    {
        if (!qEnvironmentVariableIsSet("SW_HISTORY_NETWORK_PROBE")) { QSKIP("Live GitHub download is opt-in"); }
        sw::ReleaseHistoryPanel panel;
        auto service = panel.findChild<sw::UpdateService *>();
        QSignalSpy changed(service, &sw::UpdateService::historyChanged);
        QSignalSpy failed(service, &sw::UpdateService::historyFailed);
        panel.refreshHistory();
        QTRY_VERIFY_WITH_TIMEOUT(!service->isCheckingHistory(), 20000);
        QCOMPARE(failed.size(), 0);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(service->recentReleases().size(), 3);
        auto versions = panel.findChild<QComboBox *>("releaseHistoryVersions");
        versions->setCurrentIndex(2);
        QSignalSpy ready(service, &sw::UpdateService::downloadReady);
        QSignalSpy downloadFailed(service, &sw::UpdateService::downloadFailed);
        QSignalSpy install(&panel, &sw::ReleaseHistoryPanel::installRequested);
        panel.findChild<QPushButton *>("releaseHistoryDownload")->click();
        QTRY_VERIFY_WITH_TIMEOUT(!service->isDownloading(), 60000);
        QCOMPARE(downloadFailed.size(), 0);
        QCOMPARE(ready.size(), 1);
        QCOMPARE(install.size(), 0);
        QFile file(service->downloadedInstaller());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.size(), service->latestRelease()->size);
        QCOMPARE(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex(), service->latestRelease()->sha256);
    }

    void settingsAndPermanentVersionFitSmallWindows()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer server(&engine);
        sw::Window window(&engine, &server, directory.filePath("settings.json"));
        window.resize(1100, 760);
        window.show();
        auto version = window.findChild<QLabel *>("currentVersionStatus");
        QVERIFY(version);
        QCOMPARE(version->text(), "v" + sw::Version);
        window.statusBar()->showMessage("临时状态消息");
        QTRY_VERIFY(version->isVisible());
        QVERIFY(window.statusBar()->rect().contains(QRect(version->mapTo(window.statusBar(), QPoint()), version->size())));
        capture(&window, "status-version");
        window.openSettings();
        auto dialog = window.settingsDialog;
        auto pages = dialog->findChild<sw::DetachableTabs *>("settingsPages");
        auto panel = dialog->findChild<sw::ReleaseHistoryPanel *>();
        QVERIFY(panel);
        int index = -1;
        for (int i = 0; i < pages->count(); ++i)
        {
            if (pages->tabText(i) == "版本回溯") { index = i; }
        }
        QVERIFY(index >= 0);
        {
            const QSignalBlocker blocker(pages);
            pages->setCurrentIndex(index);
        }
        QTest::qWait(60);
        capture(dialog, "history-settings");
        window.applyTheme(true);
        capture(dialog, "history-settings-dark");
        window.applyTheme(false);
        if (qEnvironmentVariableIsSet("SW_HISTORY_CAPTURE_DIR"))
        {
            pages->setCurrentIndex(0);
            auto appearance = dialog->findChild<QScrollArea *>("appearanceScroll");
            auto opacity = dialog->findChild<QSlider *>("windowBackgroundOpacity");
            auto x = dialog->findChild<QSlider *>("windowBackgroundPositionX");
            auto y = dialog->findChild<QSlider *>("windowBackgroundPositionY");
            opacity->setValue(29);
            x->setValue(41);
            y->setValue(50);
            appearance->ensureWidgetVisible(y);
            QTest::qWait(40);
            capture(dialog, "theme-sliders-light");
            window.applyTheme(true);
            capture(dialog, "theme-sliders-dark");
            window.applyTheme(false);
            const QSignalBlocker blocker(pages);
            pages->setCurrentIndex(index);
        }
        dialog->resize(440, 320);
        QTRY_COMPARE(dialog->size(), QSize(440, 320));
        auto scroll = dialog->findChild<QScrollArea *>("releaseHistoryScroll");
        auto download = panel->findChild<QPushButton *>("releaseHistoryDownload");
        scroll->ensureWidgetVisible(download);
        QTRY_VERIFY(scroll->viewport()->rect().contains(download->mapTo(scroll->viewport(), download->rect().center())));
        capture(dialog, "history-settings-compact");
        dialog->close();
        window.close();
    }
};

QTEST_MAIN(ReleaseHistoryTests)
#include "release_history_tests.moc"
