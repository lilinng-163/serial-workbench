// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "window_chrome.h"
#include "serial_control_panel.h"
#include "serial_monitor_dialog.h"
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

    void extremeColorsRemainReadable();

    void customContrastColor();

    void presetHoverUsesContrast();

    void darkPickerUsesThemePalette();

    void narrowSettingsThemeRowFits();

    void aboutLinksFollowAccent();

    void serialMonitorFollowsCustomAccent();
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
            const auto theme = sw::Theme::make(dark, QColor("#3c4cdc"), QColor(name));
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
        auto contrastInput = window.settingsDialog->findChild<QLineEdit *>("themeContrastHex");
        auto contrastApply = window.settingsDialog->findChild<QPushButton *>("themeContrastApply");
        auto hint = window.settingsDialog->findChild<QLabel *>("themeColorHint");
        QVERIFY(input && apply && contrastInput && contrastApply && hint);
        input->setText("#48e");
        input->setFocus();
        QTest::keyClick(input, Qt::Key_Return);
        QVERIFY(window.settingsDialog->isVisible());
        QCOMPARE(window.themeColor, QColor("#4488ee"));
        QCOMPARE(window.theme.accent, QColor("#dc8c38"));
        QCOMPARE(input->text(), QString("#4488EE"));
        QCOMPARE(appearance(path)["theme_color"].toString(), QString("#4488ee"));
        input->setText("#invalid");
        apply->click();
        QVERIFY(hint->text().contains("无效"));
        QCOMPARE(window.themeColor, QColor("#4488ee"));
        QCOMPARE(window.theme.accent, QColor("#dc8c38"));
        QCOMPARE(appearance(path)["theme_color"].toString(), QString("#4488ee"));
        input->setText("#4488EE");
        apply->click();
        contrastInput->setText("#e06030");
        contrastApply->click();
        QCOMPARE(window.theme.accent, QColor("#e06030"));
        QCOMPARE(appearance(path)["contrast_color"].toString(), QString("#e06030"));
        auto scroll = window.settingsDialog->findChild<QScrollArea *>("appearanceScroll");
        scroll->ensureWidgetVisible(input);
        QApplication::processEvents();
        capture(window.settingsDialog, "theme-settings-light");
        capture(&window, "theme-workspace-light");
        window.applyTheme(true);
        window.saveSettings();
        QCOMPARE(window.theme.accent, QColor("#e06030"));
        capture(window.settingsDialog, "theme-settings-dark");
        capture(&window, "theme-workspace-dark");
        window.settingsDialog->accept();
        window.close();
    }
    sw::Window restored(&engine, &server, path);
    QVERIFY(restored.theme.dark);
    QCOMPARE(restored.themeColor, QColor("#4488ee"));
    QCOMPARE(restored.theme.accent, QColor("#e06030"));
    restored.openSettings();
    restored.settingsDialog->findChild<QPushButton *>("themeColorReset")->click();
    QCOMPARE(restored.theme.accent, sw::Theme::make(true).accent);
    QVERIFY(!appearance(path).contains("theme_color"));
    QVERIFY(!appearance(path).contains("contrast_color"));
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
    QCOMPARE(window.themeColor, QColor("#814fc4"));
    QCOMPARE(window.theme.accent, QColor("#dc8c38"));
    QCOMPARE(detached->themeColor, window.themeColor);
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
    QCOMPARE(window.themeColor, QColor("#814fc4"));
    QCOMPARE(window.theme.accent, QColor("#dc8c38"));

    auto contrastChoose = detached->settingsDialog->findChild<QPushButton *>("themeContrastChoose");
    QVERIFY(contrastChoose);
    bool contrastPickerFound = false;
    QTimer::singleShot(0, [&]
    {
        auto dialog = detached->settingsDialog->findChild<QColorDialog *>("themeContrastDialog");
        if (!dialog) { return; }
        contrastPickerFound = true;
        dialog->setCurrentColor(QColor("#f2a33a"));
        capture(dialog, "theme-contrast-picker");
        dialog->accept();
    });
    contrastChoose->click();
    QVERIFY(contrastPickerFound);
    QCOMPARE(window.theme.accent, QColor("#f2a33a"));
    QCOMPARE(detached->theme.accent, window.theme.accent);
    window.applyTheme(true);
    QVERIFY(detached->theme.dark);
    QCOMPARE(detached->theme.accent, QColor("#f2a33a"));
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

