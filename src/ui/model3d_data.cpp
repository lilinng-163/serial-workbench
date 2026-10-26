// SPDX-License-Identifier: Apache-2.0
#include "model3d_data.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QQuaternion>
#include <QRegularExpression>
#include <QTextStream>
#include <QSet>
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <functional>

namespace sw
{
namespace
{
constexpr int MaxTriangles = 500000;
constexpr int MaxVertices = 1000000;
constexpr qint64 MaxBytes = 64 * 1024 * 1024;

[[noreturn]] void invalid(const QString &reason)
{
    throw std::runtime_error(reason.toUtf8().constData());
}

void checkPoint(const QVector3D &point)
{
    for (int axis = 0; axis < 3; ++axis)
    {
        if (!std::isfinite(point[axis]) || std::abs(point[axis]) > 1e9)
        {
            invalid("模型坐标无效或超过 ±1e9");
        }
    }
}

void triangle(ModelMesh &mesh, const QVector3D &a, const QVector3D &b, const QVector3D &c)
{
    checkPoint(a);
    checkPoint(b);
    checkPoint(c);
    auto normal = QVector3D::crossProduct(b - a, c - a);
    if (normal.lengthSquared() == 0)
    {
        return;
    }
    if (mesh.vertices.size() / 3 >= MaxTriangles)
    {
        invalid("模型超过 500,000 个三角面，请先简化模型");
    }
    normal.normalize();
    for (const auto &point : {a, b, c})
    {
        if (mesh.vertices.isEmpty())
        {
            mesh.minimum = point;
            mesh.maximum = point;
        }
        for (int axis = 0; axis < 3; ++axis)
        {
            mesh.minimum[axis] = qMin(mesh.minimum[axis], point[axis]);
            mesh.maximum[axis] = qMax(mesh.maximum[axis], point[axis]);
        }
        mesh.vertices.append({point, normal});
    }
}

// Ear clipping handles concave OBJ/PLY polygons as well as ordinary CAD quads.
void polygon(ModelMesh &mesh, const QList<QVector3D> &vertices, const QList<int> &indices)
{
    if (indices.size() < 3 || indices.size() > 256)
    {
        invalid("面顶点数必须在 3 到 256 之间");
    }
    for (int index : indices)
    {
        if (index < 0 || index >= vertices.size())
        {
            invalid("模型面引用了不存在的顶点");
        }
    }
    if (indices.size() == 3)
    {
        triangle(mesh, vertices[indices[0]], vertices[indices[1]], vertices[indices[2]]);
        return;
    }
    QVector3D normal;
    for (int i = 0; i < indices.size(); ++i)
    {
        const auto a = vertices[indices[i]];
        const auto b = vertices[indices[(i + 1) % indices.size()]];
        normal += QVector3D::crossProduct(a - vertices[indices[0]], b - vertices[indices[0]]);
    }
    int drop = 0;
    for (int i = 1; i < 3; ++i)
    {
        if (std::abs(normal[i]) > std::abs(normal[drop]))
        {
            drop = i;
        }
    }
    const int x = (drop + 1) % 3;
    const int y = (drop + 2) % 3;
    const auto cross = [&](int a, int b, int c)
    {
        return double(vertices[b][x] - vertices[a][x]) * double(vertices[c][y] - vertices[a][y])
            - double(vertices[b][y] - vertices[a][y]) * double(vertices[c][x] - vertices[a][x]);
    };
    const double sign = normal[drop] >= 0 ? 1 : -1;
    QList<int> remaining = indices;
    while (remaining.size() > 3)
    {
        bool clipped = false;
        for (int i = 0; i < remaining.size(); ++i)
        {
            const int a = remaining[(i + remaining.size() - 1) % remaining.size()];
            const int b = remaining[i];
            const int c = remaining[(i + 1) % remaining.size()];
            const double turn = sign * cross(a, b, c);
            if (turn < 0)
            {
                continue;
            }
            bool inside = false;
            if (turn > 0)
            {
                for (int p : remaining)
                {
                    if (p != a && p != b && p != c && sign * cross(a, b, p) >= 0
                        && sign * cross(b, c, p) >= 0 && sign * cross(c, a, p) >= 0)
                    {
                        inside = true;
                        break;
                    }
                }
            }
            if (!inside)
            {
                triangle(mesh, vertices[a], vertices[b], vertices[c]);
                remaining.removeAt(i);
                clipped = true;
                break;
            }
        }
        if (!clipped)
        {
            invalid("面无法三角化，请导出三角网格后重试");
        }
    }
    triangle(mesh, vertices[remaining[0]], vertices[remaining[1]], vertices[remaining[2]]);
}

double number(const QString &text)
{
    bool ok = false;
    const double result = text.toDouble(&ok);
    if (!ok || !std::isfinite(result))
    {
        invalid("模型包含无效数值");
    }
    return result;
}

float littleFloat(const char *data)
{
    const auto bits = qFromLittleEndian<quint32>(data);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

ModelMesh stl(const QByteArray &data)
{
    ModelMesh mesh;
    if (data.size() >= 84)
    {
        const auto count = qFromLittleEndian<quint32>(data.constData() + 80);
        if (84 + quint64(count) * 50 == quint64(data.size()))
        {
            if (count > MaxTriangles)
            {
                invalid("模型超过 500,000 个三角面");
            }
            for (quint32 i = 0; i < count; ++i)
            {
                const char *facet = data.constData() + 84 + i * 50 + 12;
                std::array<QVector3D, 3> points;
                for (int j = 0; j < 3; ++j)
                {
                    points[j] = {littleFloat(facet + j * 12), littleFloat(facet + j * 12 + 4), littleFloat(facet + j * 12 + 8)};
                }
                triangle(mesh, points[0], points[1], points[2]);
            }
            return mesh;
        }
    }
    QTextStream stream(data);
    const auto expect = [&](const QString &expected)
    {
        QString token;
        stream >> token;
        if (token != expected)
        {
            invalid("STL 结构无效，预期 " + expected);
        }
    };
    const auto vector = [&]()
    {
        QString x, y, z;
        stream >> x >> y >> z;
        return QVector3D(number(x), number(y), number(z));
    };
    expect("solid");
    stream.readLine();
    QString token;
    while (!stream.atEnd())
    {
        stream >> token;
        if (token == "endsolid")
        {
            stream.readLine();
            stream.skipWhiteSpace();
            if (stream.atEnd())
            {
                return mesh;
            }
            expect("solid");
            stream.readLine();
            continue;
        }
        if (token != "facet")
        {
            invalid("ASCII STL 缺少 facet / endsolid");
        }
        expect("normal");
        vector();
        expect("outer");
        expect("loop");
        std::array<QVector3D, 3> points;
        for (auto &point : points)
        {
            expect("vertex");
            point = vector();
        }
        expect("endloop");
        expect("endfacet");
        triangle(mesh, points[0], points[1], points[2]);
    }
    invalid("STL 文件不完整");
}

ModelMesh obj(const QByteArray &data)
{
    ModelMesh mesh;
    QList<QVector3D> vertices;
    QTextStream stream(data);
    while (!stream.atEnd())
    {
        const auto line = stream.readLine(65537);
        if (line.size() > 65536)
        {
            invalid("OBJ 行过长");
        }
        const auto words = line.section('#', 0, 0).split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (words.isEmpty())
        {
            continue;
        }
        if (words[0] == "v")
        {
            if (words.size() < 4 || vertices.size() >= MaxVertices)
            {
                invalid("OBJ 顶点数据无效或超过 1,000,000 点");
            }
            QVector3D point(number(words[1]), number(words[2]), number(words[3]));
            // A fourth homogeneous coordinate is distinct from a vertex RGB extension.
            if (words.size() == 5)
            {
                const double weight = number(words[4]);
                if (weight == 0)
                {
                    invalid("OBJ 齐次坐标 w 不能为零");
                }
                point /= weight;
            }
            checkPoint(point);
            vertices.append(point);
        }
        else if (words[0] == "f")
        {
            QList<int> indices;
            for (int i = 1; i < words.size(); ++i)
            {
                bool ok = false;
                const int index = words[i].section('/', 0, 0).toInt(&ok);
                if (!ok || index == 0)
                {
                    invalid("OBJ 面索引无效");
                }
                indices.append(index < 0 ? vertices.size() + index : index - 1);
            }
            polygon(mesh, vertices, indices);
        }
        // Materials, texture references and other objects are never opened.
    }
    return mesh;
}

struct PlyProperty
{
    QString name;
    QString type;
    QString countType;
};

struct PlyElement
{
    QString name;
    int count = 0;
    QList<PlyProperty> properties;
};

int typeSize(const QString &type)
{
    if (QStringList {"char", "uchar", "int8", "uint8"}.contains(type)) { return 1; }
    if (QStringList {"short", "ushort", "int16", "uint16"}.contains(type)) { return 2; }
    if (QStringList {"int", "uint", "int32", "uint32", "float", "float32"}.contains(type)) { return 4; }
    if (QStringList {"double", "float64"}.contains(type)) { return 8; }
    invalid("不支持的 PLY 属性类型：" + type);
}

ModelMesh ply(const QByteArray &data)
{
    QList<PlyElement> elements;
    qsizetype offset = 0;
    QString format;
    bool ended = false;
    while (offset < data.size() && offset < 65536)
    {
        const auto end = data.indexOf('\n', offset);
        if (end < 0) { break; }
        const auto words = QString::fromLatin1(data.mid(offset, end - offset)).simplified().split(' ');
        offset = end + 1;
        if (words[0] == "format" && words.size() == 3 && words[2] == "1.0") { format = words[1]; }
        else if (words[0] == "element" && words.size() == 3)
        {
            bool ok = false;
            const int count = words[2].toInt(&ok);
            if (!ok || count < 0 || count > MaxVertices || elements.size() >= 64)
            {
                invalid("PLY 元素数量无效");
            }
            elements.append({words[1], count, {}});
        }
        else if (words[0] == "property")
        {
            if (elements.isEmpty() || elements.last().properties.size() >= 64)
            {
                invalid("PLY 属性数量或顺序无效");
            }
            PlyProperty property;
            if (words.size() == 3) { property = {words[2], words[1], {}}; }
            else if (words.size() == 5 && words[1] == "list") { property = {words[4], words[3], words[2]}; }
            else { invalid("PLY 属性声明无效"); }
            typeSize(property.type);
            if (!property.countType.isEmpty()) { typeSize(property.countType); }
            elements.last().properties.append(property);
        }
        else if (words[0] == "end_header") { ended = true; break; }
    }
    if (!data.startsWith("ply") || !ended
        || !QStringList {"ascii", "binary_little_endian", "binary_big_endian"}.contains(format))
    {
        invalid("PLY 头部无效或格式不支持");
    }
    QTextStream text(data.mid(offset));
    const auto scalar = [&](const QString &type) -> double
    {
        if (format == "ascii")
        {
            QString token;
            text >> token;
            return number(token);
        }
        const int size = typeSize(type);
        if (offset + size > data.size()) { invalid("PLY 二进制数据不完整"); }
        quint64 bits = 0;
        for (int i = 0; i < size; ++i)
        {
            const int shift = format == "binary_little_endian" ? i : size - i - 1;
            bits |= quint64(quint8(data[offset + i])) << (shift * 8);
        }
        offset += size;
        if (type == "float" || type == "float32")
        {
            const quint32 raw = bits;
            float value;
            std::memcpy(&value, &raw, 4);
            return value;
        }
        if (size == 8)
        {
            double value;
            std::memcpy(&value, &bits, 8);
            return value;
        }
        if (type == "char" || type == "int8") { return qint8(bits); }
        if (type == "short" || type == "int16") { return qint16(bits); }
        if (type == "int" || type == "int32") { return qint32(bits); }
        return bits;
    };
    QList<QVector3D> vertices;
    QList<QList<int>> faces;
    int triangles = 0;
    for (const auto &element : elements)
    {
        for (int row = 0; row < element.count; ++row)
        {
            QVector3D point;
            int coordinates = 0;
            QList<int> indices;
            for (const auto &property : element.properties)
            {
                if (!property.countType.isEmpty())
                {
                    const double count = scalar(property.countType);
                    if (!std::isfinite(count) || count < 0 || count > 256 || count != std::floor(count))
                    {
                        invalid("PLY 列表长度无效或超过 256");
                    }
                    for (int i = 0; i < int(count); ++i)
                    {
                        const double index = scalar(property.type);
                        if (element.name == "face" && (property.name == "vertex_indices" || property.name == "vertex_index"))
                        {
                            if (!std::isfinite(index) || index < 0 || index >= MaxVertices || index != std::floor(index))
                            {
                                invalid("PLY 面索引无效");
                            }
                            indices.append(int(index));
                        }
                    }
                }
                else
                {
                    const double value = scalar(property.type);
                    if (element.name == "vertex")
                    {
                        const int axis = QStringList {"x", "y", "z"}.indexOf(property.name);
                        if (axis >= 0) { point[axis] = value; coordinates |= 1 << axis; }
                    }
                }
            }
            if (element.name == "vertex")
            {
                if (coordinates != 7 || vertices.size() >= MaxVertices) { invalid("PLY 缺少 x/y/z 顶点坐标"); }
                checkPoint(point);
                vertices.append(point);
            }
            if (element.name == "face")
            {
                triangles += qMax(0, int(indices.size()) - 2);
                if (indices.size() < 3 || triangles > MaxTriangles) { invalid("PLY 面数据无效或超过 500,000 个三角面"); }
                faces.append(indices);
            }
        }
    }
    ModelMesh mesh;
    for (const auto &indices : faces) { polygon(mesh, vertices, indices); }
    return mesh;
}

QJsonValue fieldValue(const QJsonObject &values, const QString &path)
{
    if (values.contains(path)) { return values[path]; }
    QJsonValue current(values);
    for (const auto &part : path.split('.'))
    {
        current = current.toObject().value(part);
    }
    return current;
}

double boundValue(const ModelBinding &binding, const QJsonObject &values, double fallback)
{
    double value = fallback;
    if (!binding.field.isEmpty())
    {
        const auto input = fieldValue(values, binding.field);
        if (!input.isDouble()) { invalid("缺少数值通道：" + binding.field); }
        value = input.toDouble();
    }
    value = value * binding.gain + binding.offset;
    if (!std::isfinite(value)) { invalid("通道换算后不是有限数值：" + binding.field); }
    return value;
}
}

QVector3D ModelMesh::center() const { return (minimum + maximum) * 0.5f; }
float ModelMesh::radius() const { return qMax((maximum - minimum).length() * 0.5f, 0.0001f); }

ModelMesh loadModelMesh(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { invalid("无法打开模型：" + file.errorString()); }
    if (file.size() > MaxBytes || file.size() <= 0) { invalid("模型为空或超过 64 MiB"); }
    const auto data = file.read(MaxBytes + 1);
    if (data.size() > MaxBytes || file.error() != QFileDevice::NoError) { invalid("读取模型失败或超过 64 MiB"); }
    const auto extension = QFileInfo(path).suffix().toLower();
    ModelMesh mesh;
    if (extension == "stl") { mesh = stl(data); }
    else if (extension == "obj") { mesh = obj(data); }
    else if (extension == "ply") { mesh = ply(data); }
    else { invalid("请选择 STL、OBJ 或 PLY 三角网格文件"); }
    if (mesh.vertices.isEmpty()) { invalid("模型中没有可显示的三角面"); }
    return mesh;
}

ModelMesh exampleModelMesh()
{
    // An asymmetric arrow-shaped body makes forward, roll and yaw unambiguous.
    const QList<QVector3D> points {{-1, -.5f, -.25f}, {1, -.5f, -.25f}, {1.65f, 0, 0},
        {1, .5f, -.25f}, {-1, .5f, -.25f}, {-1, -.5f, .25f}, {1, -.5f, .25f},
        {1, .5f, .25f}, {-1, .5f, .25f}};
    ModelMesh mesh;
    for (const QList<int> &face : {QList<int>{0, 4, 3, 1}, {5, 6, 7, 8}, {0, 1, 6, 5},
        {4, 8, 7, 3}, {0, 5, 8, 4}, {1, 3, 2}, {6, 2, 7}, {1, 2, 6}, {3, 7, 2}})
    {
        polygon(mesh, points, face);
    }
    return mesh;
}

QJsonObject ModelBinding::settings() const
{
    return {{"field", field}, {"gain", gain}, {"offset", offset}};
}

ModelBinding ModelBinding::restore(const QJsonObject &value)
{
    return {value["field"].toString().left(256), value["gain"].toDouble(1), value["offset"].toDouble()};
}

QJsonObject ModelMapping::settings() const
{
    QJsonObject result {{"mode", mode}, {"order", order}, {"intrinsic", intrinsic}, {"radians", radians}};
    const auto bindings = [](const auto &values)
    {
        QJsonArray array;
        for (const auto &value : values) { array.append(value.settings()); }
        return array;
    };
    result["position"] = bindings(position);
    result["angles"] = bindings(angles);
    result["quaternion"] = bindings(quaternion);
    result["axes"] = QJsonArray {axes[0], axes[1], axes[2]};
    return result;
}

ModelMapping ModelMapping::restore(const QJsonObject &value)
{
    ModelMapping result;
    result.mode = value["mode"] == "quaternion" ? "quaternion" : "euler";
    const auto order = value["order"].toString();
    if (QStringList {"XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX"}.contains(order)) { result.order = order; }
    result.intrinsic = value["intrinsic"].toBool(true);
    result.radians = value["radians"].toBool(false);
    const auto restoreBindings = [&](const QString &key, auto &destination)
    {
        const auto array = value[key].toArray();
        for (int i = 0; i < int(destination.size()) && i < array.size(); ++i)
        {
            destination[i] = ModelBinding::restore(array[i].toObject());
        }
    };
    restoreBindings("position", result.position);
    restoreBindings("angles", result.angles);
    restoreBindings("quaternion", result.quaternion);
    const auto axes = value["axes"].toArray();
    if (axes.size() == 3)
    {
        for (int i = 0; i < 3; ++i) { result.axes[i] = axes[i].toInt(); }
    }
    return result;
}

ModelPose evaluateModelPose(const ModelMapping &mapping, const QJsonObject &values)
{
    ModelPose result;
    try
    {
        QMatrix4x4 basis;
        basis.fill(0);
        basis(3, 3) = 1;
        QSet<int> used;
        for (int i = 0; i < 3; ++i)
        {
            if (mapping.axes[i] < -3 || mapping.axes[i] > 3 || mapping.axes[i] == 0)
            {
                invalid("坐标轴映射必须为 ±X / ±Y / ±Z");
            }
            const int axis = std::abs(mapping.axes[i]);
            if (axis < 1 || axis > 3 || used.contains(axis)) { invalid("X/Y/Z 必须分别映射到三个不同的轴"); }
            used.insert(axis);
            basis(axis - 1, i) = mapping.axes[i] > 0 ? 1 : -1;
        }
        QVector3D position;
        for (int i = 0; i < 3; ++i)
        {
            const double value = boundValue(mapping.position[i], values, 0);
            if (std::abs(value) > 1e9) { invalid("位置超过 ±1e9，请调整倍率"); }
            position[i] = value;
        }
        QQuaternion orientation;
        if (mapping.mode == "quaternion")
        {
            std::array<double, 4> q;
            double largest = 0;
            for (int i = 0; i < 4; ++i)
            {
                q[i] = boundValue(mapping.quaternion[i], values, i == 0 ? 1 : 0);
                largest = qMax(largest, std::abs(q[i]));
            }
            if (largest < 1e-12) { invalid("四元数长度为零，保留上一帧"); }
            double length = 0;
            for (double &component : q) { component /= largest; length += component * component; }
            length = std::sqrt(length);
            orientation = QQuaternion(q[0] / length, q[1] / length, q[2] / length, q[3] / length);
        }
        else
        {
            std::array<double, 3> angles;
            for (int i = 0; i < 3; ++i)
            {
                const double value = boundValue(mapping.angles[i], values, 0);
                angles[i] = mapping.radians ? std::remainder(value, 2 * M_PI) * 180 / M_PI : std::remainder(value, 360);
            }
            if (!QStringList {"XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX"}.contains(mapping.order)) { invalid("无效的欧拉角顺序"); }
            for (const auto &letter : mapping.order)
            {
                const int axis = QString("XYZ").indexOf(letter);
                QVector3D unit;
                unit[axis] = 1;
                const auto rotation = QQuaternion::fromAxisAndAngle(unit, angles[axis]);
                orientation = mapping.intrinsic ? orientation * rotation : rotation * orientation;
            }
        }
        QMatrix4x4 rotation;
        rotation.rotate(orientation);
        result.position = basis.mapVector(position);
        result.matrix.translate(result.position);
        result.matrix *= basis * rotation * basis.transposed();
    }
    catch (const std::exception &error)
    {
        result.error = QString::fromUtf8(error.what());
    }
    return result;
}

QStringList modelNumericFields(const QJsonObject &values)
{
    QStringList result;
    std::function<void(const QJsonObject &, const QString &, int)> visit;
    visit = [&](const QJsonObject &object, const QString &prefix, int depth)
    {
        if (depth > 8) { return; }
        for (auto it = object.begin(); it != object.end() && result.size() < 256; ++it)
        {
            const auto path = prefix + it.key();
            if (it->isDouble()) { result.append(path); }
            else if (it->isObject()) { visit(it->toObject(), path + ".", depth + 1); }
        }
    };
    visit(values, {}, 0);
    return result;
}
}
