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
    const auto snapshot = channel ? channel->waveform(1)["sources"].toObject() : QJsonObject();
    if (snapshot == valuesSnapshot) { return; }
    valuesSnapshot = snapshot;
    int row = 0;
    for (auto source = snapshot.begin(); source != snapshot.end(); ++source)
    {
        const auto samples = source.value().toObject()["samples"].toArray();
        if (samples.isEmpty()) { continue; }
        const auto sample = samples.last().toObject();
        if (!plot->sampleIsCurrent(source.key(), sample["index"].toInteger())) { continue; }
        const auto values = sample["values"].toObject();
        for (auto field = values.begin(); field != values.end(); ++field)
        {
            const auto name = source.key() == "default" ? field.key() : source.key() + "/" + field.key();
            const QStringList columns {
                plot->curveLabel(name), QLocale::c().toString(field.value().toDouble(), 'g', QLocale::FloatingPointShortest),
                source.key() == "default" ? "本连接" : source.key(), QString::number(sample["index"].toInteger())
            };
            auto item = table->topLevelItem(row);
            if (!item) { item = new QTreeWidgetItem(table); }
            for (int column = 0; column < columns.size(); ++column)
            {
                if (item->text(column) != columns[column]) { item->setText(column, columns[column]); }
            }
            item->setToolTip(0, field.key());
            item->setToolTip(1, columns[1] + "\n来源：" + columns[2] + "\n采样序号：" + columns[3]);
            ++row;
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
