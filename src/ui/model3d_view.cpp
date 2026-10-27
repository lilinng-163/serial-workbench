// SPDX-License-Identifier: Apache-2.0
#include "model3d_view.h"
#include <QOpenGLContext>
#include <QtMath>
#include <cmath>

namespace sw
{
ModelView::ModelView(QWidget *parent) : QOpenGLWidget(parent)
{
    setObjectName("model3dViewport");
    setMinimumSize(240, 220);
    setFocusPolicy(Qt::StrongFocus);
    QSurfaceFormat surface;
    surface.setVersion(2, 1);
    surface.setDepthBufferSize(24);
    surface.setSamples(4);
    setFormat(surface);
    setMesh(exampleModelMesh());
}

ModelView::~ModelView()
{
    cleanup();
}

void ModelView::cleanup()
{
    if (context())
    {
        makeCurrent();
        meshBuffer.destroy();
        lineBuffer.destroy();
        program.reset();
        doneCurrent();
        disconnect(context(), nullptr, this, nullptr);
    }
}

void ModelView::initializeGL()
{
    initializeOpenGLFunctions();
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &ModelView::cleanup, Qt::DirectConnection);
    program = std::make_unique<QOpenGLShaderProgram>();
    const char *vertex = R"(
        attribute highp vec3 position;
        attribute highp vec3 normal;
        uniform highp mat4 mvp;
        uniform highp mat3 normals;
        varying highp vec3 n;
        void main() { gl_Position = mvp * vec4(position, 1.0); n = normals * normal; }
    )";
    const char *fragment = R"(
        #ifdef GL_ES
        precision mediump float;
        #endif
        uniform vec4 color;
        uniform bool shaded;
        varying highp vec3 n;
        void main() {
            float light = 1.0;
            if (shaded) {
                vec3 surface = normalize(n) * (gl_FrontFacing ? 1.0 : -1.0);
                light = 0.35 + 0.65 * max(dot(surface, normalize(vec3(0.4, -0.6, 0.9))), 0.0);
            }
            gl_FragColor = vec4(color.rgb * light, color.a);
        }
    )";
    if (!program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex)
        || !program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment) || !program->link()
        || !meshBuffer.create() || !lineBuffer.create())
    {
        error = "3D 渲染初始化失败，请检查显卡驱动";
        qWarning() << "Model renderer:" << program->log();
        program.reset();
        return;
    }
    error.clear();
    dirty = true;
    meshBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    lineBuffer.setUsagePattern(QOpenGLBuffer::StreamDraw);
}

void ModelView::setMesh(const ModelMesh &value)
{
    mesh = value;
    dirty = true;
    fit();
}

void ModelView::setPose(const ModelPose &value)
{
    if (value.valid())
    {
        currentPose = value;
        update();
    }
}

void ModelView::setTheme(const Theme &value)
{
    theme = value;
    update();
}

void ModelView::setModelColor(const QColor &value)
{
    customColor = value;
    update();
}

QColor ModelView::modelColor() const
{
    return customColor.isValid() ? customColor : (theme.dark ? theme.accent.lighter(125) : theme.accent);
}

void ModelView::setModelTransform(double scale, bool center, const QVector3D &angles)
{
    modelScale = qBound(1e-6, scale, 1e6);
    centered = center;
    alignment = angles;
    update();
}

void ModelView::fit()
{
    target = currentPose.position;
    if (!centered)
    {
        QMatrix4x4 modelAlignment;
        modelAlignment.rotate(alignment.z(), 0, 0, 1);
        modelAlignment.rotate(alignment.y(), 0, 1, 0);
        modelAlignment.rotate(alignment.x(), 1, 0, 0);
        target += currentPose.matrix.mapVector(modelAlignment.mapVector(mesh.center() * modelScale));
    }
    const double aspect = double(width()) / qMax(1, height());
    const double halfVertical = qDegreesToRadians(20.0);
    const double halfHorizontal = std::atan(std::tan(halfVertical) * aspect);
    const double limitingAngle = qMin(halfVertical, halfHorizontal);
    distance = qMax(0.001, mesh.radius() * modelScale * 1.1 / std::sin(limitingAngle));
    update();
}

void ModelView::setView(const QString &direction)
{
    yaw = direction == "front" ? -90 : direction == "right" ? 0 : 42;
    elevation = direction == "top" ? 89.9f : direction == "iso" ? 26 : 0;
    fit();
    emit cameraChanged();
}

QJsonObject ModelView::cameraSettings() const
{
    return {{"yaw", yaw}, {"elevation", elevation}, {"distance", distance},
        {"target", QJsonArray {target.x(), target.y(), target.z()}}};
}

