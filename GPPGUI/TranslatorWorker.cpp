#include "TranslatorWorker.h"
#include <QMetaType>
#include <QThread>
#include <QTimer>

import std;
import ITranslator;

namespace
{
    QString qstr(const std::string& value)
    {
        return QString::fromStdString(value);
    }

    GuiRuntimeFileProgress toGuiFileProgress(const RuntimeFileProgress& file)
    {
        return {
            qstr(file.filename),
            file.total,
            file.completed,
            file.problems
        };
    }

    GuiRuntimeSuccessEvent toGuiSuccessEvent(const RuntimeSuccessEvent& event)
    {
        QStringList speakers;
        for (const std::string& speaker : event.speakers) {
            speakers.push_back(qstr(speaker));
        }
        return {
            qstr(event.timestamp),
            qstr(event.filename),
            event.index,
            speakers,
            qstr(event.sourcePreview),
            qstr(event.translationPreview),
            qstr(event.translatedBy)
        };
    }

    GuiRuntimeErrorEvent toGuiErrorEvent(const RuntimeErrorEvent& event)
    {
        return {
            qstr(event.timestamp),
            qstr(event.kind),
            qstr(event.level),
            qstr(event.message),
            qstr(event.filename),
            qstr(event.indexRange),
            event.retryCount,
            qstr(event.model),
            event.sleepSeconds
        };
    }
}

class GUIController : public IController
{

public:
    virtual void writeLog(const std::string& log) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_log += log;
    }

    virtual bool shouldStop() override
    {
        return m_worker->getShouldStop();
    }

    virtual void flush() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_log.isEmpty()) {
            QStringList lines = m_log.split('\n');
            for (QString& line : lines) {
                if (line.length() > 256) {
                    line.truncate(256);
                    line += "(...line truncated)";
                }
            }
            m_log = lines.join('\n');
            Q_EMIT m_worker->writeLogSignal(m_log);
            m_log.clear();
        }
        Q_EMIT m_worker->updateBarSignal(m_progress);
        m_progress = 0;
        if (!m_pendingRuntimeFilesProgress.empty()) {
            QVector<GuiRuntimeFileProgress> runtimeFilesProgressVec;
            runtimeFilesProgressVec.reserve((qsizetype)m_pendingRuntimeFilesProgress.size());
            for (const auto& runtimeFileProgress : m_pendingRuntimeFilesProgress | std::views::values) {
                runtimeFilesProgressVec.push_back(runtimeFileProgress);
            }
            Q_EMIT m_worker->runtimeFileProgressBatchSignal(runtimeFilesProgressVec);
            m_pendingRuntimeFilesProgress.clear();
        }
        if (!m_pendingRuntimeSuccesses.empty()) {
            Q_EMIT m_worker->runtimeSuccessBatchSignal(m_pendingRuntimeSuccesses);
            m_pendingRuntimeSuccesses.clear();
        }
        if (!m_pendingRuntimeErrors.empty()) {
            Q_EMIT m_worker->runtimeErrorBatchSignal(m_pendingRuntimeErrors);
            m_pendingRuntimeErrors.clear();
        }
    }

protected:
    virtual void onMakeBar(int totalSentences, int totalThreads) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Q_EMIT m_worker->makeBarSignal(totalSentences, totalThreads);
    }

    virtual void onAddThreadNum(int) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Q_EMIT m_worker->addThreadNumSignal();
    }

    virtual void onReduceThreadNum(int) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Q_EMIT m_worker->reduceThreadNumSignal();
    }

    virtual void onUpdateBar(int ticks, int, int) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_progress += ticks;
    }

    virtual void onRuntimeFilesReset(const std::vector<RuntimeFileProgress>& files) override
    {
        QVector<GuiRuntimeFileProgress> guiFiles;
        guiFiles.reserve((qsizetype)files.size());
        for (const RuntimeFileProgress& file : files) {
            guiFiles.push_back(toGuiFileProgress(file));
        }
        Q_EMIT m_worker->runtimeFilesResetSignal(guiFiles);
    }

    virtual void onRuntimeStageChanged(const std::string& stage, const std::string& currentFile) override
    {
        Q_EMIT m_worker->runtimeStageChangedSignal(qstr(stage), qstr(currentFile));
    }

    virtual void onRuntimeFileProgress(const RuntimeFileProgress& file) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        GuiRuntimeFileProgress guiFile = toGuiFileProgress(file);
        m_pendingRuntimeFilesProgress[guiFile.filename.toStdString()] = std::move(guiFile);
    }

    virtual void onRuntimeSuccess(const RuntimeSuccessEvent& event) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRuntimeSuccesses.push_back(toGuiSuccessEvent(event));
    }

    virtual void onRuntimeError(const RuntimeErrorEvent& event) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRuntimeErrors.push_back(toGuiErrorEvent(event));
    }

