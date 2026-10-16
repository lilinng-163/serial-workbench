// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include <QtTest>

class BackgroundTests : public QObject
{
    Q_OBJECT
private:
    QString picture(const QString &path, const QColor &color, const char *format = "PNG")
    {
        QImage image(240, 120, QImage::Format_RGB32);
        image.fill(color);
        return image.save(path, format) ? path : QString();
    }

    QColor corner(QWidget *widget)
    {
        QApplication::processEvents();
        const auto canvas = widget->window();
        const auto image = canvas->grab().toImage();
        const QRect visible = widget->visibleRegion().boundingRect();
        const QPoint point = widget->mapTo(canvas, visible.bottomRight() - QPoint(12, 12));
        return image.pixelColor(point * image.devicePixelRatio());
    }

private slots:
    void initTestCase()
    {
        QApplication::setStyle("Fusion");
        QApplication::setFont(sw::Theme::interfaceFont());
    }

    void formats_data()
    {
        QTest::addColumn<QByteArray>("format");
        for (const auto &name : {"PNG", "JPEG", "BMP", "WEBP", "TIFF"})
        {
            QTest::newRow(name) << QByteArray(name);
        }
    }

    void formats()
    {
        QFETCH(QByteArray, format);
        QTemporaryDir directory;
        const auto source = picture(directory.filePath("背景 图片.bin"), Qt::red, format.constData());
        QVERIFY(!source.isEmpty());
        QString error;
        const auto relative = sw::BackgroundImage::importFile(source, directory.filePath("profile/settings.json"), &error);
        QVERIFY2(!relative.isEmpty(), qPrintable(error));
        QVERIFY(QFile::remove(source));
        const auto stored = sw::BackgroundImage::read(directory.filePath("profile/" + relative));
        QVERIFY(!stored.isNull());
        QCOMPARE(stored.size(), QSize(240, 120));
        QVERIFY(stored.pixelColor(0, 0).red() > 240);
    }

    void gifAndInvalidInput()
    {
        QTemporaryDir directory;
        QFile gif(directory.filePath("first.gif"));
        QVERIFY(gif.open(QIODevice::WriteOnly));
        gif.write(QByteArray::fromBase64("R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7"));
        gif.close();
        QVERIFY(!sw::BackgroundImage::read(gif.fileName()).isNull());
        QFile invalid(directory.filePath("broken.png"));
        QVERIFY(invalid.open(QIODevice::WriteOnly));
        invalid.write("not a picture");
        invalid.close();
        QString error;
        QVERIFY(sw::BackgroundImage::importFile(invalid.fileName(), directory.filePath("settings.json"), &error).isEmpty());
        QVERIFY(!error.isEmpty());
        auto spec = sw::BackgroundSpec::fromJson({{"fit", "invalid"}, {"opacity", 900}, {"position_x", -10}, {"position_y", 900}});
        QCOMPARE(spec.fit, QString("cover"));
        QCOMPARE(spec.opacity, 100);
        QCOMPARE(spec.positionX, 0);
        QCOMPARE(spec.positionY, 100);
        QCOMPARE(sw::BackgroundSpec::fromJson(spec.json()).json(), spec.json());
        const auto legacy = sw::BackgroundSpec::fromJson({{"file", "old.png"}});
        QCOMPARE(legacy.positionX, 50);
        QCOMPARE(legacy.positionY, 50);
    }

