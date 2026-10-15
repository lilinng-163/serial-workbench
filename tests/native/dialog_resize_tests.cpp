// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "json_tools.h"
#include "file_transfer_dialog.h"
#include "update_dialog.h"
#include "ssh_dialogs.h"
#include <QtTest>

class DialogResizeTests : public QObject
{
    Q_OBJECT
private:
    void capture(QWidget *widget, const QString &name)
    {
        const auto directory = qEnvironmentVariable("SW_DIALOG_CAPTURE_DIR");
        if (directory.isEmpty()) { return; }
        QDir().mkpath(directory);
        QVERIFY(widget->grab().save(directory + "/" + name + ".png"));
    }

    void compact(QDialog *dialog, const QSize &size, QWidget *action)
    {
        dialog->show();
        dialog->resize(size);
        QTRY_COMPARE_WITH_TIMEOUT(dialog->size(), size, 1000);
        if (action)
        {
            QVERIFY(action->isVisible());
            const QRect bounds(action->mapTo(dialog, QPoint()), action->size());
            QVERIFY2(dialog->rect().contains(bounds), qPrintable(dialog->windowTitle()));
        }
        auto scroll = dialog->findChild<QScrollArea *>("dialogContentScroll");
        QVERIFY(scroll);
        QCOMPARE(scroll->widget()->parentWidget(), scroll->viewport());
        auto content = dialog->findChild<QWidget *>("dialogContent");
        QVERIFY(content);
        QVERIFY(content->height() >= scroll->viewport()->height());
    }

    void reach(QDialog *dialog, QWidget *field)
    {
        auto scroll = dialog->findChild<QScrollArea *>("dialogContentScroll");
        QVERIFY(scroll && field);
        for (auto parent = field->parentWidget(); parent && parent != dialog; parent = parent->parentWidget())
        {
            if (auto enclosing = qobject_cast<QScrollArea *>(parent))
            {
                scroll = enclosing;
                break;
            }
        }
        const auto visible = [&]
        {
            const auto center = field->mapTo(scroll->widget(), field->rect().center());
            scroll->ensureVisible(center.x(), center.y(), field->width() / 2 + 1, field->height() / 2 + 1);
            const QRect bounds(field->mapTo(scroll->viewport(), QPoint()), field->size());
            return scroll->viewport()->rect().contains(bounds);
        };
        QTRY_VERIFY_WITH_TIMEOUT(visible(), 1000);
    }

private slots:
    void initTestCase()
    {
        QApplication::setStyle("Fusion");
        QApplication::setFont(sw::Theme::interfaceFont());
    }

