// SPDX-License-Identifier: Apache-2.0

#include "frame_script.h"
#include <atomic>

namespace sw
{
struct FrameScriptProcessor::WorkerState
{
    std::atomic_int requested {0};
    int failedGeneration = -1;
    std::shared_ptr<const FrameScriptProgram> program;
    QMap<QString, std::shared_ptr<FrameScriptRuntime>> runtimes;
};

FrameScriptProcessor::FrameScriptProcessor(QObject *parent)
    : QObject(parent), state(std::make_shared<WorkerState>()), worker(new QObject())
{
    worker->moveToThread(&thread);
    connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
    thread.setObjectName("FrameScriptWorker");
    thread.start();
}

FrameScriptProcessor::~FrameScriptProcessor()
{
    ++state->requested;
    thread.quit();
    thread.wait();
}

void FrameScriptProcessor::configure(std::shared_ptr<const FrameScriptProgram> program)
{
    const int generation = ++state->requested;
    QMetaObject::invokeMethod(worker, [state = state, generation, program = std::move(program)]
    {
        if (state->requested != generation) { return; }
        state->program = program;
        state->failedGeneration = -1;
        state->runtimes.clear();
    }, Qt::QueuedConnection);
}

void FrameScriptProcessor::submit(const QByteArray &data, const QString &peer, qint64 eventSequence)
{
    if (data.size() > 256 * 1024 - pendingBytes || pendingJobs >= 128)
    {
        emit failed("脚本处理队列已满，解析已停用；原始收发数据继续记录");
        return;
    }
    const int generation = state->requested;
    pendingBytes += data.size();
    ++pendingJobs;
    QMetaObject::invokeMethod(worker, [this, state = state, generation, data, peer, eventSequence]
    {
        QJsonArray samples;
        QString error;
        int buffered = 0;
        if (state->requested == generation && state->program && state->failedGeneration != generation)
        {
            try
            {
                if (!state->runtimes.contains(peer))
                {
                    if (state->runtimes.size() >= 32) { state->runtimes.erase(state->runtimes.begin()); }
                    state->runtimes[peer] = std::make_shared<FrameScriptRuntime>(state->program);
                }
                samples = state->runtimes[peer]->feed(data);
                buffered = state->runtimes[peer]->bufferedBytes();
            }
            catch (const ScriptError &failure)
            {
                error = failure.display();
                state->failedGeneration = generation;
            }
            catch (const std::exception &failure)
            {
                error = QString::fromUtf8(failure.what());
                state->failedGeneration = generation;
            }
        }
        QMetaObject::invokeMethod(this, [this, state, generation, count = data.size(), peer, samples, error, buffered, eventSequence]
        {
            pendingBytes -= count;
            --pendingJobs;
            if (state->requested != generation) { return; }
            if (!error.isEmpty()) { emit failed(error); }
            else
            {
                if (!samples.isEmpty()) { emit samplesReady(peer, samples, eventSequence); }
                emit batchFinished(buffered);
            }
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
}
