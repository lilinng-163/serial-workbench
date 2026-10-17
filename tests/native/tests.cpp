// SPDX-License-Identifier: Apache-2.0

#include "window.h"
#include "ssh_profiles.h"
#include "agent_config.h"
#include "update_dialog.h"
#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <algorithm>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

static int pageIndex(sw::Window *window, sw::Pane *page)
{
    for (int i = 0; i < window->tabs->count(); ++i)
    {
        if (window->tabs->tabData(i).value<QObject *>() == page) { return i; }
    }
    return -1;
}

class NativeTests : public QObject
{
    Q_OBJECT
private slots:
    void updateReleaseOnlyAcceptsVerifiedNewInstaller()
    {
        const QString version = "0.15.11";
        const QString fileName = "SerialWorkbench-0.15.11-windows-x64-setup.exe";
        const QString url = "https://github.com/lilinng-163/serial-workbench/releases/download/v0.15.11/" + fileName;
        QJsonObject asset {
            {"name", fileName},
            {"state", "uploaded"},
            {"browser_download_url", url},
            {"digest", "sha256:" + QString(64, 'a')},
            {"size", 2048}
        };
        QJsonObject release {
            {"tag_name", "v0.15.11"},
            {"draft", false},
            {"prerelease", false},
            {"name", "Serial Workbench 0.15.11"},
            {"body", "## Fixes\n\nSSH wrapping fixed."},
            {"published_at", "2026-10-14T15:00:00Z"},
            {"assets", QJsonArray {asset}}
        };
        const auto parse = [&release]
        {
            return sw::UpdateService::parseRelease(QJsonDocument(release).toJson(), "0.15.10");
        };

        auto result = parse();
        QVERIFY(result.has_value());
        QCOMPARE(result->version, version);
        QCOMPARE(result->sha256, QByteArray(64, 'a'));
        QCOMPARE(result->size, qint64(2048));
        QCOMPARE(result->notes, QString("## Fixes\n\nSSH wrapping fixed."));
        QVERIFY(!sw::UpdateService::parseRelease(QJsonDocument(release).toJson(), version));

        release["prerelease"] = true;
        QVERIFY(!parse());
        release["prerelease"] = false;
        asset["digest"] = "";
        release["assets"] = QJsonArray {asset};
        QVERIFY(!parse());
        asset["digest"] = "sha256:" + QString(64, 'a');
        asset["browser_download_url"] = "https://example.com/installer.exe";
        release["assets"] = QJsonArray {asset};
        QVERIFY(!parse());
        asset["browser_download_url"] = url;
        asset["size"] = 0;
        release["assets"] = QJsonArray {asset};
        QVERIFY(!parse());
    }

    void updateMirrorOnlyAcceptsVerifiedNewInstaller()
    {
        const QString version = "0.15.13";
        const QString fileName = "SerialWorkbench-0.15.13-windows-x64-setup.exe";
        const QString url = "https://162.14.72.194/updates/" + fileName;
        QJsonObject manifest {
            {"version", version},
            {"filename", fileName},
            {"url", url},
            {"sha256", QString(64, 'b')},
            {"size", 2048}
        };
        const auto parse = [&manifest]
        {
            return sw::UpdateService::parseMirrorManifest(QJsonDocument(manifest).toJson(), "0.15.12");
        };

        auto result = parse();
        QVERIFY(result.has_value());
        QCOMPARE(result->version, version);
        QCOMPARE(result->url, QUrl(url));
        QCOMPARE(result->sha256, QByteArray(64, 'b'));
        QVERIFY(!sw::UpdateService::parseMirrorManifest(QJsonDocument(manifest).toJson(), version));

        manifest["url"] = "http://162.14.72.194/updates/" + fileName;
        QVERIFY(!parse());
        manifest["url"] = "https://example.com/updates/" + fileName;
        QVERIFY(!parse());
        manifest["url"] = "https://162.14.72.194:444/updates/" + fileName;
        QVERIFY(!parse());
        manifest["url"] = url;
        manifest["sha256"] = "invalid";
        QVERIFY(!parse());
        manifest["sha256"] = QString(64, 'b');
        manifest["filename"] = "other.exe";
        QVERIFY(!parse());
    }

    void updateMirrorLiveTlsProbe()
    {
        if (!qEnvironmentVariableIsSet("SERIAL_WORKBENCH_NETWORK_PROBE"))
        {
            QSKIP("Live HTTPS probe is opt-in");
        }

        QNetworkAccessManager network;
        auto *reply = network.get(QNetworkRequest(QUrl("https://162.14.72.194/updates/latest.json")));
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QVERIFY(finished.wait(15000));
        QCOMPARE(reply->error(), QNetworkReply::NoError);
        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
        QVERIFY(sw::UpdateService::parseMirrorManifest(reply->readAll(), "0.0.0"));
        reply->deleteLater();
    }

    void updateReminderStaysHiddenUntilNewVersion()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        QVERIFY(!window.updateButton->isVisible());
        QVERIFY(!window.updateProgress->isVisible());

