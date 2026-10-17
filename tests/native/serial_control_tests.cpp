// SPDX-License-Identifier: Apache-2.0
#include "serial_control_panel.h"
#include "control_card.h"
#include "serial_monitor_dialog.h"
#include "window_chrome.h"
#include "window.h"
#include "instrument_config.h"
#include <QtTest>

class SerialControlTests : public QObject
{
    Q_OBJECT
private:
    static sw::ControlCard *cardFor(QWidget *widget)
    {
        for (auto parent = widget; parent; parent = parent->parentWidget())
        {
            if (auto card = qobject_cast<sw::ControlCard *>(parent)) { return card; }
        }
        return nullptr;
    }

    static QJsonArray controls(const QJsonObject &saved)
    {
        return saved["groups"].toArray().first().toObject()["controls"].toArray();
    }

    static sw::Config loop()
    {
        sw::Config config;
        config.port = "loop://";
        return config;
    }

private slots:
    void initTestCase()
    {
        QApplication::setStyle("Fusion");
        QApplication::setFont(sw::Theme::interfaceFont());
    }

    void packets_data()
    {
        QTest::addColumn<QString>("format");
        QTest::addColumn<QString>("pattern");
        QTest::addColumn<QString>("value");
        QTest::addColumn<QByteArray>("expected");
        QTest::newRow("text") << "text" << "SET {key} {value}" << "1200" << QByteArray("SET speed 1200\r\n");
        QTest::newRow("json") << "text" << "{\"speed\":{value}}" << "42" << QByteArray("{\"speed\":42}\r\n");
        QTest::newRow("u8") << "hex" << "AA {value:u8} FF" << "255" << QByteArray::fromHex("aaffff");
        QTest::newRow("i8") << "hex" << "{value:i8}" << "-128" << QByteArray::fromHex("80");
        QTest::newRow("u16le") << "hex" << "AA 55 {value:u16le} 0D 0A" << "1200" << QByteArray::fromHex("aa55b0040d0a");
        QTest::newRow("u16be") << "hex" << "{value:u16be}" << "4660" << QByteArray::fromHex("1234");
        QTest::newRow("i16") << "hex" << "{value:i16le}" << "-2" << QByteArray::fromHex("feff");
        QTest::newRow("u32") << "hex" << "{value:u32be}" << "4294967295" << QByteArray::fromHex("ffffffff");
        QTest::newRow("i32") << "hex" << "{value:i32le}" << "-2147483648" << QByteArray::fromHex("00000080");
        QTest::newRow("f32le") << "hex" << "{value:f32le}" << "1.5" << QByteArray::fromHex("0000c03f");
        QTest::newRow("f32be") << "hex" << "{value:f32be}" << "-2.5" << QByteArray::fromHex("c0200000");
    }

    void packets()
    {
        QFETCH(QString, format);
        QFETCH(QString, pattern);
        QFETCH(QString, value);
        QFETCH(QByteArray, expected);
        auto spec = sw::defaultControl("number");
        spec["key"] = "speed";
        spec["format"] = format;
        spec["pattern"] = pattern;
        spec["value"] = value;
        QCOMPARE(sw::controlPacket(spec), expected);
    }

    void rejectsBadPackets_data()
    {
        QTest::addColumn<QString>("pattern");
        QTest::addColumn<QString>("value");
        QTest::newRow("overflow") << "{value:u8}" << "256";
        QTest::newRow("negativeUnsigned") << "{value:u16le}" << "-1";
        QTest::newRow("fractionInteger") << "{value:i16be}" << "1.5";
        QTest::newRow("floatOverflow") << "{value:f32le}" << "1e40";
        QTest::newRow("nonNumeric") << "{value:u8}" << "on";
        QTest::newRow("nonFinite") << "{value:f32be}" << "nan";
        QTest::newRow("oddHex") << "A {value:u8}" << "1";
        QTest::newRow("invalidHex") << "GG" << "1";
        QTest::newRow("unknownToken") << "{other}" << "1";
        QTest::newRow("unknownType") << "{value:u64le}" << "1";
        QTest::newRow("missingEndian") << "{value:u16}" << "1";
        QTest::newRow("unclosed") << "{value:u8" << "1";
        QTest::newRow("empty") << "" << "1";
    }

    void rejectsBadPackets()
    {
        QFETCH(QString, pattern);
        QFETCH(QString, value);
        auto spec = sw::defaultControl("number");
        spec["format"] = "hex";
        spec["pattern"] = pattern;
        spec["value"] = value;
        QVERIFY_EXCEPTION_THROWN(sw::controlPacket(spec), std::runtime_error);
    }

    void encodingAndLineEndings()
    {
        auto spec = sw::defaultControl("button");
        spec["pattern"] = "温度={value}";
        spec["value"] = "25";
        spec["encoding"] = "GBK";
        spec["newline"] = "lf";
        QCOMPARE(sw::controlPacket(spec), QByteArray::fromHex("cec2b6c83d32350a"));
        spec["encoding"] = "ASCII";
        QVERIFY_EXCEPTION_THROWN(sw::controlPacket(spec), std::runtime_error);
        spec["encoding"] = "utf-8";
        spec["newline"] = "none";
        QCOMPARE(sw::controlPacket(spec), QString("温度=25").toUtf8());
        spec["pattern"] = "{typo}";
        QVERIFY_EXCEPTION_THROWN(sw::controlPacket(spec), std::runtime_error);
    }

    void manualAndSharedHistory()
    {
        sw::Engine engine;
        auto channel = engine.open(loop());
        sw::SerialControlPanel panel;
        panel.resize(800, 600);
        panel.addControl(sw::defaultControl("number"));
        panel.setChannel(channel);
        panel.show();
        QVERIFY(!panel.findChild<QCheckBox *>("controlAutoSend")->isChecked());
        panel.findChild<QDoubleSpinBox *>("controlValue")->setValue(37);
        QTest::qWait(250);
        QCOMPARE(channel->tx, 0);
        panel.findChild<QPushButton *>("sendControl")->click();
        QCOMPARE(channel->tx, 14);
        bool found = false;
        for (const auto &entry : engine.journal.read(0)["events"].toArray())
        {
            const auto event = entry.toObject();
            if (event["direction"] == "tx")
            {
                QCOMPARE(QByteArray::fromHex(event["hex"].toString().toLatin1()), QByteArray("SET value 37\r\n"));
                found = true;
            }
        }
        QVERIFY(found);
    }

