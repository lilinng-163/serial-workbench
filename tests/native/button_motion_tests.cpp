// SPDX-License-Identifier: Apache-2.0
#include "button_motion.h"
#include "controls.h"
#include "window_chrome.h"
#include <QtTest>

class ButtonMotionTests : public QObject
{
    Q_OBJECT
private:
    static void enter(QWidget *widget)
    {
        QEnterEvent event(QPointF(4, 4), QPointF(4, 4), widget->mapToGlobal(QPoint(4, 4)));
        QApplication::sendEvent(widget, &event);
    }

    static void leave(QWidget *widget)
    {
        QEvent event(QEvent::Leave);
        QApplication::sendEvent(widget, &event);
    }

private slots:
    void initTestCase()
    {
        QApplication::setStyle("Fusion");
        QApplication::setFont(sw::Theme::interfaceFont());
    }

    void hoverInterpolatesWithoutMovingOrClicking()
    {
        QWidget host;
        host.setStyleSheet(sw::Theme::make(false).stylesheet());
        auto body = new QVBoxLayout(&host);
        auto button = new QPushButton("建立连接");
        button->setObjectName("primary");
        body->addWidget(button);
        host.resize(350, 150);
        host.show();
        QTest::qWait(40);
        auto motion = button->findChild<sw::ButtonMotion *>();
        QVERIFY(motion);
        QVERIFY(motion->testAttribute(Qt::WA_TransparentForMouseEvents));
        QCOMPARE(motion->focusPolicy(), Qt::NoFocus);
        QVERIFY(button->childAt(QPoint(4, 4)) != motion);
        leave(button);
        QTest::qWait(180);
        const auto position = button->geometry();
        const auto idle = button->grab().toImage();
        QSignalSpy clicks(button, &QAbstractButton::clicked);
        enter(button);
        QTest::qWait(35);
        QVERIFY(motion->running());
        QVERIFY(motion->hoverAmount() > 0 && motion->hoverAmount() < 1);
        const auto middle = button->grab().toImage();
        QTest::qWait(180);
        QTRY_COMPARE_WITH_TIMEOUT(motion->hoverAmount(), 1.0, 1000);
        QVERIFY(!motion->running());
        QVERIFY(middle != idle);
        QVERIFY(middle != button->grab().toImage());
        QCOMPARE(button->geometry(), position);
        QCOMPARE(motion->geometry(), button->rect());
        QCOMPARE(clicks.count(), 0);
        leave(button);
        QTest::qWait(180);
        QTRY_COMPARE_WITH_TIMEOUT(motion->hoverAmount(), 0.0, 1000);
        QCOMPARE(button->grab().toImage(), idle);
    }

    void pressToggleKeyboardAndLifetime()
    {
        QWidget host;
        host.setStyleSheet(sw::Theme::make(false).stylesheet());
        auto body = new QVBoxLayout(&host);
        auto button = new QPushButton("收发小窗");
        button->setCheckable(true);
        body->addWidget(button);
        host.show();
        QTest::qWait(30);
        auto motion = button->findChild<sw::ButtonMotion *>();
        QVERIFY(motion);
        QSignalSpy clicks(button, &QAbstractButton::clicked);
        const auto rect = button->geometry();
        QTest::mousePress(button, Qt::LeftButton);
        QTest::qWait(35);
        QVERIFY(motion->pressAmount() > 0 && motion->pressAmount() < 1);
        QCOMPARE(button->geometry(), rect);
        QTest::mouseRelease(button, Qt::LeftButton);
        QTest::qWait(180);
        QCOMPARE(clicks.count(), 1);
        QCOMPARE(motion->checkedAmount(), 1.0);
        QCOMPARE(motion->pressAmount(), 0.0);
        QTest::keyClick(button, Qt::Key_Space);
        QTest::qWait(180);
        QCOMPARE(clicks.count(), 2);
        QCOMPARE(motion->checkedAmount(), 0.0);
        enter(button);
        QTest::qWait(30);
        button->setEnabled(false);
        QVERIFY(!motion->running());
        QCOMPARE(motion->hoverAmount(), 0.0);
        QCOMPARE(motion->pressAmount(), 0.0);
        button->setEnabled(true);
        enter(button);
        button->hide();
        QVERIFY(!motion->running());
        QPointer<sw::ButtonMotion> deleted = motion;
        delete button;
        QVERIFY(deleted.isNull());
        auto late = new QToolButton(&host);
        late->setText("常用控件组");
        body->addWidget(late);
        late->show();
        QTest::qWait(30);
        QVERIFY(late->findChild<sw::ButtonMotion *>());
        auto menu = new QMenu(late);
        menu->addAction("PID 调参");
        late->setMenu(menu);
        late->setPopupMode(QToolButton::InstantPopup);
        bool shown = false;
        QTimer::singleShot(40, menu, [&]
        {
            shown = menu->isVisible();
            if (qEnvironmentVariableIsSet("ROUNDED_MENU_CAPTURE"))
            { menu->grab().save(qEnvironmentVariable("ROUNDED_MENU_CAPTURE")); }
            menu->close();
        });
        QTest::mouseClick(late, Qt::LeftButton);
        QVERIFY(shown);
    }

