// SPDX-License-Identifier: Apache-2.0

#include "controls.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace sw
{
namespace
{
QString exactValue(double value)
{
    return QLocale::c().toString(value, 'g', QLocale::FloatingPointShortest);
}

qreal mappedY(double value, const QPair<double, double> &range, const QRectF &area)
{
    const long double fraction = (static_cast<long double>(value) - range.first)
        / (static_cast<long double>(range.second) - range.first);
    const long double coordinate = area.bottom() - fraction * area.height();
    return static_cast<qreal>(std::clamp(coordinate, -1e9L, 1e9L));
}

QPair<double, double> scaledRange(const QPair<double, double> &range, qreal fraction, double factor, bool integral)
{
    const long double span = static_cast<long double>(range.second) - range.first;
    const long double anchor = range.first + span * fraction;
    const long double minimum = integral ? 1.0L : std::max(1e-300L,
        std::abs(anchor) * std::numeric_limits<double>::epsilon() * 16);
    const long double maximum = std::numeric_limits<double>::max();
    const long double nextSpan = std::clamp(span / factor, minimum, integral ? 50000.0L : maximum);
    const double low = static_cast<double>(std::clamp(anchor - nextSpan * fraction, -maximum, maximum));
    const double high = static_cast<double>(std::clamp(anchor + nextSpan * (1 - fraction), -maximum, maximum));
    return low < high ? QPair<double, double>(low, high) : range;
}

QPair<double, double> translatedRange(const QPair<double, double> &range, qreal fraction)
{
    const long double shift = (static_cast<long double>(range.second) - range.first) * fraction;
    const long double maximum = std::numeric_limits<double>::max();
    const double low = static_cast<double>(std::clamp(range.first + shift, -maximum, maximum));
    const double high = static_cast<double>(std::clamp(range.second + shift, -maximum, maximum));
    return low < high ? QPair<double, double>(low, high) : range;
}

struct AxisGrid
{
    QList<double> major;
    QList<double> minor;
};

AxisGrid axisGrid(const QPair<double, double> &range, qreal pixels, qreal labelSpacing, bool integral)
{
    // 按屏幕距离选择最接近的刻度，避免向上取整后留下过宽的格子。
    const long double span = static_cast<long double>(range.second) - range.first;
    const long double raw = span / std::max(1.0, pixels / labelSpacing);
    const long double magnitude = std::pow(10.0L, std::floor(std::log10(std::max(integral ? 1.0L : 1e-300L, raw))));
    long double step = magnitude;
    long double error = std::numeric_limits<long double>::max();
    for (long double multiple : {1.0L, 2.0L, 2.5L, 5.0L, 10.0L})
    {
        long double candidate = magnitude * multiple;
        if (integral) { candidate = std::max(1.0L, std::ceil(candidate)); }
        const long double distance = std::abs(std::log(candidate / raw));
        if (distance < error) { step = candidate; error = distance; }
    }
    AxisGrid grid;
    const bool subdivide = !integral || step >= 2;
    const long double minorStep = subdivide ? step / 2 : step;
    const long double firstIndex = std::ceil(range.first / minorStep);
    const int count = std::min(512, static_cast<int>(std::ceil(span / minorStep)) + 2);
    for (int index = 0; index < count; ++index)
    {
        const long double tickIndex = firstIndex + index;
        const long double value = tickIndex * minorStep;
        if (value > range.second + minorStep * 1e-9L) { break; }
        const double tick = std::abs(value) < minorStep * 1e-9L ? 0.0 : static_cast<double>(value);
        if (!subdivide || std::abs(std::remainder(tickIndex, 2.0L)) < 0.1L) { grid.major.append(tick); }
        else { grid.minor.append(tick); }
    }
    return grid;
}

}

WavePlot::WavePlot(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(200, 180);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip("移动鼠标查看原始采样值；点击通道显示或隐藏，右键设置样式；滚轮缩放双轴；Shift 仅横轴，Ctrl 仅纵轴；左键自由平移；双击恢复跟随");

    legendScroll = new QScrollArea(this);
    legendScroll->setObjectName("waveLegend");
    legendScroll->setFrameShape(QFrame::NoFrame);
    legendScroll->setWidgetResizable(true);
    legendScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    legendScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    legendBody = new QWidget();
    legendLayout = new QHBoxLayout(legendBody);
    legendLayout->setContentsMargins(0, 0, 0, 0);
    legendLayout->setSpacing(6);
    legendLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    legendLayout->setSizeConstraint(QLayout::SetMinimumSize);
    readoutPicker = new QToolButton(legendBody);
    readoutPicker->setObjectName("waveReadoutPicker");
    readoutPicker->setText("读数通道…");
    readoutPicker->setToolTip("选择顶部显示的读数通道，最多四个");
    connect(readoutPicker, &QToolButton::clicked, this, &WavePlot::openReadoutChannels);
    legendLayout->addWidget(readoutPicker);
    legendLayout->addStretch();
    legendScroll->setWidget(legendBody);

    auto copy = new QAction("复制当前读数", this);
    copy->setObjectName("copyWaveReadout");
    copy->setShortcut(QKeySequence::Copy);
    copy->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(copy);
    connect(copy, &QAction::triggered, this, [this]
    {
        const auto value = hovering ? inspect(hoverPosition) : QJsonObject();
        if (!value.isEmpty())
        {
            QApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact)));
        }
    });
}

