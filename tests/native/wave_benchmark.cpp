// SPDX-License-Identifier: Apache-2.0

#include "window.h"
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <cstdio>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    app.setFont(sw::Theme::interfaceFont());
    sw::Engine engine;
    sw::Config config;
    config.port = "loop://";
    auto channel = engine.open(config);
    const auto frame = [](int index)
    {
        QStringList fields;
        for (int curve = 0; curve < 16; ++curve)
        {
            fields.append(QString("ch%1=%2").arg(curve).arg(std::sin(index * 0.03 + curve), 0, 'f', 6));
        }
        return ("@wave " + fields.join(',') + "\n").toUtf8();
    };
    for (int index = 0; index < 5000; ++index) { channel->receive(frame(index)); }

    sw::Instrument instrument("waveform");
    instrument.resize(1280, 800);
    instrument.points->setValue(5000);
    instrument.show();
    instrument.tick(channel);
    QTest::qWait(50);
    QList<qint64> times;
    QList<qint64> preparation;
    const int frames = argc > 2 ? qBound(3, QString::fromLocal8Bit(argv[2]).toInt(), 1000) : 90;
    for (int index = 0; index < frames; ++index)
    {
        channel->receive(frame(5000 + index));
        QElapsedTimer elapsed;
        elapsed.start();
        instrument.tick(channel);
        preparation.append(elapsed.nsecsElapsed() / 1000);
        const auto image = instrument.plot->grab();
        if (image.isNull()) { return 2; }
        times.append(elapsed.nsecsElapsed() / 1000);
    }
    std::sort(times.begin(), times.end());
    qint64 total = 0;
    qint64 prepareTotal = 0;
    for (const auto value : times) { total += value; }
    for (const auto value : preparation) { prepareTotal += value; }
    const QJsonObject result {
        {"curves", 16}, {"samples_per_curve", 5000}, {"frames", times.size()},
        {"mean_ms", total / double(times.size()) / 1000.0},
        {"prepare_ms", prepareTotal / double(times.size()) / 1000.0},
        {"rendered_points", instrument.plot->renderedPointCount()},
        {"p95_ms", times[times.size() * 95 / 100] / 1000.0}
    };
    const auto output = QJsonDocument(result).toJson(QJsonDocument::Indented);
    if (argc > 1 && !QString::fromLocal8Bit(argv[1]).startsWith('-'))
    {
        QFile file(QString::fromLocal8Bit(argv[1]));
        if (!file.open(QIODevice::WriteOnly)) { return 3; }
        file.write(output);
    }
    printf("%s", output.constData());
}