    void automaticDebounceAndCancellation()
    {
        sw::Engine first;
        sw::Engine second;
        auto channel = first.open(loop());
        auto other = second.open(loop());
        sw::SerialControlPanel panel;
        panel.addControl(sw::defaultControl("slider"));
        panel.show();
        panel.setChannel(channel);
        panel.findChild<QCheckBox *>("controlAutoSend")->setChecked(true);
        QCOMPARE(channel->tx, 0);
        auto slider = panel.findChild<QSlider *>("controlSlider");
        slider->setValue(10);
        slider->setValue(20);
        slider->setValue(30);
        QTRY_COMPARE(channel->tx, 14);
        QTest::qWait(220);
        QCOMPARE(channel->tx, 14);
        slider->setValue(40);
        panel.setChannel(other);
        QTest::qWait(230);
        QCOMPARE(channel->tx, 14);
        QCOMPARE(other->tx, 0);
        slider->setValue(50);
        other->close();
        other->open();
        QTest::qWait(230);
        QCOMPARE(other->tx, 0);
        panel.setChannel(other);
        slider->setValue(60);
        panel.hide();
        QTest::qWait(230);
        QCOMPARE(other->tx, 0);
        panel.show();
        slider->setValue(70);
        panel.findChild<QCheckBox *>("controlAutoSend")->setChecked(false);
        QTest::qWait(230);
        QCOMPARE(other->tx, 0);
    }

    void busyAndWrongTransport()
    {
        sw::Engine engine;
        auto channel = engine.open(loop());
        sw::SerialControlPanel panel;
        panel.addControl(sw::defaultControl("button"));
        panel.setChannel(channel);
        auto send = panel.findChild<QPushButton *>("sendControl");
        QVERIFY(send->isEnabled());
        QObject transfer;
        channel->transferOwner = &transfer;
        send->click();
        QCOMPARE(channel->tx, 0);
        QVERIFY(panel.status->text().contains("传输文件"));
        panel.setChannel(channel);
        QVERIFY(!send->isEnabled());
        channel->transferOwner.clear();
        auto demo = engine.waveformDemo();
        panel.setChannel(demo);
        QVERIFY(!send->isEnabled());
    }

    void persistenceAndWidgets()
    {
        sw::Engine engine;
        auto channel = engine.open(loop());
        sw::SerialControlPanel panel;
        panel.addExamples();
        panel.show();
        panel.setChannel(channel);
        panel.findChild<QCheckBox *>("controlAutoSend")->setChecked(true);
        QTest::qWait(20);
        const auto saved = panel.settings();
        QCOMPARE(controls(saved).size(), 5);
        panel.restore(saved);
        QTest::qWait(220);
        QCOMPARE(channel->tx, 0);
        QCOMPARE(panel.settings(), saved);
        QVERIFY(panel.findChild<QSlider *>("controlSlider"));
        QVERIFY(panel.findChild<QCheckBox *>("controlSwitch"));
        QVERIFY(panel.findChild<QComboBox *>("controlSelect"));
        const QJsonArray pages {QJsonObject {{"kind", "control"}, {"serial_controls", saved}}};
        QCOMPARE(sw::migrateInstrumentPages(pages), pages);
    }

