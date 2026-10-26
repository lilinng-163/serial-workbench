// SPDX-License-Identifier: Apache-2.0
#include "instrument_config.h"
#include "window.h"
#include "model3d_panel.h"
#include <QJsonObject>

namespace sw
{
void Instrument::clearDisplay()
{
    if (model) { model->clearDisplay(); return; }
    if (plot) { plot->clearSamples(waveChannel, points->value()); }
    if (waveChannel && isStructured())
    {
        for (auto source = waveChannel->sources.cbegin(); source != waveChannel->sources.cend(); ++source)
        {
            cursors[source.key()] = source.value()->parsedCount;
        }
    }
    valuesSnapshot = {};
    if (table) { table->clear(); }
    if (detail) { detail->clear(); }
    info->setText("已清空，等待新数据");
}

void Instrument::refreshValues(Channel *channel)
{
    QJsonObject counters;
    if (channel)
    {
        for (auto source = channel->sources.cbegin(); source != channel->sources.cend(); ++source)
        {
            counters[source.key()] = source.value()->count;
        }
    }
    const QJsonObject snapshot {{"channel", channel ? channel->id : QString()}, {"counters", counters}, {"limit", points->value()}};
    if (snapshot == valuesSnapshot) { return; }
    valuesSnapshot = snapshot;
    int row = 0;
    if (channel)
    {
        for (auto source = channel->sources.cbegin(); source != channel->sources.cend(); ++source)
        {
            QMap<QString, QJsonObject> latest;
            const auto &buffer = *source.value();
            int remaining = points->value();
            for (auto sample = buffer.samples.rbegin(); sample != buffer.samples.rend() && remaining-- > 0; ++sample)
            {
                if (!plot->sampleIsCurrent(source.key(), (*sample)["index"].toInteger())) { break; }
                const auto stream = (*sample)["stream"].toString();
                if (!latest.contains(stream)) { latest.insert(stream, *sample); }
                if (latest.size() == buffer.waveStreams.size()) { break; }
            }
            for (const auto &sample : latest)
            {
                const auto values = sample["values"].toObject();
                for (auto field = values.begin(); field != values.end(); ++field)
                {
                    const auto name = WavePlot::sampleCurveName(source.key(), sample, field.key());
                    const auto stream = sample["stream"].toString();
                    const QStringList columns {
                        plot->curveLabel(name), QLocale::c().toString(field.value().toDouble(), 'g', QLocale::FloatingPointShortest),
                        (source.key() == "default" ? "本连接" : source.key()) + (stream.isEmpty() ? "" : " / " + stream),
                        QString::number(sample["index"].toInteger())
                    };
                    auto item = table->topLevelItem(row);
                    if (!item) { item = new QTreeWidgetItem(table); }
                    for (int column = 0; column < columns.size(); ++column)
                    {
                        if (item->text(column) != columns[column]) { item->setText(column, columns[column]); }
                    }
                    item->setToolTip(0, name);
                    item->setToolTip(1, columns[1] + "\n来源：" + columns[2] + "\n采样序号：" + columns[3]);
                    ++row;
                }
            }
        }
    }
    while (table->topLevelItemCount() > row) { delete table->takeTopLevelItem(row); }
}

QJsonArray migrateInstrumentPages(const QJsonArray &pages)
{
    QJsonArray result;
    QJsonArray values;
    for (const auto &entry : pages)
    {
        auto page = entry.toObject();
        const auto kind = page["kind"].toString();
        if (kind == "values")
        {
            values.append(page);
            continue;
        }
        if (kind == "structured")
        {
            const int parser = page["parser"].toInt();
            for (const auto &format : QStringList {"json", "xml"})
            {
                if ((parser == 1 && format != "json") || (parser == 2 && format != "xml")) { continue; }
                auto separate = page;
                separate["kind"] = format;
                separate.remove("parser");
                const auto title = page["title"].toString();
                separate["title"] = title.isEmpty() || title == "JSON / XML" ? QString()
                    : parser == 0 ? title + " · " + format.toUpper() : title;
                result.append(separate);
            }
        }
        else if (kind == "waveform" || kind == "json" || kind == "xml" || kind == "control" || kind == "model3d")
        {
            result.append(page);
        }
    }
    for (const auto &entry : values)
    {
        auto page = entry.toObject();
        bool merged = false;
        for (qsizetype index = 0; index < result.size(); ++index)
        {
            auto existing = result[index].toObject();
            if (existing["kind"] != "waveform" || existing["source_key"] != page["source_key"]) { continue; }
            existing["show_values"] = true;
            result[index] = existing;
            merged = true;
            break;
        }
        if (!merged)
        {
            page["kind"] = "waveform";
            page["show_values"] = true;
            if (page["title"] == "实时数值") { page.remove("title"); }
            result.append(page);
        }
    }
    return result;
}
}