void ModelView::restoreCamera(const QJsonObject &settings)
{
    if (settings.isEmpty()) { return; }
    yaw = std::remainder(settings["yaw"].toDouble(42), 360);
    elevation = qBound(-89.9, settings["elevation"].toDouble(26), 89.9);
    distance = qBound(0.001, settings["distance"].toDouble(6), 1e12);
    const auto point = settings["target"].toArray();
    for (int i = 0; i < 3; ++i) { target[i] = qBound(-1e12, i < point.size() ? point[i].toDouble() : 0, 1e12); }
    update();
}

QMatrix4x4 ModelView::modelMatrix() const
{
    QMatrix4x4 model = currentPose.matrix;
    model.rotate(alignment.z(), 0, 0, 1);
    model.rotate(alignment.y(), 0, 1, 0);
    model.rotate(alignment.x(), 1, 0, 0);
    model.scale(modelScale);
    if (centered) { model.translate(-mesh.center()); }
    return model;
}

QJsonObject ModelView::modelState() const
{
    const auto vector = [](const QVector3D &v) { return QJsonArray {v.x(), v.y(), v.z()}; };
    const auto matrix = modelMatrix();
    QJsonArray rows;
    for (int row = 0; row < 4; ++row)
    {
        rows.append(QJsonArray {matrix(row, 0), matrix(row, 1), matrix(row, 2), matrix(row, 3)});
    }
    QVector3D low;
    QVector3D high;
    for (int corner = 0; corner < 8; ++corner)
    {
        const auto point = matrix.map(QVector3D(corner & 1 ? mesh.maximum.x() : mesh.minimum.x(),
            corner & 2 ? mesh.maximum.y() : mesh.minimum.y(), corner & 4 ? mesh.maximum.z() : mesh.minimum.z()));
        for (int axis = 0; axis < 3; ++axis)
        {
            low[axis] = corner ? qMin(low[axis], point[axis]) : point[axis];
            high[axis] = corner ? qMax(high[axis], point[axis]) : point[axis];
        }
    }
    return {{"triangles", mesh.vertices.size() / 3}, {"local_min", vector(mesh.minimum)},
        {"local_max", vector(mesh.maximum)}, {"world_bounds_min", vector(low)}, {"world_bounds_max", vector(high)},
        {"position", vector(currentPose.position)}, {"model_matrix_rows", rows},
        {"effective_color", modelColor().name()}, {"render_error", error}};
}

