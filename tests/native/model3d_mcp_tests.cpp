// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "model3d_panel.h"
#include <QtTest>

class ModelMcpTests : public QObject
{
    Q_OBJECT
    QTemporaryDir directory;

    QString fixture(const QString &name, const QByteArray &bytes)
    {
        const auto path = directory.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) { qFatal("fixture failed"); }
        return path;
    }

    QString mesh()
    {
        return fixture("模型.obj", "v 0 0 0\nv 2 0 0\nv 0 1 0\nv 0 0 1\nf 1 2 3\nf 1 4 2\nf 1 3 4\nf 2 4 3\n");
    }

    static QJsonObject get(sw::McpServer &mcp, const QString &id)
    {
        return mcp.call("get_model_state", {{"view_id", id}}).toObject();
    }

    static QJsonObject configure(sw::McpServer &mcp, const QString &id, const QJsonObject &patch)
    {
        return mcp.call("configure_model_view", {{"view_id", id}, {"expected_revision", get(mcp, id)["revision"]},
            {"settings", patch}}).toObject();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(directory.isValid());
        QApplication::setStyle("Fusion");
    }

    void importStateAndAtomicConfiguration()
    {
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("import.json"));
        QVERIFY_EXCEPTION_THROWN(mcp.call("open_model_view", {{"file_path", mesh()}}), std::exception);
        engine.allowControl = true;
        QVERIFY_EXCEPTION_THROWN(mcp.call("open_model_view", {{"file_path", "relative.stl"}}), std::exception);
        const auto opened = mcp.call("open_model_view", {{"file_path", mesh()}}).toObject();
        const auto id = opened["view_id"].toString();
        QVERIFY(!id.isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!get(mcp, id)["loading"].toBool(), 5000);
        auto state = get(mcp, id);
        QCOMPARE(state["load_error"].toString(), QString());
        QCOMPARE(state["model"].toObject()["triangles"].toInt(), 4);
        QVERIFY(!state["using_example_mesh"].toBool());
        QCOMPARE(mcp.call("list_model_views", {}).toObject()["views"].toArray().size(), 1);
        state = configure(mcp, id, {{"manual_position", QJsonArray {3, 4, 5}}, {"scale", 2},
            {"centered", false}, {"follow_theme", false}, {"color", "#123abc"}});
        QCOMPARE(state["model"].toObject()["position"].toArray(), QJsonArray({3, 4, 5}));
        QCOMPARE(state["model"].toObject()["world_bounds_max"].toArray(), QJsonArray({7, 6, 7}));
        QCOMPARE(state["model"].toObject()["effective_color"].toString(), QString("#123abc"));
        auto panel = window.findChild<sw::ModelPanel *>();
        QVERIFY(panel);
        QCOMPARE(panel->findChild<QDoubleSpinBox *>("modelManualPosition0")->value(), 3.0);
        const auto before = panel->settings();
        for (const auto &patch : QList<QJsonObject> {
            {{"scale", 0}, {"color", "#fff"}}, {{"mapping", QJsonObject {{"axes", QJsonArray {1, 1, 3}}}}},
            {{"manual_position", QJsonArray {1, 2}}}, {{"scale", "2"}}, {{"unexpected", true}},
            {{"mapping", QJsonObject {{"angles", QJsonArray {QJsonObject {}}}}}}, {{"paused", 1}}})
        {
            QVERIFY_EXCEPTION_THROWN(configure(mcp, id, patch), std::exception);
            QCOMPARE(panel->settings(), before);
        }
        const auto oldRevision = state["revision"];
        panel->findChild<QDoubleSpinBox *>("modelManualPosition0")->setValue(9);
        QVERIFY_EXCEPTION_THROWN(mcp.call("configure_model_view", {{"view_id", id},
            {"expected_revision", oldRevision}, {"settings", QJsonObject {{"scale", 3}}}}), std::exception);
        QCOMPARE(get(mcp, id)["model"].toObject()["position"].toArray()[0].toDouble(), 9.0);
        // Malformed replacements do not erase a loaded model or report success.
        const auto bad = fixture("bad.stl", "not a mesh");
        mcp.call("configure_model_view", {{"view_id", id}, {"expected_revision", get(mcp, id)["revision"]}, {"file_path", bad}});
        QTRY_VERIFY_WITH_TIMEOUT(!get(mcp, id)["loading"].toBool(), 5000);
        QVERIFY(!get(mcp, id)["load_error"].toString().isEmpty());
        QCOMPARE(get(mcp, id)["model"].toObject()["triangles"].toInt(), 4);
    }

    void telemetryIsolationAndPermissions()
    {
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("telemetry.json"));
        engine.allowControl = true;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        const auto id = mcp.call("open_model_view", {{"file_path", mesh()}, {"channel_id", channel->id}}).toObject()["view_id"].toString();
        QTRY_VERIFY(!get(mcp, id)["loading"].toBool());
        configure(mcp, id, {{"stream", "imu"}});
        channel->receive("@wave[imu] roll=0,pitch=0,yaw=90\n@wave[motor] roll=0,pitch=0,yaw=0\n");
        const auto cursor = engine.journal.sequence;
        auto state = get(mcp, id);
        QCOMPARE(state["telemetry"].toObject()["rotation_sequence"].toInt(), 1);
        auto panel = window.findChild<sw::ModelPanel *>();
        QVERIFY((panel->view->pose().matrix.mapVector({1, 0, 0}) - QVector3D(0, 1, 0)).length() < .0001);
        configure(mcp, id, {{"mapping", QJsonObject {{"mode", "quaternion"}}}});
        channel->receive("@wave[imu] qw=1,qx=0,qy=0,qz=0\n@wave[imu] qw=0,qx=0,qy=0,qz=0\n");
        state = get(mcp, id);
        QCOMPARE(state["telemetry"].toObject()["rotation_sequence"].toInt(), 3);
        channel->receive("@wave[imu] qw=0,qx=0,qy=0,qz=0\n");
        state = get(mcp, id);
        QVERIFY(!state["telemetry"].toObject()["errors"].toArray().isEmpty());
        QCOMPARE(state["telemetry"].toObject()["rotation_sequence"].toInt(), 3);
        const auto invalidFrameErrors = state["telemetry"].toObject().value("errors");
        state = configure(mcp, id, {{"paused", true}, {"follow_theme", false}, {"color", "#abcdef"}});
        QCOMPARE(state["telemetry"].toObject()["rotation_sequence"].toInt(), 3);
        QCOMPARE(state["telemetry"].toObject()["errors"], invalidFrameErrors);
        QVERIFY(state["status"].toString().contains("上一有效值"));
        QVERIFY(engine.journal.sequence > cursor);
        const auto untouched = engine.journal.sequence;
        engine.allowControl = false;
        QVERIFY(!get(mcp, id).isEmpty());
        QVERIFY_EXCEPTION_THROWN(configure(mcp, id, {{"scale", 2}}), std::exception);
        QCOMPARE(engine.journal.sequence, untouched);
        mcp.setProtocolEnabled("serial", false);
        QVERIFY(mcp.call("list_model_views", {}).toObject()["views"].toArray().isEmpty());
        QVERIFY_EXCEPTION_THROWN(get(mcp, id), std::exception);
        QVERIFY_EXCEPTION_THROWN(mcp.call("capture_model_view", {{"view_id", id}}), std::exception);
        engine.allowControl = true;
        QVERIFY_EXCEPTION_THROWN(mcp.call("open_model_view", {{"file_path", mesh()}, {"channel_id", channel->id}}), std::exception);
    }

    void backgroundCapture()
    {
        if (QGuiApplication::platformName() == "offscreen") { QSKIP("Real desktop OpenGL capture is tested on the Windows platform"); }
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("capture.json"));
        engine.allowControl = true;
        const auto id = mcp.call("open_model_view", {{"file_path", mesh()}}).toObject()["view_id"].toString();
        QTRY_VERIFY(!get(mcp, id)["loading"].toBool());
        configure(mcp, id, {{"follow_theme", false}, {"color", "#2aaacf"}});
        auto panel = window.findChild<sw::ModelPanel *>();
        QVERIFY(panel);
        panel->view->resize(640, 640);
        panel->view->fit();
        const auto squareDistance = panel->view->cameraSettings()["distance"].toDouble();
        panel->view->resize(320, 640);
        panel->view->fit();
        QVERIFY(panel->view->cameraSettings()["distance"].toDouble() > squareDistance * 1.8);
        const auto image = mcp.call("capture_model_view", {{"view_id", id}, {"max_size", 640}}).toObject();
        auto frame = QImage::fromData(QByteArray::fromBase64(image["image_png_base64"].toString().toLatin1()), "PNG");
        QVERIFY(!frame.isNull());
        QVERIFY(qMax(frame.width(), frame.height()) <= 640);
        QVERIFY(!window.isVisible());
        const auto destination = qEnvironmentVariable("SW_MODEL_CAPTURE_DIR");
        if (!destination.isEmpty())
        {
            QDir().mkpath(destination);
            QVERIFY(frame.save(destination + "/mcp-background.png"));
            window.resize(1280, 900);
            for (auto pane : window.panes)
            {
                if (pane->searchViewId == id) { window.showPane(pane); }
            }
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            QTest::qWait(150);
            QVERIFY(window.screen()->grabWindow(window.winId()).save(destination + "/mcp-gui.png"));
        }
    }
};

QTEST_MAIN(ModelMcpTests)
#include "model3d_mcp_tests.moc"
