// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core.h"
#include "serial_transfer.h"
#include <QtWidgets>

namespace sw
{
class FileTransferDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FileTransferDialog(Channel *channel, QWidget *parent = nullptr);
    ~FileTransferDialog() override;
    // Also usable by the separately built transport demo, without a real port.
    FileTransferDialog(SerialTransfer *transfer, QWidget *parent = nullptr);
    SerialTransfer *transfer;
    QComboBox *direction;
    QComboBox *protocol;
    QLineEdit *path;
    QLineEdit *expectedSize;
    QProgressBar *progress;
    QLabel *summary;
    QLabel *status;
    QPushButton *start;
    QPushButton *save;
    QPushButton *stop;
    bool begin();
    void refresh();
    void reject() override;
    void closeEvent(QCloseEvent *event) override;

signals:
    void starting();

private:
    QPointer<Channel> channel;
    QElapsedTimer elapsed;
    bool succeeded = false;
    QPushButton *browse;
    QLabel *hint;
    QFormLayout *form;
    void releaseChannel();
};
}