QPair<double, double> WavePlot::sampleBounds() const
{
    double first = std::numeric_limits<double>::max();
    double last = std::numeric_limits<double>::lowest();
    for (const auto &points : series)
    {
        if (points.isEmpty()) { continue; }
        first = std::min(first, points.first().x());
        last = std::max(last, points.last().x());
    }
    if (last < first) { return {0, 1}; }
    return {first, std::max(first + 1, last)};
}

QPair<double, double> WavePlot::horizontalRange() const
{
    if (!following) { return horizontalView; }
    const auto bounds = sampleBounds();
    return followSpan > 0 ? QPair<double, double>(bounds.second - followSpan, bounds.second) : bounds;
}

QPair<double, double> WavePlot::verticalRange() const
{
    return manualVertical ? verticalView : valueRange();
}

void WavePlot::setFollowLatest(bool enabled)
{
    if (following == enabled) { return; }
    if (!enabled) { horizontalView = horizontalRange(); }
    else
    {
        // 跟随只平移横轴；保留当前横轴跨度和纵轴显示范围。
        followSpan = horizontalView.second - horizontalView.first;
        verticalView = verticalRange();
        manualVertical = true;
    }
    following = enabled;
    emit followLatestChanged(enabled);
    invalidatePlot();
}

void WavePlot::resetView()
{
    followSpan = 0;
    manualVertical = false;
    dragging = false;
    hovering = false;
    unsetCursor();
    const bool changed = !following;
    following = true;
    if (changed) { emit followLatestChanged(true); }
    invalidatePlot();
}

QPair<double, double> WavePlot::valueRange() const
{
    if (!rangeDirty) { return cachedRange; }
    double low = std::numeric_limits<double>::max();
    double high = std::numeric_limits<double>::lowest();
    for (auto curve = series.cbegin(); curve != series.cend(); ++curve)
    {
        if (!isCurveVisible(curve.key())) { continue; }
        for (const auto &point : curve.value())
        {
            low = std::min(low, point.y());
            high = std::max(high, point.y());
        }
    }
    if (high < low) { low = 0; high = 1; }
    const long double extent = high == low ? std::abs(static_cast<long double>(low))
        : static_cast<long double>(high) - low;
    const long double padding = std::max(0.01L, extent) * 0.1L;
    const long double maximum = std::numeric_limits<double>::max();
    cachedRange = {static_cast<double>(std::max(-maximum, static_cast<long double>(low) - padding)),
                   static_cast<double>(std::min(maximum, static_cast<long double>(high) + padding))};
    rangeDirty = false;
    return cachedRange;
}

QRectF WavePlot::plotArea() const
{
    if (dragging) { return dragArea; }
    const auto range = verticalRange();
    const QFontMetrics metrics(font());
    int labels = 0;
    const qreal gridHeight = qMax(1, height() - metrics.height() * 2 - 92);
    for (double value : axisGrid(range, gridHeight, qMax(60, metrics.height() * 3), false).major)
    {
        labels = qMax(labels, metrics.horizontalAdvance(QLocale::c().toString(value, 'g', 7)));
    }
    const int titleSpace = showY && !yAxisLabel().isEmpty() ? metrics.height() + 8 : 0;
    const int left = showY ? qBound(64, labels + 16, qMax(64, width() / 3)) + titleSpace : 12;
    const int top = metrics.height() * 2 + 32;
    return QRectF(left, top, qMax(1, width() - left - 24), qMax(1, height() - top - (showX ? 60 : 12)));
}