    void imageFitOpacityAndResize()
    {
        QImage stripes(200, 100, QImage::Format_RGB32);
        stripes.fill(Qt::green);
        {
            QPainter painter(&stripes);
            painter.fillRect(QRect(0, 0, 50, 100), Qt::red);
            painter.fillRect(QRect(150, 0, 50, 100), Qt::blue);
        }
        sw::BackgroundSpec spec;
        spec.opacity = 100;
        sw::BackgroundImage background;
        const auto render = [&](const QSize &size, qreal ratio = 1.0)
        {
            QImage result(size * ratio, QImage::Format_RGB32);
            result.setDevicePixelRatio(ratio);
            result.fill(Qt::white);
            QPainter painter(&result);
            background.paint(painter, QRect(QPoint(), size));
            return result;
        };
        background.set(spec, stripes);
        const auto crop = render(QSize(100, 100));
        QCOMPARE(crop.pixelColor(10, 50), QColor(Qt::green));
        QCOMPARE(crop.pixelColor(90, 50), QColor(Qt::green));
        QCOMPARE(render(QSize(100, 100)), crop);
        spec.fit = "contain";
        background.set(spec, stripes);
        const auto fitted = render(QSize(100, 100));
        QCOMPARE(fitted.pixelColor(50, 5), QColor(Qt::white));
        QCOMPARE(fitted.pixelColor(5, 50), QColor(Qt::red));
        const auto wide = render(QSize(200, 100), 2);
        QCOMPARE(wide.pixelColor(10, 100), QColor(Qt::red));
        QCOMPARE(wide.pixelColor(390, 100), QColor(Qt::blue));
        spec.fit = "tile";
        background.set(spec, stripes);
        QCOMPARE(render(QSize(500, 100)).pixelColor(210, 50), QColor(Qt::red));
        spec.opacity = 0;
        background.set(spec, stripes);
        QCOMPARE(render(QSize(100, 100)).pixelColor(10, 50), QColor(Qt::white));
        spec.opacity = 50;
        background.set(spec, stripes);
        const auto faded = render(QSize(100, 100)).pixelColor(10, 50);
        QVERIFY(faded.red() > 245 && faded.green() > 115 && faded.green() < 140);
        QImage alpha(2, 2, QImage::Format_ARGB32);
        alpha.fill(QColor(255, 0, 0, 128));
        spec.opacity = 100;
        spec.fit = "cover";
        background.set(spec, alpha);
        const auto blended = render(QSize(100, 100)).pixelColor(20, 20);
        QVERIFY(blended.red() > 245 && blended.green() > 115 && blended.green() < 140);
    }

    void imagePositioning()
    {
        QImage stripes(200, 100, QImage::Format_RGB32);
        stripes.fill(Qt::green);
        {
            QPainter painter(&stripes);
            painter.fillRect(0, 0, 50, 100, Qt::red);
            painter.fillRect(150, 0, 50, 100, Qt::blue);
        }
        const auto vertical = stripes.transformed(QTransform().rotate(90));
        sw::BackgroundSpec spec;
        spec.opacity = 100;
        sw::BackgroundImage background;
        const auto render = [&](const QImage &source, qreal ratio = 1.0, QSize size = QSize(100, 100))
        {
            background.set(spec, source);
            QImage result(size * ratio, QImage::Format_RGB32);
            result.setDevicePixelRatio(ratio);
            result.fill(Qt::white);
            QPainter painter(&result);
            background.paint(painter, QRect(QPoint(), size));
            return result;
        };
        QCOMPARE(render(stripes).pixelColor(10, 50), QColor(Qt::green));
        spec.positionX = 0;
        QCOMPARE(render(stripes).pixelColor(10, 50), QColor(Qt::red));
        spec.positionX = 100;
        QCOMPARE(render(stripes).pixelColor(90, 50), QColor(Qt::blue));
        QCOMPARE(render(stripes, 2).pixelColor(180, 100), QColor(Qt::blue));
        QCOMPARE(render(stripes, 1, QSize(120, 100)).pixelColor(110, 50), QColor(Qt::blue));
        spec.positionY = 0;
        QCOMPARE(render(vertical).pixelColor(50, 10), QColor(Qt::red));
        spec.positionY = 100;
        QCOMPARE(render(vertical).pixelColor(50, 90), QColor(Qt::blue));
        spec.fit = "contain";
        spec.positionY = 0;
        QCOMPARE(render(stripes).pixelColor(50, 10), QColor(Qt::green));
        QCOMPARE(render(stripes).pixelColor(50, 90), QColor(Qt::white));
        spec.positionY = 100;
        QCOMPARE(render(stripes).pixelColor(50, 10), QColor(Qt::white));
        QCOMPARE(render(stripes).pixelColor(50, 90), QColor(Qt::green));
        spec.positionX = 0;
        QCOMPARE(render(vertical).pixelColor(10, 50), QColor(Qt::green));
        QCOMPARE(render(vertical).pixelColor(90, 50), QColor(Qt::white));
        spec.positionX = 100;
        QCOMPARE(render(vertical).pixelColor(10, 50), QColor(Qt::white));
        QCOMPARE(render(vertical).pixelColor(90, 50), QColor(Qt::green));
        spec.fit = "tile";
        spec.positionX = 50;
        spec.positionY = 50;
        QCOMPARE(render(stripes).pixelColor(10, 50), QColor(Qt::red));
        spec.positionX = 75;
        QCOMPARE(render(stripes).pixelColor(10, 50), QColor(Qt::blue));
        spec.positionY = 75;
        QCOMPARE(render(vertical).pixelColor(50, 10), QColor(Qt::blue));
    }

