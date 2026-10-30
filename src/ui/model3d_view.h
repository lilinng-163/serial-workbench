// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "controls.h"
#include "model3d_data.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>

namespace sw
{
class ModelView : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit ModelView(QWidget *parent = nullptr);
    ~ModelView() override;
    void setMesh(const ModelMesh &value);
    void setPose(const ModelPose &value);
    void setTheme(const Theme &value);
    void setModelColor(const QColor &value);
    QColor modelColor() const;
    void setModelTransform(double scale, bool centered, const QVector3D &alignment);
    void fit();
    void setView(const QString &direction);
    QJsonObject cameraSettings() const;
    void restoreCamera(const QJsonObject &settings);
    ModelPose pose() const { return currentPose; }
    QString renderError() const { return error; }
    QJsonObject modelState() const;
    QImage capture(int maxSize);
    void setAntialiasing(int samples);
    int antialiasing() const { return requestedSamples; }
    bool isNavigating() const { return navigating; }

signals:
    void cameraChanged();
    void renderQualityChanged(int samples);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void cleanup();
    void requestFrame();
    QMatrix4x4 modelMatrix() const;
    void lines(const QList<QVector3D> &points, const QColor &color, const QMatrix4x4 &matrix);
    Theme theme;
    QColor customColor;
    ModelMesh mesh;
    ModelPose currentPose;
    QOpenGLBuffer meshBuffer;
    QOpenGLBuffer lineBuffer;
    std::unique_ptr<QOpenGLShaderProgram> program;
    bool dirty = true;
    QString error;
    double modelScale = 1;
    bool centered = true;
    QVector3D alignment;
    float yaw = 42;
    float elevation = 26;
    float distance = 6;
    QVector3D target;
    QPointF previousMouse;
    bool navigating = false;
    QTimer frameTimer;
    int requestedSamples = 4;
    int effectiveSamples = 0;
    bool qualityReported = false;
    bool qualityDirty = true;
    bool fittedCamera = true;
    std::unique_ptr<QOpenGLFramebufferObject> multisampleBuffer;
};
}