QColor WavePlot::curveColor(const QString &name) const
{
    const auto style = curveStyle(name);
    if (style.color.isValid()) { return style.color; }
    const QList<QColor> colors {theme.accentStroke, theme.green, QColor("#6c92a0"), QColor("#b0839a"), theme.red, QColor("#9e9a52")};
    return colors[colorSlots.value(name, 0) % colors.size()];
}

bool WavePlot::isCurveVisible(const QString &name) const
{
    return !hiddenCurves.contains(name);
}

void WavePlot::setCurveVisible(const QString &name, bool visible)
{
    if (visible) { hiddenCurves.remove(name); }
    else { hiddenCurves.insert(name); }
    rangeDirty = true;
    manualVertical = false;
    hovering = false;
    updateLegend();
    invalidatePlot();
    emit presentationChanged();
}

void WavePlot::setXAxisVisible(bool visible)
{
    if (showX == visible) { return; }
    showX = visible;
    dragging = false;
    invalidatePlot();
}

void WavePlot::setYAxisVisible(bool visible)
{
    if (showY == visible) { return; }
    showY = visible;
    dragging = false;
    invalidatePlot();
}

void WavePlot::invalidatePlot()
{
    imageDirty = true;
    update();
}

void WavePlot::dataChanged()
{
    ++revision;
    rangeDirty = true;
    for (auto curve = series.cbegin(); curve != series.cend(); ++curve)
    {
        if (!colorSlots.contains(curve.key())) { colorSlots.insert(curve.key(), colorSlots.size()); }
    }
    updateLegend();
    invalidatePlot();
}

void WavePlot::setSamples(const QJsonObject &data)
{
    boundChannel.clear();
    clearedSamples.clear();
    sourceCursors.clear();
    sourceCurves.clear();
    series.clear();
    resetView();
    const auto sources = data["sources"].toObject();
    for (auto source = sources.constBegin(); source != sources.constEnd(); ++source)
    {
        for (const auto &value : source.value().toObject()["samples"].toArray())
        {
            const auto sample = value.toObject();
            const auto values = sample["values"].toObject();
            for (auto field = values.constBegin(); field != values.constEnd(); ++field)
            {
                const QString name = source.key() == "default" ? field.key() : source.key() + "/" + field.key();
                if (!series.contains(name) && series.size() >= 16) { continue; }
                series[name].append(QPointF(sample["index"].toDouble(), field.value().toDouble()));
            }
        }
    }
    dataChanged();
}

bool WavePlot::updateChannel(Channel *channel, int limit)
{
    if (boundChannel != channel)
    {
        clearedSamples.clear();
        resetView();
    }
    bool changed = boundChannel != channel || boundLimit != limit;
    if (changed)
    {
        series.clear();
        sourceCursors.clear();
        sourceCurves.clear();
        boundChannel = channel;
        boundLimit = limit;
    }
    if (!channel)
    {
        if (!series.isEmpty()) { series.clear(); changed = true; }
        if (changed) { dataChanged(); }
        return changed;
    }

    for (const auto &key : sourceCursors.keys())
    {
        if (channel->sources.contains(key)) { continue; }
        for (const auto &name : sourceCurves.take(key)) { series.remove(name); }
        sourceCursors.remove(key);
        changed = true;
    }
    for (auto source = channel->sources.constBegin(); source != channel->sources.constEnd(); ++source)
    {
        const auto &buffer = *source.value();
        qint64 cursor = sourceCursors.value(source.key(), clearedSamples.value(source.key(), -1));
        if (cursor == buffer.count) { continue; }
        if (cursor > buffer.count)
        {
            clearedSamples.remove(source.key());
            for (const auto &name : sourceCurves.take(source.key())) { series.remove(name); }
            cursor = -1;
        }
        const int size = static_cast<int>(buffer.samples.size());
        const int first = qMax(0, size - limit);
        // 先移除窗口外的数据，为后续出现的新通道释放曲线名额。
        if (size > 0)
        {
            const double oldest = buffer.samples[first]["index"].toDouble();
            for (const auto &name : sourceCurves.value(source.key()))
            {
                auto &points = series[name];
                const auto retained = std::lower_bound(points.cbegin(), points.cend(), oldest,
                    [](const QPointF &point, double value) { return point.x() < value; });
                points.remove(0, static_cast<int>(retained - points.cbegin()));
                if (points.isEmpty())
                {
                    series.remove(name);
                    sourceCurves[source.key()].removeAll(name);
                }
            }
        }
        int begin = size - 1;
        while (begin >= first && buffer.samples[begin]["index"].toInteger() > cursor) { --begin; }
        for (int index = begin + 1; index < size; ++index)
        {
            const auto &sample = buffer.samples[index];
            const auto values = sample["values"].toObject();
            for (auto field = values.constBegin(); field != values.constEnd(); ++field)
            {
                const QString name = source.key() == "default" ? field.key() : source.key() + "/" + field.key();
                if (!series.contains(name) && series.size() >= 16) { continue; }
                series[name].append(QPointF(sample["index"].toDouble(), field.value().toDouble()));
                if (!sourceCurves[source.key()].contains(name)) { sourceCurves[source.key()].append(name); }
            }
        }
        sourceCursors[source.key()] = buffer.count;
        changed = true;
    }
    if (changed) { dataChanged(); }
    return changed;
}

