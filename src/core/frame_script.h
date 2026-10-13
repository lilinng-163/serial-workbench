// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QtCore>
#include <memory>
#include <stdexcept>

namespace sw
{
class ScriptError : public std::runtime_error
{
public:
    ScriptError(int line, const QString &message);
    int line;
    QString detail;
    QString display() const;
};

class FrameScriptProgram
{
public:
    struct Impl;
    static std::shared_ptr<const FrameScriptProgram> compile(const QString &code);
    static QString example();

private:
    explicit FrameScriptProgram(std::shared_ptr<const Impl> syntax);
    std::shared_ptr<const Impl> syntax;
    friend class FrameScriptRuntime;
};

class FrameScriptRuntime
{
public:
    explicit FrameScriptRuntime(std::shared_ptr<const FrameScriptProgram> program);
    ~FrameScriptRuntime();
    QJsonArray feed(const QByteArray &data);
    int bufferedBytes() const;

private:
    struct Impl;
    std::unique_ptr<Impl> state;
};

// All runtime state belongs to the worker thread. Results are delivered to
// the owner thread, with stale generations discarded after reset or disable.
class FrameScriptProcessor : public QObject
{
    Q_OBJECT
public:
    explicit FrameScriptProcessor(QObject *parent = nullptr);
    ~FrameScriptProcessor() override;
    void configure(std::shared_ptr<const FrameScriptProgram> program);
    void submit(const QByteArray &data, const QString &peer = {}, qint64 eventSequence = 0);

signals:
    void samplesReady(const QString &peer, const QJsonArray &samples, qint64 eventSequence);
    void batchFinished(int bufferedBytes);
    void failed(const QString &message);

private:
    struct WorkerState;
    std::shared_ptr<WorkerState> state;
    QThread thread;
    QObject *worker;
    qsizetype pendingBytes = 0;
    int pendingJobs = 0;
};
}
