// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "model3d_view.h"
#include "model3d_tabs.h"
#include <QFutureWatcher>

namespace sw
{
struct ModelLoadResult
{
    ModelMesh mesh;
    QString error;
};

class ModelPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ModelPanel(QWidget *parent = nullptr);
    void tick(Channel *channel);
    void resetSource(Channel *channel);
    void clearDisplay();
    void setTheme(const Theme &theme);
    void loadFile(const QString &path);
    QJsonObject settings() const;
    void restore(const QJsonObject &settings, bool reload = true);
    QJsonObject state() const;
    void configure(const QJsonObject &patch);
    QJsonObject exportConfiguration() const;
    void importConfiguration(const QJsonObject &document);
    bool loading() const { return importPending; }
    ModelView *view;
    QLabel *status;
    ModelMapping mapping() const;

signals:
    void changed();
    void modelLoaded(bool success);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    struct BindingRow
    {
        Combo *field;
        QDoubleSpinBox *gain;
        QDoubleSpinBox *offset;
    };
    BindingRow bindingRow(QGridLayout *layout, int row, const QString &label, const QString &field, const QString &id);
    void scheduleSave();
    void applyMapping(const QStringList &incomingErrors = {});
    void refreshFieldNames(const QJsonObject &values);
    void applyModelTransform();
    void syncCameraFields();
    QString modelPath;
    QString pendingPath;
    QString peerKey = "default";
    QString streamKey;
    QStringList fields;
    QJsonObject latestValues;
    QJsonObject positionValues;
    QJsonObject rotationValues;
    QVector3D telemetryPosition;
    QMatrix4x4 telemetryRotation;
    QColor customColor = QColor("#de8c32");
    qint64 lastSample = -1;
    qint64 positionSample = -1;
    qint64 rotationSample = -1;
    QString loadError;
    QStringList mappingErrors;
    qint64 floorSample = -1;
    int generation = -1;
    QPointer<Channel> channel;
    bool restoring = false;
    bool importPending = false;
    QJsonObject pendingCamera;
    QTimer saveTimer;
    QTimer cameraFieldsTimer;
    QFutureWatcher<ModelLoadResult> loader;
    QPushButton *import;
    QLabel *fileLabel;
    Combo *feed;
    Combo *peer;
    Combo *stream;
    QCheckBox *pause;
    Combo *mode;
    Combo *order;
    Combo *convention;
    Combo *units;
    QStackedWidget *rotationPages;
    std::array<BindingRow, 3> position;
    std::array<BindingRow, 3> angles;
    std::array<BindingRow, 4> quaternion;
    std::array<Combo *, 3> axes;
    std::array<QDoubleSpinBox *, 3> alignment;
    std::array<QDoubleSpinBox *, 3> manualPosition;
    QDoubleSpinBox *scale;
    QCheckBox *centered;
    QCheckBox *followTheme;
    QLineEdit *colorHex;
    ModelTabs *mappingTabs;
    Combo *antialiasing;
    QLabel *rotationConvention;
    std::array<QDoubleSpinBox *, 6> cameraFields;
};
}