        emit window.updater->updateAvailable("0.15.11");
        QVERIFY(window.updateButton->isVisible());
        QVERIFY(window.updateButton->text().contains("0.15.11"));
        QVERIFY(!window.updateProgress->isVisible());
        QTest::mouseClick(window.updateButton, Qt::LeftButton);
        QVERIFY(window.updateDialog);
        QVERIFY(window.updateDialog->isVisible());
        QVERIFY(!window.updateDialog->isModal());
        auto background = window.updateDialog->findChild<QPushButton *>("updateBackground");
        QVERIFY(background);
        QTest::mouseClick(background, Qt::LeftButton);
        QVERIFY(!window.updateDialog->isVisible());
        QVERIFY(window.isVisible());
        emit window.updater->downloadReady("unused-test-installer.exe");
        QVERIFY(window.isVisible());
        QVERIFY(window.updateButton->text().contains("已下载"));
    }

    void terminalWrappingCancelsPendingWrapOnControl()
    {
        sw::TerminalView terminal;
        terminal.columns = 20;
        terminal.rows = 6;
        terminal.reset();
        terminal.feed(QByteArray(20, 'A'));
        QVERIFY(terminal.wrapPending);
        terminal.feed("\r\nB");
        QCOMPARE(terminal.y, 1);
        QCOMPARE(terminal.x, 1);
        QCOMPARE(terminal.grid[1][0].character, QChar('B'));
        terminal.reset();
        terminal.feed(QByteArray(20, 'A'));
        terminal.feed("\x1b[20GZ");
        QCOMPARE(terminal.y, 0);
        QCOMPARE(terminal.grid[0][19].character, QChar('Z'));
        terminal.reset();
        terminal.feed("\x1b[?7l");
        terminal.feed(QByteArray(80, 'X'));
        QCOMPARE(terminal.y, 0);
        QCOMPARE(terminal.x, 19);
        terminal.reset();
        terminal.feed(QByteArray(19, 'A') + QString("中").toUtf8());
        QCOMPARE(terminal.y, 1);
        QCOMPARE(terminal.grid[1][0].character, QChar(u'中'));
    }

    void sshLivePtyResize()
    {
        const auto target = qEnvironmentVariable("SERIAL_WORKBENCH_SSH_PROBE");
        if (target.isEmpty()) { QSKIP("Live SSH probe is opt-in"); }
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.resize(1000, 600);
        window.show();
        window.protocol->setCurrentIndex(4);
        window.sshTarget->setCurrentText(target);
        window.connectionAction();
        auto channel = window.active->channel;
        auto terminal = window.active->terminal;
        QByteArray received;
        connect(channel, &sw::Channel::bytesReceived, this, [&received](const QByteArray &data) { received += data; });
        QTRY_COMPARE_WITH_TIMEOUT(channel->state, QString("connected"), 20000);
        QTRY_COMPARE(channel->terminalSize, QSize(terminal->columns, terminal->rows));
        channel->send("printf '__SIZE__ '; stty size\r");
        const auto firstSize = QString("__SIZE__ %1 %2").arg(terminal->rows).arg(terminal->columns).toLatin1();
        QTRY_VERIFY_WITH_TIMEOUT(terminal->text().contains(QString::fromLatin1(firstSize)), 20000);
        const QSize previousSize(terminal->columns, terminal->rows);
        window.resize(680, 480);
        QTRY_VERIFY(QSize(terminal->columns, terminal->rows) != previousSize);
        QTRY_COMPARE(channel->terminalSize, QSize(terminal->columns, terminal->rows));
        QTest::qWait(150);
        channel->send("printf '__RESIZE__ '; stty size\r");
        const auto nextSize = QString("__RESIZE__ %1 %2").arg(terminal->rows).arg(terminal->columns).toLatin1();
        const bool resized = QTest::qWaitFor([terminal, &nextSize] { return terminal->text().contains(QString::fromLatin1(nextSize)); }, 5000);
        if (!resized) { qDebug() << "EXPECTED" << nextSize << "SCREEN" << terminal->text(); }
        QVERIFY(resized);
        channel->send("read -e -p '__READ__' value; printf '\\n__KEYS__<%s>\\n' \"$value\"\r");
        QTRY_VERIFY_WITH_TIMEOUT(terminal->text().trimmed().endsWith("__READ__"), 10000);
        QTest::keyClicks(terminal, "middle");
        QTest::keyClick(terminal, Qt::Key_Home);
        QTest::keyClicks(terminal, "start-");
        QTest::keyClick(terminal, Qt::Key_End);
        QTest::keyClicks(terminal, "-end");
        QTest::keyClick(terminal, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(terminal->text().contains("__KEYS__<start-middle-end>"), 10000);
        channel->send(QByteArray(150, 'A'));
        const bool visible = QTest::qWaitFor([terminal] { return terminal->text().count('A') >= 150; }, 10000);
        if (!visible)
        {
            qDebug().noquote() << terminal->text();
            qDebug() << "GRID" << terminal->columns << terminal->rows << terminal->x << terminal->y;
        }
        const auto capture = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
        if (!capture.isEmpty()) { terminal->grab().save(QDir(capture).filePath("ssh-wrap.png")); }
        channel->send(QByteArray(1, char(3)));
        channel->close();
        QVERIFY(visible);
        QVERIFY(terminal->x < terminal->columns);
    }

    void sshStartupControlSequencesDoNotSignalOnline()
    {
#ifdef Q_OS_WIN
        sw::Engine engine;
        engine.sshExecutable = QCoreApplication::applicationDirPath() + "/fake_ssh.exe";
        sw::Config config;
        config.kind = "ssh";
        config.sshHost = "pty-wait";
        auto channel = engine.open(config);
        QVERIFY2(channel->error.isEmpty(), qPrintable(channel->error));
        QTRY_VERIFY(channel->rx > 0);
        QVERIFY2(channel->state != "error", qPrintable(channel->error));
        QCOMPARE(channel->state, QString("connecting"));
        QCOMPARE(channel->ssh->write("!"), qint64(1));
        QTRY_COMPARE(channel->state, QString("connected"));
        channel->close();
#endif
    }

    void updateDialogLiveBackgroundDownload()
    {
        if (!qEnvironmentVariableIsSet("SERIAL_WORKBENCH_NETWORK_PROBE")) { QSKIP("Live HTTPS download is opt-in"); }
        sw::UpdateService service;
        QSignalSpy available(&service, &sw::UpdateService::updateAvailable);
        const auto originalVersion = QCoreApplication::applicationVersion();
        QCoreApplication::setApplicationVersion("0.0.0");
        service.check();
        const bool found = available.wait(20000);
        QCoreApplication::setApplicationVersion(originalVersion.isEmpty() ? sw::Version : originalVersion);
        QVERIFY(found);
        QVERIFY(service.latestRelease());
        QVERIFY(!service.latestRelease()->notes.isEmpty());
        QWidget parent;
        parent.setStyleSheet(sw::Theme::make(false).stylesheet());
        parent.setFont(sw::Theme::interfaceFont());
        parent.show();
        sw::UpdateDialog dialog(&service, &parent);
        dialog.show();
        QVERIFY(!dialog.isModal());
        auto notes = dialog.findChild<QTextBrowser *>("updateNotes");
        QVERIFY(!notes->toPlainText().isEmpty());
        QSignalSpy ready(&service, &sw::UpdateService::downloadReady);
        QTest::mouseClick(dialog.findChild<QPushButton *>("updateAction"), Qt::LeftButton);
        QVERIFY(service.isDownloading());
        QTest::mouseClick(dialog.findChild<QPushButton *>("updateBackground"), Qt::LeftButton);
        QVERIFY(!dialog.isVisible());
        QVERIFY(parent.isVisible());
        QVERIFY(ready.wait(120000));
        QVERIFY(parent.isVisible());
        QVERIFY(!dialog.isVisible());
        QFile installer(service.downloadedInstaller());
        QVERIFY(installer.open(QIODevice::ReadOnly));
        QCOMPARE(installer.size(), service.latestRelease()->size);
        QCOMPARE(QCryptographicHash::hash(installer.readAll(), QCryptographicHash::Sha256).toHex(), service.latestRelease()->sha256);
        dialog.showNormal();
        QVERIFY(dialog.findChild<QPushButton *>("updateAction")->text().contains("安装"));
        const auto capture = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
        if (!capture.isEmpty()) { dialog.grab().save(QDir(capture).filePath("update-dialog.png")); }
    }

    void dataFontChangesPersistImmediatelyAndCompletely()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.json");
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, path);
        QFont selected("Consolas", 17, QFont::Bold, true);
        selected.setUnderline(true);
        window.setDataFont(selected);
        QFile saved(path);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const auto appearance = QJsonDocument::fromJson(saved.readAll()).object()["appearance"].toObject();
        QVERIFY(!appearance["font_spec"].toString().isEmpty());
        sw::Engine otherEngine;
        sw::McpServer otherMcp(&otherEngine);
        sw::Window restored(&otherEngine, &otherMcp, path);
        QCOMPARE(restored.dataFont.family(), selected.family());
        QCOMPARE(restored.dataFont.pointSize(), selected.pointSize());
        QCOMPARE(restored.dataFont.weight(), selected.weight());
        QCOMPARE(restored.dataFont.italic(), selected.italic());
        QCOMPARE(restored.dataFont.underline(), selected.underline());
        restored.active->terminal->ensurePolished();
        QVERIFY(restored.active->terminal->font().italic());
        QVERIFY(restored.active->terminal->font().bold());
    }

    void waveformDragMatchesScreen_data()
    {
        QTest::addColumn<int>("plotWidth");
        QTest::addColumn<qreal>("range");
        QTest::addColumn<qreal>("scale");

        QTest::newRow("compact-short-history") << 212 << 100.0 << 4.0;
        QTest::newRow("wide-short-history") << 912 << 100.0 << 4.0;
        QTest::newRow("wide-long-history") << 912 << 5000.0 << 4.0;
        QTest::newRow("wide-high-zoom") << 912 << 5000.0 << 20.0;
    }

    void waveformDragMatchesScreen()
    {
        QFETCH(int, plotWidth);
        QFETCH(qreal, range);
        QFETCH(qreal, scale);

        sw::WavePlot plot;
        plot.resize(plotWidth + 88, 360);
        plot.series["signal"] = {{0, 0}, {range, 1}};
        const auto wheel = [&plot](const QPointF &position, int delta, Qt::KeyboardModifiers modifiers)
        {
            QWheelEvent event(position, plot.mapToGlobal(position.toPoint()), QPoint(), QPoint(0, delta),
                              Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
            QApplication::sendEvent(&plot, &event);
        };
        for (int step = 0; step < int(scale); ++step) { wheel(plot.plotArea().center(), 120, Qt::ShiftModifier); }
        const auto initial = plot.horizontalRange();
        const qreal span = initial.second - initial.first;
        const auto mouse = [&plot](QEvent::Type type, const QPointF &position,
                                  Qt::MouseButton button, Qt::MouseButtons buttons)
        {
            QMouseEvent event(type, position, plot.mapToGlobal(position.toPoint()),
                              button, buttons, Qt::NoModifier);
            QApplication::sendEvent(&plot, &event);
        };
        const auto drag = [&mouse](const QPointF &start, const QPointF &end,
                                  Qt::MouseButton button = Qt::LeftButton)
        {
            mouse(QEvent::MouseButtonPress, start, button, button);
            mouse(QEvent::MouseMove, end, Qt::NoButton, button);
            mouse(QEvent::MouseButtonRelease, end, button, Qt::NoButton);
        };
        const QPointF start = plot.plotArea().center();
        const qreal width = plot.plotArea().width();
        drag(start, start + QPointF(10.25, 0));
        const qreal displacement = (initial.first - plot.horizontalRange().first) / span * width;
        QVERIFY2(qAbs(displacement - 10.25) < 0.000001,
                 "The curve must move the same number of screen pixels as the mouse.");
        QCOMPARE(plot.cursor().shape(), Qt::ArrowCursor);
        QVERIFY(!plot.followsLatest());

        const auto once = plot.horizontalRange();
        mouse(QEvent::MouseMove, start + QPointF(40, 0), Qt::NoButton, Qt::NoButton);
        drag(QPointF(20, 100), QPointF(30, 100));
        drag(start, start + QPointF(10, 0), Qt::RightButton);
        QCOMPARE(plot.horizontalRange(), once);

        mouse(QEvent::MouseButtonDblClick, start, Qt::LeftButton, Qt::LeftButton);
        QCOMPARE(plot.horizontalRange(), qMakePair(0.0, double(range)));
        QVERIFY(plot.followsLatest());
        // Full-history mode must also allow a drag; it used to be clamped to zero.
        drag(start, start + QPointF(10, 0));
        QVERIFY(plot.horizontalRange().first < 0);
        QVERIFY(!plot.followsLatest());
    }

    void waveformAxesZoomAndLivePan()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        for (int index = 0; index < 100; ++index)
        {
            channel->receive(QString("@wave temperature=%1\n").arg(20 + index * 0.1).toUtf8());
        }
        sw::WavePlot plot;
        plot.setFont(sw::Theme::defaultDataFont());
        plot.resize(1000, 600);
        plot.updateChannel(channel, 100);
        plot.show();
        const auto wheel = [&plot](QPointF position, Qt::KeyboardModifiers modifiers)
        {
            QWheelEvent event(position, plot.mapToGlobal(position.toPoint()), QPoint(), QPoint(0, 120),
                              Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
            QApplication::sendEvent(&plot, &event);
        };
        const auto xBefore = plot.horizontalRange();
        const auto yBefore = plot.verticalRange();
        const auto area = plot.plotArea();
        const QPointF pointer(area.left() + area.width() * 0.25, area.top() + area.height() * 0.3);
        const double anchor = xBefore.first + (xBefore.second - xBefore.first) * 0.25;
        wheel(pointer, Qt::ShiftModifier);
        const auto xZoom = plot.horizontalRange();
        QVERIFY(xZoom.second - xZoom.first < xBefore.second - xBefore.first);
        QCOMPARE(plot.verticalRange(), yBefore);
        QVERIFY(qAbs(xZoom.first + (xZoom.second - xZoom.first) * 0.25 - anchor) < 1e-9);
        QVERIFY(!plot.followsLatest());
        wheel(plot.plotArea().center(), Qt::ControlModifier);
        QCOMPARE(plot.horizontalRange(), xZoom);
        QVERIFY(plot.verticalRange().second - plot.verticalRange().first < yBefore.second - yBefore.first);
        const auto yZoom = plot.verticalRange();
        const auto capture = qEnvironmentVariable("SW_WAVE_CAPTURE_DIR");
        if (!capture.isEmpty())
        {
            QMouseEvent move(QEvent::MouseMove, plot.plotArea().center(),
                plot.mapToGlobal(plot.plotArea().center().toPoint()), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&plot, &move);
            QVERIFY(plot.grab().save(capture + "/wave-navigation.png"));

            // 视觉检查使用多个相近量级的信号，方便比较网格密度与线条。
            QJsonArray samples;
            for (int index = 0; index < 300; ++index)
            {
                const double phase = index * 0.045;
                const QJsonObject values {
                    {"sine", std::sin(phase)},
                    {"slow", 0.5 * std::cos(phase * 0.4)},
                    {"steps", std::floor(std::sin(phase * 0.7) * 6) / 6},
                    {"ramp", (index % 37) / 18.0 - 1}
                };
                samples.append(QJsonObject {{"index", index + 1}, {"values", values}});
            }
            const QJsonObject source {{"samples", samples}};
            const QJsonObject sources {{"default", source}};
            sw::WavePlot preview;
            preview.setFont(sw::Theme::defaultDataFont());
            preview.resize(1100, 560);
            preview.setSamples({{"sources", sources}});
            preview.show();
            QVERIFY(preview.grab().save(capture + "/wave-grid-light.png"));
            preview.theme = sw::Theme::make(true);
            preview.invalidatePlot();
            QVERIFY(preview.grab().save(capture + "/wave-grid-dark.png"));
            preview.resize(500, 360);
            QVERIFY(preview.grab().save(capture + "/wave-grid-narrow.png"));
        }
        channel->receive("@wave temperature=99\n");
        plot.updateChannel(channel, 100);
        QCOMPARE(plot.horizontalRange(), xZoom);
        QCOMPARE(plot.verticalRange(), yZoom);
        const auto panArea = plot.plotArea();
        const QPointF panStart = panArea.center();
        const auto mouse = [&plot](QEvent::Type type, QPointF position, Qt::MouseButton button, Qt::MouseButtons buttons)
        {
            QMouseEvent event(type, position, plot.mapToGlobal(position.toPoint()), button, buttons, Qt::NoModifier);
            QApplication::sendEvent(&plot, &event);
        };
        mouse(QEvent::MouseButtonPress, panStart, Qt::LeftButton, Qt::LeftButton);
        channel->receive("@wave temperature=100\n");
        plot.updateChannel(channel, 100);
        mouse(QEvent::MouseMove, panStart + QPointF(25, 20), Qt::NoButton, Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease, panStart + QPointF(25, 20), Qt::LeftButton, Qt::NoButton);
        const auto pannedX = plot.horizontalRange();
        const auto pannedY = plot.verticalRange();
        QVERIFY(qAbs(pannedX.first - (xZoom.first - (xZoom.second - xZoom.first) * 25 / panArea.width())) < 1e-9);
        QVERIFY(qAbs(pannedY.first - (yZoom.first + (yZoom.second - yZoom.first) * 20 / panArea.height())) < 1e-9);
        plot.resize(500, 400);
        QCOMPARE(plot.horizontalRange(), pannedX);
        QCOMPARE(plot.verticalRange(), pannedY);
        plot.resetView();
        QVERIFY(plot.followsLatest());
        QCOMPARE(plot.horizontalRange().second, 102.0);
        const auto fullX = plot.horizontalRange();
        wheel(plot.plotArea().center(), Qt::NoModifier);
        QVERIFY(plot.horizontalRange().second - plot.horizontalRange().first < fullX.second - fullX.first);
        QVERIFY(plot.verticalRange().second < 110);

        sw::Instrument instrument("waveform");
        QVERIFY(instrument.points->suffix().isEmpty());
        QVERIFY(instrument.waveFollow->isChecked());
        instrument.tick(channel);
        instrument.plot->setFollowLatest(false);
        QVERIFY(!instrument.waveFollow->isChecked());
        instrument.waveFollow->setChecked(true);
        QVERIFY(instrument.plot->followsLatest());
    }

    void waveformFollowPreservesView_data()
    {
        QTest::addColumn<bool>("manualY");
        QTest::newRow("auto-y") << false;
        QTest::newRow("manual-y") << true;
    }

    void waveformFollowPreservesView()
    {
        QFETCH(bool, manualY);
        sw::Engine engine;
        auto channel = engine.waveformDemo();
        channel->demoTimer.stop();
        for (int index = 500; index < 6000; ++index)
        {
            channel->receive(QString("@wave sine=%1,square=%2,triangle=%3,sawtooth=%4\n")
                .arg(std::sin(index * 0.0628), 0, 'f', 6)
                .arg(index % 100 < 50 ? 0.8 : -0.8)
                .arg(std::asin(std::sin(index * 0.044)) * 2 / 3.141592653589793, 0, 'f', 6)
                .arg((index % 143) / 71.0 - 1, 0, 'f', 6).toUtf8());
        }
        sw::Instrument instrument("waveform");
        instrument.points->setValue(5000);
        instrument.resize(1150, 660);
        instrument.show();
        instrument.tick(channel);
        auto plot = instrument.plot;
        plot->setFont(sw::Theme::defaultDataFont());
        const auto wheel = [plot](Qt::KeyboardModifiers modifiers)
        {
            const auto position = plot->plotArea().center();
            QWheelEvent event(position, plot->mapToGlobal(position.toPoint()), QPoint(), QPoint(0, 120),
                              Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
            QApplication::sendEvent(plot, &event);
        };
        for (int index = 0; index < 11; ++index) { wheel(Qt::ShiftModifier); }
        if (manualY) { wheel(Qt::ControlModifier); }
        QVERIFY(!instrument.waveFollow->isChecked());
        const auto beforeX = plot->horizontalRange();
        const auto beforeY = plot->verticalRange();
        const double span = beforeX.second - beforeX.first;
        QVERIFY(span < 1200);
        QVERIFY(beforeX.second < 6000);
        const auto capture = qEnvironmentVariable("SW_FOLLOW_CAPTURE_DIR");
        const QString suffix = manualY ? "manual-y" : "auto-y";
        if (!capture.isEmpty())
        {
            QTest::qWait(30);
            QVERIFY(plot->grab().save(capture + "/before-" + suffix + ".png"));
        }

        instrument.waveFollow->setChecked(true);
        QVERIFY(plot->followsLatest());
        auto following = plot->horizontalRange();
        QVERIFY(qAbs(following.second - following.first - span) < 1e-9);
        QCOMPARE(following.second, 6000.0);
        QCOMPARE(plot->verticalRange(), beforeY);
        if (!capture.isEmpty())
        {
            QTest::qWait(30);
            QVERIFY(plot->grab().save(capture + "/after-" + suffix + ".png"));
        }

        // 新数据及历史淘汰不能改变已选的横轴跨度或纵轴范围。
        for (int index = 0; index < 5001; ++index) { channel->receive("@wave sine=1000\n"); }
        instrument.tick(channel);
        following = plot->horizontalRange();
        QCOMPARE(following.second, 11001.0);
        QVERIFY(qAbs(following.second - following.first - span) < 1e-9);
        QCOMPARE(plot->verticalRange(), beforeY);
        instrument.waveFollow->setChecked(false);
        channel->receive("@wave sine=2000\n");
        instrument.tick(channel);
        QCOMPARE(plot->horizontalRange(), following);
        instrument.waveFollow->setChecked(true);
        following = plot->horizontalRange();
        QCOMPARE(following.second, 11002.0);
        QVERIFY(qAbs(following.second - following.first - span) < 1e-9);
        QCOMPARE(plot->verticalRange(), beforeY);
        plot->resize(750, 430);
        QCOMPARE(plot->horizontalRange(), following);

        instrument.waveFollow->setChecked(false);
        QPushButton *fit = nullptr;
        for (auto button : instrument.findChildren<QPushButton *>())
        {
            if (button->text() == "自动缩放") { fit = button; break; }
        }
        QVERIFY(fit);
        fit->click();
        QVERIFY(instrument.waveFollow->isChecked());
        following = plot->horizontalRange();
        QCOMPARE(following.second - following.first, 4999.0);
        QVERIFY(plot->verticalRange() != beforeY);
    }

    void structuredDemoSelection()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        auto wave = engine.waveformDemo();
        auto page = window.newInstrument("json");
        auto picker = page->instrument->source;
        const auto key = sw::Config::structuredDemo().key();
        QVERIFY(picker->findData(sw::Config::waveformDemo().key()) < 0);
        const int index = picker->findData(key);
        QVERIFY(index > 0);
        picker->setCurrentIndex(index);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, index));
        auto channel = page->channel.data();
        QVERIFY(channel);
        QVERIFY(channel->config.isStructuredDemo());
        QVERIFY(channel != wave);
        QCOMPARE(engine.channels.size(), 2);
        QCOMPARE(engine.structuredDemo(), channel);
        QCOMPARE(engine.open(sw::Config::structuredDemo()), channel);
        QCOMPARE(channel->demoTimer.interval(), 1000);
        const auto source = channel->sources.value("default");
        QVERIFY(source);
        QCOMPARE(source->count, 0);
        QVERIFY(source->samples.empty());
        QVERIFY(source->parsedCount >= 6);
        bool json = false;
        bool xml = false;
        for (const auto &record : source->records)
        {
            if (record["format"] == "json")
            {
                json = true;
                QVERIFY(record["value"].toObject().contains("telemetry"));
            }
            if (record["format"] == "xml")
            {
                xml = true;
                QVERIFY(record["text"].toString().contains("<temperature unit=\"C\">"));
            }
        }
        QVERIFY(json && xml);
        QCOMPARE(wave->sources["default"]->parsedCount, 0);
        QVERIFY(wave->sources["default"]->count >= 500);
        const qint64 before = source->parsedCount;
        QTRY_VERIFY(source->parsedCount > before);
        QTRY_VERIFY(page->instrument->table->topLevelItemCount() >= 3);
        QVERIFY_EXCEPTION_THROWN(channel->send("test"), std::runtime_error);
        const auto capture = qEnvironmentVariable("SW_DEMO_CAPTURE_DIR");
        if (!capture.isEmpty()) { QVERIFY(window.grab().save(capture + "/structured-demo.png")); }
        auto fromWave = window.newInstrument("json", wave);
        QCOMPARE(fromWave->channel, channel);
        auto backToWave = window.newInstrument("waveform", channel);
        QCOMPARE(backToWave->channel, wave);
        QVERIFY(backToWave->instrument->source->findData(key) < 0);
        QCOMPARE(engine.channels.size(), 2);
        window.close();
    }

    void structuredDemoRestoreAndMigration()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("settings.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QJsonObject oldPage {{"kind", "structured"}, {"source_key", sw::Config::waveformDemo().key()}};
        file.write(QJsonDocument(QJsonObject {{"widget_pages", QJsonArray {oldPage}}}).toJson());
        file.close();
        for (int pass = 0; pass < 2; ++pass)
        {
            sw::Engine engine;
            sw::McpServer mcp(&engine);
            sw::Window window(&engine, &mcp, path);
            QCOMPARE(engine.channels.size(), 1);
            auto channel = engine.channels.first();
            QVERIFY(channel->config.isStructuredDemo());
            QCOMPARE(channel->state, QString("connected"));
            bool found = false;
            for (auto page : window.panes)
            {
                if (page->instrument && page->instrument->kind == "json")
                {
                    QCOMPARE(page->channel, channel);
                    QCOMPARE(page->sourceKey, sw::Config::structuredDemo().key());
                    QVERIFY(page->instrument->source->findData(sw::Config::waveformDemo().key()) < 0);
                    found = true;
                }
            }
            QVERIFY(found);
            QVERIFY(channel->sources["default"]->samples.empty());
            QVERIFY(channel->sources["default"]->parsedCount >= 6);
            window.close();
        }
    }

    void waveformDemoSelection()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        auto page = window.newInstrument("waveform");
        auto picker = page->instrument->source;
        const QString demoKey = sw::Config::waveformDemo().key();
        const int demoIndex = picker->findData(demoKey);
        QVERIFY(demoIndex > 0);
        QVERIFY(engine.channels.isEmpty());
        const int tabs = window.tabs->count();
        picker->setCurrentIndex(demoIndex);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, demoIndex));
        auto channel = page->channel.data();
        QVERIFY(channel);
        QVERIFY(channel->config.isWaveformDemo());
        QCOMPARE(channel->state, QString("connected"));
        QCOMPARE(engine.channels.size(), 1);
        QCOMPARE(window.tabs->count(), tabs);
        QCOMPARE(engine.waveformDemo(), channel);
        QCOMPARE(engine.open(sw::Config::waveformDemo()), channel);
        QVERIFY(!channel->serial && !channel->client && !channel->udp && !channel->ssh);
        const auto source = channel->sources.value("default");
        QVERIFY(source);
        QVERIFY(source->count >= 500);
        QCOMPARE(channel->demoTimer.interval(), 20);
        const auto values = source->samples.back()["values"].toObject();
        QCOMPARE(values.size(), 4);
        for (const auto &name : QStringList {"sine", "triangle", "square", "sawtooth"}) { QVERIFY(values.contains(name)); }
        const qint64 before = source->count;
        QTRY_VERIFY(source->count > before);
        QTRY_COMPARE(page->instrument->plot->series.size(), 4);
        QVERIFY_EXCEPTION_THROWN(channel->send("test"), std::runtime_error);
        const auto events = engine.journal.read(0, 200, channel->id);
        QCOMPARE(engine.journal.read(0, 200, channel->id), events);
        QVERIFY(!channel->waveform(10)["sources"].toObject().isEmpty());
        const auto capture = qEnvironmentVariable("SW_DEMO_CAPTURE_DIR");
        if (!capture.isEmpty())
        {
            QTest::qWait(100);
            QVERIFY(window.grab().save(capture + "/wave-demo.png"));
        }
        sw::Config config;
        config.port = "loop://";
        auto real = engine.open(config);
        const int realIndex = picker->findData(config.key());
        QVERIFY(realIndex > 0);
        picker->setCurrentIndex(realIndex);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, realIndex));
        QCOMPARE(page->channel, real);
        real->receive("@wave device_value=42\n");
        QTRY_VERIFY(page->instrument->plot->series.contains("device_value"));
        QVERIFY(!page->instrument->plot->series.contains("sine"));
        const int again = picker->findData(demoKey);
        picker->setCurrentIndex(again);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, again));
        QCOMPARE(page->channel, channel);
        QCOMPARE(engine.channels.size(), 2);
        QCOMPARE(real->state, QString("connected"));
        window.close();
    }

    void waveformDemoRestore()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("settings.json");
        const QString demoKey = sw::Config::waveformDemo().key();
        {
            sw::Engine engine;
            sw::McpServer mcp(&engine);
            sw::Window window(&engine, &mcp, path);
            auto page = window.newInstrument("waveform");
            auto picker = page->instrument->source;
            const int index = picker->findData(demoKey);
            picker->setCurrentIndex(index);
            QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, index));
            QVERIFY(page->channel);
            window.close();
        }
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, path);
        QCOMPARE(engine.channels.size(), 1);
        auto channel = engine.channels.first();
        QVERIFY(channel->config.isWaveformDemo());
        QCOMPARE(channel->state, QString("connected"));
        bool found = false;
        for (auto page : window.panes)
        {
            if (page->instrument && page->sourceKey == demoKey)
            {
                QCOMPARE(page->channel, channel);
                found = true;
            }
        }
        QVERIFY(found);
        auto second = window.newInstrument("waveform");
        auto picker = second->instrument->source;
        const int index = picker->findData(demoKey);
        picker->setCurrentIndex(index);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, index));
        QCOMPARE(second->channel, channel);
        QCOMPARE(engine.channels.size(), 1);
        channel->close();
        QCOMPARE(channel->state, QString("closed"));
        QVERIFY(!channel->demoTimer.isActive());
        const int restart = picker->findData(demoKey);
        picker->setCurrentIndex(restart);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, restart));
        QCOMPARE(channel->state, QString("connected"));
        QCOMPARE(engine.channels.size(), 1);
        window.close();
    }

    void waveformIncrementalHistory()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        sw::WavePlot plot;
        channel->receive("@wave a=1,b=2\n");
        QVERIFY(plot.updateChannel(channel, 3));
        const auto revision = plot.dataRevision();
        QVERIFY(!plot.updateChannel(channel, 3));
        channel->receive("ordinary log\n{\"a\":100}\n");
        QVERIFY(!plot.updateChannel(channel, 3));
        QCOMPARE(plot.dataRevision(), revision);
        channel->receive("@wave a=2,b=4\n@wave a=3,b=6\n@wave a=4,b=8\n");
        QVERIFY(plot.updateChannel(channel, 3));
        QCOMPARE(plot.series["a"].size(), 3);
        QCOMPARE(plot.series["a"].first(), QPointF(2, 2));
        QCOMPARE(plot.series["b"].last(), QPointF(4, 8));
        QVERIFY(plot.updateChannel(channel, 2));
        QCOMPARE(plot.series["a"].size(), 2);
        QCOMPARE(plot.series["a"].first().x(), 3.0);
        channel->sources["default"] = std::make_shared<sw::Source>();
        channel->receive("@wave a=99\n");
        QVERIFY(plot.updateChannel(channel, 2));
        QCOMPARE(plot.series.size(), 1);
        QCOMPARE(plot.series["a"].first(), QPointF(1, 99));
        channel->receive("@wave remote=7\n", "peer");
        QVERIFY(plot.updateChannel(channel, 2));
        QVERIFY(plot.series.contains("peer/remote"));
        channel->sources.remove("peer");
        QVERIFY(plot.updateChannel(channel, 2));
        QVERIFY(!plot.series.contains("peer/remote"));
        QVERIFY(plot.updateChannel(nullptr, 2));
        QVERIFY(plot.series.isEmpty());
        QStringList fields;
        for (int index = 0; index < 16; ++index) { fields.append(QString("old%1=1").arg(index)); }
        channel->receive(("@wave " + fields.join(',') + "\n").toUtf8());
        plot.updateChannel(channel, 1);
        QCOMPARE(plot.series.size(), 16);
        channel->receive("@wave new_channel=9\n");
        plot.updateChannel(channel, 1);
        QCOMPARE(plot.series.size(), 1);
        QCOMPARE(plot.series["new_channel"].last().y(), 9.0);
    }

    void waveformExactInspectionAndVisibility()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->receive("@wave temperature=0,count=4294967293\n"
            "@wave temperature=1.2345678901234567,count=4294967294\n"
            "@wave temperature=2,count=4294967295\n");
        sw::WavePlot plot;
        plot.setFont(sw::Theme::defaultDataFont());
        plot.resize(1000, 520);
        plot.updateChannel(channel, 1000);
        plot.setCurveVisible("count", false);
        plot.show();
        QTest::qWait(40);
        const auto position = plot.plotArea().center();
        const auto hit = plot.inspect(position);
        QCOMPARE(hit["channel"].toString(), "temperature");
        QCOMPARE(hit["sample"].toInt(), 2);
        QCOMPARE(hit["value"].toDouble(), 1.2345678901234567);
        QVERIFY(plot.inspect(QPointF(2, 2)).isEmpty());
        auto buttons = plot.findChildren<QToolButton *>("waveChannel");
        QCOMPARE(buttons.size(), 2);
        bool exactLatest = false;
        for (auto button : buttons)
        {
            if (button->property("channelName") == "count")
            {
                exactLatest = button->text().contains("4294967295");
                QVERIFY(!button->isChecked());
            }
        }
        QVERIFY(exactLatest);
        QMouseEvent move(QEvent::MouseMove, position, plot.mapToGlobal(position.toPoint()),
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&plot, &move);
        auto copy = plot.findChild<QAction *>("copyWaveReadout");
        QVERIFY(copy);
        copy->trigger();
        const auto copied = QJsonDocument::fromJson(QApplication::clipboard()->text().toUtf8()).object();
        QCOMPARE(copied, hit);
        const auto capture = qEnvironmentVariable("SW_WAVE_CAPTURE_DIR");
        if (!capture.isEmpty())
        {
            QVERIFY(plot.grab().save(capture + "/wave-readout-light.png"));
            plot.theme = sw::Theme::make(true);
            plot.invalidatePlot();
            QVERIFY(plot.grab().save(capture + "/wave-readout-dark.png"));
        }
        plot.setCurveVisible("temperature", false);
        QVERIFY(plot.inspect(position).isEmpty());
        plot.setCurveVisible("temperature", true);
        QVERIFY(!plot.inspect(position).isEmpty());
    }

    void waveformDenseDataPreservesSamples()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        for (int index = 0; index < 5000; ++index)
        {
            channel->receive(index == 2499 ? "@wave value=123.456\n" : "@wave value=0\n");
        }
        sw::WavePlot plot;
        plot.resize(400, 300);
        plot.updateChannel(channel, 5000);
        QVERIFY(!plot.grab().isNull());
        QCOMPARE(plot.series["value"].size(), 5000);
        QVERIFY(plot.renderedPointCount() < 1000);
        QCOMPARE(plot.series["value"][2499].y(), 123.456);
        const auto area = plot.plotArea();
        const QPointF peak(area.left() + 2499.0 / 4999 * area.width(), area.top());
        QCOMPARE(plot.inspect(peak)["value"].toDouble(), 123.456);
        QCOMPARE(channel->sources["default"]->samples.size(), size_t(5000));
    }

    void waveformLargeConstantReadout()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->receive("@wave large=1e300\n@wave large=1e300\n");
        sw::WavePlot plot;
        plot.resize(800, 400);
        plot.updateChannel(channel, 100);
        plot.show();
        QVERIFY(!plot.grab().isNull());
        const auto readout = plot.inspect(plot.plotArea().center());
        QCOMPARE(readout["channel"].toString(), "large");
        QCOMPARE(readout["value"].toDouble(), 1e300);
    }

    void waveformFrameClockAndFreeze()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        sw::Instrument instrument("waveform");
        instrument.resize(800, 600);
        instrument.show();
        instrument.tick(channel);
        QCOMPARE(instrument.frameTimer.interval(), 16);
        QCOMPARE(instrument.frameTimer.timerType(), Qt::PreciseTimer);
        QVERIFY(instrument.frameTimer.isActive());
        channel->receive("@wave value=1\n");
        QTRY_COMPARE(instrument.plot->series["value"].size(), 1);
        instrument.freeze->setChecked(true);
        QVERIFY(!instrument.frameTimer.isActive());
        channel->receive("@wave value=2\n");
        QTest::qWait(40);
        QCOMPARE(instrument.plot->series["value"].size(), 1);
        instrument.freeze->setChecked(false);
        QTRY_COMPARE(instrument.plot->series["value"].size(), 2);
        instrument.hide();
        QVERIFY(!instrument.frameTimer.isActive());
    }

    void sshProfilesAndSession()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QDir().mkpath(directory.filePath("includes"));

        QFile included(directory.filePath("includes/hosts.conf"));
        QVERIFY(included.open(QIODevice::WriteOnly));
        included.write("Host lab-server lab-backup\n  HostName 192.0.2.10\nInclude ../config\n");
        included.close();

        QFile configFile(directory.filePath("config"));
        QVERIFY(configFile.open(QIODevice::WriteOnly));
        configFile.write("# local configuration\nHost local-host *.example !blocked\n  User tester\nInclude includes/*.conf\n");
        configFile.close();

        QCOMPARE(sw::SshProfiles::aliases(configFile.fileName()),
            QStringList({"local-host", "lab-server", "lab-backup"}));

        sw::Config config;
        config.kind = "ssh";
        config.sshHost = "lab-server";
        config.sshIdentity = directory.filePath("test key");
        QCOMPARE(sw::Config::fromJson(config.json()).key(), config.key());
        config.validate();
        config.sshHost = "-oProxyCommand=evil";
        QVERIFY_EXCEPTION_THROWN(config.validate(), std::runtime_error);
        config.sshHost = "lab-server";

        sw::Engine engine;
        engine.sshExecutable = QCoreApplication::applicationDirPath() + "/fake_ssh.exe";
        QVERIFY(QFile::exists(engine.sshExecutable));
        auto channel = engine.open(config);
        QVERIFY2(channel->error.isEmpty(), qPrintable(channel->error));
        QTRY_VERIFY(channel->state == "connected" || channel->state == "error");
        QVERIFY2(channel->state == "connected", qPrintable(channel->error));
        QTRY_VERIFY(channel->rx >= 6);