QImage ModelView::capture(int maxSize)
{
    // A separate, non-visible viewport uses the same mesh, renderer and camera.
    // It also works when the shared runtime was started without a GUI window.
    // Never pump events here: another MCP call must not change the pose mid-frame.
    ModelView renderer;
    renderer.setAttribute(Qt::WA_DontShowOnScreen);
    renderer.setMinimumSize(1, 1);
    renderer.resize(size().scaled(maxSize, maxSize, Qt::KeepAspectRatio));
    renderer.setTheme(theme);
    renderer.setMesh(mesh);
    renderer.setPose(currentPose);
    renderer.setModelColor(customColor);
    renderer.setModelTransform(modelScale, centered, alignment);
    renderer.restoreCamera(cameraSettings());
    renderer.show();
    auto frame = renderer.grabFramebuffer();
    if (!renderer.isValid() || !renderer.renderError().isEmpty() || frame.isNull())
    {
        fail("无法捕获 3D 视口：OpenGL 渲染不可用。请在本机桌面环境运行工作台并检查显卡驱动。");
    }
    return frame.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void ModelView::lines(const QList<QVector3D> &points, const QColor &color, const QMatrix4x4 &matrix)
{
    lineBuffer.bind();
    lineBuffer.allocate(points.constData(), int(points.size() * sizeof(QVector3D)));
    program->setUniformValue("mvp", matrix);
    program->setUniformValue("color", color);
    program->setUniformValue("shaded", false);
    program->enableAttributeArray("position");
    program->setAttributeBuffer("position", GL_FLOAT, 0, 3, sizeof(QVector3D));
    program->disableAttributeArray("normal");
    program->setAttributeValue("normal", QVector3D(0, 0, 1));
    glDrawArrays(GL_LINES, 0, points.size());
    lineBuffer.release();
}

void ModelView::paintGL()
{
    QPainter painter(this);
    painter.beginNativePainting();
    // QPainter leaves depth writes disabled. Reset before clearing so old poses
    // cannot survive in the depth buffer and cut holes into the next frame.
    glDepthMask(GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(theme.panel.redF(), theme.panel.greenF(), theme.panel.blueF(), 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!program)
    {
        painter.endNativePainting();
        painter.setPen(theme.red);
        painter.drawText(rect().adjusted(20, 20, -20, -20), Qt::AlignCenter | Qt::TextWordWrap, error);
        return;
    }
    const float radius = qMax(float(mesh.radius() * modelScale), 0.0001f);
    const float azimuth = qDegreesToRadians(yaw);
    const float altitude = qDegreesToRadians(elevation);
    QMatrix4x4 view;
    view.lookAt(target + distance * QVector3D(std::cos(altitude) * std::cos(azimuth),
        std::cos(altitude) * std::sin(azimuth), std::sin(altitude)), target, {0, 0, 1});
    QMatrix4x4 projection;
    projection.perspective(40, float(width()) / qMax(1, height()), qMax(.00001f, distance / 10000),
        qMax(distance * 10, radius * 100));
    const auto vp = projection * view;
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    program->bind();
    program->setUniformValue("normals", QMatrix3x3());
    const double step = std::pow(10, std::floor(std::log10(radius / 3)));
    QList<QVector3D> grid;
    for (int i = -10; i <= 10; ++i)
    {
        grid.append({float(i * step), float(-10 * step), 0});
        grid.append({float(i * step), float(10 * step), 0});
        grid.append({float(-10 * step), float(i * step), 0});
        grid.append({float(10 * step), float(i * step), 0});
    }
    QColor gridColor = theme.line;
    gridColor.setAlpha(105);
    // The reference grid is a backdrop, not a surface that cuts into the model.
    glDepthMask(GL_FALSE);
    lines(grid, gridColor, vp);
    glDepthMask(GL_TRUE);
    const auto model = modelMatrix();
    meshBuffer.bind();
    if (dirty)
    {
        meshBuffer.allocate(mesh.vertices.constData(), int(mesh.vertices.size() * sizeof(ModelVertex)));
        dirty = false;
    }
    program->setUniformValue("mvp", vp * model);
    program->setUniformValue("normals", model.normalMatrix());
    program->setUniformValue("color", modelColor());
    program->setUniformValue("shaded", true);
    program->enableAttributeArray("position");
    program->enableAttributeArray("normal");
    program->setAttributeBuffer("position", GL_FLOAT, offsetof(ModelVertex, position), 3, sizeof(ModelVertex));
    program->setAttributeBuffer("normal", GL_FLOAT, offsetof(ModelVertex, normal), 3, sizeof(ModelVertex));
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertices.size());
    meshBuffer.release();
    const QList<QColor> axisColors {QColor("#db6560"), QColor("#5da87b"), QColor("#639edb")};
    for (int i = 0; i < 3; ++i)
    {
        QVector3D axis;
        axis[i] = radius * 1.7f;
        lines({QVector3D(), axis}, axisColors[i], vp);
    }
    program->release();
    glDisable(GL_DEPTH_TEST);
    painter.endNativePainting();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(theme.muted);
    painter.drawText(QRect(16, 10, width() - 32, 24), "世界坐标 · Z 向上");
    painter.drawText(QRect(16, height() - 30, width() - 32, 22), "左键旋转 · 中键平移 · 滚轮缩放 · 双击适配");
    const QPointF origin(width() - 54, 60);
    for (int i = 0; i < 3; ++i)
    {
        QVector3D axis;
        axis[i] = 1;
        const auto direction = view.mapVector(axis);
        const QPointF end = origin + QPointF(direction.x() * 29, -direction.y() * 29);
        painter.setPen(QPen(axisColors[i], 2.5, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(origin, end);
        painter.drawText(end + QPointF(3, -3), QString("XYZ")[i]);
    }
}

void ModelView::mousePressEvent(QMouseEvent *event)
{
    previousMouse = event->position();
    setFocus();
    event->accept();
}

void ModelView::mouseMoveEvent(QMouseEvent *event)
{
    const auto delta = event->position() - previousMouse;
    previousMouse = event->position();
    if (event->buttons() & Qt::LeftButton)
    {
        yaw = std::remainder(yaw - delta.x() * .4, 360);
        elevation = qBound(-89.9, elevation + delta.y() * .4, 89.9);
    }
    else if (event->buttons() & Qt::MiddleButton)
    {
        const float a = qDegreesToRadians(yaw);
        const float e = qDegreesToRadians(elevation);
        const QVector3D right(-std::sin(a), std::cos(a), 0);
        const QVector3D up(-std::sin(e) * std::cos(a), -std::sin(e) * std::sin(a), std::cos(e));
        target += (up * delta.y() - right * delta.x()) * (distance * .75 / qMax(1, height()));
    }
    update();
}

void ModelView::mouseReleaseEvent(QMouseEvent *event)
{
    emit cameraChanged();
    event->accept();
}

void ModelView::wheelEvent(QWheelEvent *event)
{
    distance = qBound(0.001, distance * std::pow(.85, event->angleDelta().y() / 120.0), 1e12);
    update();
    emit cameraChanged();
    event->accept();
}

void ModelView::mouseDoubleClickEvent(QMouseEvent *)
{
    fit();
    emit cameraChanged();
}
}
