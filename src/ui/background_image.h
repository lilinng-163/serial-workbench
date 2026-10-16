// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QtWidgets>

namespace sw
{
struct BackgroundSpec
{
    QString file;
    QString name;
    QString fit = "cover";
    int opacity = 25;
    int positionX = 50;
    int positionY = 50;
    QJsonObject json() const;
    static BackgroundSpec fromJson(const QJsonObject &value);
};

class BackgroundImage
{
public:
    void set(const BackgroundSpec &specification, const QImage &source);
    bool hasImage() const;
    void paint(QPainter &painter, const QRect &bounds) const;
    static QImage read(const QString &path, QString *error = nullptr);
    static QString importFile(const QString &path, const QString &settingsPath, QString *error = nullptr);

private:
    BackgroundSpec spec;
    QImage image;
    mutable QPixmap cache;
    mutable QSize cacheSize;
    mutable qreal cacheRatio = 0;
};

class BackgroundSurface : public QWidget
{
    Q_OBJECT
public:
    explicit BackgroundSurface(QWidget *parent = nullptr);
    BackgroundImage background;
    QColor base;
    bool transparentBase = false;
    bool windowAligned = false;
    void paintEvent(QPaintEvent *event) override;
};
}