    void hoverStaysInsideAndKeyboardFocusFollowsParents()
    {
        QWidget host;
        host.setObjectName("root");
        const auto theme = sw::Theme::make(false);
        host.setStyleSheet(theme.stylesheet());
        host.resize(500, 260);
        auto container = new QWidget(&host);
        container->setGeometry(30, 40, 420, 180);
        auto button = new QPushButton("发送", container);
        button->setObjectName("outline");
        button->setGeometry(20, 20, 180, 40);
        host.show();
        QTest::qWait(40);
        QTest::mouseMove(&host, QPoint(480, 240));
        leave(button);
        QTest::qWait(180);
        auto motion = button->findChild<sw::ButtonMotion *>();
        QVERIFY(motion && motion->outerRing());
        auto ring = motion->outerRing();
        QVERIFY(ring->testAttribute(Qt::WA_TransparentForMouseEvents));
        QCOMPARE(ring->parentWidget(), &host);
        const auto before = host.grab().toImage();
        enter(button);
        QTest::qWait(180);
        QVERIFY(!ring->isVisible());
        const auto location = button->mapTo(&host, QPoint());
        QCOMPARE(ring->geometry(), QRect(location, button->size()));
        const auto after = host.grab().toImage();
        const auto outside = (location + QPoint(button->width() / 2, -4)) * after.devicePixelRatio();
        QCOMPARE(before.pixelColor(outside), after.pixelColor(outside));
        button->clearFocus();
        button->setFocus(Qt::TabFocusReason);
        QApplication::processEvents();
        QVERIFY(ring->isVisible());
        QVERIFY(host.childAt(location + QPoint(10, 10)) == button);
        container->move(60, 70);
        QTest::qWait(20);
        QCOMPARE(ring->pos(), button->mapTo(&host, QPoint()));
        button->clearFocus();
        leave(button);
        QTest::qWait(180);
        QVERIFY(!ring->isVisible());
        enter(button);
        QTest::qWait(40);
        button->setEnabled(false);
        QVERIFY(!ring->isVisible());
        QPointer<QWidget> removed = ring;
        delete button;
        QVERIFY(removed.isNull());
    }

    void checkboxAnimatesInsideIndicator()
    {
        QWidget host;
        host.setObjectName("root");
        host.setStyleSheet(sw::Theme::make(false).stylesheet());
        auto body = new QVBoxLayout(&host);
        auto check = new QCheckBox("跟随最新");
        body->addWidget(check);
        host.resize(300, 100);
        host.show();
        QTest::qWait(40);
        auto motion = check->findChild<sw::CheckBoxMotion *>();
        QVERIFY(motion);
        QVERIFY(motion->testAttribute(Qt::WA_TransparentForMouseEvents));
        QCOMPARE(motion->focusPolicy(), Qt::NoFocus);
        QSignalSpy toggles(check, &QCheckBox::toggled);
        const auto bounds = check->geometry();
        check->setChecked(true);
        QVERIFY(motion->running());
        QTRY_VERIFY_WITH_TIMEOUT(motion->checkedAmount() > 0 && motion->checkedAmount() < 1, 300);
        QCOMPARE(check->geometry(), bounds);
        QTest::qWait(180);
        QCOMPARE(motion->checkedAmount(), 1.0);
        check->setFocus(Qt::TabFocusReason);
        QTest::keyClick(check, Qt::Key_Space);
        QTest::qWait(180);
        QCOMPARE(toggles.size(), 2);
        QCOMPARE(motion->checkedAmount(), 0.0);
        QTest::mouseClick(check, Qt::LeftButton, Qt::NoModifier, QPoint(8, check->height() / 2));
        QCOMPARE(toggles.size(), 3);
        QTest::qWait(35);
        check->setEnabled(false);
        QVERIFY(!motion->running());
        QCOMPARE(motion->checkedAmount(), 1.0);
        check->setEnabled(true);
        check->setTristate(true);
        check->setCheckState(Qt::PartiallyChecked);
        QTest::qWait(180);
        const auto prefix = qEnvironmentVariable("POPUP_STYLE_CAPTURE");
        if (!prefix.isEmpty()) { QVERIFY(check->grab().save(prefix + "-check-partial.png")); }
        check->setCheckState(Qt::Unchecked);
        QTest::qWait(35);
        check->hide();
        QVERIFY(!motion->running());
        QPointer<sw::CheckBoxMotion> deleted = motion;
        delete check;
        QVERIFY(deleted.isNull());
    }

