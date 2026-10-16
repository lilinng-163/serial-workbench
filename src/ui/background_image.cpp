// SPDX-License-Identifier: Apache-2.0
#include "background_image.h"
#include <QImageReader>

namespace sw
{
QJsonObject BackgroundSpec::json() const
{
    return {{"file", file}, {"name", name}, {"fit", fit}, {"opacity", opacity},
        {"position_x", positionX}, {"position_y", positionY}};
}

BackgroundSpec BackgroundSpec::fromJson(const QJsonObject &value)
{
    BackgroundSpec result;
    result.file = value["file"].toString();
    result.name = value["name"].toString(QFileInfo(result.file).fileName());
    result.fit = value["fit"].toString("cover");
    if (!QStringList {"cover", "contain", "tile"}.contains(result.fit)) { result.fit = "cover"; }
    result.opacity = qBound(0, value["opacity"].toInt(25), 100);
    result.positionX = qBound(0, value["position_x"].toInt(50), 100);
    result.positionY = qBound(0, value["position_y"].toInt(50), 100);
    return result;
}

QImage BackgroundImage::read(const QString &path, QString *error)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    reader.setDecideFormatFromContent(true);
    const QSize size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > 64000000)
    {
        if (error) { *error = "图片无法识别或尺寸过大，请选择不超过 6400 万像素的图片。"; }
        return {};
    }
    if (size.width() > 4096 || size.height() > 4096)
    {
        reader.setScaledSize(size.scaled(QSize(4096, 4096), Qt::KeepAspectRatio));
    }
    auto image = reader.read();
    if (image.isNull())
    {
        if (error) { *error = "无法读取图片：" + reader.errorString(); }
        return {};
    }
    if (image.width() > 4096 || image.height() > 4096)
    {
        image = image.scaled(QSize(4096, 4096), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image.convertToFormat(QImage::Format_RGBA8888);
}

QString BackgroundImage::importFile(const QString &path, const QString &settingsPath, QString *error)
{
    const auto image = read(path, error);
    if (image.isNull()) { return {}; }
    QCryptographicHash digest(QCryptographicHash::Sha256);
    digest.addData(QByteArray::number(image.width()) + "x" + QByteArray::number(image.height()));
    digest.addData(QByteArray::fromRawData(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes()));
    const QString relative = "backgrounds/" + QString::fromLatin1(digest.result().toHex()) + ".png";
    const QDir directory(QFileInfo(settingsPath).absolutePath());
    if (!directory.mkpath("backgrounds"))
    {
        if (error) { *error = "无法创建背景图片保存目录。"; }
        return {};
    }
    QSaveFile output(directory.filePath(relative));
    if (!output.open(QIODevice::WriteOnly) || !image.save(&output, "PNG") || !output.commit())
    {
        if (error) { *error = "无法保存背景图片：" + output.errorString(); }
        return {};
    }
    return relative;
}

void BackgroundImage::set(const BackgroundSpec &specification, const QImage &source)
{
    if (image.cacheKey() != source.cacheKey() || spec.fit != specification.fit
        || spec.positionX != specification.positionX || spec.positionY != specification.positionY)
    {
        cache = {};
        cacheSize = {};
    }
    spec = specification;
    image = source;
}

bool BackgroundImage::hasImage() const
{
    return !image.isNull();
}

void BackgroundImage::paint(QPainter &painter, const QRect &bounds) const
{
    if (image.isNull() || spec.opacity == 0 || bounds.isEmpty()) { return; }
    const qreal ratio = painter.device()->devicePixelRatioF();
    if (cache.isNull() || cacheSize != bounds.size() || !qFuzzyCompare(cacheRatio, ratio))
    {
        cacheSize = bounds.size();
        cacheRatio = ratio;
        cache = QPixmap(qCeil(bounds.width() * ratio), qCeil(bounds.height() * ratio));
        cache.setDevicePixelRatio(ratio);
        cache.fill(Qt::transparent);
        QPainter target(&cache);
        target.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF area(QPointF(), QSizeF(bounds.size()));
        const qreal horizontal = qBound(0, spec.positionX, 100) / 100.0;
        const qreal vertical = qBound(0, spec.positionY, 100) / 100.0;
        if (spec.fit == "tile")
        {
            const QPointF origin(image.width() * (0.5 - horizontal), image.height() * (0.5 - vertical));
            target.drawTiledPixmap(area, QPixmap::fromImage(image), origin);
        }
        else if (spec.fit == "contain")
        {
            const QSizeF size = QSizeF(image.size()).scaled(area.size(), Qt::KeepAspectRatio);
            const QRectF destination((area.width() - size.width()) * horizontal,
                (area.height() - size.height()) * vertical, size.width(), size.height());
            target.drawImage(destination, image);
        }
        else
        {
            const qreal scale = qMax(area.width() / image.width(), area.height() / image.height());
            const QSizeF size(area.width() / scale, area.height() / scale);
            const QRectF source((image.width() - size.width()) * horizontal,
                (image.height() - size.height()) * vertical, size.width(), size.height());
            target.drawImage(area, image, source);
        }
    }
    painter.save();
    painter.setOpacity(painter.opacity() * spec.opacity / 100.0);
    painter.drawPixmap(bounds.topLeft(), cache);
    painter.restore();
}

BackgroundSurface::BackgroundSurface(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void BackgroundSurface::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    if (!transparentBase) { painter.fillRect(rect(), base); }
    const QRect bounds = windowAligned ? QRect(mapFrom(window(), QPoint()), window()->size()) : rect();
    background.paint(painter, bounds);
}
}