void WavePlot::clearSamples(Channel *channel, int limit)
{
    boundChannel = channel;
    boundLimit = limit;
    series.clear();
    sourceCursors.clear();
    sourceCurves.clear();
    clearedSamples.clear();
    if (channel)
    {
        for (auto source = channel->sources.cbegin(); source != channel->sources.cend(); ++source)
        {
            clearedSamples[source.key()] = source.value()->count;
            sourceCursors[source.key()] = source.value()->count;
        }
    }
    resetView();
    dataChanged();
}

bool WavePlot::sampleIsCurrent(const QString &source, qint64 index) const
{
    return index > clearedSamples.value(source, -1);
}

void WavePlot::updateLegend()
{
    const auto channels = readoutChannels();
    for (const auto &name : legendButtons.keys())
    {
        if (!channels.contains(name)) { delete legendButtons.take(name); }
    }
    const QFontMetrics metrics(font());
    const int cardWidth = qMax(144, metrics.horizontalAdvance("-0.0000000e+000") + 48);
    for (const auto &name : channels)
    {
        auto button = legendButtons.value(name);
        if (!button)
        {
            button = new QToolButton(legendBody);
            button->setObjectName("waveChannel");
            button->setProperty("channelName", name);
            button->setCheckable(true);
            button->setFocusPolicy(Qt::TabFocus);
            button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            button->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(button, &QWidget::customContextMenuRequested, this, [this, button, name](const QPoint &position)
            {
                curveMenu(name, button->mapToGlobal(position));
            });
            legendLayout->insertWidget(channels.indexOf(name), button, 0, Qt::AlignTop);
            legendButtons[name] = button;
            connect(button, &QToolButton::clicked, this, [this, name](bool checked)
            {
                setCurveVisible(name, checked);
            });
        }
        if (legendLayout->indexOf(button) != channels.indexOf(name))
        {
            legendLayout->insertWidget(channels.indexOf(name), button, 0, Qt::AlignTop);
        }
        static const QVector<QPointF> empty;
        const auto curve = series.constFind(name);
        const auto &samples = curve == series.cend() ? empty : curve.value();
        const QString suffix = yUnit.isEmpty() ? QString() : " " + yUnit;
        const QString value = samples.isEmpty() ? "—" : exactValue(samples.last().y()) + suffix;
        const int textWidth = cardWidth - 48;
        const QString compact = metrics.horizontalAdvance(value) <= textWidth || samples.isEmpty() ? value
            : QLocale::c().toString(samples.last().y(), 'g', 8) + suffix;
        const QString text = metrics.elidedText(curveLabel(name), Qt::ElideRight, textWidth)
            + "\n" + metrics.elidedText(compact, Qt::ElideRight, textWidth);
        if (button->text() != text) { button->setText(text); }
        button->setFont(font());
        button->setFixedSize(cardWidth, metrics.height() * 2 + 8);
        button->setChecked(isCurveVisible(name));
        button->setToolTip(curveLabel(name) + " = " + value + "\n点击显示或隐藏曲线；右键设置颜色、线宽和线型");
        const QString style = QString("QToolButton { border: none; padding: 2px 8px; background: transparent; color: %1; } QToolButton:hover { color: %2; }")
            .arg(isCurveVisible(name) ? theme.text.name() : theme.muted.name(), theme.accentText.name());
        if (button->styleSheet() != style) { button->setStyleSheet(style); }
        const QColor color = isCurveVisible(name) ? curveColor(name) : theme.line;
        const auto appearance = curveStyle(name);
        if (button->property("curveColor").value<QColor>() != color
            || button->property("curveWidth").toDouble() != appearance.width
            || button->property("curveLine").toInt() != int(appearance.line))
        {
            const qreal ratio = devicePixelRatioF();
            QPixmap marker(qCeil(24 * ratio), qCeil(16 * ratio));
            marker.setDevicePixelRatio(ratio);
            marker.fill(Qt::transparent);
            QPainter painter(&marker);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(color, qMax(1.5, appearance.width), appearance.line));
            painter.drawLine(1, 8, 23, 8);
            button->setIcon(QIcon(marker));
            button->setIconSize(QSize(24, 16));
            button->setProperty("curveColor", color);
            button->setProperty("curveWidth", appearance.width);
            button->setProperty("curveLine", int(appearance.line));
        }
    }
    readoutPicker->setVisible(!series.isEmpty() || customReadouts);
    auto palette = legendBody->palette();
    palette.setColor(QPalette::Window, theme.panel);
    legendBody->setPalette(palette);
    legendBody->setAutoFillBackground(true);
    legendScroll->viewport()->setPalette(palette);
    legendScroll->setGeometry(64, 2, qMax(1, width() - 88), QFontMetrics(font()).height() * 2 + 24);
}