    void editorPreservesSettingsAndDoesNotSend()
    {
        sw::Engine engine;
        auto channel = engine.open(loop());
        sw::SerialControlPanel panel;
        auto spec = sw::defaultControl("slider");
        spec["format"] = "hex";
        spec["pattern"] = "AA {value:u16be}";
        panel.addControl(spec);
        panel.setChannel(channel);
        panel.show();
        panel.findChild<QCheckBox *>("controlAutoSend")->setChecked(true);
        panel.findChild<QSlider *>("controlSlider")->setValue(12);
        bool inspected = false;
        QTimer::singleShot(0, &panel, [&]
        {
            auto dialog = panel.findChild<QDialog *>("serialControlEditor");
            QVERIFY(dialog);
            QTimer::singleShot(1000, dialog, &QDialog::reject);
            QCOMPARE(dialog->findChild<QComboBox *>("controlFormat")->currentData().toString(), QString("hex"));
            QCOMPARE(dialog->findChild<QComboBox *>("controlNewline")->currentData().toString(), QString("crlf"));
            auto pattern = dialog->findChild<QPlainTextEdit *>("controlPattern");
            QCOMPARE(pattern->toPlainText(), QString("AA {value:u16be}"));
            pattern->setPlainText("55 {value:u16le}");
            auto save = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save);
            QVERIFY(save->isEnabled());
            dialog->resize(360, 300);
            QTest::qWait(30);
            QVERIFY(dialog->height() <= 330);
            if (qEnvironmentVariableIsSet("SERIAL_CONTROL_EDITOR_CAPTURE"))
            { QVERIFY(dialog->grab().save(qEnvironmentVariable("SERIAL_CONTROL_EDITOR_CAPTURE"))); }
            inspected = true;
            save->click();
        });
        panel.findChild<QPushButton *>("editControl")->click();
        QVERIFY(inspected);
        QTest::qWait(220);
        QCOMPARE(channel->tx, 0);
        QCOMPARE(controls(panel.settings())[0].toObject()["pattern"].toString(), QString("55 {value:u16le}"));
        QCOMPARE(controls(panel.settings())[0].toObject()["value"].toString(), QString("12"));
        panel.findChild<QPushButton *>("sendControl")->click();
        QCOMPARE(channel->tx, 3);
    }

    void fineStepCanBeSavedAndRestored()
    {
        sw::SerialControlPanel panel;
        panel.addControl(sw::defaultControl("slider"));
        panel.show();
        bool saved = false;
        QTimer::singleShot(0, &panel, [&]
        {
            auto dialog = panel.findChild<QDialog *>("serialControlEditor");
            QVERIFY(dialog);
            QTimer::singleShot(1500, dialog, &QDialog::reject);
            dialog->findChild<QSpinBox *>("controlEditorDecimals")->setValue(4);
            dialog->findChild<QDoubleSpinBox *>("controlEditor_step")->setValue(0.0001);
            auto save = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save);
            QVERIFY2(save->isEnabled(), qPrintable(dialog->findChild<QLabel *>("controlValidation")->text()));
            saved = true;
            save->click();
        });
        panel.findChild<QPushButton *>("editControl")->click();
        QVERIFY(saved);
        const auto spec = controls(panel.settings())[0].toObject();
        QCOMPARE(spec["step"].toDouble(), 0.0001);
        QCOMPARE(spec["decimals"].toInt(), 4);
        auto slider = panel.findChild<QSlider *>("controlSlider");
        QCOMPARE(slider->maximum(), 1000000);
        slider->setValue(1);
        QCOMPARE(controls(panel.settings())[0].toObject()["value"].toString(), QString("0.0001"));
        slider->setValue(slider->maximum());
        QCOMPARE(controls(panel.settings())[0].toObject()["value"].toString(), QString("100.0000"));
        const auto state = panel.settings();
        panel.restore(state);
        QCOMPARE(panel.settings(), state);
    }

    void continuousDragSendsPromptlyAndKeepsFinalValue()
    {
        sw::Engine engine;
        auto channel = engine.open(loop());
        sw::SerialControlPanel panel;
        panel.addControl(sw::defaultControl("slider"));
        panel.addControl(sw::defaultControl("number"));
        panel.setChannel(channel);
        panel.show();
        const auto modes = panel.findChildren<QCheckBox *>("controlAutoSend");
        QCOMPARE(modes.size(), 2);
        auto slider = panel.findChild<QSlider *>("controlSlider");
        auto sliderCard = cardFor(slider);
        QVERIFY(sliderCard);
        sliderCard->findChild<QCheckBox *>("controlAutoSend")->setChecked(true);
        sw::ControlCard *numberCard = nullptr;
        for (auto card : panel.findChildren<sw::ControlCard *>())
        { if (card != sliderCard) { numberCard = card; } }
        QVERIFY(numberCard);
        QVERIFY(!numberCard->findChild<QCheckBox *>("controlAutoSend")->isChecked());
        QSignalSpy received(channel, &sw::Channel::bytesReceived);
        slider->setValue(1);
        QTRY_VERIFY_WITH_TIMEOUT(!received.isEmpty(), 100);
        const auto firstBytes = channel->tx;
        QSignalSpy saves(&panel, &sw::SerialControlPanel::changed);
        for (int value = 2; value <= 40; ++value)
        {
            slider->setValue(value);
            QTest::qWait(6);
        }
        QVERIFY(channel->tx > firstBytes * 2);
        QCOMPARE(saves.count(), 0);
        QTest::qWait(100);
        QByteArray last;
        int transmissions = 0;
        for (const auto &entry : engine.journal.read(0)["events"].toArray())
        {
            const auto event = entry.toObject();
            if (event["direction"] == "tx")
            {
                ++transmissions;
                last = QByteArray::fromHex(event["hex"].toString().toLatin1());
            }
        }
        QVERIFY(transmissions >= 3 && transmissions < 20);
        QCOMPARE(last, QByteArray("SET value 40\r\n"));
        const auto sent = channel->tx;
        numberCard->findChild<QDoubleSpinBox *>("controlValue")->setValue(77);
        QTest::qWait(100);
        QCOMPARE(channel->tx, sent);
        QTRY_VERIFY(saves.count() > 0);
    }

    void groupsMigratePersistAndCancelPending()
    {
        sw::Engine engine;
        auto channel = engine.open(loop());
        auto legacy = sw::defaultControl("slider");
        legacy.remove("auto_send");
        sw::SerialControlPanel panel;
        panel.restore({{"controls", QJsonArray {legacy}}, {"auto_send", true}});
        panel.setChannel(channel);
        panel.show();
        QVERIFY(panel.findChild<QCheckBox *>("controlAutoSend")->isChecked());
        const QString first = panel.settings()["active_group"].toString();
        panel.findChild<QSlider *>("controlSlider")->setValue(17);
        const QString second = panel.addGroup("电机参数");
        QVERIFY(!second.isEmpty() && first != second);
        QTest::qWait(80);
        QCOMPARE(channel->tx, 0);
        panel.addControl(sw::defaultControl("number"));
        QVERIFY(!panel.findChild<QCheckBox *>("controlAutoSend")->isChecked());
        panel.findChild<QDoubleSpinBox *>("controlValue")->setValue(55);
        const auto saved = panel.settings();
        QCOMPARE(saved["groups"].toArray().size(), 2);
        sw::SerialControlPanel restored;
        restored.restore(saved);
        QCOMPARE(restored.settings(), saved);
        QCOMPARE(restored.settings()["active_group"].toString(), second);
        QVERIFY(restored.selectGroup(first));
        QCOMPARE(restored.findChild<QDoubleSpinBox *>("controlValue")->value(), 17.0);
        QVERIFY(restored.findChild<QCheckBox *>("controlAutoSend")->isChecked());
        QVERIFY(restored.selectGroup(second));
        QCOMPARE(restored.findChild<QDoubleSpinBox *>("controlValue")->value(), 55.0);
        QVERIFY(!restored.selectGroup("missing"));
    }

    void reorderAndMoveBetweenGroups()
    {
        sw::SerialControlPanel panel;
        const auto first = panel.settings()["active_group"].toString();
        auto a = sw::defaultControl("number");
        a["key"] = "a";
        a["title"] = "参数 A";
        panel.addControl(a);
        auto b = sw::defaultControl("number");
        b["key"] = "b";
        b["title"] = "参数 B";
        panel.addControl(b);
        panel.show();
        auto cards = panel.findChildren<sw::ControlCard *>();
        QCOMPARE(cards.size(), 2);
        QAction *raise = nullptr;
        for (auto action : cards[0]->findChildren<QAction *>())
        {
            if (action->text() == "移至最上层") { raise = action; break; }
        }
        QVERIFY(raise && raise->isEnabled());
        raise->trigger();
        QCOMPARE(controls(panel.settings())[0].toObject()["key"].toString(), QString("a"));
        const auto second = panel.addGroup("温控");
        QVERIFY(panel.selectGroup(first));
        bool inspected = false;
        QTimer::singleShot(0, &panel, [&]
        {
            auto dialog = panel.findChild<QDialog *>("serialControlEditor");
            QVERIFY(dialog);
            QTimer::singleShot(1500, dialog, &QDialog::reject);
            auto group = dialog->findChild<QComboBox *>("controlEditorGroup");
            group->setCurrentIndex(group->findData(second));
            auto save = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save);
            QVERIFY(save->isEnabled());
            inspected = true;
            save->click();
        });
        QPushButton *editA = nullptr;
        for (auto title : panel.findChildren<QLabel *>("controlCardTitle"))
        {
            if (title->text() == "参数 A") { editA = cardFor(title)->findChild<QPushButton *>("editControl"); }
        }
        QVERIFY(editA);
        editA->click();
        QVERIFY(inspected);
        auto state = panel.settings();
        QCOMPARE(state["groups"].toArray()[0].toObject()["controls"].toArray().size(), 1);
        QCOMPARE(state["groups"].toArray()[1].toObject()["controls"].toArray()[0].toObject()["key"].toString(), QString("a"));
        QVERIFY(panel.selectGroup(second));
        QCOMPARE(panel.findChildren<QDoubleSpinBox *>("controlValue").size(), 1);
    }

    void groupPopupShowsEveryEntryAndSwitchesRepeatedly()
    {
        sw::installWindowChrome();
        sw::SerialControlPanel panel;
        panel.setStyleSheet(sw::Theme::make(false).stylesheet());
        panel.resize(1000, 700);
        panel.move(20, 0);
        panel.addPresetGroup("pid");
        panel.addPresetGroup("pid");
        panel.addPresetGroup("motor");
        panel.show();
        auto picker = panel.findChild<QComboBox *>("controlGroup");
        QCOMPARE(picker->count(), 4);
        const auto before = panel.settings()["groups"].toArray();
        for (int attempt = 0; attempt < 12; ++attempt)
        {
            const int index = attempt % 4;
            const auto expected = picker->itemData(index).toString();
            picker->showPopup();
            QApplication::processEvents();
            auto popup = QApplication::activePopupWidget();
            QVERIFY(popup);
            auto fade = popup->findChild<QVariantAnimation *>("popupFade");
            if (fade) { QTRY_COMPARE(fade->state(), QAbstractAnimation::Stopped); }
            if (auto menu = qobject_cast<QMenu *>(popup))
            {
                QCOMPARE(menu->actions().size(), 4);
                for (auto action : menu->actions()) { QVERIFY(menu->rect().contains(menu->actionGeometry(action))); }
                const auto capture = qEnvironmentVariable("SERIAL_LAYOUT_CAPTURE_DIR");
                if (attempt == 0 && !capture.isEmpty())
                {
                    QDir().mkpath(capture);
                    QVERIFY(menu->grab().save(capture + "/control-group-popup.png"));
                }
                QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(menu->actions()[index]).center());
            }
            else
            {
                auto view = picker->view();
                QVERIFY(view->viewport()->rect().contains(view->visualRect(picker->model()->index(0, 0))));
                QVERIFY(view->viewport()->rect().contains(view->visualRect(picker->model()->index(3, 0))));
                QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                    view->visualRect(picker->model()->index(index, 0)).center());
            }
            QTRY_COMPARE(panel.settings()["active_group"].toString(), expected);
            QTRY_VERIFY(!QApplication::activePopupWidget());
            QCOMPARE(picker->count(), 4);
        }
        QCOMPARE(panel.settings()["groups"].toArray().size(), before.size());
        panel.close();
    }

    void freePresetFollowsViewportWidthWithoutSending()
    {
        sw::Engine engine;
        auto channel = engine.open(loop());
        sw::SerialControlPanel panel;
        panel.setChannel(channel);
        panel.setStyleSheet(sw::Theme::make(false).stylesheet());
        panel.resize(940, 700);
        panel.addPresetGroup("motor");
        panel.show();
        QTest::qWait(60);
        auto cards = panel.findChildren<sw::ControlCard *>();
        QCOMPARE(cards.size(), 6);
        const QRect first = cards[0]->geometry();
        const QRect second = cards[1]->geometry();
        const auto values = panel.settings()["groups"].toArray();
        panel.resize(1440, 800);
        QTest::qWait(60);
        QVERIFY(cards[0]->width() > first.width() + 100);
        QVERIFY(cards[1]->x() > second.x() + 100);
        QVERIFY(!cards[0]->geometry().intersects(cards[1]->geometry()));
        QCOMPARE(cards[0]->y(), first.y());
        QCOMPARE(channel->tx, 0);
        panel.resize(940, 700);
        QTest::qWait(60);
        QVERIFY(qAbs(cards[0]->width() - first.width()) <= 2);
        QVERIFY(qAbs(cards[1]->x() - second.x()) <= 2);
        QCOMPARE(panel.settings()["groups"].toArray()[0].toObject()["controls"], values[0].toObject()["controls"]);
        QCOMPARE(channel->tx, 0);
        const auto capture = qEnvironmentVariable("SERIAL_LAYOUT_CAPTURE_DIR");
        if (!capture.isEmpty())
        {
            QDir().mkpath(capture);
            QVERIFY(panel.grab().save(capture + "/control-layout.png"));
            panel.resize(1440, 800);
            QTest::qWait(60);
            QVERIFY(panel.grab().save(capture + "/control-layout-wide.png"));
        }
        panel.close();
    }

    void deleteGroupConfirmationFitsAndCancelsSafely()
    {
        sw::installWindowChrome();
        sw::SerialControlPanel panel;
        panel.setStyleSheet(sw::Theme::make(false).stylesheet());
        panel.addPresetGroup("motor");
        panel.resize(1000, 700);
        panel.show();
        QTest::qWait(60);
        QAction *remove = nullptr;
        for (auto action : panel.findChildren<QAction *>())
        {
            if (action->text() == "删除控件组") { remove = action; }
        }
        QVERIFY(remove);
        const auto before = panel.settings();
        bool inspected = false;
        QTimer::singleShot(0, &panel, [&]
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QTimer::singleShot(2000, dialog, &QDialog::reject);
            auto buttons = dialog->findChild<QDialogButtonBox *>();
            QVERIFY(buttons);
            QVERIFY(dialog->contentsRect().contains(QRect(buttons->mapTo(dialog, QPoint()), buttons->size())));
            auto confirm = buttons->button(QDialogButtonBox::Ok);
            auto cancel = buttons->button(QDialogButtonBox::Cancel);
            QVERIFY(confirm && cancel);
            QCOMPARE(confirm->text(), QString("删除控件组"));
            QCOMPARE(cancel->text(), QString("取消"));
            const auto capture = qEnvironmentVariable("SERIAL_LAYOUT_CAPTURE_DIR");
            if (!capture.isEmpty())
            {
                QDir().mkpath(capture);
                QVERIFY(dialog->grab().save(capture + "/delete-group.png"));
                if (QGuiApplication::platformName() == "windows")
                {
                    dialog->raise();
                    dialog->activateWindow();
                    QTest::qWait(150);
                    QVERIFY(dialog->screen()->grabWindow(dialog->winId()).save(capture + "/delete-group-native.png"));
                }
            }
            inspected = true;
            cancel->click();
        });
        remove->trigger();
        QVERIFY(inspected);
        QCOMPARE(panel.settings(), before);
        QTimer::singleShot(0, &panel, [&]
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        remove->trigger();
        QCOMPARE(panel.settings()["groups"].toArray().size(), 1);
        QCOMPARE(panel.findChild<QComboBox *>("controlGroup")->count(), 1);
        panel.close();
    }

    void compactGridThemeAndReopen()
    {
        sw::SerialControlPanel panel;
        panel.addExamples();
        panel.resize(900, 650);
        auto mode = panel.findChild<QComboBox *>("controlColumns");
        mode->setCurrentIndex(mode->findData(2));
        QMetaObject::invokeMethod(mode, "activated", Q_ARG(int, mode->currentIndex()));
        const auto light = sw::Theme::make(false).stylesheet();
        QVERIFY(!light.contains(QRegularExpression("\\$[a-zA-Z]+")));
        auto palette = panel.palette();
        palette.setColor(QPalette::Window, Qt::black);
        palette.setColor(QPalette::Base, Qt::black);
        panel.setPalette(palette);
        panel.setStyleSheet(light);
        panel.show();
        QTest::qWait(40);
        auto cards = panel.findChildren<QFrame *>("serialControlCard");
        QCOMPARE(cards.size(), 5);
        QCOMPARE(cards[0]->y(), cards[1]->y());
        QVERIFY(cards[1]->x() > cards[0]->x());
        QVERIFY(cards[0]->height() <= 180);
        QCOMPARE(cards[0]->height(), cards[1]->height());
        auto state = panel.settings();
        panel.setStyleSheet(sw::Theme::make(true).stylesheet());
        panel.restore(state);
        panel.setStyleSheet(sw::Theme::make(false).stylesheet());
        QTest::qWait(40);
        auto canvas = panel.findChild<QWidget *>("controlCanvas");
        const auto image = canvas->grab().toImage();
        cards = panel.findChildren<QFrame *>("serialControlCard");
        const QPoint gap(cards[0]->geometry().right() + 3, cards[0]->geometry().center().y());
        const auto color = image.pixelColor(gap * image.devicePixelRatio());
        QCOMPARE(color, sw::Theme::make(false).sidebar);
        if (qEnvironmentVariableIsSet("SERIAL_CONTROL_GRID_CAPTURE"))
        { QVERIFY(panel.grab().save(qEnvironmentVariable("SERIAL_CONTROL_GRID_CAPTURE"))); }
        panel.resize(560, 600);
        QTest::qWait(30);
        cards = panel.findChildren<QFrame *>("serialControlCard");
        QCOMPARE(cards[0]->x(), cards[1]->x());
        QVERIFY(cards[1]->y() > cards[0]->y());
    }

    void freeDragResizeDeleteAndRestore()
    {
        sw::Engine engine;
        auto channel = engine.open(loop());
        sw::SerialControlPanel panel;
        panel.resize(940, 700);
        auto spec = sw::defaultControl("slider");
        spec["auto_send"] = true;
        panel.addControl(spec);
        panel.addControl(sw::defaultControl("switch"));
        panel.setChannel(channel);
        panel.show();
        QTest::qWait(40);
        panel.addControl(sw::defaultControl("button"));
        QTest::qWait(20);
        for (auto visible : panel.findChildren<sw::ControlCard *>()) { QVERIFY(visible->isVisible()); }
        QCOMPARE(panel.findChild<QComboBox *>("controlColumns")->currentData().toInt(), 0);
        auto card = cardFor(panel.findChild<QSlider *>("controlSlider"));
        QVERIFY(card);
        auto header = card->findChild<QWidget *>("controlDragHandle");
        QVERIFY(header);
        const QRect original = card->geometry();
        auto slider = panel.findChild<QSlider *>("controlSlider");
        slider->setValue(12);
        const QPoint press = header->mapToGlobal(QPoint(8, 12));
        QTest::mousePress(header, Qt::LeftButton, Qt::NoModifier, QPoint(8, 12));
        const QPoint moved = press + QPoint(75, 55);
        QMouseEvent motion(QEvent::MouseMove, QPointF(card->mapFromGlobal(moved)), QPointF(moved), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(card, &motion);
        QTest::mouseRelease(card, Qt::LeftButton, Qt::NoModifier, card->mapFromGlobal(moved));
        QCOMPARE(card->pos(), original.topLeft() + QPoint(75, 55));
        QTest::qWait(80);
        QCOMPARE(channel->tx, 0);
        const QSize before = card->size();
        auto grip = card->findChild<QWidget *>("controlResizeGrip");
        QVERIFY(grip && grip->isVisible());
        const QPoint resizePress = grip->mapToGlobal(QPoint(8, 8));
        QTest::mousePress(grip, Qt::LeftButton, Qt::NoModifier, QPoint(8, 8));
        const QPoint resized = resizePress + QPoint(100, 65);
        QMouseEvent resizeMotion(QEvent::MouseMove, QPointF(card->mapFromGlobal(resized)), QPointF(resized), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(card, &resizeMotion);
        QTest::mouseRelease(card, Qt::LeftButton, Qt::NoModifier, card->mapFromGlobal(resized));
        QCOMPARE(card->size(), before + QSize(100, 65));
        QCOMPARE(channel->tx, 0);
        const auto saved = panel.settings();
        sw::SerialControlPanel restored;
        restored.resize(600, 500);
        restored.restore(saved);
        restored.show();
        QTest::qWait(40);
        auto copied = restored.findChildren<sw::ControlCard *>();
        QCOMPARE(copied.size(), 3);
        auto restoredCard = cardFor(restored.findChild<QSlider *>("controlSlider"));
        QVERIFY(restoredCard);
        QCOMPARE(restoredCard->geometry(), card->geometry());
        QCOMPARE(restored.settings(), saved);
        restoredCard->findChild<sw::IconButton *>("deleteControl")->click();
        QTRY_COMPARE(restored.findChildren<sw::ControlCard *>().size(), 2);
        QCOMPARE(controls(restored.settings()).size(), 2);
        for (int attempt = 0; attempt < 3; ++attempt) { restored.restore(restored.settings()); }
        QCOMPARE(restored.findChildren<sw::ControlCard *>().size(), 2);
        QCOMPARE(controls(restored.settings())[0].toObject()["type"].toString(), QString("switch"));
    }

    void groupDialogFitsAndUsesChineseButtons()
    {
        sw::installWindowChrome();
        sw::updateWindowChrome(sw::Theme::make(false), {}, {});
        sw::SerialControlPanel panel;
        panel.show();
        bool inspected = false;
        QTimer::singleShot(0, &panel, [&]
        {
            auto dialog = panel.findChild<QDialog *>("controlGroupNameDialog");
            QVERIFY(dialog);
            QTimer::singleShot(1500, dialog, &QDialog::reject);
            auto entry = dialog->findChild<QLineEdit *>("controlGroupName");
            auto buttons = dialog->findChild<QDialogButtonBox *>();
            QVERIFY(entry && buttons);
            entry->setText("电机控制参数");
            auto confirm = buttons->button(QDialogButtonBox::Ok);
            QCOMPARE(confirm->text(), QString("确定"));
            QCOMPARE(buttons->button(QDialogButtonBox::Cancel)->text(), QString("取消"));
            QVERIFY(dialog->contentsRect().contains(QRect(buttons->mapTo(dialog, QPoint()), buttons->size())));
            QVERIFY(!entry->geometry().intersects(buttons->geometry()));
            if (qEnvironmentVariableIsSet("SERIAL_GROUP_DIALOG_CAPTURE"))
            { QVERIFY(dialog->grab().save(qEnvironmentVariable("SERIAL_GROUP_DIALOG_CAPTURE"))); }
            inspected = true;
            confirm->click();
        });
        panel.findChild<QPushButton *>("addControlGroup")->click();
        QVERIFY(inspected);
        QCOMPARE(panel.settings()["groups"].toArray().size(), 2);
        QCOMPARE(panel.findChild<QComboBox *>("controlGroup")->currentText(), QString("电机控制参数"));
    }

    void presetGroupsPreserveControlsAndDoNotSend()
    {
        sw::Engine engine;
        auto channel = engine.open(loop());
        sw::SerialControlPanel panel;
        panel.setChannel(channel);
        panel.addControl(sw::defaultControl("number"));
        const auto originalGroup = panel.settings()["active_group"].toString();
        const auto originalControls = controls(panel.settings());
        const QStringList ids {"pid", "motor", "temperature", "pwm"};
        QSet<QString> groupIds {originalGroup};
        for (const auto &id : ids)
        {
            auto menuAction = panel.findChild<QAction *>("controlPreset_" + id);
            QVERIFY(menuAction);
            menuAction->trigger();
            const auto state = panel.settings();
            const auto active = state["active_group"].toString();
            QVERIFY(!groupIds.contains(active));
            groupIds.insert(active);
            const auto group = state["groups"].toArray().last().toObject();
            QCOMPARE(group["id"].toString(), active);
            const auto items = group["controls"].toArray();
            QVERIFY(items.size() >= 5);
            QSet<QString> keys;
            for (const auto &entry : items)
            {
                const auto spec = entry.toObject();
                QVERIFY(!spec["auto_send"].toBool());
                QVERIFY(!keys.contains(spec["key"].toString()));
                keys.insert(spec["key"].toString());
                const auto bytes = sw::controlPacket(spec);
                QVERIFY(bytes.startsWith("SET ") && bytes.endsWith("\r\n"));
                if (spec["type"].toString() == "number" || spec["type"].toString() == "slider")
                {
                    QVERIFY(spec["maximum"].toDouble() > spec["minimum"].toDouble());
                    const auto value = spec["value"].toString().toDouble();
                    QVERIFY(value >= spec["minimum"].toDouble() && value <= spec["maximum"].toDouble());
                }
            }
        }
        QCOMPARE(panel.settings()["groups"].toArray().size(), 5);
        QVERIFY(panel.selectGroup(originalGroup));
        QCOMPARE(controls(panel.settings()), originalControls);
        QVERIFY(panel.addPresetGroup("pid"));
        QCOMPARE(panel.settings()["groups"].toArray().last().toObject()["name"].toString(), QString("PID 调参（2）"));
        const auto state = panel.settings();
        QVERIFY(!panel.addPresetGroup("unknown"));
        QCOMPARE(panel.settings(), state);
        sw::SerialControlPanel restored;
        restored.restore(state);
        QCOMPARE(restored.settings(), state);
        QTest::qWait(100);
        QCOMPARE(channel->tx, 0);
    }

    void presetsRespectGroupLimit()
    {
        sw::SerialControlPanel panel;
        for (int i = 1; i < 32; ++i) { QVERIFY(!panel.addGroup(QString::number(i)).isEmpty()); }
        const auto state = panel.settings();
        QVERIFY(!panel.addPresetGroup("pid"));
        QCOMPARE(panel.settings(), state);
        QVERIFY(panel.status->text().contains("32"));
    }

    void clearControlsScopeCancellationAndPendingSends()
    {
        sw::Engine engine;
        auto channel = engine.open(loop());
        sw::SerialControlPanel panel;
        panel.resize(900, 600);
        panel.addExamples();
        panel.setChannel(channel);
        panel.show();
        const auto first = panel.settings()["active_group"].toString();
        QVERIFY(panel.addPresetGroup("pid"));
        const auto second = panel.settings()["active_group"].toString();
        channel->receive("KEEP_HISTORY");
        const auto history = engine.journal.read(0, 1000, channel->id);
        QTest::qWait(40);
        auto original = panel.settings();
        QTimer::singleShot(0, &panel, [&]
        {
            auto dialog = panel.findChild<QDialog *>("clearControlsDialog");
            QVERIFY(dialog);
            QTimer::singleShot(1000, dialog, &QDialog::reject);
            QVERIFY(dialog->findChild<QLabel *>("clearControlsSummary")->text().contains("13"));
            dialog->reject();
        });
        panel.findChild<QPushButton *>("clearAllControls")->click();
        QCOMPARE(panel.settings(), original);
        const auto approve = [&]
        {
            QTimer::singleShot(0, &panel, [&]
            {
                auto dialog = panel.findChild<QDialog *>("clearControlsDialog");
                QVERIFY(dialog);
                QTimer::singleShot(1000, dialog, &QDialog::reject);
                dialog->findChild<QPushButton *>("confirmClearControls")->click();
            });
        };
        QVERIFY(!panel.findChild<QAction *>("clearCurrentControlGroup"));
        QCOMPARE(panel.settings()["active_group"].toString(), second);
        QCOMPARE(panel.settings()["groups"].toArray().size(), 2);
        QCOMPARE(panel.settings()["groups"].toArray().last().toObject()["controls"].toArray().size(), 8);
        QCOMPARE(controls(panel.settings()).size(), 5);
        for (auto layer : panel.findChildren<QToolButton *>("controlLayerMenu"))
        {
            QCOMPARE(layer->text(), QString("图层"));
            QCOMPARE(layer->menu()->actions().size(), 2);
        }
        QVERIFY(panel.selectGroup(first));
        panel.findChild<QCheckBox *>("controlAutoSend")->setChecked(true);
        panel.findChild<QSlider *>("controlSlider")->setValue(99);
        approve();
        panel.findChild<QPushButton *>("clearAllControls")->click();
        QTest::qWait(120);
        QCOMPARE(channel->tx, 0);
        QCOMPARE(channel->state, QString("connected"));
        QCOMPARE(engine.journal.read(0, 1000, channel->id), history);
        const auto empty = panel.settings();
        QCOMPARE(empty["groups"].toArray().size(), 2);
        for (const auto &group : empty["groups"].toArray())
        { QVERIFY(group.toObject()["controls"].toArray().isEmpty()); }
        sw::SerialControlPanel restored;
        restored.restore(empty);
        QCOMPARE(restored.settings(), empty);
        panel.clearControls();
        QVERIFY(panel.status->text().contains("没有控件"));
    }

    void presetAndClearDialogLayout()
    {
        sw::SerialControlPanel panel;
        panel.resize(950, 850);
        const auto theme = sw::Theme::make(false);
        sw::updateWindowChrome(theme, {}, {});
        panel.setStyleSheet(theme.stylesheet());
        panel.setTheme(theme);
        QVERIFY(panel.addPresetGroup("pid"));
        panel.show();
        QTest::qWait(40);
        panel.findChild<QPushButton *>("tidyControls")->click();
        QTest::qWait(30);
        if (qEnvironmentVariableIsSet("SERIAL_PRESET_CAPTURE"))
        { QVERIFY(panel.grab().save(qEnvironmentVariable("SERIAL_PRESET_CAPTURE"))); }
        panel.resize(560, 600);
        QTest::qWait(30);
        for (const auto &name : {"addSerialControl", "addControlExamples", "controlMonitorToggle", "clearAllControls", "tidyControls"})
        {
            auto button = panel.findChild<QPushButton *>(name);
            QVERIFY(button);
            QVERIFY(panel.contentsRect().contains(QRect(button->mapTo(&panel, QPoint()), button->size())));
        }
        bool inspected = false;
        QTimer::singleShot(0, &panel, [&]
        {
            auto dialog = panel.findChild<QDialog *>("clearControlsDialog");
            QVERIFY(dialog);
            QTimer::singleShot(2000, dialog, &QDialog::reject);
            dialog->resize(340, 230);
            QTest::qWait(30);
            auto buttons = dialog->findChild<QDialogButtonBox *>();
            QVERIFY(buttons->button(QDialogButtonBox::Cancel)->isDefault());
            QVERIFY(dialog->contentsRect().contains(buttons->geometry()));
            if (qEnvironmentVariableIsSet("SERIAL_CLEAR_CAPTURE"))
            { QVERIFY(dialog->grab().save(qEnvironmentVariable("SERIAL_CLEAR_CAPTURE"))); }
            inspected = true;
            QTest::keyClick(dialog, Qt::Key_Return);
        });
        panel.findChild<QPushButton *>("clearAllControls")->click();
        QVERIFY(inspected);
        QCOMPARE(panel.findChildren<sw::ControlCard *>().size(), 8);
    }

    void editorThemeBackground_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }

    void editorThemeBackground()
    {
        QFETCH(bool, dark);
        const auto theme = sw::Theme::make(dark);
        sw::updateWindowChrome(theme, {}, {});
        sw::SerialControlPanel panel;
        panel.setStyleSheet(theme.stylesheet());
        panel.setTheme(theme);
        panel.addControl(sw::defaultControl("slider"));
        panel.show();
        bool inspected = false;
        QTimer::singleShot(0, &panel, [&]
        {
            auto dialog = panel.findChild<QDialog *>("serialControlEditor");
            QVERIFY(dialog);
            QTimer::singleShot(2000, dialog, &QDialog::reject);
            dialog->resize(620, 620);
            auto content = dialog->findChild<QWidget *>("controlEditorContent");
            auto scroll = dialog->findChild<QScrollArea *>("controlEditorScroll");
            QVERIFY(content && scroll);
            QTest::qWait(40);
            const auto capture = content->grab().toImage();
            QCOMPARE(capture.pixelColor(QPoint(2, 2) * capture.devicePixelRatio()), theme.background);
            scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
            QTest::qWait(20);
            if (qEnvironmentVariableIsSet("SERIAL_CONTROL_EDITOR_THEME_CAPTURE"))
            {
                QVERIFY(dialog->grab().save(qEnvironmentVariable("SERIAL_CONTROL_EDITOR_THEME_CAPTURE")
                    + (dark ? "-dark.png" : "-light.png")));
            }
            inspected = true;
            dialog->reject();
        });
        panel.findChild<QPushButton *>("editControl")->click();
        QVERIFY(inspected);
    }

    void monitorMirrorsControlsAndPreservesHistory()
    {
        sw::Engine engine;
        auto channel = engine.open(loop());
        sw::SerialControlPanel panel;
        panel.addControl(sw::defaultControl("number"));
        panel.setChannel(channel);
        panel.show();
        panel.findChild<QPushButton *>("controlMonitorToggle")->click();
        auto monitor = panel.findChild<sw::SerialMonitorDialog *>();
        QVERIFY(monitor && monitor->isVisible());
        QVERIFY(!monitor->isModal());
        auto log = monitor->findChild<sw::LogView *>("serialMonitorLog");
        panel.findChild<QDoubleSpinBox *>("controlValue")->setValue(42);
        panel.findChild<QPushButton *>("sendControl")->click();
        QTRY_VERIFY(log->toPlainText().count("SET value 42") >= 2);
        QVERIFY(log->toPlainText().contains("TX"));
        QVERIFY(log->toPlainText().contains("RX"));
        const auto history = engine.journal.read(0, 1000, channel->id);
        monitor->refresh();
        QCOMPARE(engine.journal.read(0, 1000, channel->id), history);
        monitor->findChild<sw::IconButton *>("serialMonitorPause")->setChecked(true);
        channel->send("PAUSED_SAMPLE\r\n");
        QTest::qWait(80);
        QVERIFY(!log->toPlainText().contains("PAUSED_SAMPLE"));
        QVERIFY(channel->rx > 0);
        monitor->findChild<sw::IconButton *>("serialMonitorPause")->setChecked(false);
        QTRY_VERIFY(log->toPlainText().contains("PAUSED_SAMPLE"));
        const auto beforeClear = engine.journal.read(0, 1000, channel->id);
        monitor->findChild<sw::IconButton *>("serialMonitorClear")->click();
        QVERIFY(log->toPlainText().isEmpty());
        QCOMPARE(engine.journal.read(0, 1000, channel->id), beforeClear);
        monitor->findChild<sw::IconButton *>("serialMonitorHex")->setChecked(true);
        QVERIFY(!log->toPlainText().contains("50 41 55 53 45 44"));
        channel->send(QByteArray::fromHex("aa5501"));
        QTRY_VERIFY(log->toPlainText().contains("AA 55 01"));
    }

    void monitorSourceIsolationManualSendAndBusyPort()
    {
        sw::Engine engine;
        sw::Engine otherEngine;
        auto first = engine.open(loop());
        auto other = otherEngine.open(loop());
        first->receive("FIRST_ONLY\r\n");
        other->receive("OTHER_ONLY\r\n");
        sw::SerialMonitorDialog monitor;
        monitor.setChannel(first);
        monitor.show();
        auto log = monitor.findChild<sw::LogView *>("serialMonitorLog");
        QTRY_VERIFY(log->toPlainText().contains("FIRST_ONLY"));
        QVERIFY(!log->toPlainText().contains("OTHER_ONLY"));
        monitor.setChannel(other);
        QTRY_VERIFY(log->toPlainText().contains("OTHER_ONLY"));
        QVERIFY(!log->toPlainText().contains("FIRST_ONLY"));
        auto input = monitor.findChild<QLineEdit *>("serialMonitorPayload");
        input->setText("STATUS?");
        monitor.findChild<QPushButton *>("serialMonitorSend")->click();
        QTRY_VERIFY(log->toPlainText().count("STATUS?") >= 2);
        QCOMPARE(first->tx, 0);
        QCOMPARE(other->tx, 9);
        QObject transfer;
        other->transferOwner = &transfer;
        monitor.refresh();
        QVERIFY(!monitor.findChild<QPushButton *>("serialMonitorSend")->isEnabled());
        QMetaObject::invokeMethod(input, "returnPressed");
        QCOMPARE(other->tx, 9);
        QVERIFY(monitor.findChild<QLabel *>("serialMonitorMessage")->text().contains("传输文件"));
        monitor.setChannel(engine.waveformDemo());
        QVERIFY(log->toPlainText().isEmpty());
        QVERIFY(!monitor.findChild<QPushButton *>("serialMonitorSend")->isEnabled());
        monitor.setChannel(other);
        otherEngine.channels.remove(other->id);
        delete other;
        monitor.refresh();
        QVERIFY(log->toPlainText().isEmpty());
    }

    void monitorReopenAndOptionsPersist()
    {
        sw::SerialControlPanel panel;
        panel.resize(900, 600);
        panel.show();
        auto toggle = panel.findChild<QPushButton *>("controlMonitorToggle");
        toggle->click();
        auto monitor = panel.findChild<sw::SerialMonitorDialog *>();
        QVERIFY(monitor);
        monitor->resize(620, 380);
        monitor->findChild<sw::IconButton *>("serialMonitorHex")->setChecked(true);
        monitor->findChild<QComboBox *>("serialMonitorEncoding")->setCurrentText("GBK");
        monitor->close();
        QVERIFY(!toggle->isChecked());
        const auto state = panel.settings();
        QVERIFY(!state["monitor"].toObject()["geometry"].toString().isEmpty());
        toggle->click();
        QVERIFY(monitor->isVisible());
        QCOMPARE(monitor->size(), QSize(620, 380));
        sw::SerialControlPanel restored;
        restored.restore(state);
        QVERIFY(!restored.findChild<sw::SerialMonitorDialog *>());
        restored.show();
        restored.findChild<QPushButton *>("controlMonitorToggle")->click();
        auto copied = restored.findChild<sw::SerialMonitorDialog *>();
        QVERIFY(copied && copied->isVisible());
        QVERIFY(copied->findChild<sw::IconButton *>("serialMonitorHex")->isChecked());
        QCOMPARE(copied->findChild<QComboBox *>("serialMonitorEncoding")->currentText(), QString("GBK"));
        QCOMPARE(copied->size(), QSize(620, 380));
        copied->resize(400, 300);
        QTest::qWait(30);
        QVERIFY(copied->contentsRect().contains(QRect(copied->findChild<QPushButton *>("serialMonitorSend")->mapTo(copied, QPoint()),
            copied->findChild<QPushButton *>("serialMonitorSend")->size())));
    }

    void monitorWindowWithControlsAndGlobalFont()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.resize(1220, 880);
        auto channel = engine.open(loop());
        auto page = window.newInstrument("control", channel);
        page->instrument->controls->addExamples();
        window.show();
        QTest::qWait(40);
        page->instrument->controls->findChild<QPushButton *>("tidyControls")->click();
        page->instrument->controls->findChild<QPushButton *>("controlMonitorToggle")->click();
        auto monitor = page->instrument->controls->findChild<sw::SerialMonitorDialog *>();
        QVERIFY(monitor && monitor->isVisible());
        auto log = monitor->findChild<sw::LogView *>("serialMonitorLog");
        channel->send("SET speed 1200\r\n");
        channel->receive("speed=1200,temperature=25.0\r\n");
        QTRY_VERIFY(log->toPlainText().contains("temperature=25.0"));
        auto font = window.dataFont;
        font.setPointSize(14);
        window.setDataFont(font);
        QCOMPARE(log->font().pointSize(), 14);
        const auto history = engine.journal.read(0, 1000, channel->id);
        monitor->findChild<sw::IconButton *>("serialMonitorClear")->click();
        QCOMPARE(engine.journal.read(0, 1000, channel->id), history);
        auto receive = window.findPane(channel);
        QVERIFY(receive);
        window.tabs->setCurrentIndex(window.panes.indexOf(receive));
        window.activate(window.tabs->currentIndex());
        receive->tick();
        QTRY_VERIFY(receive->log->toPlainText().contains("SET speed 1200"));
        window.tabs->setCurrentIndex(window.panes.indexOf(page));
        window.activate(window.tabs->currentIndex());
        channel->send("SET temperature 25.0\r\n");
        QTRY_VERIFY(log->toPlainText().contains("SET temperature 25.0"));
        QTest::qWait(80);
        if (qEnvironmentVariableIsSet("SERIAL_CONTROL_STYLE_CAPTURE"))
        { QVERIFY(window.grab().save(qEnvironmentVariable("SERIAL_CONTROL_STYLE_CAPTURE"))); }
        if (qEnvironmentVariableIsSet("SERIAL_CONTROL_MONITOR_CAPTURE"))
        { QVERIFY(monitor->grab().save(qEnvironmentVariable("SERIAL_CONTROL_MONITOR_CAPTURE"))); }
    }

    void windowPageAndRestore()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.json");
        {
            sw::Engine engine;
            sw::McpServer mcp(&engine);
            sw::Window window(&engine, &mcp, path);
            auto demo = engine.waveformDemo();
            auto page = window.newInstrument("control", demo);
            QVERIFY(page);
            QVERIFY(!page->channel);
            QVERIFY(page->sourceKey.isEmpty());
            QCOMPARE(page->pageTitle, QString("串口控制"));
            auto channel = engine.open(loop());
            page->sourceKey = channel->config.key();
            page->refreshSource();
            QCOMPARE(page->channel.data(), channel);
            page->instrument->controls->addExamples();
            for (int i = 1; i < page->instrument->source->count(); ++i)
            { QVERIFY(page->instrument->source->itemData(i).toString().startsWith("serial:")); }
            window.saveSettings();
        }
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window restored(&engine, &mcp, path);
        sw::Pane *page = nullptr;
        for (auto pane : restored.panes)
        { if (pane->instrument && pane->instrument->controls) { page = pane; } }
        QVERIFY(page);
        QCOMPARE(controls(page->instrument->controls->settings()).size(), 5);
        QCOMPARE(engine.channels.size(), 0);
        for (int i = 0; i < restored.tabs->count(); ++i)
        {
            if (restored.tabs->tabText(i) == "串口控制") { restored.tabs->setCurrentIndex(i); restored.activate(i); break; }
        }
        restored.resize(1180, 880);
        restored.show();
        QTest::qWait(100);
        for (auto mode : page->instrument->controls->findChildren<QCheckBox *>("controlAutoSend"))
        {
            QVERIFY(!mode->isWindow());
            QVERIFY(!mode->testAttribute(Qt::WA_NativeWindow));
        }
        if (qEnvironmentVariableIsSet("SERIAL_CONTROL_CAPTURE"))
        { QVERIFY(restored.grab().save(qEnvironmentVariable("SERIAL_CONTROL_CAPTURE"))); }
    }
};

QTEST_MAIN(SerialControlTests)
#include "serial_control_tests.moc"