    void independentAreasPersistenceAndReset()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath("profile/settings.json");
        const auto mainImage = picture(directory.filePath("主界面.png"), Qt::green);
        const auto receiveImage = picture(directory.filePath("收发区.jpg"), Qt::red, "JPEG");
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        {
            sw::Window window(&engine, &mcp, path);
            window.show();
            window.openSettings();
            QVERIFY(window.importBackground(false, mainImage));
            auto mainOpacity = window.settingsDialog->findChild<QSlider *>("windowBackgroundOpacity");
            auto receiveOpacity = window.settingsDialog->findChild<QSlider *>("receiveBackgroundOpacity");
            QVERIFY(mainOpacity && receiveOpacity);
            mainOpacity->setValue(100);
            auto mainX = window.settingsDialog->findChild<QSlider *>("windowBackgroundPositionX");
            auto mainY = window.settingsDialog->findChild<QSlider *>("windowBackgroundPositionY");
            auto receiveX = window.settingsDialog->findChild<QSlider *>("receiveBackgroundPositionX");
            auto receiveY = window.settingsDialog->findChild<QSlider *>("receiveBackgroundPositionY");
            QVERIFY(mainX && mainY && receiveX && receiveY);
            mainX->setValue(20);
            mainY->setValue(80);
            receiveX->setValue(75);
            receiveY->setValue(10);
            QCOMPARE(window.windowBackground.positionX, 20);
            QCOMPARE(window.windowBackground.positionY, 80);
            QCOMPARE(window.receiveBackground.positionX, 75);
            QCOMPARE(window.receiveBackground.positionY, 10);
            QCOMPARE(window.settingsDialog->findChild<QLabel *>("windowBackgroundPositionXValue")->text(), QString("20%"));
            window.settingsDialog->hide();
            QCOMPARE(corner(window.sidebar), QColor(Qt::green));
            QCOMPARE(corner(window.active->log->viewport()), QColor(Qt::green));
            QVERIFY(window.importBackground(true, receiveImage));
            receiveOpacity->setValue(100);
            QVERIFY(corner(window.active->log->viewport()).red() > 240);
            QVERIFY(corner(window.active->receiveSurface).red() > 240);
            QCOMPARE(corner(window.sidebar), QColor(Qt::green));
            window.active->log->setPlainText(QString("Sample 123\n").repeated(300));
            window.active->log->verticalScrollBar()->setValue(40);
            QVERIFY(corner(window.active->log->viewport()).red() > 240);
            window.active->setTerminal(true);
            window.active->terminal->feed("ANSI \x1b[32mgreen\x1b[0m\r\n");
            QVERIFY(corner(window.active->terminal).red() > 240);
            QVERIFY(window.active->terminal->text().contains("green"));
            window.applyTheme(true);
            window.resize(1000, 700);
            QVERIFY(corner(window.active->terminal).red() > 240);
            QCOMPARE(corner(window.sidebar), QColor(Qt::green));
            const auto captured = qEnvironmentVariable("SW_BACKGROUND_CAPTURE_DIR");
            if (!captured.isEmpty())
            {
                QDir().mkpath(captured);
                window.active->setTerminal(false);
                window.active->log->clear();
                window.applyTheme(false);
                QVERIFY(window.grab().save(captured + "/background-light.png"));
                window.applyTheme(true);
                QVERIFY(window.grab().save(captured + "/background-dark.png"));
                window.openSettings();
                QVERIFY(window.settingsDialog->grab().save(captured + "/background-settings.png"));
                window.settingsDialog->hide();
            }
            window.saveSettings();
            QVERIFY(QFile::remove(mainImage));
            QVERIFY(QFile::remove(receiveImage));
            window.close();
        }
        sw::Window restored(&engine, &mcp, path);
        restored.show();
        QVERIFY(!restored.windowBackgroundImage.isNull());
        QVERIFY(!restored.receiveBackgroundImage.isNull());
        QCOMPARE(restored.windowBackground.opacity, 100);
        QCOMPARE(restored.receiveBackground.opacity, 100);
        QCOMPARE(restored.windowBackground.positionX, 20);
        QCOMPARE(restored.windowBackground.positionY, 80);
        QCOMPARE(restored.receiveBackground.positionX, 75);
        QCOMPARE(restored.receiveBackground.positionY, 10);
        restored.openSettings();
        QCOMPARE(restored.settingsDialog->findChild<QSlider *>("windowBackgroundPositionX")->value(), 20);
        QCOMPARE(restored.settingsDialog->findChild<QSlider *>("receiveBackgroundPositionY")->value(), 10);
        restored.settingsDialog->findChild<QPushButton *>("windowBackgroundPositionReset")->click();
        QCOMPARE(restored.windowBackground.positionX, 50);
        QCOMPARE(restored.windowBackground.positionY, 50);
        QCOMPARE(restored.windowBackground.opacity, 100);
        QVERIFY(!restored.windowBackgroundImage.isNull());
        QCOMPARE(restored.receiveBackground.positionX, 75);
        QCOMPARE(restored.receiveBackground.positionY, 10);
        const auto reset = restored.settingsDialog->findChild<QPushButton *>("receiveBackgroundReset");
        QVERIFY(reset);
        reset->click();
        restored.settingsDialog->hide();
        QVERIFY(restored.receiveBackgroundImage.isNull());
        QCOMPARE(restored.receiveBackground.positionX, 50);
        QCOMPARE(restored.receiveBackground.positionY, 50);
        QVERIFY(!restored.windowBackgroundImage.isNull());
        QCOMPARE(corner(restored.active->log->viewport()), QColor(Qt::green));
        restored.settingsDialog->findChild<QPushButton *>("windowBackgroundReset")->click();
        QVERIFY(restored.windowBackgroundImage.isNull());
        QCOMPARE(corner(restored.sidebar), restored.theme.sidebar);
        QCOMPARE(corner(restored.active->log->viewport()), restored.theme.panel);
        restored.close();
    }

    void invalidImportAndDetachedWindows()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.show();
        window.openSettings();
        auto source = picture(directory.filePath("image.png"), Qt::blue);
        QVERIFY(window.importBackground(false, source));
        const auto previous = window.windowBackground.json();
        QString error;
        QVERIFY(!window.importBackground(false, directory.filePath("missing.png"), &error));
        QCOMPARE(window.windowBackground.json(), previous);
        window.newConnection();
        auto detached = window.detachConnection(window.tabs->currentIndex(), QPoint(100, 100));
        QVERIFY(detached);
        QCOMPARE(detached->windowBackground.json(), window.windowBackground.json());
        source = picture(directory.filePath("receive.png"), Qt::red);
        QVERIFY(detached->importBackground(true, source));
        QCOMPARE(detached->receiveBackgroundImage.cacheKey(), window.receiveBackgroundImage.cacheKey());
        detached->openSettings();
        detached->settingsDialog->findChild<QSlider *>("receiveBackgroundPositionX")->setValue(31);
        detached->settingsDialog->findChild<QSlider *>("receiveBackgroundPositionY")->setValue(69);
        QCOMPARE(window.receiveBackground.positionX, 31);
        QCOMPARE(window.receiveBackground.positionY, 69);
        QCOMPARE(detached->receiveBackground.json(), window.receiveBackground.json());
        QCOMPARE(window.settingsDialog->findChild<QSlider *>("receiveBackgroundPositionX")->value(), 31);
        QCOMPARE(window.settingsDialog->findChild<QSlider *>("receiveBackgroundPositionY")->value(), 69);
        detached->close();
        window.close();
    }
};
QTEST_MAIN(BackgroundTests)
#include "background_tests.moc"