void WavePlot::rebuildImage()
{
    const qreal scale = devicePixelRatioF();
    const QSize pixels(qCeil(width() * scale), qCeil(height() * scale));
    if (image.size() != pixels || image.devicePixelRatioF() != scale)
    {
        image = QPixmap(pixels);
        image.setDevicePixelRatio(scale);
    }
    image.fill(theme.panel);
    QPainter painter(&image);
    painter.setFont(font());
    painter.setRenderHint(QPainter::TextAntialiasing);
    const auto area = plotArea();
    const auto range = verticalRange();
    const auto horizontal = horizontalRange();
    const double span = horizontal.second - horizontal.first;
    const double left = horizontal.first;
    const QFontMetrics metrics(font());
    const auto yGrid = axisGrid(range, area.height(), qMax(60, metrics.height() * 3), false);
    const QPair<double, double> displayRange {xAxisValue(horizontal.first), xAxisValue(horizontal.second)};
    const int xLabelWidth = qMax(metrics.horizontalAdvance(QLocale::c().toString(displayRange.first, 'g', 7)),
        metrics.horizontalAdvance(QLocale::c().toString(displayRange.second, 'g', 7)));
    const qreal labelSpacing = qMax(88, xLabelWidth + 20);
    const auto xGrid = axisGrid(displayRange, area.width(), labelSpacing, xIncrement == 1);

    // 主、次网格分别控制明度；数字只标在主刻度，保持密度与可读性。
    QColor minorColor = theme.line;
    minorColor.setAlpha(75);
    QColor majorColor = theme.line;
    majorColor.setAlpha(155);
    QPen minorPen(minorColor, 1, Qt::DashLine);
    minorPen.setDashPattern({2, 4});
    QPen majorPen(majorColor, 1, Qt::DashLine);
    majorPen.setDashPattern({3, 3});
    painter.setPen(minorPen);
    for (double value : yGrid.minor)
    {
        const qreal y = qRound(mappedY(value, range, area)) + 0.5;
        painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }
    for (double sample : xGrid.minor)
    {
        const qreal x = qRound(area.left() + (sample / xIncrement - left) / span * area.width()) + 0.5;
        painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
    }
    for (double value : yGrid.major)
    {
        const qreal y = qRound(mappedY(value, range, area)) + 0.5;
        painter.setPen(majorPen);
        painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
        if (!showY) { continue; }
        painter.setPen(theme.muted);
        painter.drawLine(QPointF(area.left() - 4, y), QPointF(area.left(), y));
        painter.setPen(theme.text);
        const int labelLeft = yAxisLabel().isEmpty() ? 2 : metrics.height() + 10;
        painter.drawText(QRectF(labelLeft, qRound(y) - 11, area.left() - labelLeft - 10, 22), Qt::AlignRight | Qt::AlignVCenter,
            QLocale::c().toString(value, 'g', 7));
    }
    QRectF lastLabel;
    const qreal labelWidth = qMax(24, xLabelWidth + 8);
    for (double sample : xGrid.major)
    {
        const qreal x = qRound(area.left() + (sample / xIncrement - left) / span * area.width()) + 0.5;
        painter.setPen(majorPen);
        painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        if (!showX) { continue; }
        painter.setPen(theme.muted);
        painter.drawLine(QPointF(x, area.bottom()), QPointF(x, area.bottom() + 5));
        const qreal labelX = qBound(2.0, x - labelWidth / 2, width() - labelWidth - 2);
        const QRectF label(labelX, area.bottom() + 8, labelWidth, 22);
        if (lastLabel.isEmpty() || label.left() >= lastLabel.right() + 8)
        {
            painter.setPen(theme.text);
            painter.drawText(label, Qt::AlignCenter, QLocale::c().toString(sample, 'g', 7));
            lastLabel = label;
        }
    }
    if (showX)
    {
        painter.setPen(theme.muted);
        painter.drawLine(QPointF(area.left(), area.bottom()), QPointF(area.right(), area.bottom()));
        painter.setPen(theme.text);
        painter.drawText(QRectF(area.left(), height() - 22, area.width(), 20), Qt::AlignCenter,
            metrics.elidedText(xAxisLabel(), Qt::ElideRight, qFloor(area.width())));
    }
    if (showY && !yAxisLabel().isEmpty())
    {
        painter.save();
        painter.setPen(theme.text);
        painter.translate(2, area.center().y());
        painter.rotate(-90);
        painter.drawText(QRectF(-area.height() / 2, 0, area.height(), metrics.height() + 4), Qt::AlignCenter,
            metrics.elidedText(yAxisLabel(), Qt::ElideRight, qFloor(area.height())));
        painter.restore();
    }
    painter.setPen(theme.text);
    if (series.isEmpty())
    {
        painter.drawText(area, Qt::AlignCenter | Qt::TextWordWrap, "等待 @wave 报文\n点击「数据格式」查看示例");
    }
    renderedPoints = 0;
    painter.setClipRect(area);
    painter.setRenderHint(QPainter::Antialiasing);
    for (auto curve = series.cbegin(); curve != series.cend(); ++curve)
    {
        if (!isCurveVisible(curve.key()) || curve.value().isEmpty()) { continue; }
        const auto &points = curve.value();
        auto begin = std::lower_bound(points.cbegin(), points.cend(), left,
            [](const QPointF &point, double value) { return point.x() < value; });
        if (begin != points.cbegin()) { --begin; }
        auto end = std::upper_bound(begin, points.cend(), left + span,
            [](double value, const QPointF &point) { return value < point.x(); });
        if (end != points.cend()) { ++end; }
        QPolygonF vertices;
        const auto append = [&](const QPointF &point)
        {
            vertices.append({area.left() + (point.x() - left) / span * area.width(), mappedY(point.y(), range, area)});
        };
        // 每列保留原始最小值与最大值，减少绘制顶点，同时保留短脉冲。
        for (auto point = begin; point != end;)
        {
            const int column = qFloor((point->x() - left) / span * area.width());
            auto minimum = point;
            auto maximum = point;
            auto next = point;
            while (next != end && qFloor((next->x() - left) / span * area.width()) == column)
            {
                if (next->y() < minimum->y()) { minimum = next; }
                if (next->y() > maximum->y()) { maximum = next; }
                ++next;
            }
            if (minimum < maximum) { append(*minimum); append(*maximum); }
            else if (maximum < minimum) { append(*maximum); append(*minimum); }
            else { append(*minimum); }
            point = next;
        }
        renderedPoints += vertices.size();
        // Cosmetic 细线使用 Qt 的快速抗锯齿线段路径，避免通用多边形描边。
        const auto appearance = curveStyle(curve.key());
        QPen pen(curveColor(curve.key()), appearance.width, appearance.line);
        pen.setCosmetic(true);
        painter.setPen(pen);
        painter.drawPolyline(vertices);
        if (vertices.size() == 1) { painter.drawEllipse(vertices.first(), 2, 2); }
    }
    imageDirty = false;
    imageFont = font();
    imageColors = {theme.panel, theme.line, theme.text, theme.accentStroke, theme.green, theme.red, theme.muted};
}

