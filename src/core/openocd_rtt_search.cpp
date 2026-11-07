// SPDX-License-Identifier: Apache-2.0
#include "openocd.h"
#include "core.h"

namespace sw
{
namespace
{
QString plainRttText(QString text)
{
    static const QRegularExpression ansi(R"(\x1b(?:\[[0-?]*[ -/]*[@-~]|\][^\x07\x1b]*(?:\x07|\x1b\\)|[ -/]*[0-~]))");
    static const QRegularExpression controls(R"([\x00-\x08\x0b\x0c\x0e-\x1f\x7f])");
    text.remove(ansi);
    text.replace("\r\n", "\n");
    text.replace('\r', '\n');
    text.remove(controls);
    return text;
}
}

void OpenOcdService::setRttFilter(const SearchFilter &filter)
{
    if (filter.settings() == rttSearchFilter.settings()) { return; }
    rttSearchFilter = filter;
    ++rttFilterRevision;
    emit rttFilterChanged();
}

QJsonObject OpenOcdService::searchRtt(int offset, int limit, const QString &snapshot, qint64 after, qint64 through) const
{
    if (offset < 0 || offset > 512 * 1024 || limit < 1 || limit > 2000) { fail("RTT 搜索分页参数超出范围"); }
    const auto last = through < 0 ? rttSequence : through;
    const auto first = rttRecords.isEmpty() ? rttSequence : rttRecords.first()["seq"].toInteger();
    const auto currentSnapshot = QString("%1:%2:%3:%4:%5").arg(rttJob).arg(first).arg(last).arg(after).arg(rttFilterRevision);
    if (!snapshot.isEmpty() && snapshot != currentSnapshot) { fail("RTT 内容已变化，请从 offset=0 重新搜索"); }

    QString text;
    QString pending;
    int connection = 0;
    for (const auto &record : rttRecords)
    {
        if (!rttSearchFilter.error.isEmpty()) { break; }
        const auto seq = record["seq"].toInteger();
        if (seq <= after) { continue; }
        if (seq > last) { break; }
        const auto nextConnection = record["connection"].toInt();
        if (nextConnection != connection && !pending.isEmpty())
        {
            text += plainRttText(std::move(pending));
            if (!text.endsWith('\n')) { text += '\n'; }
            pending.clear();
        }
        connection = nextConnection;
        pending += record["text"].toString();
    }
    text += plainRttText(std::move(pending));

    QJsonArray rows;
    qsizetype outputBytes = 0;
    int matches = 0;
    int lineNumber = 0;
    bool pageFull = false;
    for (qsizetype start = 0; start < text.size();)
    {
        auto end = text.indexOf('\n', start);
        if (end < 0) { end = text.size(); }
        const auto size = end - start;
        const auto line = text.mid(start, qMin<qsizetype>(size, 8192));
        start = end + 1;
        ++lineNumber;
        if (line.isEmpty() || !rttSearchFilter.matches(line)) { continue; }
        if (++matches <= offset || pageFull || rows.size() >= limit) { continue; }
        const QJsonObject row {{"line", lineNumber}, {"text", line}, {"truncated", size > line.size()}};
        const auto bytes = QJsonDocument(row).toJson(QJsonDocument::Compact).size();
        if (outputBytes + bytes > 512 * 1024)
        {
            pageFull = true;
            continue;
        }
        rows.append(row);
        outputBytes += bytes;
    }
    const auto next = offset + rows.size();
    return {{"view_id", viewId}, {"job_id", rttJob}, {"scope", "rtt_lines"}, {"rows", rows},
        {"filter", rttSearchFilter.settings()}, {"error", rttSearchFilter.error}, {"snapshot", currentSnapshot},
        {"total_matches", matches}, {"next_offset", next}, {"has_more", next < matches}};
}
}