namespace
{
qreal relativeLuminance(const QColor &color)
{
    const auto linear = [](qreal value)
    {
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

qreal contrastRatio(const QColor &a, const QColor &b)
{
    const qreal first = relativeLuminance(a);
    const qreal second = relativeLuminance(b);
    return (qMax(first, second) + 0.05) / (qMin(first, second) + 0.05);
}
}

void ThemeColorTests::extremeColorsRemainReadable()
{
    for (bool dark : {false, true})
    {
        for (const auto &main : {"#000000", "#ffffff", "#ff0000", "#00ff00", "#0000ff", "#808080", "#4488ee", "#ff77cc"})
        {
            const auto surfaces = sw::Theme::make(dark, QColor(main), QColor("#e06030"));
            QVERIFY2(contrastRatio(surfaces.text, surfaces.background) >= 4.5, main);
            QVERIFY2(contrastRatio(surfaces.text, surfaces.panel) >= 4.5, main);
        }
        for (const auto &accent : {"#000000", "#ffffff", "#ff0000", "#00ff00", "#0000ff", "#808080", "#4488ee", "#ff77cc"})
        {
            const auto theme = sw::Theme::make(dark, QColor("#3c4cdc"), QColor(accent));
            QVERIFY2(contrastRatio(theme.onAccent, theme.accent) >= 4.5, accent);
        }
    }
}

void ThemeColorTests::customContrastColor()
{
    for (bool dark : {false, true})
    {
        const QColor main("#3c4cdc");
        const QColor fallback = dark ? QColor("#edaa50") : QColor("#dc8c38");
        const auto plain = sw::Theme::make(dark);
        QCOMPARE(plain.accent, fallback);

        const auto surfaces = sw::Theme::make(dark, main);
        QCOMPARE(surfaces.accent, fallback);
        QVERIFY(surfaces.background != plain.background);
        QVERIFY(surfaces.panel != plain.panel);

        for (const auto &name : {"#000000", "#ffffff", "#808080", "#4488ee", "#ff77cc"})
        {
            const auto chosen = sw::Theme::make(dark, main, QColor(name));
            QCOMPARE(chosen.accent, QColor(name));
            QVERIFY2(chosen.railAccent.lightnessF() >= 0.65, name);
            QVERIFY2(chosen.stylesheet().contains(QColor(name).name()), name);
            QVERIFY2(contrastRatio(chosen.onAccent, chosen.accent) >= 4.5, name);
            QVERIFY2(contrastRatio(chosen.text, chosen.panel) >= 4.5, name);
        }
    }
}

void ThemeColorTests::presetHoverUsesContrast()
{
    QTemporaryDir directory;
    sw::Engine engine;
    sw::McpServer server(&engine);
    sw::Window window(&engine, &server, directory.filePath("settings.json"));
    window.show();
    window.setThemeColor(QColor("#4488ee"));
    window.setThemeContrast(QColor("#e06030"));

    const qreal hoverHue = window.theme.hover.hslHueF();
    const qreal accentHue = window.theme.accent.hslHueF();
    const qreal separation = qAbs(hoverHue - accentHue);
    QVERIFY2(qMin(separation, 1.0 - separation) < 0.08, qPrintable(window.theme.hover.name()));

    auto menu = window.newPageMenu();
    menu->show();
    QTest::qWait(50);
    for (auto action : menu->actions())
    {
        if (action->objectName() == "newWidgetPage") { menu->setActiveAction(action); }
    }
    QTest::qWait(50);
    capture(menu, "theme-new-page-menu");
    menu->close();
    delete menu;
    window.close();
}

void ThemeColorTests::darkPickerUsesThemePalette()
{
    QTemporaryDir directory;
    sw::Engine engine;
    sw::McpServer server(&engine);
    sw::Window window(&engine, &server, directory.filePath("settings.json"));
    window.applyTheme(true);
    window.show();
    window.openSettings();
    auto choose = window.settingsDialog->findChild<QPushButton *>("themeColorChoose");
    QVERIFY(choose);
    bool checked = false;
    QTimer::singleShot(0, [&]
    {
        auto dialog = window.settingsDialog->findChild<QColorDialog *>("themeColorDialog");
        if (!dialog)
        {
            if (auto modal = QApplication::activeModalWidget()) { modal->close(); }
            return;
        }
        checked = true;
        QVERIFY2(dialog->palette().color(QPalette::Window).lightnessF() < 0.5,
            qPrintable(dialog->palette().color(QPalette::Window).name()));
        capture(dialog, "theme-color-picker-dark");
        dialog->reject();
    });
    choose->click();
    QVERIFY(checked);
    window.close();
}

void ThemeColorTests::narrowSettingsThemeRowFits()
{
    QTemporaryDir directory;
    sw::Engine engine;
    sw::McpServer server(&engine);
    sw::Window window(&engine, &server, directory.filePath("settings.json"));
    window.show();
    window.openSettings();
    auto dialog = window.settingsDialog;
    dialog->resize(520, 380);
    QTest::qWait(60);
    auto scroll = dialog->findChild<QScrollArea *>("appearanceScroll");
    auto input = dialog->findChild<QLineEdit *>("themeColorHex");
    auto choose = dialog->findChild<QPushButton *>("themeColorChoose");
    auto apply = dialog->findChild<QPushButton *>("themeColorApply");
    auto contrastInput = dialog->findChild<QLineEdit *>("themeContrastHex");
    auto contrastChoose = dialog->findChild<QPushButton *>("themeContrastChoose");
    auto contrastApply = dialog->findChild<QPushButton *>("themeContrastApply");
    QVERIFY(scroll && input && choose && apply && contrastInput && contrastChoose && contrastApply);
    scroll->ensureWidgetVisible(input);
    QApplication::processEvents();
    for (auto widget : {static_cast<QWidget *>(choose), static_cast<QWidget *>(input), static_cast<QWidget *>(apply)})
    {
        const auto bounds = QRect(widget->mapTo(dialog, QPoint(0, 0)), widget->size());
        QVERIFY2(dialog->rect().contains(bounds), widget->objectName().toUtf8());
    }
    scroll->ensureWidgetVisible(contrastInput);
    QApplication::processEvents();
    for (auto widget : {static_cast<QWidget *>(contrastChoose), static_cast<QWidget *>(contrastInput),
             static_cast<QWidget *>(contrastApply)})
    {
        const auto bounds = QRect(widget->mapTo(dialog, QPoint(0, 0)), widget->size());
        QVERIFY2(dialog->rect().contains(bounds), widget->objectName().toUtf8());
    }
    capture(dialog, "theme-settings-narrow");
    window.close();
}

void ThemeColorTests::aboutLinksFollowAccent()
{
    QTemporaryDir directory;
    sw::Engine engine;
    sw::McpServer server(&engine);
    sw::Window window(&engine, &server, directory.filePath("settings.json"));
    window.show();
    window.openSettings();
    window.setThemeColor(QColor("#814fc4"));
    window.setThemeContrast(QColor("#814fc4"));
    auto pages = window.settingsDialog->findChild<sw::DetachableTabs *>("settingsPages");
    auto sources = window.settingsDialog->findChild<QLabel *>("aboutSources");
    QVERIFY(pages && sources);
    pages->setCurrentIndex(2);
    QApplication::processEvents();
    QCOMPARE(sources->palette().color(QPalette::Link), QColor("#814fc4"));
    capture(window.settingsDialog, "theme-about-links");
    window.close();
}

void ThemeColorTests::serialMonitorFollowsCustomAccent()
{
    QTemporaryDir directory;
    sw::Engine engine;
    sw::McpServer server(&engine);
    sw::Window window(&engine, &server, directory.filePath("settings.json"));
    window.show();
    sw::Config config;
    config.port = "loop://";
    auto channel = engine.open(config);
    auto control = window.newInstrument("control", channel);
    QVERIFY(control && control->instrument && control->instrument->controls);
    auto toggle = control->instrument->controls->findChild<QPushButton *>("controlMonitorToggle");
    QVERIFY(toggle);
    toggle->setChecked(true);
    auto monitor = control->instrument->controls->receiveMonitor();
    QVERIFY(monitor);
    window.setThemeColor(QColor("#814fc4"));
    window.setThemeContrast(QColor("#814fc4"));
    QTRY_COMPARE(monitor->logView()->theme.accent, QColor("#814fc4"));
    capture(monitor, "theme-serial-monitor");
    monitor->close();
    window.close();
}

QTEST_MAIN(ThemeColorTests)
#include "theme_color_tests.moc"
