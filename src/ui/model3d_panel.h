// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "model3d_view.h"
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
    void restore(const QJsonObject &settings);
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
    void applyMapping();
    void refreshFieldNames();
    void applyModelTransform();
    QString modelPath;
    QString pendingPath;
    QString peerKey = "default";
    QStringList fields;
    QJsonObject latestValues;
    qint64 lastSample = -1;
    qint64 floorSample = -1;
    int generation = -1;
    QPointer<Channel> channel;
    bool restoring = false;
    QJsonObject pendingCamera;
    QTimer saveTimer;
    QFutureWatcher<ModelLoadResult> loader;
    QPushButton *import;
    QLabel *fileLabel;
    Combo *feed;
    Combo *peer;
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
    QDoubleSpinBox *scale;
    QCheckBox *centered;
};
}
