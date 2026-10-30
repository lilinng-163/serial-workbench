// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "model3d_panel.h"
#include <QtTest>
#include <algorithm>
#include <cstdio>

namespace
{
QJsonObject distribution(QList<double> values)
{
    std::sort(values.begin(), values.end());
    double total = 0;
    for (auto value : values) { total += value; }
    return {{"count", values.size()}, {"mean_ms", total / qMax(1, values.size())},
        {"p95_ms", values.value(values.size() * 95 / 100)}, {"max_ms", values.value(values.size() - 1)}};
}

QByteArray frame(int index)
{
    return QString("@wave roll=%1,pitch=0,yaw=0,qw=1,qx=0,qy=0,qz=0,axis=1,seq=%2\r\n")
        .arg(index % 60).arg(index).toUtf8();
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    app.setFont(sw::Theme::interfaceFont());
    QTemporaryDir directory;
    QJsonObject result;
    QList<double> enumeration;
    for (int i = 0; i < 12; ++i)
    {
        QElapsedTimer elapsed;
        elapsed.start();
        sw::Engine::ports();
        enumeration.append(elapsed.nsecsElapsed() / 1e6);
    }
    result["serial_enumeration"] = distribution(enumeration);

    sw::Engine engine;
    sw::Config config;
    config.port = "loop://";
    auto channel = engine.open(config);
    sw::McpServer mcp(&engine);
    sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
    auto receive = window.findPane(channel);
    auto pane = window.newInstrument("model3d", channel);
    auto panel = pane->instrument->model;
    panel->configure({{"antialiasing", 8}});
    if (argc > 2 && !QString::fromLocal8Bit(argv[2]).startsWith('-'))
    {
        panel->loadFile(QString::fromLocal8Bit(argv[2]));
        while (panel->loading()) { QTest::qWait(10); }
        if (!panel->state()["load_error"].toString().isEmpty()) { return 3; }
    }
    window.resize(1400, 950);
    window.show();
    QTest::qWait(150);
    auto view = panel->view;
    QList<double> heartbeat;
    QList<double> moves;
    QElapsedTimer pulse;
    pulse.start();
    QTimer heartbeatTimer;
    heartbeatTimer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&heartbeatTimer, &QTimer::timeout, &app, [&]
    {
        heartbeat.append(pulse.nsecsElapsed() / 1e6);
        pulse.restart();
    });
    QTimer telemetryTimer;
    int sequence = 0;
    QObject::connect(&telemetryTimer, &QTimer::timeout, &app, [&] { channel->receive(frame(++sequence)); });
    QTimer mouseTimer;
    int input = 0;
    const auto center = view->rect().center();
    QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, center);
    QObject::connect(&mouseTimer, &QTimer::timeout, &app, [&]
    {
        ++input;
        const QPointF point = center + QPoint(input % 200, input % 40);
        QMouseEvent event(QEvent::MouseMove, point, view->mapToGlobal(point.toPoint()),
            Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QElapsedTimer elapsed;
        elapsed.start();
        QApplication::sendEvent(view, &event);
        moves.append(elapsed.nsecsElapsed() / 1e6);
    });
    telemetryTimer.start(50);
    mouseTimer.start(4);
    heartbeatTimer.start(5);
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    loop.exec();
    mouseTimer.stop();
    heartbeatTimer.stop();
    telemetryTimer.stop();
    QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, center);
    result["mouse_handler"] = distribution(moves);
    result["event_loop"] = distribution(heartbeat);
    result["received_during_drag"] = sequence;
    result["actual_msaa"] = view->modelState()["antialiasing_effective"];
    result["model_triangles"] = view->modelState()["triangles"];

    window.timer.stop();
    window.portTimer.stop();
    for (int i = 0; i < 6000; ++i) { channel->receive(frame(++sequence)); }
    window.showPane(receive);
    QTest::qWait(30);
    QList<double> catchup;
    QElapsedTimer total;
    total.start();
    while (receive->cursor < engine.journal.sequence)
    {
        QElapsedTimer elapsed;
        elapsed.start();
        receive->tick();
        receive->catchupTimer.stop(); // Measure one batch; async catch-up is covered by stream_ui_tests.
        QApplication::processEvents();
        catchup.append(elapsed.nsecsElapsed() / 1e6);
    }
    result["receive_catchup"] = distribution(catchup);
    result["receive_catchup_total_ms"] = total.nsecsElapsed() / 1e6;
    result["receive_blocks"] = receive->log->document()->blockCount();
    result["last_frame_visible"] = receive->log->toPlainText().contains("seq=" + QString::number(sequence));

    const auto output = QJsonDocument(result).toJson(QJsonDocument::Indented);
    if (argc > 1)
    {
        QFile file(QString::fromLocal8Bit(argv[1]));
        if (!file.open(QIODevice::WriteOnly)) { return 2; }
        file.write(output);
    }
    std::printf("%s", output.constData());
}
