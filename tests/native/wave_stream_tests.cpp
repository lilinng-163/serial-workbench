// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include <QtTest>

class WaveStreamTests : public QObject
{
    Q_OBJECT

    static QJsonArray samples(const QJsonObject &result, const QString &peer = "default")
    {
        return result["sources"].toObject()[peer].toObject()["samples"].toArray();
    }

private slots:
    void parsingFilteringAndMcpIsolation()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto first = engine.open(config);
        auto second = new sw::Channel(&engine, config);
        engine.channels.insert(second->id, second);
        sw::McpServer mcp(&engine);

        first->receive("@wave[im");
        first->receive("u] yaw=10\r");
        first->receive("\n@wave[motor] yaw=100\n@wave[imu] yaw=20\n@wave yaw=7\n");
        first->receive("@wave[imu] yaw=99\n", "peer-a");
        second->receive("@wave[imu] yaw=-10\n");
        const auto all = first->waveform();
        QCOMPARE(samples(all).size(), 4);
        QVERIFY(!samples(all).last().toObject().contains("stream"));
        const auto source = all["sources"].toObject()["default"].toObject();
        QCOMPARE(source["available_streams"].toArray(), QJsonArray({"", "imu", "motor"}));

        QJsonObject args {{"channel_id", first->id}, {"stream", "imu"}, {"limit", 1}};
        const auto filtered = mcp.call("read_waveform", args).toObject();
        QCOMPARE(samples(filtered).size(), 1);
        QCOMPARE(samples(filtered).first().toObject()["index"].toInt(), 3);
        QCOMPARE(samples(filtered).first().toObject()["values"].toObject()["yaw"].toInt(), 20);
        QCOMPARE(samples(filtered, "peer-a").first().toObject()["values"].toObject()["yaw"].toInt(), 99);
        QCOMPARE(filtered["sources"].toObject()["default"].toObject()["total_samples"].toInt(), 4);
        QCOMPARE(mcp.call("read_waveform", args).toObject(), filtered);
        QCOMPARE(first->waveform(), all); // Reads do not consume shared history.
        args["channel_id"] = second->id;
        QCOMPARE(samples(mcp.call("read_waveform", args).toObject()).first().toObject()["values"].toObject()["yaw"].toInt(), -10);
        QCOMPARE(samples(first->waveform(10, QString())).size(), 1);
        QVERIFY(samples(first->waveform(10, QString("absent"))).isEmpty());
        args["stream"] = QJsonValue::Null;
        QVERIFY_EXCEPTION_THROWN(mcp.call("read_waveform", args), std::runtime_error);
        args["stream"] = "imu";
        mcp.setProtocolEnabled("serial", false);
        QVERIFY_EXCEPTION_THROWN(mcp.call("read_waveform", args), std::runtime_error);

        bool schemaFound = false;
        for (const auto &entry : mcp.tools())
        {
            const auto tool = entry.toObject();
            if (tool["name"] != "read_waveform") { continue; }
            QCOMPARE(tool["inputSchema"].toObject()["properties"].toObject()["stream"].toObject()["type"].toString(), QString("string"));
            schemaFound = true;
        }
        QVERIFY(schemaFound);
    }

    void malformedStreamsAndBounds()
    {
        sw::Engine engine;
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        const QList<QByteArray> invalid {
            "@wave[] v=1\n", "@wave[bad/name] v=1\n", "@wave[1bad] v=1\n",
            "@wave[imu]v=1\n", "@wave[imu v=1\n", "@wave[imu]\tv=1\n",
            "@wave[" + QByteArray(33, 'a') + "] v=1\n",
            "@wave[" + QByteArray::fromHex("ff") + "] v=1\n"
        };
        for (const auto &line : invalid) { channel->receive(line); }
        QCOMPARE(channel->sources["default"]->count, 0);
        QCOMPARE(channel->sources["default"]->rejected, invalid.size());
        channel->receive(QString("@wave[姿态_1] v=1\n").toUtf8());
        for (int i = 1; i < 32; ++i)
        {
            channel->receive(QString("@wave[s%1] v=%1\n").arg(i).toUtf8());
        }
        channel->receive("@wave[overflow] v=1\n");
        QCOMPARE(channel->sources["default"]->waveStreams.size(), 32);
        QCOMPARE(channel->sources["default"]->rejected, invalid.size() + 1);
        channel->receive("@wave[s1] v=" + QByteArray(9000, '1'));
        channel->receive("\n@wave[s1] v=2\n");
        QCOMPARE(channel->sources["default"]->rejected, invalid.size() + 2);
        for (int i = 0; i < 5001; ++i) { channel->receive("@wave[s1] v=3\n"); }
        QCOMPARE(channel->sources["default"]->samples.size(), size_t(5000));
        QCOMPARE(samples(channel->waveform(5000, QString("s1"))).size(), 5000);
        QVERIFY(samples(channel->waveform(5000, QString("s2"))).isEmpty());
    }

    void plotsAndValuesKeepStreamNames()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        sw::McpServer mcp(&engine);
        sw::Window window(&engine, &mcp, directory.filePath("settings.json"));
        sw::Config config;
        config.port = "loop://";
        auto channel = engine.open(config);
        auto instrument = window.newInstrument("waveform", channel)->instrument;
        instrument->valuesToggle->setChecked(true);
        channel->receive("@wave[imu] yaw=10\n@wave[motor] yaw=100\n@wave[imu] yaw=20\n@wave yaw=7\n");
        instrument->tick(channel);
        QCOMPARE(instrument->plot->series.size(), 3);
        QCOMPARE(instrument->plot->series["[imu]/yaw"], QVector<QPointF>({{1, 10}, {3, 20}}));
        QCOMPARE(instrument->plot->series["[motor]/yaw"], QVector<QPointF>({{2, 100}}));
        QCOMPARE(instrument->table->topLevelItemCount(), 3);
        QCOMPARE(instrument->table->topLevelItem(1)->text(0), QString("[imu]/yaw"));
        QCOMPARE(instrument->table->topLevelItem(1)->text(1), QString("20"));
        QCOMPARE(instrument->table->topLevelItem(2)->text(0), QString("[motor]/yaw"));
        sw::WavePlot snapshot;
        snapshot.setSamples(channel->waveform());
        QCOMPARE(snapshot.series, instrument->plot->series);
        instrument->clearDisplay();
        channel->receive("@wave[motor] yaw=200\n");
        instrument->tick(channel);
        QCOMPARE(instrument->table->topLevelItemCount(), 1);
        QCOMPARE(instrument->table->topLevelItem(0)->text(0), QString("[motor]/yaw"));
        QCOMPARE(instrument->table->topLevelItem(0)->text(1), QString("200"));
    }
};

QTEST_MAIN(WaveStreamTests)
#include "wave_stream_tests.moc"
