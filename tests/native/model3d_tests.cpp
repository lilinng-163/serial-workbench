// SPDX-License-Identifier: Apache-2.0
#include "model3d_panel.h"
#include "instrument_config.h"
#include "window.h"
#include <QtTest>
#include <QDataStream>
#include <QtMath>
#include <limits>

class Model3dTests : public QObject
{
    Q_OBJECT
    QTemporaryDir directory;

    QString file(const QString &name, const QByteArray &bytes)
    {
        const auto path = directory.filePath(name);
        QFile out(path);
        if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size())
        {
            qFatal("Cannot write model fixture");
        }
        return path;
    }

    static bool near(const QVector3D &actual, const QVector3D &expected)
    {
        return (actual - expected).length() < 0.0001f;
    }

    static QByteArray triangleStl()
    {
        return "solid test\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\n"
               "vertex 2 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid test\n";
    }

    static QByteArray arrowObj()
    {
        return "v -1 -.4 -.2\nv .4 -.4 -.2\nv .4 -.8 -.2\nv 1.4 0 -.2\n"
               "v .4 .8 -.2\nv .4 .4 -.2\nv -1 .4 -.2\nv 0 0 .5\n"
               "f 7 6 5 4 3 2 1\nf 1 2 8\nf 2 3 8\nf 3 4 8\n"
               "f 4 5 8\nf 5 6 8\nf 6 7 8\nf 7 1 8\n";
    }

    void capture(QWidget *widget, const QString &name)
    {
        const auto path = qEnvironmentVariable("SW_MODEL_CAPTURE_DIR");
        if (path.isEmpty()) { return; }
        QVERIFY(QDir().mkpath(path));
        QVERIFY(widget->screen()->grabWindow(widget->winId()).save(path + "/" + name + ".png"));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(directory.isValid());
        QApplication::setStyle("Fusion");
        QApplication::setFont(sw::Theme::interfaceFont());
    }

    void stlVariantsAndInvalidFiles()
    {
        const auto ascii = sw::loadModelMesh(file("模型.stl", triangleStl()));
        QCOMPARE(ascii.vertices.size(), 3);
        QVERIFY(near(ascii.maximum, {2, 1, 0}));
        QVERIFY(near(ascii.vertices[0].normal, {0, 0, 1}));
        QCOMPARE(sw::loadModelMesh(file("multi.stl", triangleStl() + triangleStl())).vertices.size(), 6);
        QByteArray binary(80, '\0');
        binary.replace(0, 5, "solid"); // Binary headers may start with "solid".
        QDataStream data(&binary, QIODevice::Append);
        data.setByteOrder(QDataStream::LittleEndian);
        data.setFloatingPointPrecision(QDataStream::SinglePrecision);
        data << quint32(1);
        for (float value : {0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 2.f, 0.f, 0.f, 0.f, 1.f, 0.f}) { data << value; }
        data << quint16(0);
        QCOMPARE(binary.size(), 134);
        QCOMPARE(sw::loadModelMesh(file("binary.stl", binary)).vertices.size(), 3);
        QVERIFY_EXCEPTION_THROWN(sw::loadModelMesh(file("short.stl", binary.chopped(1))), std::exception);
        QVERIFY_EXCEPTION_THROWN(sw::loadModelMesh(file("empty.stl", "solid empty\nendsolid empty\n")), std::exception);
        QVERIFY_EXCEPTION_THROWN(sw::loadModelMesh(file("nan.stl", QByteArray(triangleStl()).replace("vertex 2", "vertex nan"))), std::exception);
        QVERIFY_EXCEPTION_THROWN(sw::loadModelMesh(file("bad.fbx", "unsupported")), std::exception);
        QFile large(file("large.stl", {}));
        QVERIFY(large.open(QIODevice::WriteOnly));
        QVERIFY(large.resize(64 * 1024 * 1024 + 1));
        large.close();
        QVERIFY_EXCEPTION_THROWN(sw::loadModelMesh(large.fileName()), std::exception);
    }

    void objPolygonsAndIndices()
    {
        const auto mesh = sw::loadModelMesh(file("concave.obj", "mtllib never-open.mtl\n"
            "v 0 0 0\nv 2 0 0\nv 2 2 0\nv 1 1 0\nv 0 2 0\nf -5/1 -4/2 -3/3 -2/4 -1/5\n"));
        QCOMPARE(mesh.vertices.size(), 9);
        float area = 0;
        for (int i = 0; i < mesh.vertices.size(); i += 3)
        {
            area += QVector3D::crossProduct(mesh.vertices[i + 1].position - mesh.vertices[i].position,
                mesh.vertices[i + 2].position - mesh.vertices[i].position).length() / 2;
        }
        QVERIFY(qAbs(area - 3) < 0.0001);
        QVERIFY_EXCEPTION_THROWN(sw::loadModelMesh(file("index.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n")), std::exception);
    }

    void plyAsciiAndBothByteOrders()
    {
        const QByteArray properties = "element vertex 3\nproperty float x\nproperty float y\nproperty float z\n"
            "property uchar red\nelement face 1\nproperty list uchar int vertex_indices\nend_header\n";
        QCOMPARE(sw::loadModelMesh(file("ascii.ply", "ply\nformat ascii 1.0\n" + properties
            + "0 0 0 255\n2 0 0 255\n0 1 0 255\n3 0 1 2\n")).vertices.size(), 3);
        for (bool little : {true, false})
        {
            QByteArray bytes = "ply\nformat binary_" + QByteArray(little ? "little" : "big") + "_endian 1.0\n" + properties;
            QDataStream data(&bytes, QIODevice::Append);
            data.setByteOrder(little ? QDataStream::LittleEndian : QDataStream::BigEndian);
            data.setFloatingPointPrecision(QDataStream::SinglePrecision);
            for (const auto &point : {QVector3D(0, 0, 0), QVector3D(2, 0, 0), QVector3D(0, 1, 0)})
            {
                data << point.x() << point.y() << point.z() << quint8(255);
            }
            data << quint8(3) << qint32(0) << qint32(1) << qint32(2);
            const auto mesh = sw::loadModelMesh(file("binary.ply", bytes));
            QCOMPARE(mesh.vertices.size(), 3);
            QVERIFY(near(mesh.maximum, {2, 1, 0}));
            QVERIFY_EXCEPTION_THROWN(sw::loadModelMesh(file("truncated.ply", bytes.chopped(1))), std::exception);
        }
    }

    void eulerQuaternionAndAxisMapping()
    {
        sw::ModelMapping mapping;
        const QJsonObject values {{"roll", 90}, {"pitch", 0}, {"yaw", 90}};
        auto pose = sw::evaluateModelPose(mapping, values);
        QVERIFY(pose.valid());
        QVERIFY(near(pose.matrix.mapVector({1, 0, 0}), {0, 1, 0}));
        QVERIFY(near(pose.matrix.mapVector({0, 1, 0}), {0, 0, 1}));
        mapping.intrinsic = false;
        pose = sw::evaluateModelPose(mapping, values);
        QVERIFY(near(pose.matrix.mapVector({1, 0, 0}), {0, 0, 1}));
        for (const auto &order : QStringList {"XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX"})
        {
            mapping.order = order;
            mapping.intrinsic = true;
            const auto intrinsic = sw::evaluateModelPose(mapping, {{"roll", 31}, {"pitch", 47}, {"yaw", 63}});
            mapping.order = order;
            std::reverse(mapping.order.begin(), mapping.order.end());
            mapping.intrinsic = false;
            const auto extrinsic = sw::evaluateModelPose(mapping, {{"roll", 31}, {"pitch", 47}, {"yaw", 63}});
            QVERIFY(near(intrinsic.matrix.mapVector({1, 2, 3}), extrinsic.matrix.mapVector({1, 2, 3})));
        }
        mapping = {};
        mapping.radians = true;
        pose = sw::evaluateModelPose(mapping, {{"roll", 0}, {"pitch", 0}, {"yaw", M_PI / 2}});
        QVERIFY(near(pose.matrix.mapVector({1, 0, 0}), {0, 1, 0}));
        mapping.mode = "quaternion";
        for (double magnitude : {1.0, 1e200})
        {
            pose = sw::evaluateModelPose(mapping, {{"qw", magnitude}, {"qx", 0}, {"qy", 0}, {"qz", magnitude}});
            QVERIFY(pose.valid());
            QVERIFY(near(pose.matrix.mapVector({1, 0, 0}), {0, 1, 0}));
        }
        QVERIFY(!sw::evaluateModelPose(mapping, {{"qw", 0}, {"qx", 0}, {"qy", 0}, {"qz", 0}}).valid());
        QVERIFY(!sw::evaluateModelPose(mapping, {}).valid());
        mapping = {};
        mapping.position[0] = {"position.x", 2, 1};
        mapping.position[1] = {"", 1, 3};
        mapping.axes = {2, -1, 3};
        pose = sw::evaluateModelPose(mapping, {{"position", QJsonObject {{"x", 2}}}, {"roll", 0}, {"pitch", 0}, {"yaw", 90}});
        QVERIFY(pose.valid());
        QVERIFY(near(pose.position, {-3, 5, 0}));
        QVERIFY(near(pose.matrix.mapVector({0, 1, 0}), {-1, 0, 0}));
        QCOMPARE(sw::ModelMapping::restore(mapping.settings()).settings(), mapping.settings());
        QCOMPARE(sw::modelNumericFields({{"position", QJsonObject {{"x", 2}}}, {"text", "skip"}}), QStringList {"position.x"});
        mapping = {};
        mapping.axes = {1, 2, -3};
        pose = sw::evaluateModelPose(mapping, {{"roll", 90}, {"pitch", 0}, {"yaw", 0}});
        QVERIFY(near(pose.matrix.mapVector({0, 1, 0}), {0, 0, -1}));
        mapping.axes = {1, 1, 3};
        QVERIFY(!sw::evaluateModelPose(mapping, values).valid());
        mapping.axes = {std::numeric_limits<int>::min(), 2, 3};
        QVERIFY(!sw::evaluateModelPose(mapping, values).valid());
    }

    void sharedFramesPauseResetAndScript()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        sw::ModelPanel panel;
        panel.tick(channel);
        channel->receive("@wave roll=0,pitch=0,yaw=90\n");
        const auto history = channel->waveform();
        panel.tick(channel);
        QVERIFY(near(panel.view->pose().matrix.mapVector({1, 0, 0}), {0, 1, 0}));
        QCOMPARE(channel->waveform(), history);
        channel->receive("@wave roll=0,pitch=0\n");
        panel.tick(channel);
        QVERIFY(near(panel.view->pose().matrix.mapVector({1, 0, 0}), {0, 1, 0}));
        QVERIFY(panel.status->text().contains("保留"));
        auto pause = panel.findChild<QCheckBox *>("model3dPause");
        pause->setChecked(true);
        channel->receive("@wave roll=0,pitch=0,yaw=180\n");
        panel.tick(channel);
        QVERIFY(near(panel.view->pose().matrix.mapVector({1, 0, 0}), {0, 1, 0}));
        panel.clearDisplay();
        pause->setChecked(false);
        panel.tick(channel);
        QVERIFY(near(panel.view->pose().matrix.mapVector({1, 0, 0}), {1, 0, 0}));
        channel->receive("@wave roll=0,pitch=0,yaw=45\n", "peer-a");
        panel.tick(channel);
        QVERIFY(near(panel.view->pose().matrix.mapVector({1, 0, 0}), {1, 0, 0}));
        auto settings = panel.settings();
        settings["peer"] = "peer-a";
        panel.restore(settings);
        panel.resetSource(channel);
        panel.tick(channel);
        QVERIFY(near(panel.view->pose().matrix.mapVector({1, 0, 0}), {float(M_SQRT1_2), float(M_SQRT1_2), 0}));
        settings["peer"] = "default";
        settings["feed"] = "script";
        auto source = channel->sources["default"];
        source->scriptCount = 1;
        source->scriptRecords.push_back({{"seq", 1}, {"value", QJsonObject {{"roll", 0}, {"pitch", 0}, {"yaw", -90}}}});
        panel.restore(settings);
        panel.tick(channel);
        QVERIFY(near(panel.view->pose().matrix.mapVector({1, 0, 0}), {0, -1, 0}));
        QCOMPARE(source->samples.size(), size_t(3));
        ++channel->generation;
        panel.tick(channel);
        QVERIFY(near(panel.view->pose().matrix.mapVector({1, 0, 0}), {1, 0, 0}));
        source->scriptCount = 2;
        source->scriptRecords.push_back({{"seq", 2}, {"value", QJsonObject {{"roll", 0}, {"pitch", 0}, {"yaw", 90}}}});
        panel.tick(channel);
        QVERIFY(near(panel.view->pose().matrix.mapVector({1, 0, 0}), {0, 1, 0}));
        panel.tick(nullptr);
        QVERIFY(near(panel.view->pose().matrix.mapVector({1, 0, 0}), {1, 0, 0}));
    }

    void sourceSwitchUpdatesAnimationImmediately()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto first = engine.open(config);
        sw::Channel second(&engine, config);
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("switch-settings.json"));
        auto pane = window.newInstrument("model3d", first);
        first->receive("@wave roll=0,pitch=0,yaw=90\n");
        pane->instrument->tick(first);
        second.receive("@wave roll=0,pitch=0,yaw=-90\n");
        pane->setChannel(&second);
        QCOMPARE(pane->instrument->waveChannel.data(), &second);
        // The faster animation timer must not read the old source while waiting
        // for the main window's next refresh.
        pane->instrument->tick(pane->instrument->waveChannel);
        QVERIFY(near(pane->instrument->model->view->pose().matrix.mapVector({1, 0, 0}), {0, -1, 0}));
        pane->setChannel(nullptr);
        QVERIFY(!pane->instrument->waveChannel);
    }

    void importFailureAndPagePersistence()
    {
        const auto modelFile = file("方向模型.obj", arrowObj());
        const auto settingsFile = directory.filePath("restore-settings.json");
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        {
            sw::Window window(&engine, &mcp, settingsFile);
            auto pane = window.newInstrument("model3d");
            QVERIFY(pane);
            auto panel = pane->instrument->model;
            QVERIFY(panel);
            QSignalSpy loaded(panel, &sw::ModelPanel::modelLoaded);
            panel->loadFile(modelFile);
            QTRY_COMPARE(loaded.size(), 1);
            QVERIFY(loaded.takeFirst()[0].toBool());
            QCOMPARE(panel->settings()["file"].toString(), modelFile);
            panel->loadFile(directory.filePath("missing.stl"));
            QTRY_COMPARE(loaded.size(), 1);
            QVERIFY(!loaded.takeFirst()[0].toBool());
            QCOMPARE(panel->settings()["file"].toString(), modelFile);
            panel->findChild<QDoubleSpinBox *>("modelPosition0Offset")->setValue(12.5);
            panel->findChild<QComboBox *>("model3dRotationMode")->setCurrentIndex(1);
            window.saveSettings();
        }
        sw::Window restored(&engine, &mcp, settingsFile);
        sw::ModelPanel *panel = nullptr;
        for (auto pane : restored.panes)
        {
            if (pane->instrument && pane->instrument->model) { panel = pane->instrument->model; }
        }
        QVERIFY(panel);
        QCOMPARE(panel->settings()["file"].toString(), modelFile);
        QCOMPARE(panel->mapping().mode, QString("quaternion"));
        QCOMPARE(panel->mapping().position[0].offset, 12.5);
        QTRY_VERIFY(panel->findChild<QPushButton *>("import3dModel")->isEnabled());
        QCOMPARE(sw::migrateInstrumentPages({QJsonObject {{"kind", "model3d"}}}).size(), 1);
    }

    void realWindowRenderingAndControls()
    {
        if (QGuiApplication::platformName() == "offscreen") { QSKIP("Requires a native OpenGL window"); }
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("window-settings.json"));
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        auto pane = window.newInstrument("model3d", channel);
        auto panel = pane->instrument->model;
        panel->loadFile(file("render.obj", arrowObj()));
        window.resize(1380, 920);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_VERIFY(panel->findChild<QPushButton *>("import3dModel")->isEnabled());
        QTRY_VERIFY(panel->view->isValid());
        QVERIFY2(panel->view->renderError().isEmpty(), qPrintable(panel->view->renderError()));
        channel->receive("@wave roll=15,pitch=-20,yaw=35\n");
        QTRY_VERIFY(panel->status->text().contains("已更新"));
        panel->findChild<QTabWidget *>("model3dMappingTabs")->setCurrentIndex(1);
        QTest::qWait(250);
        qInfo() << "Capture initial framebuffer";
        const auto first = panel->view->grabFramebuffer();
        QVERIFY(!first.isNull());
        const auto captureDir = qEnvironmentVariable("SW_MODEL_CAPTURE_DIR");
        if (!captureDir.isEmpty())
        {
            QDir().mkpath(captureDir);
            first.save(captureDir + "/framebuffer.png");
        }
        panel->view->makeCurrent();
        auto gl = panel->view->context()->functions();
        qInfo() << "GL renderer" << reinterpret_cast<const char *>(gl->glGetString(GL_RENDERER))
                << "format" << panel->view->context()->format();
        panel->view->doneCurrent();
        QSet<QRgb> colors;
        for (int y = 0; y < first.height(); y += 3)
        {
            for (int x = 0; x < first.width(); x += 3) { colors.insert(first.pixel(x, y)); }
        }
        QVERIFY(colors.size() > 20);
        capture(&window, "model3d-light");
        channel->receive("@wave roll=-45,pitch=30,yaw=110\n");
        QTRY_VERIFY(near(panel->view->pose().matrix.mapVector({1, 0, 0}),
            sw::evaluateModelPose({}, {{"roll", -45}, {"pitch", 30}, {"yaw", 110}}).matrix.mapVector({1, 0, 0})));
        QVERIFY(panel->view->grabFramebuffer() != first);
        // Returning to the same pose must not retain depth from the other pose.
        channel->receive("@wave roll=15,pitch=-20,yaw=35\n");
        panel->tick(channel);
        QCOMPARE(panel->view->grabFramebuffer(), first);
        const auto camera = panel->view->cameraSettings();
        QTest::mousePress(panel->view, Qt::LeftButton, {}, QPoint(150, 130));
        QTest::mouseMove(panel->view, QPoint(220, 170), 40);
        QTest::mouseRelease(panel->view, Qt::LeftButton, {}, QPoint(220, 170));
        QVERIFY(panel->view->cameraSettings() != camera);
        window.applyTheme(true);
        QTest::qWait(250);
        capture(&window, "model3d-dark");
        panel->findChild<QTabWidget *>("model3dMappingTabs")->setCurrentIndex(2);
        capture(&window, "model3d-axes");
        qInfo() << "Detach model window";
        auto detached = window.detachConnection(window.tabs->currentIndex(), QPoint(150, 120), false);
        QVERIFY(detached);
        QTest::qWait(150);
        QVERIFY(panel->view->isValid());
        QVERIFY(!panel->view->grabFramebuffer().isNull());
        detached->transferPane(pane, &window);
        QTest::qWait(150);
        QVERIFY(panel->view->isValid());
        QVERIFY(!panel->view->grabFramebuffer().isNull());
        window.close();
    }
};

QTEST_MAIN(Model3dTests)
#include "model3d_tests.moc"