QJsonObject WavePlot::inspect(const QPointF &position) const
{
    const auto area = plotArea();
    if (!area.contains(position)) { return {}; }
    const auto horizontal = horizontalRange();
    const double span = horizontal.second - horizontal.first;
    const double left = horizontal.first;
    const double target = left + (position.x() - area.left()) / area.width() * span;
    const auto range = verticalRange();
    double closest = std::numeric_limits<double>::max();
    QJsonObject result;
    for (auto curve = series.cbegin(); curve != series.cend(); ++curve)
    {
        if (!isCurveVisible(curve.key()) || curve.value().isEmpty()) { continue; }
        const auto &points = curve.value();
        const auto found = std::lower_bound(points.cbegin(), points.cend(), target,
            [](const QPointF &point, double value) { return point.x() < value; });
        for (int back = 0; back < 2; ++back)
        {
            if ((back == 0 && found == points.cend()) || (back == 1 && found == points.cbegin())) { continue; }
            const auto &point = back == 0 ? *found : *(found - 1);
            if (point.x() < left || point.x() > left + span) { continue; }
            const qreal x = area.left() + (point.x() - left) / span * area.width();
            const qreal y = mappedY(point.y(), range, area);
            if (y < area.top() || y > area.bottom()) { continue; }
            const double distance = std::hypot(x - position.x(), y - position.y());
            if (distance < closest)
            {
                closest = distance;
                result = {{"channel", curve.key()}, {"sample", point.x()}, {"value", point.y()}};
            }
        }
    }
    return result;
}