    void popupWindowsHaveFourRoundedCorners_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }

    void popupWindowsHaveFourRoundedCorners()
    {
        QFETCH(bool, dark);
        const auto theme = sw::Theme::make(dark);
        QWidget host;
        host.setObjectName("root");
        host.setStyleSheet(theme.stylesheet());
        auto body = new QVBoxLayout(&host);
        auto combo = new sw::Combo();
        combo->addItems({"utf-8", "gbk", "ascii", "latin-1"});
        body->addWidget(combo);
        body->addWidget(new QCheckBox("跟随最新"));
        host.resize(320, 160);
        host.show();
        QTest::qWait(30);
        const auto verify = [&](QWidget *popup)
        {
            QVERIFY(popup->property("roundedPopup").toBool());
            QVERIFY(popup->windowFlags().testFlag(Qt::FramelessWindowHint));
            QVERIFY(popup->windowFlags().testFlag(Qt::NoDropShadowWindowHint));
            QTRY_VERIFY(!popup->mask().isEmpty());
            QVERIFY(!popup->mask().contains(popup->rect().topLeft()));
            QVERIFY(!popup->mask().contains(popup->rect().topRight()));
            QVERIFY(!popup->mask().contains(popup->rect().bottomLeft()));
            QVERIFY(!popup->mask().contains(popup->rect().bottomRight()));
            QVERIFY(popup->mask().contains(popup->rect().center()));
            const auto shot = popup->grab().toImage();
            const qreal ratio = shot.devicePixelRatio();
            QVERIFY(shot.pixelColor(0, 0).alpha() < 32);
            QCOMPARE(shot.pixelColor(QPoint(2, popup->height() / 2) * ratio), theme.panel);
        };
        const auto prefix = qEnvironmentVariable("POPUP_STYLE_CAPTURE") + (dark ? "-dark-" : "-light-");
        combo->showPopup();
        QApplication::processEvents();
        auto popup = combo->view()->window();
        auto comboFade = popup->findChild<QVariantAnimation *>("popupFade", Qt::FindDirectChildrenOnly);
        QVERIFY(comboFade);
        QCOMPARE(comboFade->duration(), 120);
        const auto comboBounds = popup->geometry();
        comboFade->setCurrentTime(50);
        QVERIFY(comboFade->currentValue().toReal() > 0 && comboFade->currentValue().toReal() < 1);
        QCOMPARE(popup->geometry(), comboBounds);
        QTest::qWait(160);
        QCOMPARE(comboFade->state(), QAbstractAnimation::Stopped);
        QCOMPARE(popup->windowOpacity(), 1.0);
        verify(popup);
        if (qEnvironmentVariableIsSet("POPUP_STYLE_CAPTURE")) { QVERIFY(popup->grab().save(prefix + "encoding.png")); }
        QTest::keyClick(combo->view(), Qt::Key_Down);
        QTest::keyClick(combo->view(), Qt::Key_Enter);
        QCOMPARE(combo->currentText(), QString("gbk"));
        QCOMPARE(comboFade->state(), QAbstractAnimation::Stopped);
        QCOMPARE(popup->windowOpacity(), 1.0);
        combo->showPopup();
        QVERIFY(comboFade->state() == QAbstractAnimation::Running);
        combo->hidePopup();
        QCOMPARE(comboFade->state(), QAbstractAnimation::Stopped);
        QCOMPARE(popup->windowOpacity(), 1.0);
        QMenu menu(&host);
        for (const auto &title : {"数值波形", "JSON", "XML", "串口控制"}) { menu.addAction(title); }
        menu.actions().first()->setCheckable(true);
        menu.actions().first()->setChecked(true);
        menu.popup(host.mapToGlobal(QPoint(30, 50)));
        QApplication::processEvents();
        menu.setActiveAction(menu.actions()[2]);
        auto menuFade = menu.findChild<QVariantAnimation *>("popupFade", Qt::FindDirectChildrenOnly);
        QVERIFY(menuFade);
        QCOMPARE(menuFade->duration(), comboFade->duration());
        menuFade->setCurrentTime(50);
        QVERIFY(menuFade->currentValue().toReal() > 0 && menuFade->currentValue().toReal() < 1);
        QTest::qWait(160);
        QCOMPARE(menu.windowOpacity(), 1.0);
        verify(&menu);
        if (qEnvironmentVariableIsSet("POPUP_STYLE_CAPTURE")) { QVERIFY(menu.grab().save(prefix + "menu.png")); }
        menu.resize(menu.width() + 20, menu.height() + 10);
        QApplication::processEvents();
        verify(&menu);
        QSignalSpy triggered(&menu, &QMenu::triggered);
        QTest::mouseClick(&menu, Qt::LeftButton, Qt::NoModifier, menu.actionGeometry(menu.actions()[2]).center());
        QCOMPARE(triggered.size(), 1);
        QVERIFY(!menu.isVisible());
    }

    void roundedButtonsAndTheme_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }

    void roundedButtonsAndTheme()
    {
        QFETCH(bool, dark);
        const auto theme = sw::Theme::make(dark);
        sw::updateWindowChrome(theme, {}, {});
        QWidget host;
        host.setObjectName("root");
        host.setStyleSheet(theme.stylesheet());
        auto body = new QVBoxLayout(&host);
        body->setContentsMargins(24, 24, 24, 24);
        body->setSpacing(14);
        auto heading = new QLabel("串口工作台 · 操作按钮");
        body->addWidget(heading);
        auto row = new QHBoxLayout();
        auto primary = new QPushButton("建立连接");
        primary->setObjectName("primary");
        auto outline = new QPushButton("发送");
        outline->setObjectName("outline");
        auto toggle = new QPushButton("收发小窗");
        toggle->setCheckable(true);
        row->addWidget(primary);
        row->addWidget(outline);
        row->addWidget(toggle);
        body->addLayout(row);
        auto toolbar = new QHBoxLayout();
        for (const auto &glyph : {"pause", "trash", "hex", "follow", "gear"})
        {
            auto icon = new sw::IconButton(glyph, glyph, &host);
            icon->theme = theme;
            toolbar->addWidget(icon);
        }
        auto menu = new QPushButton("常用控件组");
        auto options = new QMenu(menu);
        options->addAction("PID 调参");
        menu->setMenu(options);
        toolbar->addWidget(menu);
        toolbar->addStretch();
        body->addLayout(toolbar);
        auto checks = new QHBoxLayout();
        for (const auto &text : {"跟随最新", "X 轴", "Y 轴"})
        {
            auto check = new QCheckBox(text);
            check->setChecked(true);
            checks->addWidget(check);
        }
        body->addLayout(checks);
        body->addWidget(new QLineEdit("SET speed 1200"));
        auto tabs = new sw::DetachableTabs(&host);
        tabs->setObjectName("roundedPreviewTabs");
        tabs->addTab(new QWidget(), "串口控制");
        tabs->addTab(new QWidget(), "外观设置");
        tabs->setMaximumHeight(100);
        body->addWidget(tabs);
        auto disabled = new QPushButton("发送（未连接）");
        disabled->setEnabled(false);
        body->addWidget(disabled);
        body->addStretch();
        host.resize(600, 440);
        host.show();
        QTest::qWait(40);
        QTest::mouseMove(&host, QPoint(580, 320));
        for (auto button : host.findChildren<QAbstractButton *>()) { leave(button); }
        primary->setFocus(Qt::OtherFocusReason);
        QTest::qWait(180);
        const auto corners = host.grab().toImage();
        const auto corner = primary->mapTo(&host, QPoint(0, 0)) * corners.devicePixelRatio();
        QCOMPARE(corners.pixelColor(corner), theme.background);
        const auto capture = [&](const QString &state)
        {
            if (qEnvironmentVariableIsSet("BUTTON_STYLE_CAPTURE"))
            {
                QVERIFY(host.grab().save(qEnvironmentVariable("BUTTON_STYLE_CAPTURE")
                    + (dark ? "-dark-" : "-light-") + state + ".png"));
            }
        };
        capture("idle");
        enter(primary);
        QTest::qWait(35);
        capture("transition");
        QTest::qWait(180);
        capture("hover");
        QTest::mousePress(primary, Qt::LeftButton);
        QTest::qWait(45);
        capture("pressed");
        QTest::mouseRelease(primary, Qt::LeftButton);
        leave(primary);
        toggle->setChecked(true);
        outline->setFocus(Qt::TabFocusReason);
        QTest::qWait(180);
        capture("keyboard-toggle");
    }
};

QTEST_MAIN(ButtonMotionTests)
#include "button_motion_tests.moc"