#ifdef Q_OS_WIN
        const auto sizeCursor = engine.journal.sequence;
        channel->resizeTerminal(QSize(57, 18));
        channel->send(QByteArray(1, char(5)));
        const auto resizedReply = [&]
        {
            QByteArray reply;
            for (const auto &entry : engine.journal.read(sizeCursor, 1000, channel->id)["events"].toArray())
            {
                const auto event = entry.toObject();
                if (event["direction"] == "rx") { reply += QByteArray::fromHex(event["hex"].toString().toLatin1()); }
            }
            return reply.contains("PTY_SIZE 57 18");
        };
        QTRY_VERIFY(resizedReply());
#endif

        const QByteArray input("private-input\n");
        sw::TerminalView echoed;
        connect(channel, &sw::Channel::bytesReceived, &echoed, &sw::TerminalView::feed);
        const auto receivedBeforeInput = channel->rx;
        const auto sent = channel->send(input);
        QCOMPARE(sent["bytes_sent"].toInt(), input.size());
        QTRY_VERIFY(channel->rx > receivedBeforeInput);
        QTRY_VERIFY(echoed.text().contains("private-input"));
        const auto events = engine.journal.read(0)["events"].toArray();
        for (const auto &value : events)
        {
            const auto event = value.toObject();
            QVERIFY(event["direction"].toString() != "tx");
        }
        channel->close();
        QCOMPARE(channel->state, QString("closed"));

        sw::McpServer mcp(&engine);
        QCOMPARE(mcp.call("get_status", {}).toObject()["counts_by_protocol"].toObject()["ssh"].toInt(), 0);

        sw::Engine uiEngine;
        uiEngine.sshExecutable = engine.sshExecutable;
        sw::McpServer uiMcp(&uiEngine);
        sw::Window window(&uiEngine, &uiMcp, directory.filePath("settings.json"));
        window.show();
        window.protocol->setCurrentIndex(4);
        QCOMPARE(window.configStack->currentIndex(), 2);
        window.sshTarget->setCurrentText("lab-server");
        window.sshIdentity->setText(directory.filePath("test key"));
        QCOMPARE(window.form().sshHost, QString("lab-server"));
        window.connectionAction();
        QTRY_COMPARE(window.active->channel->state, QString("connected"));
        QVERIFY(window.active->terminalMode);
        QCOMPARE(window.active->channel->config.sshIdentity, directory.filePath("test key"));
        window.active->tick();
        auto terminal = window.active->terminal;
        terminal->setFocus();
        QTRY_VERIFY(terminal->hasFocus());
        QSignalSpy transmitted(terminal, &sw::TerminalView::transmit);
        const auto receivedBeforeTab = window.active->channel->rx;
        QTest::keyClick(terminal, Qt::Key_Tab);
        QCOMPARE(transmitted.count(), 1);
        QCOMPARE(transmitted.first().first().toByteArray(), QByteArray("\t"));
        QVERIFY(terminal->hasFocus());
        QTRY_VERIFY(window.active->channel->rx > receivedBeforeTab);
        QCOMPARE(window.active->channel->tx, 1);
        QTest::keyClick(terminal, Qt::Key_Tab, Qt::ShiftModifier);
        QCOMPARE(transmitted.count(), 2);
        QCOMPARE(transmitted.last().first().toByteArray(), QByteArray("\x1b[Z"));
        QVERIFY(terminal->hasFocus());
        QTest::keyClick(terminal, Qt::Key_Home, Qt::KeypadModifier);
        QCOMPARE(transmitted.count(), 3);
        QCOMPARE(transmitted.last().first().toByteArray(), QByteArray("\x1b[7~"));
        QTest::keyClick(terminal, Qt::Key_End, Qt::KeypadModifier);
        QCOMPARE(transmitted.count(), 4);
        QCOMPARE(transmitted.last().first().toByteArray(), QByteArray("\x1b[4~"));
        QVERIFY(terminal->inputMethodQuery(Qt::ImEnabled).toBool());
        const QRectF inputRectangle = terminal->inputMethodQuery(Qt::ImCursorRectangle).toRectF();
        QVERIFY(terminal->rect().contains(inputRectangle.toRect()));
        QInputMethodEvent preedit("ni", {});
        QApplication::sendEvent(terminal, &preedit);
        QCOMPARE(terminal->preeditText, QString("ni"));
        QInputMethodEvent commit;
        commit.setCommitString("你");
        QApplication::sendEvent(terminal, &commit);
        QCOMPARE(transmitted.count(), 5);
        QCOMPARE(transmitted.last().first().toByteArray(), QString("你").toUtf8());
        terminal->enabledInput = false;
        QTest::keyClick(terminal, Qt::Key_Tab);
        QCOMPARE(transmitted.count(), 5);
        QVERIFY(!terminal->hasFocus());
        window.close();
    }

    void initTestCase()
    {
        QApplication::setStyle("Fusion");
        QApplication::setFont(sw::Theme::interfaceFont());
    }

    void gearRotationAndToolbarHints()
    {
        sw::IconButton gear("gear", "设置");
        QSignalSpy turns(&gear.rotationAnimation, &QVariantAnimation::finished);
        QEnterEvent enter(gear.rect().center(), gear.rect().center(), gear.rect().center());
        QEvent leave(QEvent::Leave);

        QApplication::sendEvent(&gear, &enter);
        QTest::qWait(190);
        QVERIFY(gear.progress > 0.9);
        QVERIFY(gear.rotation > 0 && gear.rotation < 360);
        QTRY_COMPARE(turns.count(), 1);
        QCOMPARE(gear.rotationAnimation.endValue().toReal(), 360.0);
        QCOMPARE(gear.rotation, 0.0);

        QApplication::sendEvent(&gear, &leave);
        QTest::qWait(190);
        QVERIFY(gear.progress < 0.1);
        QVERIFY(gear.rotation > 0 && gear.rotation < 360);
        QTRY_COMPARE(turns.count(), 2);
        QCOMPARE(gear.rotation, 0.0);

        QApplication::sendEvent(&gear, &enter);
        QTest::qWait(60);
        const qreal beforeLeave = gear.rotation;
        QApplication::sendEvent(&gear, &leave);
        QCOMPARE(gear.rotation, beforeLeave);
        QTRY_COMPARE(turns.count(), 4);
        QCOMPARE(gear.rotation, 0.0);

        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        for (const auto control : window.active->monitorPage->findChildren<sw::IconButton *>())
        {
            QVERIFY(control->text().isEmpty());
            QVERIFY(!control->toolTip().isEmpty());
        }
        QCOMPARE(window.active->modeButton->size(), QSize(36, 36));
        window.active->setTerminal(true);
        QVERIFY(window.active->modeButton->text().isEmpty());
        QCOMPARE(window.active->modeButton->toolTip(), QString("返回收发日志"));
        QCOMPARE(window.active->modeButton->accessibleName(), window.active->modeButton->toolTip());
        window.close();
    }

    void readableFontsAndMigration()
    {
        const QFont font = sw::Theme::defaultDataFont();
        const auto raw = QRawFont::fromFont(font);
        QVERIFY(raw.isValid());
        QVERIFY(raw.supportsCharacter('M'));
        QCOMPARE(font.styleStrategy(), QFont::PreferAntialias);
        const auto brandRaw = QRawFont::fromFont(QFont(sw::Theme::fontFamily()));
        QVERIFY(brandRaw.isValid());
        QVERIFY(brandRaw.supportsCharacter('S'));
        const auto interfaceRaw = QRawFont::fromFont(sw::Theme::interfaceFont());
        for (const QChar character : QString("连接参数端口波特率数据校验停止位流控设置发送搜索"))
        {
            QVERIFY2(interfaceRaw.supportsCharacter(character.unicode()), qPrintable(QString(character)));
        }
        const QFontMetrics metrics(font);
        QCOMPARE(metrics.horizontalAdvance("iiii"), metrics.horizontalAdvance("WWWW"));
        QCOMPARE(metrics.horizontalAdvance("0000"), metrics.horizontalAdvance("WWWW"));

        QTemporaryDir directory;
        const QString path = directory.filePath("settings.json");
        const auto writeAppearance = [&](const QString &family, int size)
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QJsonDocument(QJsonObject {{"appearance", QJsonObject {
                {"font_family", family}, {"font_size", size}
            }}}).toJson());
        };
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        writeAppearance(sw::Theme::fontFamily(12), 18);
        {
            sw::Window window(&engine, &mcp, path);
            window.ensurePolished();
            auto title = window.findChild<QLabel *>("brandTitle");
            QVERIFY(title);
            QVERIFY(!title->font().family().contains("Fusion Pixel", Qt::CaseInsensitive));
            QVERIFY(window.active->terminal->font().families().contains("Microsoft YaHei UI"));
            QCOMPARE(window.dataFont.family(), font.family());
            QCOMPARE(window.dataFont.pointSize(), font.pointSize());
            window.close();
        }
        writeAppearance("Consolas", 14);
        {
            sw::Window window(&engine, &mcp, path);
            QCOMPARE(window.dataFont.family(), QString("Consolas"));
            QCOMPARE(window.dataFont.pointSize(), 14);
            window.close();
        }
    }

    void ansiResetAndTerminal()
    {
        sw::AnsiState state;
        state.sgr({32}, sw::Theme::make(false));
        QVERIFY(state.foreground.isValid());
        state.sgr({0}, sw::Theme::make(false));
        QVERIFY(!state.foreground.isValid());
        QVERIFY(!state.background.isValid());
        sw::LogView log;
        sw::Journal journal;
        log.feed(journal.append("a", "rx", "\x1b[32mGREEN\x1b[0m ordinary\r\n"));
        QVERIFY(log.toPlainText().contains("GREEN ordinary"));
        QTextCursor cursor(log.document());
        cursor.setPosition(log.toPlainText().indexOf("ordinary") + 2);
        QCOMPARE(cursor.charFormat().foreground().color(), log.theme.text);
        sw::TerminalView terminal;
        terminal.feed("hello\rworld\x1b[2J\x1b[Hdone");
        QVERIFY(terminal.text().trimmed().startsWith("done"));
        const auto bytes = QString("中文").toUtf8();
        terminal.feed(bytes.left(2));
        terminal.feed(bytes.mid(2));
        QVERIFY(terminal.text().contains("中"));
    }
    void journalCursors()
    {
        sw::Journal journal;
        journal.append("a", "RX", "first");
        journal.append("a", "RX", "second");
        QCOMPARE(journal.read(0, 1)["events"].toArray().size(), 1);
        QCOMPARE(journal.read(0)["events"].toArray().size(), 2);
        QCOMPARE(journal.read(1)["events"].toArray().size(), 1);
        QCOMPARE(journal.sequence, 2);
    }
    void directToolbarActions()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.port->setCurrentText("loop://");
        window.connectionAction();
        auto pane = window.active;
        auto channel = pane->channel;
        channel->receive("first message\n");
        pane->tick();
        QVERIFY(pane->log->toPlainText().contains("first message"));

        QTest::mouseClick(pane->pause, Qt::LeftButton);
        QVERIFY(pane->pause->toolTip().contains("继续显示"));
        channel->receive("received while paused\n");
        pane->tick();
        QVERIFY(!pane->log->toPlainText().contains("received while paused"));
        QVERIFY(engine.journal.read(0)["events"].toArray().last().toObject()["text"].toString().contains("received while paused"));
        QTest::mouseClick(pane->pause, Qt::LeftButton);
        pane->tick();
        QVERIFY(pane->log->toPlainText().contains("received while paused"));

        QTest::mouseClick(pane->follow, Qt::LeftButton);
        QVERIFY(!pane->log->follow);
        QTest::mouseClick(pane->follow, Qt::LeftButton);
        QVERIFY(pane->log->follow);
        const auto history = engine.journal.sequence;
        auto clear = pane->findChild<sw::IconButton *>("clearDisplay");
        QTest::mouseClick(clear, Qt::LeftButton);
        QVERIFY(pane->log->toPlainText().isEmpty());
        QCOMPARE(engine.journal.sequence, history);
        QVERIFY(pane->exportButton->isEnabled());

        pane->payload->setPlainText("ping");
        pane->newline->setCurrentIndex(0);
        pane->interval->setValue(40);
        const auto sent = channel->tx;
        QTest::mouseClick(pane->repeat, Qt::LeftButton);
        QVERIFY(pane->repeatTimer.isActive());
        QVERIFY(pane->repeat->toolTip().contains("停止"));
        QTRY_VERIFY(channel->tx >= sent + 8);
        pane->interval->setValue(60);
        QCOMPARE(pane->repeatTimer.interval(), 60);
        QTest::mouseClick(pane->repeat, Qt::LeftButton);
        QVERIFY(!pane->repeatTimer.isActive());
        const auto stopped = channel->tx;
        QTest::qWait(150);
        QCOMPARE(channel->tx, stopped);
        window.close();
    }

    void connectionIndicatorFollowsSelectedPage()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        auto capture = [&window](const QString &name)
        {
            const auto directory = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
            if (!directory.isEmpty())
            {
                QCoreApplication::processEvents();
                window.sidebar->grab().save(QDir(directory).filePath(name));
                window.grab().save(QDir(directory).filePath("workspace-" + name));
            }
        };

        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Offline);
        QCOMPARE(window.connectionState->text(), QString("未连接"));
        QCOMPARE(window.connectButton->text(), QString("建立连接"));
        capture("connection-offline.png");

        window.port->setCurrentText("loop://");
        QTest::mouseClick(window.connectButton, Qt::LeftButton);
        auto receive = window.active;
        auto channel = receive->channel;
        QVERIFY(channel);
        QCOMPARE(channel->state, QString("connected"));
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Online);
        QCOMPARE(window.connectionState->text(), QString("已连接"));
        QCOMPARE(window.connectButton->text(), QString("断开连接"));
        capture("connection-online.png");
        window.applyTheme(true);
        capture("connection-online-dark.png");
        window.applyTheme(false);

        const auto captureDirectory = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
        if (!captureDirectory.isEmpty())
        {
            receive->setTerminal(true);
            channel->receive("\r\nuser@device:~$ status\r\n"
                             "Serial Workbench | connection online\r\n"
                             "CPU 8%   Memory 124 MB   Latency 2 ms\r\n"
                             "终端已就绪，支持中文输入与复制粘贴。\r\n"
                             "user@device:~$ ");
            QTest::qWait(160);
            capture("terminal-controls.png");
            auto toolbar = receive->monitorPage->findChild<QWidget *>("monitorToolbar");
            QVERIFY(toolbar);
            QVERIFY(toolbar->grab().save(QDir(captureDirectory).filePath("toolbar-terminal.png")));
            const QRect gearRect(window.gear->mapTo(&window, QPoint()), window.gear->size());
            QVERIFY(window.grab().copy(gearRect).save(QDir(captureDirectory).filePath("gear.png")));
            auto title = window.findChild<QLabel *>("brandTitle");
            QVERIFY(title);
            const QRect titleRect(title->mapTo(&window, QPoint()), title->size());
            QVERIFY(window.grab().copy(titleRect).save(QDir(captureDirectory).filePath("brand-title.png")));
            QTest::mouseMove(window.gear, window.gear->rect().center());
            QEnterEvent enter(window.gear->rect().center(), window.gear->rect().center(),
                              window.gear->mapToGlobal(window.gear->rect().center()));
            QApplication::sendEvent(window.gear, &enter);
            QTest::qWait(200);
            QVERIFY(window.grab().copy(gearRect).save(QDir(captureDirectory).filePath("gear-hover.png")));
            QTest::mouseMove(toolbar, toolbar->rect().center());
            QEvent leave(QEvent::Leave);
            QApplication::sendEvent(window.gear, &leave);
            window.applyTheme(true);
            capture("terminal-controls-dark.png");
            window.applyTheme(false);
            receive->setTerminal(false);
            QTest::qWait(120);
            capture("controls-log.png");
            window.applyTheme(true);
            capture("controls-log-dark.png");
            window.applyTheme(false);
        }

        window.newConnection();
        QVERIFY(!window.active->channel);
        QCOMPARE(channel->state, QString("connected"));
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Offline);
        QCOMPARE(window.connectionState->text(), QString("未连接"));
        QCOMPARE(window.connectButton->text(), QString("建立连接"));

        auto waveform = window.newInstrument("waveform", channel);
        QCOMPARE(window.parameterPane, receive);
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Online);
        QCOMPARE(window.connectionState->text(), QString("已连接"));
        QTest::mouseClick(window.connectButton, Qt::LeftButton);
        QCOMPARE(channel->state, QString("closed"));
        QCOMPARE(window.active, waveform);
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Offline);
        QCOMPARE(window.connectionState->text(), QString("已断开"));
        QCOMPARE(window.connectButton->text(), QString("建立连接"));

        QTest::mouseClick(window.connectButton, Qt::LeftButton);
        QCOMPARE(channel->state, QString("connected"));
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Online);
        QCOMPARE(window.connectionState->text(), QString("已连接"));
        QCOMPARE(window.connectButton->text(), QString("断开连接"));
        window.close();
    }

    void connectionIndicatorDistinguishesPendingAndFailure()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.port->setCurrentText("loop://");
        window.connectionAction();
        auto channel = window.active->channel;
        QVERIFY(channel);

        // Drive the real state notification without opening a network connection.
        channel->status("connecting");
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Pending);
        QCOMPARE(window.connectionState->text(), QString("连接中"));
        QCOMPARE(window.connectButton->text(), QString("取消连接"));
        QTest::mouseClick(window.connectButton, Qt::LeftButton);
        QCOMPARE(channel->state, QString("closed"));
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Offline);
        QCOMPARE(window.connectionState->text(), QString("已断开"));

        channel->status("reconnecting", "Device temporarily unavailable");
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Pending);
        QCOMPARE(window.connectionState->text(), QString("重连中"));
        QCOMPARE(window.connectButton->text(), QString("停止重连"));

        channel->status("error", "Connection failed");
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Error);
        QCOMPARE(window.connectionState->text(), QString("连接异常"));
        QCOMPARE(window.connectButton->text(), QString("建立连接"));

        channel->status("listening");
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Online);
        QCOMPARE(window.connectionState->text(), QString("监听中"));
        QCOMPARE(window.connectButton->text(), QString("停止监听"));
        QTest::mouseClick(window.connectButton, Qt::LeftButton);
        QCOMPARE(channel->state, QString("closed"));
        QCOMPARE(window.connectionLed->state(), sw::StatusLed::State::Offline);
        QCOMPARE(window.connectionState->text(), QString("已断开"));
        window.close();
    }

    void instrumentRenameLeavesIconsVisible()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.resize(1100, 740);
        window.show();
        for (const auto &kind : QStringList {"json", "xml", "waveform"})
        {
            auto page = window.newInstrument(kind);
            const int index = pageIndex(&window, page);
            for (int iconSize : {16, 20})
            {
                window.tabs->setIconSize(QSize(iconSize, iconSize));
                QApplication::processEvents();
                const auto tab = window.tabs->tabRect(index);
                const QRect iconArea(tab.left() + 17, tab.center().y() - iconSize / 2, iconSize, iconSize);
                const auto before = window.tabs->grab(iconArea).toImage();
                window.beginTabRename(index);
                auto editor = window.tabs->findChild<QLineEdit *>("tabTitleEditor");
                QVERIFY(editor);
                QApplication::processEvents();
                QVERIFY(editor->geometry().left() >= iconArea.right() + 4);
                const auto close = window.tabs->tabButton(index, QTabBar::RightSide);
                QVERIFY(editor->geometry().right() < close->geometry().left());
                QCOMPARE(window.tabs->grab(iconArea).toImage(), before);
                const auto captureDir = qEnvironmentVariable("SW_TAB_CAPTURE_DIR");
                if (!captureDir.isEmpty() && iconSize == 16)
                {
                    QDir().mkpath(captureDir);
                    QVERIFY(window.tabs->grab().save(captureDir + "/rename-" + kind + ".png"));
                    QVERIFY(window.grab().save(captureDir + "/page-" + kind + ".png"));
                }
                QTest::keyClick(editor, Qt::Key_Escape);
                QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            }
        }
        window.close();
    }

    void tabsRenameOnlyFromContextMenuAndF2()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        auto first = window.active;
        window.newConnection();
        window.tabs->setCurrentIndex(pageIndex(&window, first));
        const auto position = window.tabs->tabRect(pageIndex(&window, first)).center();
        QTest::mouseDClick(window.tabs, Qt::MiddleButton, Qt::NoModifier, position);
        QTest::qWait(20);
        QVERIFY(!window.tabs->findChild<QLineEdit *>("tabTitleEditor"));
        QTest::mouseDClick(window.tabs, Qt::LeftButton, Qt::NoModifier, position);
        QTest::qWait(20);
        QVERIFY(!window.tabs->findChild<QLineEdit *>("tabTitleEditor"));
        first->payload->setFocus();
        QTest::keyClick(first->payload, Qt::Key_F2);
        QTRY_VERIFY(window.tabs->findChild<QLineEdit *>("tabTitleEditor"));
        auto editor = window.tabs->findChild<QLineEdit *>("tabTitleEditor");
        QTest::keyClicks(editor, "F2 title");
        QTest::keyClick(editor, Qt::Key_Return);
        QTRY_COMPARE(first->pageTitle, QString("F2 title"));
        QTRY_VERIFY(!window.tabs->findChild<QLineEdit *>("tabTitleEditor"));
        QTimer::singleShot(0, &window, []
        {
            auto menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            auto rename = menu->findChild<QAction *>("renameTab");
            QVERIFY(rename);
            rename->trigger();
            menu->close();
        });
        QMetaObject::invokeMethod(window.tabs, "customContextMenuRequested", Q_ARG(QPoint, position));
        QTRY_VERIFY(window.tabs->findChild<QLineEdit *>("tabTitleEditor"));
        editor = window.tabs->findChild<QLineEdit *>("tabTitleEditor");
        QTest::keyClicks(editor, "Context title");
        QTest::keyClick(editor, Qt::Key_Return);
        QTRY_COMPARE(first->pageTitle, QString("Context title"));
        QTRY_VERIFY(!window.tabs->findChild<QLineEdit *>("tabTitleEditor"));
        auto other = window.newConnection();
        const int count = window.tabs->count();
        QTest::mouseClick(window.tabs, Qt::MiddleButton, Qt::NoModifier,
            window.tabs->tabRect(pageIndex(&window, other)).center());
        QCOMPARE(window.tabs->count(), count - 1);
        QVERIFY(!window.tabs->findChild<QLineEdit *>("tabTitleEditor"));
        QCOMPARE(first->pageTitle, QString("Context title"));
        window.close();
    }

    void activeTabAndPageShareSurface_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }

    void activeTabAndPageShareSurface()
    {
        QFETCH(bool, dark);
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.applyTheme(dark);
        auto wave = window.newInstrument("waveform", nullptr, false);
        QApplication::processEvents();
        const auto tab = window.tabs->tabRect(window.tabs->currentIndex());
        const auto tabImage = window.tabs->grab().toImage();
        const auto pageImage = wave->instrument->grab().toImage();
        const auto tabColor = tabImage.pixelColor(QPoint(tab.center().x(), tab.top() + 5) * tabImage.devicePixelRatio());
        const auto pageColor = pageImage.pixelColor(QPoint(wave->instrument->width() - 5, 5) * pageImage.devicePixelRatio());
        QCOMPARE(tabColor, window.theme.panel);
        QCOMPARE(pageColor, tabColor);
        window.openSettings();
        auto pages = window.settingsDialog->findChild<QTabWidget *>("settingsPages");
        QVERIFY(pages);
        QApplication::processEvents();
        const auto bar = pages->tabBar();
        const auto settingsImage = pages->grab().toImage();
        const auto ratio = settingsImage.devicePixelRatio();
        QCOMPARE(bar->x() + bar->tabRect(0).left(), 0);
        QCOMPARE(settingsImage.pixelColor(QPoint(0, bar->geometry().center().y()) * ratio), window.theme.panel);
        QCOMPARE(settingsImage.pixelColor(QPoint(0, bar->geometry().bottom() + 4) * ratio), window.theme.panel);
        for (int index : {1, 2, 0})
        {
            pages->setCurrentIndex(index);
            QApplication::processEvents();
            const auto image = pages->grab().toImage();
            const int top = bar->geometry().bottom() + 1;
            const auto corner = image.pixelColor(QPoint(0, top) * ratio);
            QCOMPARE(corner, index == 0 ? window.theme.panel : window.theme.background);
            QCOMPARE(image.pixelColor(QPoint(9, top + 9) * ratio), window.theme.panel);
            const auto selected = bar->tabRect(index);
            const auto barImage = bar->grab().toImage();
            const qreal barRatio = barImage.devicePixelRatio();
            const int bottom = bar->height() - 1;
            const int right = selected.right() + 1 - 12;
            QCOMPARE(barImage.pixelColor(QPoint(right + 3, bottom) * barRatio), window.theme.panel);
            QCOMPARE(barImage.pixelColor(QPoint(right + 3, bottom - 7) * barRatio).alpha(), 0);
            if (index != 0)
            {
                QCOMPARE(barImage.pixelColor(QPoint(selected.left() - 4, bottom) * barRatio), window.theme.panel);
                QCOMPARE(barImage.pixelColor(QPoint(selected.left() - 4, bottom - 7) * barRatio).alpha(), 0);
            }
        }
        window.settingsDialog->hide();
        window.close();
    }

    void customReceiveTabTitleSurvivesConnectionAndDetach()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.json");
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        {
            sw::Window window(&engine, &mcp, path);
            window.show();
            auto receive = window.active;
            const int index = pageIndex(&window, receive);
            window.activateWindow();
            window.tabs->setFocus();
            QApplication::processEvents();
            QTest::keyClick(window.tabs, Qt::Key_F2);
            QTRY_VERIFY(window.tabs->findChild<QLineEdit *>("tabTitleEditor") != nullptr);
            auto editor = window.tabs->findChild<QLineEdit *>("tabTitleEditor");
            QVERIFY(editor);
            QTest::keyClicks(editor, "Bench A");
            QTest::keyClick(editor, Qt::Key_Escape);
            QTRY_COMPARE(window.tabs->tabText(index), QString("新收发页"));
            QTRY_VERIFY(window.tabs->findChild<QLineEdit *>("tabTitleEditor") == nullptr);

            window.beginTabRename(index);
            editor = window.tabs->findChild<QLineEdit *>("tabTitleEditor");
            QVERIFY(editor);
            QTest::keyClicks(editor, "Bench A");
            QTest::keyClick(editor, Qt::Key_Return);
            QCOMPARE(window.tabs->tabText(index), QString("Bench A"));
            QCOMPARE(receive->pageTitle, QString("Bench A"));

            window.port->setCurrentText("loop://");
            window.connectionAction();
            QVERIFY(receive->channel);
            QCOMPARE(window.tabs->tabText(pageIndex(&window, receive)), QString("Bench A"));
            QCOMPARE(receive->channel->name, QString("loop://"));
            window.applyTheme(true);
            QCOMPARE(window.tabs->tabText(pageIndex(&window, receive)), QString("Bench A"));

            auto detached = window.detachConnection(pageIndex(&window, receive), QPoint(200, 180));
            QVERIFY(detached);
            QCOMPARE(detached->tabs->tabText(pageIndex(detached, receive)), QString("Bench A"));
            QCOMPARE(detached->windowTitle(), QString("Bench A · Serial Workbench"));
            detached->close();
            QCOMPARE(window.tabs->tabText(pageIndex(&window, receive)), QString("Bench A"));
            window.tabs->setCurrentIndex(pageIndex(&window, receive));
            window.saveSettings();
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(QJsonDocument::fromJson(file.readAll()).object()["appearance"].toObject()["tab_title"].toString(), QString("Bench A"));
            window.close();
        }
        sw::Window restored(&engine, &mcp, path);
        QCOMPARE(restored.tabs->tabText(0), QString("Bench A"));
        restored.close();
    }

    void customWidgetTabTitleRestores()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.json");
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        {
            sw::Window window(&engine, &mcp, path);
            window.show();
            auto wave = window.newInstrument("waveform");
            window.activateWindow();
            window.tabs->setFocus();
            QApplication::processEvents();
            QTest::keyClick(window.tabs, Qt::Key_F2);
            QTRY_VERIFY(window.tabs->findChild<QLineEdit *>("tabTitleEditor") != nullptr);
            auto editor = window.tabs->findChild<QLineEdit *>("tabTitleEditor");
            QVERIFY(editor);
            QVERIFY(editor->hasFocus());
            QTest::keyClicks(editor, "Motor RPM");
            QCOMPARE(editor->text(), QString("Motor RPM"));
            editor->clearFocus();
            QCOMPARE(wave->pageTitle, QString("Motor RPM"));
            QTRY_COMPARE(window.tabs->tabText(pageIndex(&window, wave)), QString("Motor RPM"));
            window.saveSettings();
            window.close();
        }
        sw::Window restored(&engine, &mcp, path);
        auto renamed = std::find_if(restored.panes.begin(), restored.panes.end(), [](auto pane)
        {
            return pane->isInstrument() && pane->pageTitle == "Motor RPM";
        });
        QVERIFY(renamed != restored.panes.end());
        QCOMPARE(restored.tabs->tabText(pageIndex(&restored, *renamed)), QString("Motor RPM"));
        restored.close();
    }

    void serialReconfiguration()
    {
        sw::Engine engine;
        sw::Config config;
        QVERIFY_EXCEPTION_THROWN(config.validate(), std::runtime_error);
        config.port = "loop://";
        auto channel = engine.open(config);
        QCOMPARE(channel->state, "connected");
        channel->send("hello");
        QTRY_COMPARE(channel->rx, 5);
        config.baudrate = 1200;
        QCOMPARE(engine.open(config), channel);
        QCOMPARE(channel->appliedBaud, 1200);
        QCOMPARE(channel->config.baudrate, 1200);
        channel->close();
        QCOMPARE(engine.open(config), channel);
        QCOMPARE(channel->rx, 5);
        QCOMPARE(sw::Engine::encode("AA 10", true, "utf-8", "none"), QByteArray::fromHex("AA10"));
        QVERIFY_EXCEPTION_THROWN(sw::Engine::encode("GG", true, "utf-8", "none"), std::runtime_error);
    }
    void networkRoundTrip()
    {
        sw::Engine engine;
        sw::Config config;
        config.kind = "tcp-server";
        config.bindPort = 0;
        auto server = engine.open(config);
        QCOMPARE(server->state, "listening");
        config.kind = "tcp-client";
        config.networkPort = server->listener->serverPort();
        auto client = engine.open(config);
        QTRY_COMPARE(client->state, "connected");
        QTRY_COMPARE(server->peers.size(), 1);
        client->send("round trip");
        QTRY_COMPARE(server->rx, 10);
        server->send("reply", server->peers.firstKey());
        QTRY_COMPARE(client->rx, 5);
        config.kind = "udp";
        auto udp = engine.open(config);
        QUdpSocket socket;
        socket.writeDatagram("datagram", QHostAddress::LocalHost, udp->udp->localPort());
        QTRY_COMPARE(udp->rx, 8);
    }
    void strictWaveFrames_data()
    {
        QTest::addColumn<QByteArray>("frame");
        QTest::addColumn<QJsonObject>("expected");
        QTest::addColumn<bool>("malformed");

        QTest::newRow("mixed-numeric-types")
            << QByteArray("@wave count=4294967295,temperature=-25.50,precision=1.2345678901234567\r\n")
            << QJsonObject {{"count", 4294967295.0}, {"temperature", -25.5}, {"precision", 1.2345678901234567}} << false;
        QTest::newRow("scientific-and-spacing")
            << QByteArray("@wave  _count = +12 , small=.125, power=-1.25e-3, whole=3.\t\n")
            << QJsonObject {{"_count", 12}, {"small", 0.125}, {"power", -0.00125}, {"whole", 3}} << false;
        QTest::newRow("chinese-names")
            << QString("@wave 温度=25.5,电压=3.3\r").toUtf8()
            << QJsonObject {{"温度", 25.5}, {"电压", 3.3}} << false;
        QTest::newRow("maximum-name")
            << ("@wave " + QByteArray(64, 'a') + "=1\n")
            << QJsonObject {{QString(64, 'a'), 1}} << false;

        QStringList fields;
        QJsonObject values;
        for (int index = 0; index < 16; ++index)
        {
            const QString name = "ch" + QString::number(index);
            fields.append(name + "=" + QString::number(index));
            values.insert(name, index);
        }
        QTest::newRow("sixteen-channels") << ("@wave " + fields.join(',') + "\n").toUtf8() << values << false;
        QTest::newRow("too-many-channels")
            << ("@wave " + fields.join(',') + ",ch16=16\n").toUtf8() << QJsonObject() << true;

        const auto ignored = [](const char *name, const QByteArray &frame)
        {
            QTest::newRow(name) << frame << QJsonObject() << false;
        };
        ignored("plain-integer", "123\n");
        ignored("plain-csv", "1,2,3\n");
        ignored("plain-named", "temperature=25,voltage=3.3\n");
        ignored("json-object", "{\"temperature\":25,\"voltage\":3.3}\n");
        ignored("json-array", "[1,2,3]\n");
        ignored("prefix-inside-log", "I (120) device: @wave count=1\n");
        ignored("indented-prefix", " @wave count=1\n");
        ignored("wrong-case", "@WAVE count=1\n");
        ignored("wrong-separator", "@wave\tcount=1\n");

        const auto rejected = [](const char *name, const QByteArray &frame)
        {
            QTest::newRow(name) << frame << QJsonObject() << true;
        };
        rejected("empty-frame", "@wave \n");
        rejected("unnamed-csv", "@wave 1,2,3\n");
        rejected("prefixed-json", "@wave {\"count\":1}\n");
        rejected("duplicate-name", "@wave count=1,count=2\n");
        rejected("too-long-name", "@wave " + QByteArray(65, 'a') + "=1\n");
        rejected("numeric-name", "@wave 1count=1\n");
        rejected("trailing-comma", "@wave count=1,\n");
        rejected("empty-field", "@wave count=1,,value=2\n");
        rejected("colon-assignment", "@wave count:1\n");
        rejected("semicolon-separator", "@wave count=1;value=2\n");
        rejected("hexadecimal-value", "@wave count=0x10\n");
        rejected("nan-value", "@wave count=1,value=NaN\n");
        rejected("infinite-value", "@wave value=inf\n");
        rejected("overflow-value", "@wave value=1e999\n");
        rejected("unit-in-value", "@wave voltage=3.3V\n");
        rejected("log-after-value", "@wave count=1 device running\n");
        rejected("invalid-utf8-name", "@wave " + QByteArray::fromHex("FF") + "=1\n");
    }

    void strictWaveFrames()
    {
        QFETCH(QByteArray, frame);
        QFETCH(QJsonObject, expected);
        QFETCH(bool, malformed);

        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->receive(frame);
        const auto waveform = channel->waveform();
        QCOMPARE(waveform["protocol"].toString(), "wave/1");
        const auto source = waveform["sources"].toObject()["default"].toObject();
        const auto samples = source["samples"].toArray();
        QCOMPARE(samples.size(), expected.isEmpty() ? 0 : 1);
        QCOMPARE(source["rejected_lines"].toInt(), malformed ? 1 : 0);
        if (!expected.isEmpty())
        {
            QCOMPARE(samples.first().toObject()["values"].toObject(), expected);
        }
        const auto raw = engine.journal.read(0)["events"].toArray().last().toObject();
        QCOMPARE(QByteArray::fromHex(raw["hex"].toString().toLatin1()), frame);
    }

    void strictWaveFragmentationAndRecovery()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->receive("@wa", "peer-a");
        channel->receive("@wave temperature=30,count=2\r\n", "peer-b");
        const auto bytes = QString("ve count=1,温度=25.5\r\n").toUtf8();
        for (const char byte : bytes) { channel->receive(QByteArray(1, byte), "peer-a"); }
        auto sources = channel->waveform()["sources"].toObject();
        QCOMPARE(sources["peer-a"].toObject()["total_samples"].toInt(), 1);
        QCOMPARE(sources["peer-b"].toObject()["total_samples"].toInt(), 1);
        QCOMPARE(sources["peer-a"].toObject()["samples"].toArray().first().toObject()["values"].toObject()["温度"].toDouble(), 25.5);
        channel->receive("@wave count=3");
        QCOMPARE(channel->sources["default"]->count, 0);
        channel->receive("\n");
        QCOMPARE(channel->sources["default"]->count, 1);

        channel->receive(QByteArray(9000, 'x'));
        QVERIFY(channel->sources["default"]->wavePending.size() <= 8192);
        channel->receive("@wave fake=99\n@wave recovered=4\n");
        QCOMPARE(channel->sources["default"]->count, 2);
        QCOMPARE(channel->sources["default"]->rejected, 0);

        channel->receive("@wave value=" + QByteArray(9000, '1'));
        QCOMPARE(channel->sources["default"]->rejected, 1);
        channel->receive(QByteArray(9000, '2'));
        channel->receive("\r\n@wave recovered=5\r\n");
        QCOMPARE(channel->sources["default"]->count, 3);
        QCOMPARE(channel->sources["default"]->rejected, 1);

        channel->receive("@wave value=" + QByteArray(9000, '1') + "\n@wave recovered=6\n");
        QCOMPARE(channel->sources["default"]->count, 4);
        QCOMPARE(channel->sources["default"]->rejected, 2);
    }

    void strictWaveChannelsAndLiveValues()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.port->setCurrentText("loop://");
        window.connectionAction();
        auto channel = window.active->channel;
        auto values = window.newInstrument("values", channel, false)->instrument;
        auto wave = window.newInstrument("waveform", channel, false)->instrument;
        channel->receive("@wave count=4294967295,precision=1.2345678901234567\n");
        channel->receive("@wave precision=2.5,count=4294967294\n");
        wave->tick(channel);
        QCOMPARE(wave->plot->series.size(), 2);
        QCOMPARE(wave->plot->series["count"].size(), 2);
        QCOMPARE(wave->plot->series["count"][0].y(), 4294967295.0);
        QCOMPARE(wave->plot->series["count"][1].y(), 4294967294.0);
        values->tick(channel);
        QCOMPARE(values->table->topLevelItemCount(), 2);
        QCOMPARE(values->table->topLevelItem(0)->text(0), "count");
        QCOMPARE(values->table->topLevelItem(0)->text(1), "4294967294");
        QCOMPARE(values->table->topLevelItem(1)->text(1), "2.5");
        channel->receive("@wave precision=1.2345678901234567\n");
        values->tick(channel);
        QCOMPARE(values->table->topLevelItem(0)->text(1).toDouble(), 1.2345678901234567);
    }

    void fragmentedDecoders()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->receive("@wave temp=24,");
        channel->receive("voltage=3.3\n");
        QCOMPARE(channel->waveform()["sources"].toObject()["default"].toObject()["samples"].toArray().size(), 1);
        channel->configureParser("auto");
        channel->receive("{\"name\":\"");
        const auto chinese = QString("中文").toUtf8();
        channel->receive(chinese.left(2));
        channel->receive(chinese.mid(2) + "\"}\n<root><x>1</x></root>\n");
        QCOMPARE(channel->structured()["sources"].toObject()["default"].toObject()["records"].toArray().size(), 2);
        channel->configureParser("json", "gbk");
        const auto encoded = QTextCodec::codecForName("GBK")->fromUnicode("{\"gbk\":\"中文\"}");
        channel->receive(encoded.left(9));
        channel->receive(encoded.mid(9));
        const auto record = channel->structured()["sources"].toObject()["default"].toObject()["records"].toArray().last().toObject();
        QCOMPARE(record["value"].toObject()["gbk"].toString(), "中文");
    }
    void compactSerialFieldsAndIntegratedTabs()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.resizeParameters(220);
        QApplication::processEvents();
        window.flow->setCurrentText("xonxoff");
        QStyleOptionComboBox option;
        option.initFrom(window.flow);
        option.currentText = window.flow->currentText();
        const auto field = window.flow->style()->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, window.flow);
        QVERIFY(field.width() >= window.flow->fontMetrics().horizontalAdvance("xonxoff"));
        QCOMPARE(window.tabs->geometry().bottom(), window.tabHeader->rect().bottom());
        window.beginTabRename(0);
        auto editor = window.tabs->findChild<QLineEdit *>("tabTitleEditor");
        QVERIFY(editor);
        editor->clear();
        QApplication::processEvents();
        const auto shot = editor->grab().toImage();
        const auto bottom = shot.pixelColor(shot.width() / 2, shot.height() - 1);
        QVERIFY(bottom != window.theme.accent);
        window.close();
    }

    void layoutAndTabs()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        QVERIFY(mcp.start(0));
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        QTest::qWait(200);
        const int distance = window.plus->geometry().left() - window.tabs->geometry().right();
        QVERIFY2(distance >= 0 && distance < 16, qPrintable(QString::number(distance)));
        window.port->setCurrentText("COM9");
        QVERIFY(window.port->lineEdit()->width() > window.port->fontMetrics().horizontalAdvance("COM9") + 12);
        window.active->payload->setPlainText("first tab");
        auto first = window.active;
        auto second = window.newConnection();
        second->payload->setPlainText("second tab");
        window.tabs->setCurrentIndex(0);
        QCOMPARE(window.active, first);
        QCOMPARE(first->payload->toPlainText(), "first tab");
        window.tabs->moveTab(0, 1);
        QCOMPARE(window.active, first);
        QCOMPARE(window.stack->currentWidget(), first);
        QTest::mouseMove(window.gear, window.gear->rect().center());
        // Inject the event as well: headless runners may not own the desktop cursor.
        QEnterEvent enter(window.gear->rect().center(), window.gear->rect().center(), window.gear->mapToGlobal(window.gear->rect().center()));
        QApplication::sendEvent(window.gear, &enter);
        QTest::qWait(190);
        QVERIFY(window.gear->progress > 0.9);
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(window.gear, &leave);
        QTest::qWait(190);
        QVERIFY(window.gear->progress < 0.1);
        auto page = window.newInstrument("waveform", nullptr, false);
        QVERIFY(page->instrument->plot);
        window.tabs->setCurrentIndex(pageIndex(&window, first));
        QVERIFY(first->findChildren<QTabWidget *>().isEmpty());
        const auto original = first->splitter->sizes();
        first->splitter->setSizes({250, 250});
        QVERIFY(first->splitter->sizes() != original);
        window.openSettings();
        QVERIFY(window.settingsDialog->isVisible());
        window.settingsDialog->hide();
        window.close();
    }
    void logWrappingAndPersistence()
    {
        sw::LogView log;
        log.setFont(sw::Theme::defaultDataFont());
        log.resize(300, 180);
        log.show();
        sw::Journal journal;
        const QByteArray message = QString("长消息保留全部内容 %1 end-of-message\n")
            .arg(QString(240, 'A')).toUtf8();
        log.feed(journal.append("wrap-test", "rx", message));
        const auto original = log.toPlainText();
        QTRY_VERIFY(log.document()->firstBlock().layout()->lineCount() > 1);
        QCOMPARE(log.horizontalScrollBar()->maximum(), 0);
        const int narrowLines = log.document()->firstBlock().layout()->lineCount();
        log.resize(600, 180);
        QTRY_VERIFY(log.document()->firstBlock().layout()->lineCount() < narrowLines);

        QTextCursor selection(log.document());
        selection.select(QTextCursor::Document);
        log.setTextCursor(selection);
        log.setWrapping(false);
        QTRY_VERIFY(log.horizontalScrollBar()->maximum() > 0);
        QVERIFY(log.horizontalScrollBar()->isVisible());
        QCOMPARE(log.document()->firstBlock().layout()->lineCount(), 1);
        log.horizontalScrollBar()->setValue(log.horizontalScrollBar()->maximum());
        QVERIFY(log.horizontalScrollBar()->value() > 0);
        QCOMPARE(log.toPlainText(), original);
        QCOMPARE(log.textCursor().selectedText(), selection.selectedText());
        log.setWrapping(true);
        QTRY_COMPARE(log.horizontalScrollBar()->maximum(), 0);
        QVERIFY(!log.horizontalScrollBar()->isVisible());
        QCOMPARE(log.toPlainText(), original);

        QTemporaryDir directory;
        const auto path = directory.filePath("wrap-settings.json");
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        {
            sw::Window window(&engine, &mcp, path);
            window.show();
            auto pane = window.active;
            QVERIFY(pane->wrapLines->isChecked());
            QTest::mouseClick(pane->wrapLines, Qt::LeftButton);
            QCOMPARE(pane->log->lineWrapMode(), QTextEdit::NoWrap);
            pane->setTerminal(true);
            QVERIFY(pane->wrapLines->isHidden());
            pane->setTerminal(false);
            QVERIFY(!pane->wrapLines->isHidden());
            QVERIFY(!pane->wrapLines->isChecked());
            window.close();
        }
        {
            sw::Window restored(&engine, &mcp, path);
            QVERIFY(!restored.active->wrapLines->isChecked());
            QCOMPARE(restored.active->log->lineWrapMode(), QTextEdit::NoWrap);
            restored.close();
        }
    }

    void buttonFocusFeedback()
    {
        QWidget host;
        host.setStyleSheet(sw::Theme::make(false).stylesheet());
        auto layout = new QHBoxLayout(&host);
        auto action = new sw::IconButton("trash", "清空");
        auto textAction = new QPushButton("导出");
        auto toggle = new sw::IconButton("time", "时间戳");
        toggle->setCheckable(true);
        auto primary = new QPushButton("建立连接");
        primary->setObjectName("primary");
        layout->addWidget(action);
        layout->addWidget(textAction);
        layout->addWidget(toggle);
        layout->addWidget(primary);
        layout->addStretch();
        host.resize(420, 100);
        host.show();
        host.activateWindow();
        QTest::qWait(100);
        QTest::mouseMove(&host, QPoint(400, 80));
        action->setFocus(Qt::OtherFocusReason);
        QTest::qWait(180);
        const auto idle = action->grab().toImage();
        const auto textIdle = textAction->grab().toImage();
        const auto primaryIdle = primary->grab().toImage();

        QSignalSpy activated(action, &QAbstractButton::clicked);
        QTest::mouseClick(action, Qt::LeftButton);
        QTest::mouseMove(&host, QPoint(400, 80));
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(action, &leave);
        QTest::qWait(180);
        QCOMPARE(activated.count(), 1);
        QVERIFY(action->hasFocus());
        QVERIFY(!action->property("keyboardFocus").toBool());
        QCOMPARE(action->grab().toImage(), idle);

        QTest::keyClick(action, Qt::Key_Tab);
        QVERIFY(textAction->hasFocus());
        QVERIFY(textAction->property("keyboardFocus").toBool());
        QTest::keyClick(textAction, Qt::Key_Tab, Qt::ShiftModifier);
        QVERIFY(action->hasFocus());
        QVERIFY(action->property("keyboardFocus").toBool());
        QVERIFY(action->grab().toImage() != idle);
        QTest::keyClick(action, Qt::Key_Space);
        QCOMPARE(activated.count(), 2);

        QTest::mouseClick(textAction, Qt::LeftButton);
        QTest::mouseMove(&host, QPoint(400, 80));
        QVERIFY(!textAction->property("keyboardFocus").toBool());
        QApplication::sendEvent(textAction, &leave);
        QTest::qWait(180);
        QCOMPARE(textAction->grab().toImage(), textIdle);
        QTest::mouseClick(toggle, Qt::LeftButton);
        QTest::mouseMove(&host, QPoint(400, 80));
        QVERIFY(toggle->isChecked());
        QVERIFY(!toggle->property("keyboardFocus").toBool());
        QTest::keyClick(toggle, Qt::Key_Tab);
        QVERIFY(primary->hasFocus());
        QVERIFY(primary->grab().toImage() != primaryIdle);
        QTest::mouseClick(primary, Qt::LeftButton);
        QTest::mouseMove(&host, QPoint(400, 80));
        QApplication::sendEvent(primary, &leave);
        QTest::qWait(180);
        QCOMPARE(primary->grab().toImage(), primaryIdle);
    }
    void uiBaudAndPersistence()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.json");
        {
            sw::Engine engine;
            sw::McpServer mcp(&engine);
            mcp.start(0);
            sw::Window window(&engine, &mcp, path);
            window.show();
            QTest::qWait(180);
            window.port->setCurrentText("loop://");
            window.connectionAction();
            QVERIFY(window.active->channel);
            QCOMPARE(window.active->channel->state, "connected");
            window.baud->setCurrentText("1200");
            window.applyBaud();
            QCOMPARE(window.active->channel->appliedBaud, 1200);
            auto source = window.active->channel;
            window.newInstrument("waveform", source, false);
            window.newInstrument("json", source, false);
            window.active->setTerminal(true);
            window.active->setTerminal(false);
            QVERIFY(!window.active->modeButton->isChecked());
            window.applyTheme(true);
            engine.allowControl = true;
            window.close();
        }
        {
            sw::Engine engine;
            sw::McpServer mcp(&engine);
            mcp.start(0);
            sw::Window window(&engine, &mcp, path);
            QCOMPARE(window.port->currentText(), "NULL");
            QCOMPARE(window.baud->currentText(), "1200");
            QVERIFY(engine.allowControl);
            QVERIFY(window.theme.dark);
            QCOMPARE(window.panes.size(), 3);
            QCOMPARE(window.panes[1]->instrument->kind, "waveform");
            QCOMPARE(window.panes[2]->instrument->kind, "json");
            QCOMPARE(window.panes[1]->sourceKey, "serial:loop://");
            QVERIFY(!window.panes[1]->channel);
            QVERIFY(engine.channels.isEmpty());
        }
    }
    void toolbarAndPortUpdates()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        QJsonArray available;
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        QTest::qWait(200);
        QVERIFY(window.bits->isVisible());
        QVERIFY(window.flow->isVisible());
        QVERIFY(window.dtr->isVisible());
        auto pane = window.active;
        QTest::mouseClick(pane->timestamps, Qt::LeftButton);
        QVERIFY(!pane->log->timestamps);
        QTest::mouseClick(pane->hexDisplay, Qt::LeftButton);
        QVERIFY(pane->log->hex);
        auto search = qobject_cast<sw::SearchBox *>(pane->filter);
        QVERIFY(search);
        search->setText("device");
        QVERIFY(search->clearButton->isVisible());
        QVERIFY(qAbs(search->clearButton->geometry().center().y() - search->rect().center().y()) <= 1);
        QTest::mouseClick(search->clearButton, Qt::LeftButton);
        QVERIFY(search->text().isEmpty());
        QVERIFY(pane->log->filter.isEmpty());
        QTest::mouseClick(pane->modeButton, Qt::LeftButton);
        QVERIFY(pane->terminalMode);
        QVERIFY(pane->sender->isHidden());
        QSignalSpy transmitted(pane->terminal, &sw::TerminalView::transmit);
        QTest::keyClick(pane->terminal, Qt::Key_Insert);
        QVERIFY(!pane->terminal->blockCursor);
        QVERIFY(!pane->cursorShape->isChecked());
        QCOMPARE(transmitted.count(), 0);
        QTest::mouseClick(pane->cursorShape, Qt::LeftButton);
        QVERIFY(pane->terminal->blockCursor);
        QTest::mouseClick(pane->modeButton, Qt::LeftButton);
        QVERIFY(!pane->terminalMode);
        QVERIFY(pane->sender->isVisible());
        if (QGuiApplication::platformName() == "windows")
        {
            // ASCII protocol dumps must retain equal character cells.
            const QFontMetrics terminalMetrics(pane->terminal->font());
            const int cell = terminalMetrics.horizontalAdvance('M');
            for (int character = 32; character < 127; ++character)
            {
                QCOMPARE(terminalMetrics.horizontalAdvance(QChar(character)), cell);
            }
        }
        QVERIFY(pane->newline->itemText(3).contains("\\r\\n"));
        QVERIFY(pane->newline->itemText(3).contains("CRLF"));

        available = {QJsonObject {{"port", "COM44"}, {"description", "Test device"}}};
        window.portProvider = [&available] { return available; };
        window.refreshPorts();
        QVERIFY(window.port->findText("COM44") >= 0);
        window.port->setCurrentText("COM44");
        available = {QJsonObject {{"port", "COM45"}}};
        window.port->lineEdit()->setSelection(1, 3);
        window.portTimer.setInterval(30);
        QTRY_VERIFY(window.port->findText("COM45") >= 0);
        QCOMPARE(window.port->currentText(), "NULL");
        QCOMPARE(window.active->draft.port, QString("NULL"));
        QVERIFY(window.portDetails->isHidden());
        QVERIFY(window.port->findText("COM44") < 0);
        window.port->setCurrentText("loop://");
        available = {};
        window.refreshPorts();
        QCOMPARE(window.port->currentText(), "loop://");
        window.connectionAction();
        QVERIFY(window.active->channel);
        auto channel = window.active->channel;
        window.bits->setCurrentText("7");
        emit window.bits->activated(window.bits->currentIndex());
        QCOMPARE(channel->config.bytesize, 7);
        window.dtr->setChecked(!window.dtr->isChecked());
        QCOMPARE(channel->config.dtr, window.dtr->isChecked());
        pane->payload->setPlainText("line");
        pane->newline->setCurrentIndex(3);
        pane->sendCurrent();
        QTRY_COMPARE(channel->rx, 6);
        QCOMPARE(QByteArray::fromHex(engine.journal.read(0)["events"].toArray().last().toObject()["hex"].toString().toLatin1()), QByteArray("line\r\n"));
        window.close();
    }

    void livePortDiscovery()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        QJsonArray available {
            QJsonObject {{"port", "COM8"}, {"description", "USB UART"}},
            QJsonObject {{"port", "COM9"}}
        };
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.portProvider = [&available] { return available; };
        window.portTimer.setInterval(30);
        window.refreshPorts();
        window.show();
        window.port->setCurrentText("COM8");
        window.port->lineEdit()->setSelection(1, 2);
        QSignalSpy reset(window.port->model(), &QAbstractItemModel::modelReset);
        QSignalSpy inserted(window.port->model(), &QAbstractItemModel::rowsInserted);
        QSignalSpy removed(window.port->model(), &QAbstractItemModel::rowsRemoved);
        available[0] = QJsonObject {
            {"port", "COM8"}, {"description", "Updated device"}, {"manufacturer", "Silicon Labs"},
            {"vendor_id", 0x10c4}, {"product_id", 0xea60}, {"serial_number", "UART1234"},
            {"system_location", "test-device-path"}
        };
        window.refreshPorts();
        QCOMPARE(window.port->currentText(), "COM8");
        QCOMPARE(window.port->lineEdit()->selectedText(), "OM");
        const auto tooltip = window.port->itemData(1, Qt::ToolTipRole).toString();
        QVERIFY(tooltip.contains("Updated device"));
        QVERIFY(tooltip.contains("Silicon Labs"));
        QVERIFY(tooltip.contains("VID 10C4 · PID EA60"));
        QVERIFY(tooltip.contains("UART1234"));
        QVERIFY(tooltip.contains("test-device-path"));
        QVERIFY(window.portDetails->text().contains("VID 10C4 · PID EA60"));
        QVERIFY(window.portDetails->text().contains("UART1234"));
        QVERIFY(window.portDetails->isVisible());
        for (auto text : window.findChildren<QLabel *>())
        {
            QVERIFY(text->text() != "实时扫描");
        }
        QCOMPARE(reset.count(), 0);
        QCOMPARE(inserted.count(), 0);
        QCOMPARE(removed.count(), 0);

        available.removeAt(0);
        QTRY_COMPARE(window.port->currentText(), QString("NULL"));
        QCOMPARE(window.active->draft.port, QString("NULL"));
        QCOMPARE(window.port->findText("COM8"), -1);
        QCOMPARE(removed.count(), 1);

        window.port->setCurrentText("COM9");
        window.port->showPopup();
        QTRY_VERIFY(window.port->view()->isVisible());
        available.append(QJsonObject {{"port", "COM10"}});
        QTRY_VERIFY(window.port->findText("COM10") > 0);
        QCOMPARE(window.port->currentText(), "COM9");
        available.removeAt(0);
        QTRY_COMPARE(window.port->currentText(), QString("NULL"));
        QVERIFY(window.port->view()->isVisible());
        QCOMPARE(reset.count(), 0);
        window.port->hidePopup();

        // Mouse/keyboard editing must never erase NULL or alter a selected COM.
        for (const QString &selected : {QString("NULL"), QString("COM10")})
        {
            window.port->setCurrentIndex(window.port->findText(selected));
            QTest::mouseClick(window.port->lineEdit(), Qt::LeftButton);
            QTest::keyClick(window.port->lineEdit(), Qt::Key_A, Qt::ControlModifier);
            QTest::keyClick(window.port->lineEdit(), Qt::Key_Backspace);
            QTest::keyClick(window.port->lineEdit(), Qt::Key_Delete);
            QTest::keyClicks(window.port->lineEdit(), "COM99");
            QVERIFY(!window.port->testAttribute(Qt::WA_InputMethodEnabled));
            QVERIFY(!window.port->lineEdit()->testAttribute(Qt::WA_InputMethodEnabled));
            QCOMPARE(window.port->currentText(), selected);
        }
        available.append(QJsonObject {{"port", "COM11"}});
        window.refreshPorts();
        QCOMPARE(window.port->currentText(), "COM10");
        QCOMPARE(window.port->findText("COM99"), -1);
        window.port->showPopup();
        QTRY_VERIFY(window.port->view()->isVisible());
        const auto item = window.port->model()->index(window.port->findText("COM11"), 0);
        QTest::mouseClick(window.port->view()->viewport(), Qt::LeftButton, Qt::NoModifier,
            window.port->view()->visualRect(item).center());
        QCOMPARE(window.port->currentText(), QString("COM11"));

        window.port->setCurrentText("COM8");
        window.refreshPorts();
        QCOMPARE(window.port->currentText(), "NULL");
        sw::Config saved;
        saved.port = "COM8";
        window.loadForm(saved);
        QCOMPARE(window.port->currentText(), "NULL");

        // A lost device must not disable the existing connection's stop action.
        available.append(QJsonObject {{"port", "COM9"}, {"description", "Retained USB UART"}, {"hwid", "OLD-SERIAL"}});
        window.refreshPorts();
        sw::Config loopback;
        loopback.port = "loop://";
        auto channel = engine.open(loopback);
        channel->config.port = "COM9";
        window.tabs->setCurrentIndex(window.stack->indexOf(window.findPane(channel)));
        window.loadForm(channel->config);
        QCOMPARE(window.port->currentText(), "COM9");
        available = {};
        window.refreshPorts();
        QCOMPARE(window.port->currentText(), "COM9");
        QCOMPARE(window.port->findText("COM9"), -1);
        QCOMPARE(window.connectButton->text(), QString("停止重连"));
        QVERIFY(window.portDetails->text().contains("Retained USB UART"));
        QVERIFY(window.portDetails->text().contains("OLD-SERIAL"));
        QCOMPARE(channel->state, QString("reconnecting"));
        QVERIFY(channel->retryTimer.isActive());
        QVERIFY(channel->error.contains("COM9"));
        window.connectionAction();
        QCOMPARE(channel->state, "closed");
        QVERIFY(!channel->retryTimer.isActive());
        QCOMPARE(channel->config.port, QString("COM9"));
        window.close();
    }

    void serialRecoveryIsQuiet()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        // Simulate discovery without opening any physical device.
        QJsonArray available {QJsonObject {{"port", "COM65535"}}};
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.portProvider = [&available] { return available; };
        window.refreshPorts();
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        channel->config.port = "COM65535";
        window.tabs->setCurrentIndex(window.stack->indexOf(window.findPane(channel)));
        window.loadForm(channel->config);
        auto pane = window.active;
        window.show();
        channel->receive("preserved data\r\n");
        const auto identity = channel->id;
        const auto before = engine.journal.sequence;

        available = {};
        window.refreshPorts();
        QCOMPARE(engine.journal.sequence, before + 1);
        const auto reason = channel->error;
        QTRY_VERIFY(channel->attempts >= 3);
        QCOMPARE(engine.journal.sequence, before + 1);
        QCOMPARE(channel->error, reason);
        QCOMPARE(channel->state, QString("reconnecting"));
        QCOMPARE(window.port->currentText(), QString("COM65535"));
        QCOMPARE(window.connectButton->text(), QString("停止重连"));
        QVERIFY(window.serialInfo->text().contains("等待设备恢复"));
        pane->tick();
        QCOMPARE(pane->log->toPlainText().count("已断开"), 1);
        QVERIFY(pane->log->toPlainText().contains("preserved data"));

        window.newConnection();
        window.tabs->setCurrentIndex(window.stack->indexOf(pane));
        window.loadForm(channel->config);
        QCOMPARE(window.port->currentText(), QString("COM65535"));
        QCOMPARE(window.connectButton->text(), QString("停止重连"));
        window.refreshPorts();
        QCOMPARE(window.port->currentText(), QString("COM65535"));

        // A successful recovery retains identity/history and re-arms notification.
        channel->config.port = "loop://";
        channel->open();
        QCOMPARE(channel->state, QString("connected"));
        QCOMPARE(channel->id, identity);
        QVERIFY(!channel->retryTimer.isActive());
        QCOMPARE(engine.journal.sequence, before + 2);
        QVERIFY(engine.journal.read(before + 1)["events"].toArray().first().toObject()["stream_reset"].toBool());
        channel->config.port = "COM65535";
        window.loadForm(channel->config);
        window.refreshPorts();
        QCOMPARE(channel->state, QString("reconnecting"));
        QCOMPARE(engine.journal.sequence, before + 3);
        window.connectionAction();
        QCOMPARE(channel->state, QString("closed"));
        QVERIFY(!channel->retryTimer.isActive());
        const auto stopped = engine.journal.sequence;
        QTest::qWait(400);
        QCOMPARE(engine.journal.sequence, stopped);
        window.refreshPorts();
        QCOMPARE(window.port->currentText(), QString("NULL"));
        window.close();
    }

    void detachableConnectionsAndPages()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        QTest::qWait(180);
        window.port->setCurrentText("loop://");
        window.connectionAction();
        auto pane = window.active;
        auto channel = pane->channel;
        const auto id = channel->id;
        pane->payload->setPlainText("preserved draft");
        auto wavePage = window.newInstrument("waveform", channel, false);
        auto parsePage = window.newInstrument("json", channel, false);
        auto wave = wavePage->instrument;
        auto structured = parsePage->instrument;
        auto detached = window.detachConnection(pageIndex(&window, pane), QPoint(180, 160));
        QVERIFY(detached);
        QTest::qWait(100);
        QCOMPARE(detached->active, pane);
        QCOMPARE(pane->window, detached);
        QCOMPARE(channel->id, id);
        QCOMPARE(engine.channels.size(), 1);
        window.syncConnections();
        QCOMPARE(window.findPane(channel), pane);
        auto waveWindow = window.detachConnection(pageIndex(&window, wavePage), QPoint(250, 200));
        auto parseWindow = window.detachConnection(pageIndex(&window, parsePage), QPoint(300, 220));
        QVERIFY(waveWindow && parseWindow);
        detached->newConnection();
        QVERIFY(pane->isHidden());
        QVERIFY2(wave->isVisible(), "Detached waveform must update while its receive page is hidden");
        QVERIFY(parseWindow->isVisible());
        channel->receive("@wave temp=23,voltage=3.3\n{\"answer\":42}\n");
        QTRY_VERIFY(!wave->plot->series.isEmpty());
        QTRY_VERIFY(structured->table->topLevelItemCount() > 0);
        QCOMPARE(pane->payload->toPlainText(), "preserved draft");
        detached->tabs->setCurrentIndex(pageIndex(detached, pane));
        QTRY_VERIFY(pane->log->toPlainText().contains("answer"));
        window.saveSettings();
        QFile savedFile(directory.filePath("settings.json"));
        QVERIFY(savedFile.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(savedFile.readAll()).object()["widget_pages"].toArray().size(), 2);