void WavePlot::drawInspection(QPainter &painter)
{
    if (!hovering || dragging) { return; }
    const auto hit = inspect(hoverPosition);
    if (hit.isEmpty()) { return; }
    const QString name = hit["channel"].toString();
    const auto area = plotArea();
    const auto horizontal = horizontalRange();
    const double span = horizontal.second - horizontal.first;
    const qreal x = area.left() + (hit["sample"].toDouble() - horizontal.first) / span * area.width();
    const qreal y = mappedY(hit["value"].toDouble(), verticalRange(), area);
    painter.setPen(QPen(theme.muted, 1, Qt::DashLine));
    painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
    painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    painter.setPen(QPen(curveColor(name), 2));
    painter.setBrush(theme.panel);
    painter.drawEllipse(QPointF(x, y), 4, 4);

    QStringList lines {curveLabel(name), "值  " + exactValue(hit["value"].toDouble()) + (yUnit.isEmpty() ? QString() : " " + yUnit),
        "采样  #" + QString::number(hit["sample"].toInteger())};
    if (xIncrement != 1 || !xUnit.isEmpty())
    {
        lines.append("X  " + exactValue(xAxisValue(hit["sample"].toDouble())) + " " + xUnit);
    }
    const QFontMetrics metrics(font());
    int textWidth = 0;
    for (const auto &line : lines) { textWidth = qMax(textWidth, metrics.horizontalAdvance(line)); }
    const int boxWidth = qMin(width() - 16, textWidth + 24);
    const int boxHeight = metrics.height() * lines.size() + 20;
    qreal boxX = hoverPosition.x() + 18;
    qreal boxY = hoverPosition.y() + 18;
    if (boxX + boxWidth > width() - 8) { boxX = hoverPosition.x() - boxWidth - 18; }
    if (boxY + boxHeight > height() - 44) { boxY = hoverPosition.y() - boxHeight - 18; }
    const QRectF box(qRound(qBound(8.0, boxX, qMax(8.0, width() - boxWidth - 8.0))),
        qRound(qBound(4.0, boxY, qMax(4.0, height() - boxHeight - 4.0))), boxWidth, boxHeight);
    painter.fillRect(box.translated(2, 2), QColor(0, 0, 0, 25));
    painter.fillRect(box, theme.panel);
    painter.setPen(theme.edge);
    painter.drawRect(box);
    painter.fillRect(QRectF(box.left(), box.top(), 3, box.height()), curveColor(name));
    painter.setPen(theme.text);
    for (int index = 0; index < lines.size(); ++index)
    {
        const QString text = index == 0 ? metrics.elidedText(lines[index], Qt::ElideMiddle, boxWidth - 24) : lines[index];
        painter.drawText(QPointF(box.left() + 12, box.top() + 10 + metrics.ascent() + metrics.height() * index), text);
    }
}

void WavePlot::paintEvent(QPaintEvent *)
{
    const QList<QColor> colors {theme.panel, theme.line, theme.text, theme.accentStroke, theme.green, theme.red, theme.muted};
    if (imageColors != colors || imageFont != font()) { updateLegend(); imageDirty = true; }
    if (imageDirty || image.size() != QSize(qCeil(width() * devicePixelRatioF()), qCeil(height() * devicePixelRatioF()))
        || image.devicePixelRatioF() != devicePixelRatioF())
    {
        rebuildImage();
    }
    QPainter painter(this);
    painter.drawPixmap(0, 0, image);
    drawInspection(painter);
}

