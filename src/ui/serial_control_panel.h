// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "controls.h"
#include <memory>
#include <vector>

namespace sw
{
class SerialMonitorDialog;
// The same formatter powers the editor preview and actual writes.
QByteArray controlPacket(const QJsonObject &spec);
QJsonObject defaultControl(const QString &type);
QJsonArray controlGroupPresets();

class SerialControlPanel : public QWidget
{
    Q_OBJECT
public:
    SerialMonitorDialog *receiveMonitor() const;
    explicit SerialControlPanel(QWidget *parent = nullptr);
    ~SerialControlPanel() override;
    void setChannel(Channel *channel);
    QJsonObject settings() const;
    void restore(const QJsonObject &settings);
    void addControl(const QJsonObject &spec);
    void addExamples();
    bool addPresetGroup(const QString &presetId);
    void clearControls(bool allGroups = false);
    QString addGroup(const QString &name);
    bool selectGroup(const QString &id);
    void setTheme(const Theme &theme);
    void setDataFont(const QFont &font);
    QLabel *status = nullptr;

signals:
    void changed();
    void zoomRequested(int steps);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    struct Row;
    std::vector<std::unique_ptr<Row>> rows;
    QGridLayout *cards = nullptr;
    QScrollArea *scroll = nullptr;
    QComboBox *groupPicker = nullptr;
    QComboBox *columns = nullptr;
    QBoxLayout *toolbarTools = nullptr;
    QPushButton *monitorToggle = nullptr;
    QPointer<SerialMonitorDialog> monitor;
    QJsonObject monitorSettings;
    QFont monitorFont;
    QJsonArray groups;
    QString activeGroup;
    int layoutColumns = -1;
    bool canvasInitialized = false;
    int canvasReferenceWidth = 0;
    Theme appearance;
    QLabel *empty = nullptr;
    QTimer sendTimer;
    QTimer saveTimer;
    QTimer arrangeTimer;
    QPointer<Channel> target;
    int generation = -1;
    void cancelPending();
    void rebuild();
    void arrange();
    void rememberGeometry();
    void updateCanvasExtent();
    void initializeCanvas();
    void clearCards();
    void manageGroup(const QString &action);
    void reorder(int index, int destination);
    void flushPending();
    void updatePreview(Row *row);
    void valueChanged(Row *row, const QString &value);
    void send(Row *row);
    void edit(int index);
    void showMonitor(bool visible);
};
}
