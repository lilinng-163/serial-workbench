// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QSize>
#include <QStringList>
#include <QTimer>
#include <memory>

namespace sw
{
class SshProcess : public QObject
{
    Q_OBJECT
public:
    explicit SshProcess(QObject *parent = nullptr);
    ~SshProcess() override;
    bool start(const QString &program, const QStringList &arguments, const QSize &size);
    qint64 write(const QByteArray &data);
    void resize(const QSize &size);
    void stop();
    QString errorString() const;

signals:
    void output(const QByteArray &data);
    void failed(const QString &message);
    void finished(int code);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    QTimer poll;
};
}