void WavePlot::wheelEvent(QWheelEvent *event)
{
    const auto area = plotArea();
    const auto position = event->position();
    const bool inside = area.contains(position);
    const bool xAxis = position.y() >= area.bottom() && position.x() >= area.left();
    const bool yAxis = position.x() <= area.left() && position.y() >= area.top();
    if ((!inside && !xAxis && !yAxis) || series.isEmpty()) { event->ignore(); return; }
    const qreal delta = event->angleDelta().y() != 0 ? event->angleDelta().y() / 120.0 : event->pixelDelta().y() / 80.0;
    if (delta == 0) { event->ignore(); return; }
    const double factor = std::pow(1.15, qBound(-6.0, delta, 6.0));
    const bool onlyX = xAxis || event->modifiers().testFlag(Qt::ShiftModifier);
    const bool onlyY = yAxis || event->modifiers().testFlag(Qt::ControlModifier);
    const auto horizontal = horizontalRange();
    const qreal xFraction = qBound(0.0, (position.x() - area.left()) / area.width(), 1.0);
    const double xAnchor = horizontal.first + (horizontal.second - horizontal.first) * xFraction;
    if (!onlyX)
    {
        const qreal fraction = qBound(0.0, (area.bottom() - position.y()) / area.height(), 1.0);
        verticalView = scaledRange(verticalRange(), fraction, factor, false);
        manualVertical = true;
    }
    if (!onlyY)
    {
        setFollowLatest(false);
        horizontalView = scaledRange(horizontal, xFraction, factor, true);
        // 纵轴文字宽度变化时仍将鼠标下的采样固定在原屏幕位置。
        const auto nextArea = plotArea();
        const qreal nextFraction = qBound(0.0, (position.x() - nextArea.left()) / nextArea.width(), 1.0);
        const double span = horizontalView.second - horizontalView.first;
        horizontalView = {xAnchor - span * nextFraction, xAnchor + span * (1 - nextFraction)};
    }
    invalidatePlot();
    event->accept();
}

void WavePlot::mousePressEvent(QMouseEvent *event)
{
    const auto area = plotArea();
    if (event->button() == Qt::LeftButton && !series.isEmpty() && area.contains(event->position()))
    {
        dragArea = area;
        dragStart = event->position();
        dragHorizontal = horizontalRange();
        dragVertical = verticalRange();
        dragging = true;
        setFollowLatest(false);
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void WavePlot::mouseMoveEvent(QMouseEvent *event)
{
    hoverPosition = event->position();
    hovering = plotArea().contains(hoverPosition);
    findChild<QAction *>("copyWaveReadout")->setEnabled(hovering);
    if (dragging && (event->buttons() & Qt::LeftButton))
    {
        const auto distance = event->position() - dragStart;
        horizontalView = translatedRange(dragHorizontal, -distance.x() / dragArea.width());
        verticalView = translatedRange(dragVertical, distance.y() / dragArea.height());
        manualVertical = true;
        invalidatePlot();
    }
    else
    {
        setCursor(hovering ? Qt::CrossCursor : Qt::ArrowCursor);
        update();
    }
    event->accept();
}

void WavePlot::mouseReleaseEvent(QMouseEvent *event)
{
    if (dragging && event->button() == Qt::LeftButton)
    {
        dragging = false;
        unsetCursor();
        invalidatePlot();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void WavePlot::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) { resetView(); event->accept(); return; }
    QWidget::mouseDoubleClickEvent(event);
}

void WavePlot::leaveEvent(QEvent *event)
{
    hovering = false;
    if (!dragging) { unsetCursor(); }
    update();
    QWidget::leaveEvent(event);
}

void WavePlot::resizeEvent(QResizeEvent *event)
{
    updateLegend();
    invalidatePlot();
    QWidget::resizeEvent(event);
}

void WavePlot::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    auto settings = menu.addAction("波形设置…");
    connect(settings, &QAction::triggered, this, &WavePlot::openPlotSettings);
    const auto hit = inspect(event->pos());
    if (!hit.isEmpty())
    {
        auto curve = menu.addAction("曲线设置 · " + curveLabel(hit["channel"].toString()) + "…");
        connect(curve, &QAction::triggered, this, [this, name = hit["channel"].toString()] { openCurveSettings(name); });
    }
    menu.addSeparator();
    auto copy = findChild<QAction *>("copyWaveReadout");
    copy->setEnabled(hovering && !inspect(hoverPosition).isEmpty());
    menu.addAction(copy);
    auto latest = menu.addAction("复制最新通道值");
    connect(latest, &QAction::triggered, this, [this]
    {
        QJsonObject values;
        for (auto curve = series.cbegin(); curve != series.cend(); ++curve)
        {
            if (!curve.value().isEmpty()) { values[curve.key()] = curve.value().last().y(); }
        }
        QApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Compact)));
    });
    menu.exec(event->globalPos());
}
} // namespace sw
