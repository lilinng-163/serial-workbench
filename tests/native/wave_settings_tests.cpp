// SPDX-License-Identifier: Apache-2.0

#include "window.h"
#include <QtTest>
#include <cmath>

class WaveSettingsTests : public QObject
{
    Q_OBJECT
private:
    void capture(QWidget *widget, const QString &name)
    {
        const auto directory = qEnvironmentVariable("SW_WAVE_SETTINGS_CAPTURE_DIR");
        if (directory.isEmpty()) { return; }
        QDir().mkpath(directory);
        QVERIFY(widget->grab().save(directory + "/" + name + ".png"));
    }

    void sample(sw::WavePlot &plot)
    {
        QJsonArray samples;
        for (int index = 1; index <= 120; ++index)
        {
            samples.append(QJsonObject {{"index", index}, {"values", QJsonObject {
                {"temperature", 25 + std::sin(index * 0.15)}, {"voltage", 25 + std::cos(index * 0.15)}}}});
        }
        plot.setSamples({{"sources", QJsonObject {{"default", QJsonObject {{"samples", samples}}}}}});
        plot.setFont(sw::Theme::defaultDataFont());
        plot.resize(1000, 600);
        plot.show();
        QApplication::processEvents();
    }

private slots:
    void initTestCase()
    {
        QApplication::setStyle("Fusion");
        QApplication::setFont(sw::Theme::interfaceFont());
    }

    void readoutsStayFixedAndLimitToFour()
    {
        sw::WavePlot plot;
        plot.setStyleSheet(plot.theme.stylesheet());
        plot.resize(1100, 500);
        plot.show();
        const auto samples = [&](double value)
        {
            QJsonObject values;
            for (int index = 1; index <= 12; ++index) { values[QString("ch%1").arg(index)] = value; }
            return QJsonObject {{"sources", QJsonObject {{"default", QJsonObject {{"samples", QJsonArray {
                QJsonObject {{"index", 1}, {"values", values}}
            }}}}}}};
        };
        plot.setSamples(samples(9));
        QApplication::processEvents();
        QCOMPARE(plot.readoutChannels(), QStringList({"ch1", "ch2", "ch3", "ch4"}));
        QCOMPARE(plot.series.size(), 12);
        QMap<QString, QRect> geometry;
        for (auto button : plot.findChildren<QToolButton *>("waveChannel"))
        {
            geometry[button->property("channelName").toString()] = button->geometry();
        }
        QCOMPARE(geometry.size(), 4);
        for (double value : {-123456789.12345, 1e-200, 1e200, 0.0, 999.0})
        {
            plot.setSamples(samples(value));
            QApplication::processEvents();
            for (auto button : plot.findChildren<QToolButton *>("waveChannel"))
            {
                QCOMPARE(button->geometry(), geometry[button->property("channelName").toString()]);
            }
        }
        plot.setReadoutChannels({"ch6", "ch2", "ch12", "ch8", "ch9"});
        QCOMPARE(plot.readoutChannels(), QStringList({"ch6", "ch2", "ch12", "ch8"}));
        QCOMPARE(plot.findChildren<QToolButton *>("waveChannel").size(), 4);
        capture(&plot, "fixed-four-readouts");
        sw::WavePlot restored;
        restored.restorePresentation(plot.presentationSettings());
        restored.setSamples(samples(7));
        QCOMPARE(restored.readoutChannels(), plot.readoutChannels());
        plot.setReadoutChannels({});
        QVERIFY(plot.findChildren<QToolButton *>("waveChannel").isEmpty());
    }

