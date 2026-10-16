// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "window_chrome.h"
#include <QtTest>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

class WindowChromeTests : public QObject
{
    Q_OBJECT
private:
    void verifyFrame(QWidget *widget)
    {
        widget->show();
        QApplication::processEvents();
        QVERIFY(widget->windowFlags().testFlag(Qt::ExpandedClientAreaHint));
        QVERIFY(widget->windowFlags().testFlag(Qt::NoTitleBarBackgroundHint));
        QVERIFY(!widget->windowFlags().testFlag(Qt::FramelessWindowHint));
        QVERIFY(widget->windowFlags().testFlag(Qt::CustomizeWindowHint));
        auto caption = widget->findChild<sw::BackgroundSurface *>("windowCaptionBackground", Qt::FindDirectChildrenOnly);
        QVERIFY(caption);
        QVERIFY(caption->testAttribute(Qt::WA_TransparentForMouseEvents));
        const int inset = widget->windowHandle()->safeAreaMargins().top();
        QVERIFY(caption->height() >= inset);
        QCOMPARE(caption->width(), widget->width());
        QVERIFY(widget->contentsRect().top() >= caption->height());
        if (QGuiApplication::platformName() == "windows")
        {
            QVERIFY(caption->height() * widget->devicePixelRatioF() >= 24);
            QVERIFY(caption->isVisible());
#ifdef Q_OS_WIN
            const auto native = reinterpret_cast<HWND>(widget->winId());
            bool nativeCaption = false;
            for (auto child = GetWindow(native, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
            {
                wchar_t name[128] = {};
                GetClassNameW(child, name, 128);
                if (QString::fromWCharArray(name).contains("_q_titlebar"))
                {
                    nativeCaption = true;
                    QVERIFY(IsWindowVisible(child));
                    RECT bounds = {};
                    QVERIFY(GetWindowRect(child, &bounds));
                    QVERIFY(bounds.right > bounds.left);
                    QVERIFY(bounds.bottom > bounds.top);
                    QVERIFY(bounds.bottom - bounds.top <= qCeil(caption->height() * widget->devicePixelRatioF()));
                }
            }
            QVERIFY2(nativeCaption, "Qt's native title text and button overlay must exist alongside the background");
#endif
        }
    }

    void capture(QWidget *widget, const QString &name)
    {
        const auto directory = qEnvironmentVariable("SW_CHROME_CAPTURE_DIR");
        if (directory.isEmpty()) { return; }
        QDir().mkpath(directory);
        QApplication::processEvents();
        QVERIFY(widget->grab().save(directory + "/" + name + ".png"));
        if (QGuiApplication::platformName() == "windows")
        {
            widget->raise();
            widget->activateWindow();
            QTest::qWait(150);
            QVERIFY(widget->screen()->grabWindow(widget->winId()).save(directory + "/" + name + "-native.png"));
        }
    }

private slots:
    void initTestCase()
    {
        QApplication::setStyle("Fusion");
        QApplication::setFont(sw::Theme::interfaceFont());
        sw::installWindowChrome();
    }

    void mainDialogsAndPopups()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer server(&engine);
        sw::Window window(&engine, &server, directory.filePath("settings.json"));
        verifyFrame(&window);
        QVERIFY(window.windowFlags().testFlag(Qt::WindowMaximizeButtonHint));
        QVERIFY(window.centralWidget()->y() >= window.windowHandle()->safeAreaMargins().top());
        window.openSettings();
        verifyFrame(window.settingsDialog);
        window.settingsDialog->resize(440, 320);
        QApplication::processEvents();
        QCOMPARE(window.settingsDialog->size(), QSize(440, 320));
        capture(window.settingsDialog, "settings-compact");
        auto settingsPages = window.settingsDialog->findChild<QTabWidget *>("settingsPages");
        QVERIFY(settingsPages);
        settingsPages->setCurrentIndex(1);
        capture(window.settingsDialog, "settings-ai");
        settingsPages->setCurrentIndex(2);
        capture(window.settingsDialog, "settings-about");
        window.settingsDialog->hide();
        QFontDialog fontDialog(&window);
        fontDialog.setWindowTitle("数据字体");
        verifyFrame(&fontDialog);
        fontDialog.hide();
        QColorDialog colorDialog(&window);
        colorDialog.setWindowTitle("曲线颜色");
        verifyFrame(&colorDialog);
        colorDialog.hide();
        QFileDialog fileDialog(&window, "选择背景图片", directory.path());
        verifyFrame(&fileDialog);
        QVERIFY(QApplication::testAttribute(Qt::AA_DontUseNativeDialogs));
        fileDialog.hide();
        QMessageBox message(QMessageBox::Information, "提示", "操作完成", QMessageBox::Ok, &window);
        const auto messageButtons = message.windowFlags() & Qt::WindowMinMaxButtonsHint;
        verifyFrame(&message);
        QCOMPARE(message.windowFlags() & Qt::WindowMinMaxButtonsHint, messageButtons);
        message.hide();
        QInputDialog input(&window);
        input.setWindowTitle("输入名称");
        verifyFrame(&input);
        input.hide();
        QMenu menu(&window);
        menu.addAction("测试菜单");
        menu.popup(window.mapToGlobal(QPoint(200, 200)));
        QApplication::processEvents();
        QVERIFY(!menu.findChild<QWidget *>("windowCaptionBackground", Qt::FindDirectChildrenOnly));
        menu.hide();
        window.newConnection();
        auto detached = window.detachConnection(window.tabs->currentIndex(), QPoint(100, 100));
        QVERIFY(detached);
        verifyFrame(detached);
        detached->close();
        window.close();
    }

    void continuousBackgroundAndLiveUpdates()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer server(&engine);
        sw::Window window(&engine, &server, directory.filePath("settings.json"));
        window.resize(1000, 700);
        QImage picture(600, 900, QImage::Format_RGB32);
        for (int y = 0; y < picture.height(); ++y)
        {
            for (int x = 0; x < picture.width(); ++x)
            {
                picture.setPixelColor(x, y, QColor(40 + x / 4, 40 + y / 5, 80));
            }
        }
        const auto file = directory.filePath("wallpaper.png");
        QVERIFY(picture.save(file));
        QVERIFY(window.importBackground(false, file));
        window.windowBackground.opacity = 100;
        window.windowBackground.positionX = 20;
        window.windowBackground.positionY = 80;
        window.applyBackgrounds();
        verifyFrame(&window);
        auto caption = window.findChild<sw::BackgroundSurface *>("windowCaptionBackground", Qt::FindDirectChildrenOnly);
        QVERIFY(caption->background.hasImage());
        if (QGuiApplication::platformName() == "windows")
        {
            const auto verifyPixels = [&]
            {
                QApplication::processEvents();
                const qreal ratio = window.devicePixelRatioF();
                QImage expected(window.size() * ratio, QImage::Format_RGB32);
                expected.setDevicePixelRatio(ratio);
                expected.fill(window.theme.background);
                sw::BackgroundImage background;
                background.set(window.windowBackground, window.windowBackgroundImage);
                {
                    QPainter painter(&expected);
                    background.paint(painter, window.rect());
                }
                const auto actual = window.grab().toImage();
                const QPoint top(window.width() / 2, caption->height() - 4);
                QCOMPARE(actual.pixelColor(top * actual.devicePixelRatio()), expected.pixelColor(top * ratio));
                const auto pane = window.active->log->viewport();
                const QPoint body = pane->mapTo(&window, pane->rect().bottomRight() - QPoint(12, 12));
                QCOMPARE(actual.pixelColor(body * actual.devicePixelRatio()), expected.pixelColor(body * ratio));
            };
            verifyPixels();
            window.resize(1100, 760);
            verifyPixels();
            window.windowBackground.positionY = 0;
            window.applyBackgrounds();
            verifyPixels();
            window.showMaximized();
            QApplication::processEvents();
            QVERIFY(window.isMaximized());
            verifyFrame(&window);
            window.showNormal();
            QApplication::processEvents();
            QVERIFY(!window.isMaximized());
        }
        capture(&window, "wallpaper-light");
        window.openSettings();
        auto dialogCaption = window.settingsDialog->findChild<sw::BackgroundSurface *>("windowCaptionBackground", Qt::FindDirectChildrenOnly);
        QVERIFY(dialogCaption && dialogCaption->background.hasImage());
        capture(window.settingsDialog, "wallpaper-dialog");
        window.applyTheme(true);
        QCOMPARE(caption->base, window.theme.background);
        QCOMPARE(dialogCaption->base, window.theme.background);
        QVERIFY(dialogCaption->background.hasImage());
        window.settingsDialog->hide();
        capture(&window, "wallpaper-dark");
        window.windowBackgroundImage = {};
        window.windowBackground = {};
        window.applyBackgrounds();
        QVERIFY(!caption->background.hasImage());
        QVERIFY(!dialogCaption->background.hasImage());
        capture(&window, "default-dark");
        window.close();
    }
};
QTEST_MAIN(WindowChromeTests)
#include "window_chrome_tests.moc"