    void settingsAllPagesAndRestore()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer server(&engine);
        const auto path = directory.filePath("settings.json");
        const QSize size(440, 320);
        {
            sw::Window window(&engine, &server, path);
            window.openSettings();
            auto dialog = window.settingsDialog;
            auto pages = dialog->findChild<sw::DetachableTabs *>("settingsPages");
            auto mcpPages = dialog->findChild<sw::DetachableTabs *>("mcpPages");
            for (int page = 0; page < pages->count(); ++page)
            {
                pages->setCurrentIndex(page);
                for (int tab = 0; tab < mcpPages->count(); ++tab)
                {
                    mcpPages->setCurrentIndex(tab);
                    dialog->resize(size);
                    QTRY_COMPARE_WITH_TIMEOUT(dialog->size(), size, 1000);
                }
            }
            pages->setCurrentIndex(1);
            mcpPages->setCurrentIndex(1);
            auto scroll = dialog->findChild<QScrollArea *>("mcpScroll");
            QVERIFY(scroll);
            QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
            scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
            capture(dialog, "settings-compact");
            auto clients = dialog->findChild<QScrollArea *>("clientsScroll");
            auto last = clients->findChild<QPushButton *>("check_gemini");
            clients->ensureWidgetVisible(last, 0, 0);
            QTRY_VERIFY(clients->viewport()->rect().contains(last->mapTo(clients->viewport(), last->rect().center())));
            mcpPages->setCurrentIndex(2);
            dialog->resize(980, 760);
            QApplication::processEvents();
            const auto expandedTable = window.auditTable->size();
            dialog->resize(size);
            QApplication::processEvents();
            QTRY_VERIFY(window.auditTable->height() < expandedTable.height() - 200);
            QCOMPARE(dialog->size(), size);
            auto done = dialog->findChildren<QPushButton *>().last();
            QVERIFY(dialog->rect().contains(QRect(done->mapTo(dialog, QPoint()), done->size())));
            dialog->accept();
            window.close();
        }
        sw::Window restored(&engine, &server, path);
        restored.openSettings();
        QTRY_COMPARE(restored.settingsDialog->size(), size);
        restored.settingsDialog->reject();
        restored.close();
    }

    void curveAndAxisSettings()
    {
        sw::WavePlot plot;
        plot.setStyleSheet(sw::Theme::make(false).stylesheet());
        QTimer::singleShot(0, &plot, [&]
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QTimer::singleShot(4000, dialog, &QDialog::reject);
            auto buttons = dialog->findChild<QDialogButtonBox *>();
            compact(dialog, QSize(400, 240), buttons->button(QDialogButtonBox::Save));
            auto width = dialog->findChild<QDoubleSpinBox *>("waveCurveWidth");
            reach(dialog, width);
            width->setValue(3.5);
            capture(dialog, "curve-compact");
            dialog->accept();
        });
        plot.openCurveSettings("temperature");
        QCOMPARE(plot.curveStyle("temperature").width, 3.5);
        QTimer::singleShot(0, &plot, [&]
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QTimer::singleShot(4000, dialog, &QDialog::reject);
            compact(dialog, QSize(480, 280), dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save));
            auto unit = dialog->findChild<QLineEdit *>("waveYUnit");
            reach(dialog, unit);
            unit->setText("°C");
            capture(dialog, "axes-compact");
            dialog->accept();
        });
        plot.openPlotSettings();
        QCOMPARE(plot.yAxisUnit(), QString("°C"));
    }

    void fileTransferAndUpdate()
    {
        QWidget parent;
        parent.setStyleSheet(sw::Theme::make(false).stylesheet());
        sw::SerialTransfer transfer;
        sw::FileTransferDialog file(&transfer, &parent);
        file.status->setText(QString("设备返回较长的状态信息。 ").repeated(25));
        compact(&file, QSize(480, 280), file.start);
        reach(&file, file.path);
        file.direction->setCurrentIndex(1);
        file.protocol->setCurrentIndex(0);
        reach(&file, file.expectedSize);
        capture(&file, "file-transfer-compact");
        file.reject();
        sw::UpdateService service;
        sw::UpdateDialog update(&service, &parent);
        compact(&update, QSize(440, 280), update.findChild<QPushButton *>("updateAction"));
        emit service.downloadProgress(123, 1000);
        QApplication::processEvents();
        QCOMPARE(update.size(), QSize(440, 280));
        capture(&update, "update-compact");
        update.hide();
    }

    void jsonAndScriptEditors()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer server(&engine);
        sw::Window window(&engine, &server, directory.filePath("settings.json"));
        auto json = sw::showJsonAssistant(&window, "{\"humi\":60}");
        compact(json, QSize(560, 320), nullptr);
        auto tabs = json->findChild<QTabWidget *>("jsonAssistantTabs");
        for (int index = 0; index < tabs->count(); ++index)
        {
            tabs->setCurrentIndex(index);
            QApplication::processEvents();
            QCOMPARE(json->size(), QSize(560, 320));
        }
        capture(json, "json-compact");
        json->reject();
        auto pane = window.newConnection();
        pane->openScriptEditor();
        auto script = pane->scriptDialog;
        QVERIFY(script);
        auto save = script->findChild<QPushButton *>("primary");
        compact(script, QSize(560, 320), save);
        reach(script, script->findChild<QPushButton *>("checkScript"));
        capture(script, "script-compact");
        script->reject();
        window.close();
    }

    void sshAuthentication_data()
    {
        QTest::addColumn<bool>("confirmation");
        QTest::newRow("password") << false;
        QTest::newRow("host-key") << true;
    }

    void sshAuthentication()
    {
        QFETCH(bool, confirmation);
        QWidget parent;
        sw::SshAuthPrompt prompt;
        prompt.host = QString("device-name-").repeated(15);
        prompt.message = QString("Fingerprint and authentication prompt. ").repeated(30);
        prompt.confirmation = confirmation;
        prompt.canRemember = !confirmation;
        sw::showSshAuthPrompt(&prompt, &parent, sw::Theme::make(false));
        auto dialog = parent.findChild<QDialog *>("sshAuthenticationDialog");
        QVERIFY(dialog);
        compact(dialog, QSize(440, 260), dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel));
        if (!confirmation) { reach(dialog, dialog->findChild<QLineEdit *>("sshPassword")); }
        capture(dialog, confirmation ? "ssh-host-compact" : "ssh-password-compact");
        dialog->reject();
    }

    void standardPickers()
    {
        QWidget parent;
        parent.setStyleSheet(sw::Theme::make(false).stylesheet());
        const auto check = [&](QDialog *dialog, const QString &name)
        {
            dialog->show();
            QApplication::processEvents();
            dialog->resize(dialog->minimumSizeHint().expandedTo(QSize(500, 360)));
            QTest::qWait(40);
            const auto small = dialog->size();
            QVERIFY(small.height() <= 560);
            dialog->resize(900, 640);
            QApplication::processEvents();
            dialog->resize(small);
            QTRY_COMPARE(dialog->size(), small);
            capture(dialog, name);
            dialog->reject();
        };
        QFontDialog font(&parent);
        font.setOption(QFontDialog::DontUseNativeDialog);
        check(&font, "font-picker-compact");
        QColorDialog color(Qt::red, &parent);
        color.setOption(QColorDialog::DontUseNativeDialog);
        check(&color, "color-picker-compact");
    }

    void sshCacheAndWaveHelp()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        engine.sshExecutable = QCoreApplication::applicationDirPath() + "/fake_ssh.exe";
        sw::Config config;
        config.kind = "ssh";
        config.sshHost = "cache-resize-test";
        QWidget parent;
        bool checked = false;
        QTimer::singleShot(0, &parent, [&]
        {
            auto dialog = parent.findChild<QDialog *>("sshCacheDialog");
            QVERIFY(dialog);
            QTimer::singleShot(4000, dialog, &QDialog::reject);
            compact(dialog, QSize(440, 260), dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel));
            reach(dialog, dialog->findChild<QCheckBox *>("sshClearFingerprints"));
            capture(dialog, "ssh-cache-compact");
            checked = true;
            dialog->reject();
        });
        sw::clearSshCache(config, &engine, &parent, sw::Theme::make(false));
        QVERIFY(checked);
        sw::McpServer server(&engine);
        sw::Window window(&engine, &server, directory.filePath("settings.json"));
        window.waveHelp();
        QDialog *help = nullptr;
        for (auto dialog : window.findChildren<QDialog *>())
        {
            if (dialog->windowTitle() == "数值报文格式") { help = dialog; }
        }
        QVERIFY(help);
        help->resize(360, 240);
        QTRY_COMPARE(help->size(), QSize(360, 240));
        QVERIFY(help->findChild<QTextBrowser *>()->verticalScrollBar()->maximum() > 0);
        capture(help, "wave-help-compact");
        help->close();
        window.close();
    }
};

QTEST_MAIN(DialogResizeTests)
#include "dialog_resize_tests.moc"
