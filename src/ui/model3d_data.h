// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QJsonObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <array>

namespace sw
{
struct ModelVertex
{
    QVector3D position;
    QVector3D normal;
};

struct ModelMesh
{
    QList<ModelVertex> vertices;
    QVector3D minimum;
    QVector3D maximum;
    QVector3D center() const;
    float radius() const;
};

ModelMesh loadModelMesh(const QString &path);
ModelMesh exampleModelMesh();

struct ModelBinding
{
    QString field;
    double gain = 1;
    double offset = 0;
    QJsonObject settings() const;
    static ModelBinding restore(const QJsonObject &value);
};

struct ModelMapping
{
    std::array<ModelBinding, 3> position;
    std::array<ModelBinding, 3> angles {{{"roll"}, {"pitch"}, {"yaw"}}};
    std::array<ModelBinding, 4> quaternion {{{"qw"}, {"qx"}, {"qy"}, {"qz"}}};
    QString mode = "euler";
    QString order = "ZYX";
    bool intrinsic = true;
    bool radians = false;
    // Columns of the input-to-world basis; ±1/±2/±3 mean ±X/±Y/±Z.
    std::array<int, 3> axes {{1, 2, 3}};
    QJsonObject settings() const;
    static ModelMapping restore(const QJsonObject &value);
};

struct ModelPose
{
    QMatrix4x4 matrix;
    QVector3D position;
    QString error;
    bool valid() const { return error.isEmpty(); }
};

ModelPose evaluateModelPose(const ModelMapping &mapping, const QJsonObject &values);
QStringList modelNumericFields(const QJsonObject &values);
}