    void readoutPickerSaveCancelAndDuplicates()
    {
        sw::WavePlot plot;
        sample(plot);
        const auto initial = plot.presentationSettings();
        QTimer::singleShot(0, &plot, [&]
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QTimer::singleShot(1000, dialog, &QDialog::reject);
            auto first = dialog->findChild<QComboBox *>("waveReadoutSlot0");
            first->setCurrentIndex(first->findData("voltage"));
            auto buttons = dialog->findChild<QDialogButtonBox *>();
            QVERIFY(!buttons->button(QDialogButtonBox::Save)->isEnabled());
            first->setCurrentIndex(0);
            QVERIFY(buttons->button(QDialogButtonBox::Save)->isEnabled());
            capture(dialog, "readout-picker");
            buttons->button(QDialogButtonBox::Save)->click();
        });
        plot.openReadoutChannels();
        QCOMPARE(plot.readoutChannels(), QStringList({"voltage"}));
        const auto saved = plot.presentationSettings();
        QTimer::singleShot(0, &plot, []
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->findChild<QComboBox *>("waveReadoutSlot0")->setCurrentIndex(0);
            dialog->reject();
        });
        plot.openReadoutChannels();
        QCOMPARE(plot.presentationSettings(), saved);
        QTimer::singleShot(0, &plot, []
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::RestoreDefaults)->click();
        });
        plot.openReadoutChannels();
        QCOMPARE(plot.presentationSettings(), initial);
    }

    void readoutDialogGrowthKeepsHeaderAndRows()
    {
        sw::WavePlot plot;
        sample(plot);
        plot.setStyleSheet(plot.theme.stylesheet());
        QTimer::singleShot(0, &plot, [&]
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            QApplication::processEvents();
            const auto note = dialog->findChild<QLabel *>("waveReadoutNote");
            QVERIFY(note);
            const auto noteRect = note->geometry();
            QList<QRect> rows;
            for (int slot = 0; slot < 4; ++slot)
            {
                rows.append(dialog->findChild<QComboBox *>(QString("waveReadoutSlot%1").arg(slot))->geometry());
            }
            auto buttons = dialog->findChild<QDialogButtonBox *>();
            const int bottomGap = dialog->height() - buttons->geometry().bottom();
            dialog->resize(dialog->width(), dialog->height() + 260);
            QApplication::processEvents();
            QCOMPARE(note->geometry(), noteRect);
            for (int slot = 0; slot < 4; ++slot)
            {
                QCOMPARE(dialog->findChild<QComboBox *>(QString("waveReadoutSlot%1").arg(slot))->geometry(), rows[slot]);
            }
            QCOMPARE(dialog->height() - buttons->geometry().bottom(), bottomGap);
            auto fourth = dialog->findChild<QComboBox *>("waveReadoutSlot3");
            fourth->setCurrentIndex(fourth->findData("temperature"));
            QApplication::processEvents();
            QVERIFY(!buttons->button(QDialogButtonBox::Save)->isEnabled());
            for (int slot = 0; slot < 4; ++slot)
            {
                QCOMPARE(dialog->findChild<QComboBox *>(QString("waveReadoutSlot%1").arg(slot))->geometry(), rows[slot]);
            }
            capture(dialog, "readout-picker-tall-error");
            dialog->reject();
        });
        plot.openReadoutChannels();
    }

    void stylesUnitsAndRawInspection()
    {
        sw::WavePlot plot;
        sample(plot);
        const auto original = plot.series;
        auto settings = plot.presentationSettings();
        settings["defaults"] = QJsonObject {{"width", 1.5}, {"line", int(Qt::DashLine)}};
        settings["x_title"] = "时间";
        settings["x_unit"] = "ms";
        settings["x_increment"] = 20;
        settings["y_title"] = "温度";
        settings["y_unit"] = "°C";
        plot.restorePresentation(settings);
        plot.setCurveStyle("temperature", {QColor("#f07440"), 3, Qt::DotLine});
        QCOMPARE(plot.curveStyle("voltage").width, 1.5);
        QCOMPARE(plot.curveStyle("voltage").line, Qt::DashLine);
        QCOMPARE(plot.curveColor("temperature"), QColor("#f07440"));
        QCOMPARE(plot.xAxisLabel(), QString("时间 (ms)"));
        QCOMPARE(plot.yAxisLabel(), QString("温度 (°C)"));
        QCOMPARE(plot.xAxisValue(25), 500.0);
        QCOMPARE(plot.series, original);
        const auto area = plot.plotArea();
        const auto point = original["temperature"][49];
        const auto horizontal = plot.horizontalRange();
        const auto vertical = plot.verticalRange();
        const QPointF position(area.left() + (point.x() - horizontal.first) / (horizontal.second - horizontal.first) * area.width(),
            area.bottom() - (point.y() - vertical.first) / (vertical.second - vertical.first) * area.height());
        const auto hit = plot.inspect(position);
        QCOMPARE(hit["sample"].toInt(), 50);
        QCOMPARE(hit["value"].toDouble(), point.y());
        QMouseEvent move(QEvent::MouseMove, position, plot.mapToGlobal(position.toPoint()), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&plot, &move);
        capture(&plot, "wave-custom-light");
        plot.theme = sw::Theme::make(true);
        plot.invalidatePlot();
        capture(&plot, "wave-custom-dark");
        plot.setYAxisVisible(false);
        plot.setXAxisVisible(false);
        QVERIFY(plot.plotArea().left() < area.left());
        QCOMPARE(plot.yAxisUnit(), QString("°C"));
        sw::WavePlot restored;
        restored.restorePresentation(plot.presentationSettings());
        QCOMPARE(restored.presentationSettings(), plot.presentationSettings());
        plot.resetCurveStyle("temperature");
        QCOMPARE(plot.curveStyle("temperature").width, 1.5);
        QVERIFY(!plot.curveStyle("temperature").color.isValid());
    }

    void dialogsAndClickableUnits()
    {
        sw::WavePlot plot;
        sample(plot);
        plot.setStyleSheet(plot.theme.stylesheet());
        QSignalSpy changed(&plot, &sw::WavePlot::presentationChanged);
        QTimer::singleShot(0, &plot, [&]
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) { return; }
            QTimer::singleShot(2000, dialog, &QDialog::reject);
            dialog->findChild<QLineEdit *>("waveXTitle")->setText("时间");
            dialog->findChild<QLineEdit *>("waveXUnit")->setText("ms");
            dialog->findChild<QDoubleSpinBox *>("waveXIncrement")->setValue(20);
            dialog->findChild<QLineEdit *>("waveYTitle")->setText("温度");
            for (auto button : dialog->findChildren<QToolButton *>("waveYUnitPreset"))
            {
                if (button->text() == "°C") { button->click(); }
            }
            QCOMPARE(dialog->findChild<QLineEdit *>("waveYUnit")->text(), QString("°C"));
            capture(dialog, "wave-axis-dialog");
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        });
        plot.openPlotSettings();
        QCOMPARE(changed.size(), 1);
        QCOMPARE(plot.xAxisValue(5), 100.0);
        QCOMPARE(plot.yAxisLabel(), QString("温度 (°C)"));
        const auto saved = plot.presentationSettings();
        QTimer::singleShot(0, &plot, []
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) { return; }
            dialog->findChild<QLineEdit *>("waveXUnit")->setText("µs");
            dialog->reject();
        });
        plot.openPlotSettings();
        QCOMPARE(plot.presentationSettings(), saved);
        QTimer::singleShot(0, &plot, [&]
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) { return; }
            dialog->findChild<QCheckBox *>("waveInheritStyle")->setChecked(false);
            dialog->findChild<QCheckBox *>("waveAutomaticColor")->setChecked(false);
            dialog->findChild<QDoubleSpinBox *>("waveCurveWidth")->setValue(3.5);
            dialog->findChild<QComboBox *>("waveCurveLine")->setCurrentIndex(1);
            capture(dialog, "wave-channel-dialog");
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        });
        plot.openCurveSettings("temperature");
        QCOMPARE(plot.curveStyle("temperature").width, 3.5);
        QCOMPARE(plot.curveStyle("temperature").line, Qt::DashLine);
        QVERIFY(plot.curveStyle("temperature").color.isValid());
        QToolButton *legend = nullptr;
        for (auto button : plot.findChildren<QToolButton *>("waveChannel"))
        {
            if (button->property("channelName") == "temperature") { legend = button; }
        }
        QVERIFY(legend);
        QTimer::singleShot(0, &plot, [&]
        {
            auto menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (!menu) { return; }
            auto action = menu->findChild<QAction *>("resetWaveCurve");
            if (action) { action->trigger(); }
            menu->close();
        });
        QMetaObject::invokeMethod(legend, "customContextMenuRequested", Q_ARG(QPoint, QPoint(4, 4)));
        QCOMPARE(plot.curveStyle("temperature").width, 1.0);
        QTimer::singleShot(0, &plot, []
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) { return; }
            auto buttons = dialog->findChild<QDialogButtonBox *>();
            buttons->button(QDialogButtonBox::RestoreDefaults)->click();
            buttons->button(QDialogButtonBox::Save)->click();
        });
        plot.openPlotSettings();
        QCOMPARE(plot.xAxisLabel(), QString("采样序号"));
        QVERIFY(plot.yAxisUnit().isEmpty());
        QCOMPARE(plot.xAxisValue(5), 5.0);
    }

    void reopenKeepsStyleAndLayout()
    {
        sw::Engine engine;
        auto channel = engine.waveformDemo();
        channel->demoTimer.stop();
        sw::WavePlot plot;
        plot.resize(1000, 600);
        plot.updateChannel(channel, 1000);
        plot.setStyleSheet(plot.theme.stylesheet());
        plot.show();
        QCOMPARE(plot.curveLabel("sawtooth"), QString("锯齿波"));
        const QSize expanded(680, 560);
        QTimer::singleShot(0, &plot, [&]
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) { return; }
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            auto width = dialog->findChild<QDoubleSpinBox *>("waveCurveWidth");
            QVERIFY(width->isEnabled());
            QVERIFY(width->suffix().contains("像素"));
            QVERIFY(dialog->findChild<QLabel *>("waveCurveTitle")->text().contains("锯齿波"));
            width->setFocus();
            width->selectAll();
            QTest::keyClicks(width, "4.5");
            QTest::keyClick(width, Qt::Key_Tab);
            QCOMPARE(width->value(), 4.5);
            QVERIFY(!dialog->findChild<QCheckBox *>("waveInheritStyle")->isChecked());
            QTimer::singleShot(0, dialog, []
            {
                auto colorDialog = qobject_cast<QColorDialog *>(QApplication::activeModalWidget());
                if (!colorDialog) { return; }
                colorDialog->setCurrentColor(QColor("#22aa55"));
                colorDialog->accept();
            });
            dialog->findChild<QPushButton *>("waveCurveColor")->click();
            dialog->findChild<QComboBox *>("waveCurveLine")->setCurrentIndex(3);
            dialog->resize(expanded);
            QApplication::processEvents();
            const int titleTop = dialog->findChild<QLabel *>("waveCurveTitle")->y();
            capture(dialog, "wave-channel-expanded");
            dialog->resize(expanded.width(), expanded.height() + 160);
            QApplication::processEvents();
            QCOMPARE(dialog->findChild<QLabel *>("waveCurveTitle")->y(), titleTop);
            dialog->resize(expanded);
            dialog->accept();
        });
        plot.openCurveSettings("sawtooth");
        QCOMPARE(plot.curveStyle("sawtooth").width, 4.5);
        QCOMPARE(plot.curveStyle("sawtooth").color, QColor("#22aa55"));
        QTimer::singleShot(0, &plot, [&]
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) { return; }
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            QVERIFY(!dialog->findChild<QCheckBox *>("waveInheritStyle")->isChecked());
            QVERIFY(!dialog->findChild<QCheckBox *>("waveAutomaticColor")->isChecked());
            QCOMPARE(dialog->findChild<QDoubleSpinBox *>("waveCurveWidth")->value(), 4.5);
            QCOMPARE(dialog->findChild<QComboBox *>("waveCurveLine")->currentIndex(), 3);
            QCOMPARE(dialog->findChild<QPushButton *>("waveCurveColor")->text(), QString("#22AA55"));
            QCOMPARE(dialog->size(), expanded);
            capture(dialog, "wave-channel-reopened");
            dialog->reject();
        });
        plot.openCurveSettings("sawtooth");
        QTimer::singleShot(0, &plot, []
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) { return; }
            dialog->findChild<QTabWidget *>()->setCurrentIndex(1);
            dialog->findChild<QDoubleSpinBox *>("waveCurveWidth")->setValue(2.5);
            dialog->accept();
        });
        plot.openPlotSettings();
        QTimer::singleShot(0, &plot, [&]
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) { return; }
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            QCOMPARE(dialog->findChild<QTabWidget *>()->currentIndex(), 1);
            QCOMPARE(dialog->findChild<QDoubleSpinBox *>("waveCurveWidth")->value(), 2.5);
            dialog->reject();
        });
        plot.openPlotSettings();
        QCOMPARE(plot.curveStyle("sawtooth").width, 4.5);
        QCOMPARE(plot.curveStyle("sine").width, 2.5);
        sw::WavePlot devicePlot;
        sample(devicePlot);
        QCOMPARE(devicePlot.curveLabel("sawtooth"), QString("sawtooth"));
    }

    void colorClickOverridesAutomaticStyle_data()
    {
        QTest::addColumn<bool>("acceptColor");
        QTest::addColumn<bool>("acceptSettings");
        QTest::newRow("choose-and-save") << true << true;
        QTest::newRow("cancel-color-and-save") << false << true;
        QTest::newRow("choose-and-cancel-settings") << true << false;
        QTest::newRow("cancel-both") << false << false;
    }

    void colorClickOverridesAutomaticStyle()
    {
        QFETCH(bool, acceptColor);
        QFETCH(bool, acceptSettings);
        sw::WavePlot plot;
        sample(plot);
        const auto original = plot.presentationSettings();
        const QColor previous = plot.curveColor("temperature");
        const QColor chosen("#336699");
        QTimer::singleShot(0, &plot, [&]
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            auto automatic = dialog->findChild<QCheckBox *>("waveAutomaticColor");
            auto inherit = dialog->findChild<QCheckBox *>("waveInheritStyle");
            auto color = dialog->findChild<QPushButton *>("waveCurveColor");
            QVERIFY(automatic->isChecked());
            QVERIFY(inherit->isChecked());
            QVERIFY(color->isEnabled());
            QTimer::singleShot(0, dialog, [&]
            {
                auto picker = qobject_cast<QColorDialog *>(QApplication::activeModalWidget());
                QVERIFY(picker);
                QTimer::singleShot(2000, picker, &QDialog::reject);
                QVERIFY(!automatic->isChecked());
                QVERIFY(!inherit->isChecked());
                picker->setCurrentColor(chosen);
                if (acceptColor) { picker->accept(); }
                else { picker->reject(); }
            });
            color->click();
            QVERIFY(!automatic->isChecked());
            QVERIFY(!inherit->isChecked());
            QCOMPARE(color->text(), (acceptColor ? chosen : previous).name().toUpper());
            capture(dialog, "wave-color-click-" + QString::fromLatin1(QTest::currentDataTag()));
            if (acceptSettings) { dialog->accept(); }
            else { dialog->reject(); }
        });
        plot.openCurveSettings("temperature");
        if (!acceptSettings)
        {
            QCOMPARE(plot.presentationSettings(), original);
            return;
        }
        QCOMPARE(plot.curveStyle("temperature").color, acceptColor ? chosen : previous);
        QTimer::singleShot(0, &plot, [&]
        {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            auto automatic = dialog->findChild<QCheckBox *>("waveAutomaticColor");
            auto inherit = dialog->findChild<QCheckBox *>("waveInheritStyle");
            auto color = dialog->findChild<QPushButton *>("waveCurveColor");
            QVERIFY(!automatic->isChecked());
            QVERIFY(!inherit->isChecked());
            QCOMPARE(color->text(), (acceptColor ? chosen : previous).name().toUpper());
            auto buttons = dialog->findChild<QDialogButtonBox *>();
            buttons->button(QDialogButtonBox::RestoreDefaults)->click();
            QVERIFY(automatic->isChecked());
            QVERIFY(inherit->isChecked());
            QVERIFY(color->isEnabled());
            dialog->reject();
        });
        plot.openCurveSettings("temperature");
        QCOMPARE(plot.curveStyle("temperature").color, acceptColor ? chosen : previous);
    }

    void savedWithPageAndRestoredBeforeData()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("settings.json");
        QJsonObject expected;
        {
            sw::Engine engine;
            sw::McpServer mcp(&engine);
            sw::Window window(&engine, &mcp, path);
            auto page = window.newInstrument("waveform");
            auto plot = page->instrument->plot;
            auto settings = plot->presentationSettings();
            settings["x_unit"] = "µs";
            settings["x_increment"] = 0.25;
            settings["y_unit"] = "Ω";
            plot->restorePresentation(settings);
            plot->setCurveStyle("future", {QColor("#007f92"), 2.5, Qt::DashDotLine});
            plot->setCurveVisible("hidden", false);
            expected = plot->presentationSettings();
            QFile savedFile(path);
            QVERIFY(savedFile.open(QIODevice::ReadOnly));
            const auto saved = QJsonDocument::fromJson(savedFile.readAll()).object();
            QCOMPARE(saved["widget_pages"].toArray().first().toObject()["wave_presentation"].toObject(), expected);
            window.close();
        }
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window restored(&engine, &mcp, path);
        sw::WavePlot *plot = nullptr;
        for (auto page : restored.panes)
        {
            if (page->instrument && page->instrument->plot) { plot = page->instrument->plot; }
        }
        QVERIFY(plot);
        QCOMPARE(plot->presentationSettings(), expected);
        sample(*plot);
        QCOMPARE(plot->curveStyle("future").width, 2.5);
        QVERIFY(!plot->isCurveVisible("hidden"));
        restored.close();
    }

    void instrumentPauseIcons_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::newRow("waveform") << QString("waveform");
        QTest::newRow("values") << QString("values");
        QTest::newRow("structured") << QString("structured");
    }

    void instrumentPauseIcons()
    {
        QFETCH(QString, kind);
        sw::Instrument instrument(kind);
        instrument.resize(1000, 600);
        instrument.setStyleSheet(sw::Theme::make(false).stylesheet());
        instrument.show();
        QCOMPARE(instrument.freeze->glyph, QString("pause"));
        QVERIFY(instrument.freeze->text().isEmpty());
        QCOMPARE(instrument.freeze->size(), QSize(36, 36));
        QVERIFY(instrument.freeze->toolTip().contains("暂停显示"));
        QTest::mouseClick(instrument.freeze, Qt::LeftButton);
        QVERIFY(instrument.freeze->isChecked());
        QVERIFY(instrument.freeze->toolTip().contains("继续显示"));
        QCOMPARE(instrument.freeze->accessibleName(), instrument.freeze->toolTip());
        QVERIFY(instrument.settings()["frozen"].toBool());
        capture(&instrument, "pause-" + kind);
        instrument.freeze->setFocus();
        QTest::keyClick(instrument.freeze, Qt::Key_Space);
        QVERIFY(!instrument.freeze->isChecked());
        QVERIFY(instrument.freeze->toolTip().contains("暂停显示"));
        QVERIFY(!instrument.settings()["frozen"].toBool());
    }

    void absentSerialSourceIsNotOffered()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        window.portProvider = [] { return QJsonArray(); };
        window.refreshPorts();
        auto page = window.newInstrument("values", nullptr, false, "serial:com99999");
        auto picker = page->instrument->source;
        page->refreshSource();
        QCOMPARE(picker->findData("serial:com99999"), -1);
        QCOMPARE(picker->currentIndex(), 0);
        QCOMPARE(page->sourceKey, QString("serial:com99999"));
        QVERIFY(engine.channels.isEmpty());
        window.portProvider = [] { return QJsonArray {QJsonObject {{"port", "COM99999"}}}; };
        window.refreshPorts();
        QVERIFY(picker->findData("serial:com99999") > 0);
        window.portProvider = [] { return QJsonArray(); };
        window.refreshPorts();
        QCOMPARE(picker->findData("serial:com99999"), -1);
        QCOMPARE(picker->currentIndex(), 0);
        auto channel = engine.open(sw::Config::fromJson(QJsonObject {{"port", "loop://"}}));
        channel->close();
        channel->config.port = "COM99999";
        channel->name = "COM99999";
        page->refreshSource();
        QCOMPARE(picker->findData("serial:com99999"), -1);
        QCOMPARE(engine.channels.size(), 1);
        QCOMPARE(page->sourceKey, QString("serial:com99999"));
        window.applyTheme(true);
        QVERIFY(page->instrument->freeze->theme.dark);
        window.close();
    }

    void invalidSettingsUseSafeDefaults()
    {
        sw::WavePlot plot;
        plot.restorePresentation({{"x_increment", -1}, {"x_unit", "ms\n"},
            {"defaults", QJsonObject {{"width", -20}, {"line", 99}, {"color", "bad-color"}}}});
        QCOMPARE(plot.xAxisValue(10), 10.0);
        QCOMPARE(plot.xAxisUnit(), QString("ms"));
        QCOMPARE(plot.curveStyle("a").width, 0.5);
        QCOMPARE(plot.curveStyle("a").line, Qt::SolidLine);
        QVERIFY(!plot.curveStyle("a").color.isValid());
        plot.restorePresentation({});
        QCOMPARE(plot.curveStyle("a").width, 1.0);
    }
};

QTEST_MAIN(WaveSettingsTests)
#include "wave_settings_tests.moc"
