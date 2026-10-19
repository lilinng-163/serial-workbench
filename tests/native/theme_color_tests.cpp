// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "window_chrome.h"
#include <QtTest>
#include <cmath>

class ThemeColorTests : public QObject
{
    Q_OBJECT
private:
    QJsonObject appearance(const QString &path);

    void capture(QWidget *widget, const QString &name);

private slots:
    void initTestCase();

    void contrastAndParsing();

    void editPersistRestoreAndReset();

    void pickerAcceptCancelAndDetachedSync();

    void malformedSavedColorFallsBack();
};

QJsonObject ThemeColorTests::appearance(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object()["appearance"].toObject();
}

void ThemeColorTests::capture(QWidget *widget, const QString &name)
{
    const auto directory = qEnvironmentVariable("SW_THEME_CAPTURE_DIR");
    if (!directory.isEmpty())
    {
        QDir().mkpath(directory);
        QVERIFY(widget->grab().save(directory + "/" + name + ".png"));
    }
}

void ThemeColorTests::initTestCase()
{
    QApplication::setStyle("Fusion");
    QApplication::setFont(sw::Theme::interfaceFont());
    sw::installWindowChrome();
}

void ThemeColorTests::contrastAndParsing()
{
    QCOMPARE(sw::Theme::parseColor(" #48e "), QColor("#4488ee"));
    for (const auto &value : {"red", "4488ee", "#12345", "#11223344", "#12GG00", ""})
    {
        QVERIFY(!sw::Theme::parseColor(value).isValid());
    }
    const auto luminance = [](const QColor &color)
    {
        const auto linear = [](qreal value)
        {
            return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
    };
    for (bool dark : {false, true})
    {
        for (const auto &name : {"#000000", "#ffffff", "#ff0000", "#00ff00", "#0000ff", "#4488ee", "#ff77cc"})
        {
            const auto theme = sw::Theme::make(dark, QColor(name));
            QCOMPARE(theme.accent, QColor(name));
            const qreal a = luminance(theme.accent);
            const qreal b = luminance(theme.onAccent);
            QVERIFY((qMax(a, b) + 0.05) / (qMin(a, b) + 0.05) >= 4.5);
            QVERIFY(dark ? theme.panel.lightnessF() < 0.2 : theme.panel.lightnessF() > 0.9);
        }
    }
}

void ThemeColorTests::editPersistRestoreAndReset()
{
    QTemporaryDir directory;
    const auto path = directory.filePath("settings.json");
    sw::Engine engine;
    sw::McpServer server(&engine);
    {
        sw::Window window(&engine, &server, path);
        window.show();
        window.openSettings();
        auto input = window.settingsDialog->findChild<QLineEdit *>("themeColorHex");
        auto apply = window.settingsDialog->findChild<QPushButton *>("themeColorApply");
        auto hint = window.settingsDialog->findChild<QLabel *>("themeColorHint");
        QVERIFY(input && apply && hint);
        input->setText("#48e");
        input->setFocus();
        QTest::keyClick(input, Qt::Key_Return);
        QVERIFY(window.settingsDialog->isVisible());
        QCOMPARE(window.theme.accent, QColor("#4488ee"));
        QCOMPARE(input->text(), QString("#4488EE"));
        QCOMPARE(appearance(path)["theme_color"].toString(), QString("#4488ee"));
        input->setText("#invalid");
        apply->click();
        QVERIFY(hint->text().contains("无效"));
        QCOMPARE(window.theme.accent, QColor("#4488ee"));
        QCOMPARE(appearance(path)["theme_color"].toString(), QString("#4488ee"));
        input->setText("#4488EE");
        apply->click();
        auto scroll = window.settingsDialog->findChild<QScrollArea *>("appearanceScroll");
        scroll->ensureWidgetVisible(input);
        QApplication::processEvents();
        capture(window.settingsDialog, "theme-settings-light");
        capture(&window, "theme-workspace-light");
        window.applyTheme(true);
        window.saveSettings();
        QCOMPARE(window.theme.accent, QColor("#4488ee"));
        capture(window.settingsDialog, "theme-settings-dark");
        capture(&window, "theme-workspace-dark");
        window.settingsDialog->accept();
        window.close();
    }
    sw::Window restored(&engine, &server, path);
    QVERIFY(restored.theme.dark);
    QCOMPARE(restored.theme.accent, QColor("#4488ee"));
    restored.openSettings();
    restored.settingsDialog->findChild<QPushButton *>("themeColorReset")->click();
    QCOMPARE(restored.theme.accent, sw::Theme::make(true).accent);
    QVERIFY(!appearance(path).contains("theme_color"));
    restored.close();
}

void ThemeColorTests::pickerAcceptCancelAndDetachedSync()
{
    QTemporaryDir directory;
    sw::Engine engine;
    sw::McpServer server(&engine);
    sw::Window window(&engine, &server, directory.filePath("settings.json"));
    window.show();
    window.openSettings();
    window.newConnection();
    auto detached = window.detachConnection(window.tabs->currentIndex(), QPoint(100, 100));
    QVERIFY(detached);
    detached->openSettings();
    auto choose = detached->settingsDialog->findChild<QPushButton *>("themeColorChoose");
    QVERIFY(choose);
    bool pickerFound = false;
    QTimer::singleShot(0, [&]
    {
        auto dialog = detached->settingsDialog->findChild<QColorDialog *>("themeColorDialog");
        if (!dialog) { return; }
        pickerFound = true;
        dialog->setCurrentColor(QColor("#814fc4"));
        capture(dialog, "theme-color-picker");
        dialog->accept();
    });
    choose->click();
    QVERIFY(pickerFound);
    QCOMPARE(window.theme.accent, QColor("#814fc4"));
    QCOMPARE(detached->theme.accent, window.theme.accent);
    QCOMPARE(window.active->log->theme.accent, window.theme.accent);
    QCOMPARE(detached->settingsDialog->palette().color(QPalette::Highlight), window.theme.accent);
    QCOMPARE(window.settingsDialog->findChild<QLineEdit *>("themeColorHex")->text(), QString("#814FC4"));
    QTimer::singleShot(0, [&]
    {
        auto dialog = detached->settingsDialog->findChild<QColorDialog *>("themeColorDialog");
        if (!dialog) { return; }
        dialog->setCurrentColor(Qt::red);
        dialog->reject();
    });
    choose->click();
    QCOMPARE(window.theme.accent, QColor("#814fc4"));
    window.applyTheme(true);
    QVERIFY(detached->theme.dark);
    QCOMPARE(detached->theme.accent, QColor("#814fc4"));
    detached->close();
    window.close();
}

void ThemeColorTests::malformedSavedColorFallsBack()
{
    QTemporaryDir directory;
    const auto path = directory.filePath("settings.json");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"appearance":{"theme":"light","theme_color":"#bad-value"}})");
    file.close();
    sw::Engine engine;
    sw::McpServer server(&engine);
    sw::Window window(&engine, &server, path);
    QCOMPARE(window.theme.accent, sw::Theme::make(false).accent);
    window.close();
}

QTEST_MAIN(ThemeColorTests)
#include "theme_color_tests.moc"