public:
    explicit GUIController(TranslatorWorker* worker)
	    : m_worker(worker)
    {
        m_log.reserve(1024 * 1024);
        m_flushThread = std::thread([this]()
            {
                while (m_controlling) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    flush();
                }
            });
    }

    virtual ~GUIController() override
    {
        m_controlling = false;
        if (m_flushThread.joinable()) {
            m_flushThread.join();
        }
    }

private:
    std::mutex m_mutex;
    QString m_log;
    std::thread m_flushThread;
    TranslatorWorker* m_worker;
    bool m_controlling = true;
    int m_progress = 0;
    std::map<std::string, GuiRuntimeFileProgress> m_pendingRuntimeFilesProgress;
    QVector<GuiRuntimeSuccessEvent> m_pendingRuntimeSuccesses;
    QVector<GuiRuntimeErrorEvent> m_pendingRuntimeErrors;
};

TranslatorWorker::TranslatorWorker(const fs::path& projectDir, QObject* parent)
    : QObject(parent), m_projectDir(projectDir)
{
    qRegisterMetaType<GuiRuntimeSuccessEvent>("GuiRuntimeSuccessEvent");
    qRegisterMetaType<GuiRuntimeErrorEvent>("GuiRuntimeErrorEvent");
    qRegisterMetaType<GuiRuntimeFileProgress>("GuiRuntimeFileProgress");
    qRegisterMetaType<QVector<GuiRuntimeFileProgress>>("QVector<GuiRuntimeFileProgress>");
    qRegisterMetaType<QVector<GuiRuntimeSuccessEvent>>("QVector<GuiRuntimeSuccessEvent>");
    qRegisterMetaType<QVector<GuiRuntimeErrorEvent>>("QVector<GuiRuntimeErrorEvent>");
}

void TranslatorWorker::doTranslation()
{
    m_shouldStop = false;

    const auto controller = std::make_shared<GUIController>(this);

    try {

        const std::unique_ptr<ITranslator> translator = createTranslator(m_projectDir, controller);
        if (!translator) {
            Q_EMIT translationFinishedSignal(-1);
            return;
        }
        translator->run();

    }
    catch (const std::system_error& e) {
        controller->flush();
        Q_EMIT writeLogSignal("[系统错误] " + QString::fromStdString(e.what()));
        Q_EMIT translationFinishedSignal(-2);
        return;
    }
    catch (const std::invalid_argument& e) {
        controller->flush();
        Q_EMIT writeLogSignal("[参数错误] " + QString::fromStdString(e.what()));
        Q_EMIT translationFinishedSignal(-2);
        return;
    }
    catch (const std::runtime_error& e) {
        controller->flush();
        Q_EMIT writeLogSignal("[运行时错误] " + QString::fromStdString(e.what()));
        Q_EMIT translationFinishedSignal(-2);
        return;
    }
    catch (const std::exception& e) {
        controller->flush();
        Q_EMIT writeLogSignal("[标准错误] " + QString::fromStdString(e.what()));
        Q_EMIT translationFinishedSignal(-2);
        return;
    }
    catch (...) {
        controller->flush();
        Q_EMIT writeLogSignal("[未知错误]");
        Q_EMIT translationFinishedSignal(-2);
        return;
    }

    controller->writeLog(std::string("翻译任务") + (m_shouldStop ? "已停止" : "已完成") + "。");
    controller->flush();
    Q_EMIT translationFinishedSignal(m_shouldStop ? 1 : 0);
}

void TranslatorWorker::stopTranslation()
{
    m_shouldStop = true;
}