#ifdef Q_OS_WIN
        if (QGuiApplication::platformName() == "windows")
        {
            for (auto host : QList<QWidget *> {detached, waveWindow, parseWindow})
            {
                const auto style = GetWindowLongPtr(reinterpret_cast<HWND>(host->winId()), GWL_STYLE);
                QVERIFY(style & WS_MAXIMIZEBOX);
                QVERIFY(style & WS_THICKFRAME);
                QVERIFY(style & WS_CAPTION);
            }
        }
#endif
        waveWindow->close();
        QCOMPARE(wavePage->window, &window);
        const int waveIndex = pageIndex(&window, wavePage);
        QVERIFY(window.tabs->tabButton(waveIndex, QTabBar::RightSide));
        window.tabs->moveTab(waveIndex, 0);
        QCOMPARE(window.tabs->tabData(0).value<QObject *>(), wavePage);
        window.closeTab(0);
        QCOMPARE(channel->state, "connected");
        QCOMPARE(engine.channels.size(), 1);
        detached->close();
        QCOMPARE(pane->window, &window);
        QCOMPARE(channel->state, "connected");
        parseWindow->close();
        window.tabs->setCurrentIndex(pageIndex(&window, pane));
        window.resize(480, 640);
        QTest::qWait(100);
        QCOMPARE(window.width(), 480);
        QVERIFY(window.stack->width() <= 426);
        const int gap = window.plus->geometry().left() - window.tabs->geometry().right();
        QVERIFY(gap >= 0 && gap < 16);
        QVERIFY(window.tabs->rect().contains(window.tabs->tabRect(window.tabs->currentIndex()).center()));
        auto toolbar = pane->monitorPage->findChild<QWidget *>("monitorToolbar");
        QVERIFY(toolbar);
        const int lastToolEdge = pane->exportButton->mapTo(toolbar, QPoint()).x() + pane->exportButton->width();
        QVERIFY(lastToolEdge <= toolbar->width());
        const auto captureDirectory = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
        if (!captureDirectory.isEmpty())
        {
            QVERIFY(window.grab().save(QDir(captureDirectory).filePath("workspace-narrow.png")));
        }
        QVERIFY(pane->sendButton->mapTo(pane->monitorPage, QPoint()).x() + pane->sendButton->width() <= pane->monitorPage->width());
        QVERIFY(pane->sendButton->parentWidget()->rect().contains(pane->sendButton->geometry()));
        window.close();
        QCOMPARE(channel->state, "closed");
    }

    void detachedDraftAndShutdown()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.portProvider = [] { return QJsonArray {QJsonObject {{"port", "COM77"}}}; };
        window.refreshPorts();
        window.port->setCurrentText("COM77");
        window.baud->setCurrentText("230400");
        auto draft = window.active;
        draft->payload->setPlainText("unsent content");
        auto detached = window.detachConnection(0, QPoint(200, 180));
        QCOMPARE(detached->port->currentText(), "COM77");
        QCOMPARE(detached->baud->currentText(), "230400");
        QCOMPARE(detached->active->payload->toPlainText(), "unsent content");
        detached->port->setCurrentText("loop://");
        detached->connectionAction();
        QCOMPARE(engine.channels.size(), 1);
        QCOMPARE(draft->window, detached);
        QCOMPARE(detached->panes.size(), 1);
        QVERIFY(!window.active->channel);
        auto waveform = detached->newInstrument("waveform", draft->channel, false);
        auto waveWindow = detached->detachConnection(pageIndex(detached, waveform), QPoint(250, 180));
        QPointer<sw::Window> guarded(waveWindow);
        window.close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(guarded.isNull());
        QFile file(directory.filePath("settings.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto saved = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(saved["widget_pages"].toArray().size(), 1);
    }

    void newPageMenuAndSources()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        QTest::qWait(150);
        auto receive = window.active;
        QTest::mouseClick(window.plus, Qt::LeftButton);
        auto menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        QVERIFY(menu);
        const auto captureDirectory = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
        if (!captureDirectory.isEmpty())
        {
            QCoreApplication::processEvents();
            QVERIFY(menu->grab().save(QDir(captureDirectory).filePath("new-page-menu.png")));
            menu->setActiveAction(menu->actions().first());
            QCoreApplication::processEvents();
            QVERIFY(menu->grab().save(QDir(captureDirectory).filePath("new-page-menu-hover.png")));
        }
        QCOMPARE(window.tabs->count(), 1);
        auto waveAction = menu->findChild<QAction *>("new_waveform");
        QVERIFY(waveAction);
        waveAction->trigger();
        menu->close();
        auto wavePage = window.active;
        QVERIFY(wavePage->instrument);
        QCOMPARE(window.tabs->count(), 2);
        QCOMPARE(window.stack->currentWidget(), wavePage);
        QVERIFY(window.sidebar->isVisible());
        QVERIFY(window.parameterToggle->isEnabled());
        QCOMPARE(window.parameterPane, receive);
        QVERIFY(wavePage->instrument->source->isVisible());
        QVERIFY(engine.channels.isEmpty());
        QVERIFY(!wavePage->channel);

        window.tabs->setCurrentIndex(pageIndex(&window, receive));
        QVERIFY(window.sidebar->isVisible());
        window.port->setCurrentText("loop://");
        window.connectionAction();
        auto first = receive->channel;
        auto picker = wavePage->instrument->source;
        const int sourceIndex = picker->findData(first->config.key());
        QVERIFY(sourceIndex > 0);
        picker->setCurrentIndex(sourceIndex);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, sourceIndex));
        QCOMPARE(wavePage->channel, first);
        QCOMPARE(engine.channels.size(), 1);
        window.tabs->setCurrentIndex(pageIndex(&window, wavePage));
        first->receive("@wave temp=23\n");
        QTRY_VERIFY(!wavePage->instrument->plot->series.isEmpty());
        QCOMPARE(window.parameterPane, receive);
        window.connectionAction();
        QCOMPARE(first->state, "closed");
        QCOMPARE(window.active, wavePage);
        window.connectionAction();
        QCOMPARE(first->state, "connected");
        QCOMPARE(window.active, wavePage);
        window.baud->setCurrentText("1200");
        window.applyBaud();
        QCOMPARE(first->appliedBaud, 1200);

        sw::Config otherConfig;
        otherConfig.kind = "demo";
        otherConfig.name = "第二数据源";
        auto other = engine.open(otherConfig);
        const int otherIndex = picker->findData(other->config.key());
        picker->setCurrentIndex(otherIndex);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, otherIndex));
        QCOMPARE(wavePage->channel, other);
        QCOMPARE(window.parameterPane, window.findPane(other));
        QCOMPARE(window.findPane(first), receive);
        other->receive("@wave voltage=3.3\n");
        QTRY_VERIFY(wavePage->instrument->plot->series.contains("voltage"));
        QVERIFY(!wavePage->instrument->plot->series.contains("temp"));
        engine.remove(other->id);
        QVERIFY(!wavePage->channel);
        QCOMPARE(wavePage->sourceKey, otherConfig.key());
        QVERIFY(!window.parameterPane);
        QVERIFY(!window.configStack->isEnabled());
        QCOMPARE(first->state, "connected");
        QVERIFY(picker->currentText().contains("未建立连接"));
        other = engine.open(otherConfig);
        QCOMPARE(wavePage->channel, other);
        window.closeTab(pageIndex(&window, wavePage));
        QCOMPARE(engine.channels.size(), 2);
        QCOMPARE(first->state, "connected");
        QCOMPARE(other->state, "connected");

        auto newMenu = window.newPageMenu();
        const int count = window.tabs->count();
        newMenu->findChild<QAction *>("newReceivePage")->trigger();
        QCOMPARE(window.tabs->count(), count + 1);
        QVERIFY(!window.active->isInstrument());
        QVERIFY(!window.active->channel);
        delete newMenu;
        window.close();
    }

    void independentWidgetPages()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.port->setCurrentText("loop://");
        window.connectionAction();
        auto receive = window.active;
        auto channel = receive->channel;
        auto json = window.newInstrument("json", channel);

        json->instrument->freeze->setChecked(true);
        auto detached = window.detachConnection(pageIndex(&window, json), QPoint(200, 180));
        auto xml = window.newInstrument("xml", channel);
        QVERIFY(json->pageTitle != xml->pageTitle);
        QCOMPARE(channel->parserMode, "auto");
        channel->receive("{\"temperature\":24}\n<device><voltage>3.3</voltage></device>\n");
        QTRY_COMPARE(xml->instrument->table->topLevelItemCount(), 1);
        QCOMPARE(json->instrument->table->topLevelItemCount(), 0);
        json->instrument->freeze->setChecked(false);
        QTRY_COMPARE(json->instrument->table->topLevelItemCount(), 1);
        QCOMPARE(json->instrument->table->topLevelItem(0)->text(1), "JSON");
        QCOMPARE(xml->instrument->table->topLevelItem(0)->text(1), "XML");
        detached->close();
        window.closeTab(pageIndex(&window, json));
        QCOMPARE(channel->state, "connected");
        QCOMPARE(channel->parserMode, "auto");
        channel->receive("<device><voltage>3.4</voltage></device>\n");
        QTRY_COMPARE(xml->instrument->table->topLevelItemCount(), 2);
        window.closeTab(pageIndex(&window, receive));
        QVERIFY(engine.channels.isEmpty());
        QVERIFY(!xml->channel);
        QCOMPARE(window.tabs->count(), 1);
        QCOMPARE(window.stack->currentWidget(), xml);
        QVERIFY(!window.parameterPane);
        QVERIFY(!window.configStack->isEnabled());
        QCOMPARE(window.connectButton->text(), QString("新建收发页面"));
        window.connectionAction();
        QVERIFY(!window.active->isInstrument());
        QCOMPARE(window.parameterPane, window.active);
        QVERIFY(window.configStack->isEnabled());
        window.closeTab(pageIndex(&window, window.active));
        QCOMPARE(window.stack->currentWidget(), xml);
        window.closeTab(0);
        QCOMPARE(window.tabs->count(), 1);
        QVERIFY(!window.active->isInstrument());
        QVERIFY(!window.active->channel);
        window.close();
    }

    void legacyInstrumentMigration()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"connection":{"kind":"serial","port":"COM77"},"appearance":{"instruments":[{"kind":"waveform","points":1500},{"kind":"waveform","points":2000},{"kind":"structured","parser":2}]},"views":{"serial:com88":{"instruments":[{"kind":"values","frozen":true}]}}})");
        file.close();
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, path);
        QCOMPARE(window.tabs->count(), 5);
        QVERIFY(!window.active->isInstrument());
        QCOMPARE(window.panes[1]->instrument->points->value(), 1500);
        QCOMPARE(window.panes[2]->instrument->points->value(), 2000);
        QVERIFY(window.panes[1]->pageTitle != window.panes[2]->pageTitle);
        QCOMPARE(window.panes[3]->instrument->kind, "xml");
        QCOMPARE(window.panes[1]->sourceKey, "serial:com77");
        QCOMPARE(window.panes[4]->sourceKey, "serial:com88");
        QCOMPARE(window.panes[4]->instrument->kind, "waveform");
        QVERIFY(window.panes[4]->instrument->valuesToggle->isChecked());
        QVERIFY(window.panes[4]->instrument->freeze->isChecked());
        QVERIFY(engine.channels.isEmpty());
        window.saveSettings();
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto saved = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(saved["widget_pages"].toArray().size(), 4);
        QVERIFY(!saved["appearance"].toObject().contains("instruments"));
        window.close();
    }

    void tabLabelFollowsPaintRectangle()
    {
        sw::TearTabBar bar;
        bar.setStyleSheet(sw::Theme::make(false).stylesheet());
        bar.resize(700, 48);
        bar.addTab(sw::icon("wave", QColor("#668077")), "数值波形");
        bar.addTab("JSON / XML");
        bar.setCurrentIndex(1);
        bar.show();
        QTest::qWait(30);
        QStyleOptionTab option;
        bar.initStyleOption(&option, 0);
        const auto render = [&](const QStyleOptionTab &tab)
        {
            QImage image(700, 80, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            painter.setFont(bar.font());
            bar.style()->drawControl(QStyle::CE_TabBarTab, &tab, &painter, &bar);
            return image.copy(tab.rect);
        };
        const QImage stationary = render(option);
        option.rect.translate(60, 0);
        QCOMPARE(render(option), stationary);
    }

    void widgetSidebarTracksSource()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.port->setCurrentText("loop://");
        window.connectionAction();
        auto first = window.active;
        auto second = window.newConnection();
        window.protocol->setCurrentIndex(3);
        window.bindPort->setValue(0);
        window.connectionAction();
        auto page = window.newInstrument("json", first->channel);
        QCOMPARE(window.parameterPane, first);
        QCOMPARE(window.protocol->currentIndex(), 0);
        QCOMPARE(window.port->currentText(), QString("loop://"));
        auto picker = page->instrument->source;
        const int index = picker->findData(second->channel->config.key());
        QVERIFY(index > 0);
        picker->setCurrentIndex(index);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, index));
        QCOMPARE(window.parameterPane, second);
        QCOMPARE(window.protocol->currentIndex(), 3);
        window.parameterToggle->setChecked(false);
        QVERIFY(!window.findChild<QScrollArea *>("parameterScroll")->isVisible());
        window.tabs->setCurrentIndex(pageIndex(&window, first));
        window.tabs->setCurrentIndex(pageIndex(&window, page));
        QVERIFY(!window.findChild<QScrollArea *>("parameterScroll")->isVisible());
        window.parameterToggle->setChecked(true);
        QVERIFY(window.findChild<QScrollArea *>("parameterScroll")->isVisible());
        auto detached = window.detachConnection(pageIndex(&window, page), QPoint(180, 180));
        QCOMPARE(detached->parameterPane, second);
        QCOMPARE(detached->protocol->currentIndex(), 3);
        const int firstIndex = picker->findData(first->channel->config.key());
        picker->setCurrentIndex(firstIndex);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, firstIndex));
        window.tabs->setCurrentIndex(pageIndex(&window, first));
        detached->baud->setCurrentText("9600");
        detached->applyBaud();
        QCOMPARE(first->channel->appliedBaud, 9600);
        QCOMPARE(window.baud->currentText(), QString("9600"));
        picker->setCurrentIndex(index);
        QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, index));
        detached->connectionAction();
        QCOMPARE(second->channel->state, "closed");
        QCOMPARE(detached->active, page);
        detached->close();
        window.close();
    }

    void tearGestureAndSettings()
    {
        sw::DetachableTabs tabs;
        tabs.resize(700, 500);
        auto first = new QWidget();
        auto second = new QWidget();
        tabs.addTab(first, "First");
        tabs.addTab(second, "Second");
        tabs.show();
        auto bar = qobject_cast<sw::TearTabBar *>(tabs.tabBar());
        QVERIFY(bar);
        QSignalSpy requested(bar, &sw::TearTabBar::tearRequested);
        // Isolate the gesture from the native OS move loop in an automated test.
        disconnect(bar, &sw::TearTabBar::tearRequested, &tabs, nullptr);
        const QPoint start = bar->tabRect(1).center();
        QTest::mousePress(bar, Qt::LeftButton, Qt::NoModifier, start);
        const QPoint end = start + QPoint(0, 90);
        QMouseEvent move(QEvent::MouseMove, QPointF(end), QPointF(bar->mapToGlobal(end)), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(bar, &move);
        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested.first()[0].toInt(), 1);
        QTest::mouseRelease(bar, Qt::LeftButton, Qt::NoModifier, end);
        tabs.tabBar()->moveTab(1, 0);
        auto floating = tabs.detachPage(0, QPoint(200, 200));
        QCOMPARE(floating->centralWidget(), second);
        floating->close();
        QCOMPARE(tabs.widget(0), second);
        tabs.close();

        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.openSettings();
        const auto allTabs = window.settingsDialog->findChildren<sw::DetachableTabs *>();
        QCOMPARE(allTabs.size(), 2);
        for (auto settingsTabs : allTabs)
        {
            auto page = settingsTabs->widget(0);
            auto floatingSettings = settingsTabs->detachPage(0, QPoint(180, 180));
            QCOMPARE(floatingSettings->centralWidget(), page);
            window.applyTheme(!window.theme.dark);
            QCOMPARE(floatingSettings->palette().color(QPalette::Window), window.theme.background);
            QCOMPARE(floatingSettings->styleSheet(), window.styleSheet());
            floatingSettings->close();
            QCOMPARE(settingsTabs->widget(0), page);
        }
        window.close();
    }

    void parameterSidebarResizesAndRestores()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        const auto settingsPath = directory.filePath("settings.json");
        int savedWidth = 0;
        {
            sw::Window window(&engine, &mcp, settingsPath);
            window.show();
            QTest::qWait(50);
            auto scroll = window.findChild<QScrollArea *>("parameterScroll");
            auto grip = window.parameterGrip;
            QVERIFY(scroll && grip);
            QVERIFY(grip->isVisible());
            QCOMPARE(scroll->width(), 246);
            const int workspaceWidth = window.stack->width();
            const auto center = grip->rect().center();
            const auto global = grip->mapToGlobal(center);
            QTest::mousePress(grip, Qt::LeftButton, {}, center);
            QMouseEvent move(QEvent::MouseMove, QPointF(center + QPoint(120, 0)),
                QPointF(global + QPoint(120, 0)), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(grip, &move);
            QTest::mouseRelease(grip, Qt::LeftButton, {}, grip->rect().center());
            QTRY_COMPARE(scroll->width(), 366);
            QCOMPARE(window.parameterWidth, 366);
            QVERIFY(window.stack->width() < workspaceWidth);
            QVERIFY(window.sidebar->width() <= scroll->viewport()->width());
            QVERIFY(window.port->width() > 214);
            window.parameterToggle->setChecked(false);
            QVERIFY(!grip->isVisible());
            window.parameterToggle->setChecked(true);
            QVERIFY(grip->isVisible());
            QCOMPARE(scroll->width(), 366);

            window.resizeParameters(700);
            window.resize(900, 900);
            QTest::qWait(50);
            QCOMPARE(window.parameterWidth, 700);
            QVERIFY(scroll->width() < 700);
            QVERIFY(window.stack->width() >= 320);
            window.resize(480, 640);
            QTest::qWait(50);
            QCOMPARE(window.stack->width(), 426);
            window.parameterToggle->setChecked(true);
            QVERIFY(grip->isVisible());
            QVERIFY(grip->mapTo(window.centralWidget(), grip->rect().bottomRight()).x() < window.centralWidget()->width());
            window.resizeParameters(300);
            QCOMPARE(scroll->width(), 300);
            QCOMPARE(grip->mapTo(window.centralWidget(), QPoint()).x(), 54 + 300);
            QTest::mouseDClick(grip, Qt::LeftButton);
            QCOMPARE(window.parameterWidth, 246);
            QCOMPARE(scroll->width(), 246);
            window.resize(1400, 900);
            QTest::qWait(50);
            QVERIFY(grip->isVisible());
            QTest::keyClick(grip, Qt::Key_Right, Qt::ShiftModifier);
            QCOMPARE(window.parameterWidth, 286);
            QCOMPARE(scroll->width(), 286);
            window.resizeParameters(420);
            savedWidth = window.parameterWidth;
            const auto captureDirectory = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
            if (!captureDirectory.isEmpty())
            {
                QTest::qWait(50);
                QVERIFY(window.grab().save(QDir(captureDirectory).filePath("sidebar-resized.png")));
            }
            window.close();
        }
        sw::Window restored(&engine, &mcp, settingsPath);
        restored.show();
        QTest::qWait(50);
        QCOMPARE(restored.parameterWidth, savedWidth);
        QCOMPARE(restored.findChild<QScrollArea *>("parameterScroll")->width(), savedWidth);
        restored.close();
    }

    void parameterSidebarKeepsControlsClearOfGrip()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.resize(1416, 968);
        window.show();
        QTest::qWait(100);
        auto scroll = window.findChild<QScrollArea *>("parameterScroll");
        auto side = window.sidebar;
        QVERIFY(scroll && side);
        for (int width : {220, 400, 692, 700, 900, 1034})
        {
            window.resizeParameters(width);
            window.centralWidget()->layout()->activate();
            QTest::qWait(80);
            int maxRight = 0;
            for (auto combo : side->findChildren<QComboBox *>())
            {
                if (!combo->isVisible() || !combo->isVisibleTo(side)) { continue; }
                maxRight = qMax(maxRight, combo->mapTo(side, QPoint(0, 0)).x() + combo->width());
            }
            QVERIFY2(maxRight > 0, qPrintable(QString::number(width)));
            QVERIFY2(side->width() - maxRight >= 20,
                qPrintable(QString("width=%1 gap=%2").arg(width).arg(side->width() - maxRight)));
            QVERIFY(window.parameterGrip->width() >= 10);
            QTRY_VERIFY_WITH_TIMEOUT(window.parameterGrip->mapTo(window.centralWidget(), QPoint()).x()
                >= scroll->mapTo(window.centralWidget(), QPoint()).x() + scroll->width() - 4, 1000);
        }
        const auto captureDirectory = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
        if (!captureDirectory.isEmpty())
        {
            window.resizeParameters(700);
            QTest::qWait(80);
            QVERIFY(window.grab().save(QDir(captureDirectory).filePath("sidebar-wide-boundary.png")));
        }
        window.close();
    }

    void settingsResizeAndRestore()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.json");
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        const QSize compactSize(520, 380);
        {
            sw::Window window(&engine, &mcp, path);
            window.openSettings();
            auto dialog = window.settingsDialog;
            auto pages = dialog->findChild<sw::DetachableTabs *>("settingsPages");
            auto mcpPages = dialog->findChild<sw::DetachableTabs *>("mcpPages");
            auto clients = dialog->findChild<QScrollArea *>("clientsScroll");
            auto themeChoice = dialog->findChild<QComboBox *>("themeChoice");
            QVERIFY(pages && mcpPages && clients && themeChoice);

            pages->setCurrentIndex(1);
            mcpPages->setCurrentIndex(1);
            dialog->resize(compactSize);
            QTRY_COMPARE(dialog->size(), compactSize);
            QTRY_VERIFY(clients->verticalScrollBar()->maximum() > 0);
            QCOMPARE(clients->horizontalScrollBar()->maximum(), 0);
            auto piButton = clients->widget()->findChildren<QPushButton *>().last();
            clients->ensureWidgetVisible(piButton);
            QTRY_VERIFY(clients->viewport()->rect().contains(piButton->mapTo(clients->viewport(), piButton->rect().center())));
            const auto captureDirectory = qEnvironmentVariable("SW_UI_CAPTURE_DIR");
            if (!captureDirectory.isEmpty())
            {
                QVERIFY(dialog->grab().save(QDir(captureDirectory).filePath("settings-compact.png")));
            }

            pages->setCurrentIndex(0);
            QCoreApplication::processEvents();
            const int compactFieldWidth = themeChoice->width();
            dialog->resize(980, 760);
            QTRY_VERIFY(themeChoice->width() > compactFieldWidth + 200);
            pages->setCurrentIndex(1);
            mcpPages->setCurrentIndex(2);
            QCoreApplication::processEvents();
            const int expandedTableHeight = window.auditTable->height();
            dialog->resize(compactSize);
            QTRY_VERIFY(window.auditTable->height() < expandedTableHeight - 200);
            QCOMPARE(dialog->size(), compactSize);

            pages->setCurrentIndex(2);
            auto about = dialog->findChild<QScrollArea *>("aboutScroll");
            auto version = dialog->findChild<QLabel *>("aboutVersion");
            QVERIFY(about && version);
            QVERIFY(version->text().contains(sw::Version));
            auto sources = dialog->findChild<QLabel *>("aboutSources");
            QVERIFY(sources);
            QVERIFY(sources->text().contains("https://github.com/lilinng-163/serial-workbench"));
            QVERIFY(sources->text().contains("https://162.14.72.194/"));
            QVERIFY(sources->openExternalLinks());
            QTRY_VERIFY(about->verticalScrollBar()->maximum() > 0);
            QCOMPARE(about->horizontalScrollBar()->maximum(), 0);
            if (!captureDirectory.isEmpty())
            {
                QVERIFY(dialog->grab().save(QDir(captureDirectory).filePath("about-compact.png")));
            }
            dialog->resize(980, 760);
            QTRY_COMPARE(about->verticalScrollBar()->maximum(), 0);
            QCOMPARE(about->horizontalScrollBar()->maximum(), 0);
            if (!captureDirectory.isEmpty())
            {
                QTest::qWait(60);
                const QRect viewportRect(about->viewport()->mapTo(dialog, QPoint()), about->viewport()->size());
                QVERIFY(dialog->grab().copy(viewportRect).save(QDir(captureDirectory).filePath("about-page.png")));
                QVERIFY(dialog->grab().save(QDir(captureDirectory).filePath("settings-about.png")));
                window.applyTheme(true);
                QCoreApplication::processEvents();
                QVERIFY(dialog->grab().copy(viewportRect).save(QDir(captureDirectory).filePath("about-page-dark.png")));
                window.applyTheme(false);
            }
            dialog->resize(compactSize);
            QTRY_COMPARE(dialog->size(), compactSize);
            dialog->accept();
            QVERIFY(!dialog->isVisible());
            window.close();
        }
        {
            sw::Window restored(&engine, &mcp, path);
            restored.openSettings();
            QTRY_COMPARE(restored.settingsDialog->size(), compactSize);
            restored.settingsDialog->reject();
            restored.close();
        }
    }

    void piAgentConfiguration()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto agentDirectory = directory.filePath("pi-agent");
        QVERIFY(QDir().mkpath(agentDirectory));
        const auto configPath = QDir(agentDirectory).filePath("mcp.json");
        QFile existing(configPath);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.write("{\n  \"mcpServers\": {\n    \"other-server\": {\"url\": \"http://127.0.0.1:9000/mcp\"}\n  }\n}\n");
        existing.close();

        const bool hadPreviousDirectory = qEnvironmentVariableIsSet("PI_CODING_AGENT_DIR");
        const QByteArray previousDirectory = qgetenv("PI_CODING_AGENT_DIR");
        qputenv("PI_CODING_AGENT_DIR", agentDirectory.toUtf8());

        sw::Engine engine;
        sw::McpServer mcp(&engine);
        QVERIFY(mcp.start(0));
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        QLabel result;
        sw::AgentEnvironment testEnvironment;
        testEnvironment.home = directory.path();
        testEnvironment.appData = directory.path();
        testEnvironment.localAppData = directory.path();
        testEnvironment.searchPaths = {directory.path()};
        testEnvironment.variables["PI_CODING_AGENT_DIR"] = agentDirectory;
        QFile fakeAgent(directory.filePath("pi.exe"));
        QVERIFY(fakeAgent.open(QIODevice::WriteOnly));
        fakeAgent.write("test installation marker");
        fakeAgent.close();
        const auto piAgent = sw::AgentConfig::discover(testEnvironment)[4];
        result.setText(sw::AgentConfig::configure(piAgent, mcp.url(), directory.filePath("unused-bridge.exe")));

        if (hadPreviousDirectory)
        {
            qputenv("PI_CODING_AGENT_DIR", previousDirectory);
        }
        else
        {
            qunsetenv("PI_CODING_AGENT_DIR");
        }

        QFile updatedFile(configPath);
        QVERIFY(updatedFile.open(QIODevice::ReadOnly));
        const auto updated = QJsonDocument::fromJson(updatedFile.readAll()).object();
        const auto servers = updated["mcpServers"].toObject();
        QCOMPARE(servers["other-server"].toObject()["url"].toString(), QString("http://127.0.0.1:9000/mcp"));
        QCOMPARE(servers["serial-workbench"].toObject()["url"].toString(), mcp.url());
        QVERIFY(servers["serial-workbench"].toObject()["enabled"].toBool());
        QVERIFY(result.text().contains(configPath));
        QVERIFY(result.text().contains("文件校验通过"));
        QVERIFY(result.text().contains("/reload"));

        window.openSettings();
        auto themeChoice = window.settingsDialog->findChild<QComboBox *>("themeChoice");
        QVERIFY(themeChoice);
        QCOMPARE(themeChoice->itemText(0), QString("浅色主题"));
        QCOMPARE(themeChoice->itemText(1), QString("深色主题"));
        for (const auto label : window.settingsDialog->findChildren<QLabel *>())
        {
            QVERIFY(!label->text().contains("像素掌机"));
            QVERIFY(!label->text().contains("灰绿面板 / 琥珀按键 / 清晰文字"));
        }

        window.close();
    }

    void mcpPermissions()
    {
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        QCOMPARE(mcp.tools().size(), 17);
        QVERIFY_EXCEPTION_THROWN(mcp.call("open_connection", {{"config", QJsonObject {{"kind", "serial"}, {"port", "loop://"}}}}), std::runtime_error);
        engine.allowControl = true;
        mcp.call("open_connection", {{"config", QJsonObject {{"kind", "serial"}, {"port", "loop://"}}}});
        QCOMPARE(engine.channels.size(), 1);
    }
};
QTEST_MAIN(NativeTests)
#include "tests.moc"
