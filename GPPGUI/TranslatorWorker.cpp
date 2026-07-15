#include "TranslatorWorker.h"
#include <QMetaType>
#include <QThread>
#include <QTimer>

import ITranslator;
import Tool;


class GUIController : public IController
{
public:
	void writeLog(const std::string& log) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        //m_log += log;
        const std::string_view clipped = truncateUtf8PrefixView(log, 8192);
        m_log += clipped;
        if (clipped.size() < log.size()) {
            m_log += "(...GUI Content Truncated)\n";
        }
    }

	bool shouldStop() override
    {
        return m_worker->getShouldStop();
    }

	void flush() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_log.isEmpty()) {
            QStringList lines = m_log.split('\n');
            for (QString& line : lines) {
                if (line.length() > 256) {
                    line.truncate(256);
                    line += "(...GUI Line Truncated)";
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
            Q_EMIT m_worker->runtimeTransSuccessBatchSignal(m_pendingRuntimeSuccesses);
            m_pendingRuntimeSuccesses.clear();
        }
        if (!m_pendingRuntimeErrors.empty()) {
            Q_EMIT m_worker->runtimeTransErrorBatchSignal(m_pendingRuntimeErrors);
            m_pendingRuntimeErrors.clear();
        }
    }

protected:
	void onMakeBar(int totalSentences, int totalThreads) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Q_EMIT m_worker->makeBarSignal(totalSentences, totalThreads);
    }

	void onAddThreadNum(int) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Q_EMIT m_worker->addThreadNumSignal();
    }

	void onReduceThreadNum(int) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Q_EMIT m_worker->reduceThreadNumSignal();
    }

	void onUpdateBar(int ticks, int, int) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_progress += ticks;
    }

	void onRuntimeFilesReset(const std::vector<RuntimeFileProgress>& files) override
    {
        QVector<GuiRuntimeFileProgress> guiFiles;
        guiFiles.reserve((qsizetype)files.size());
        for (const RuntimeFileProgress& file : files) {
            guiFiles.push_back({
                QString::fromStdString(file.filename),
                file.total,
                file.completed,
                file.problems
            });
        }
        Q_EMIT m_worker->runtimeFilesResetSignal(guiFiles);
    }

	void onRuntimeStageChanged(const std::string& stage, const std::string& currentFile) override
    {
        Q_EMIT m_worker->runtimeStageChangedSignal(QString::fromStdString(stage), QString::fromStdString(currentFile));
    }

	void onRuntimeFileProgress(const RuntimeFileProgress& file) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        GuiRuntimeFileProgress guiFile{
            QString::fromStdString(file.filename),
            file.total,
            file.completed,
            file.problems
        };
        m_pendingRuntimeFilesProgress[guiFile.filename.toStdString()] = std::move(guiFile);
    }

	void onRuntimeTransSuccess(const RuntimeTransSuccessEvent& event) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        QStringList speakers;
        for (const std::string& speaker : event.speakers) {
            speakers.push_back(QString::fromStdString(speaker));
        }
        m_pendingRuntimeSuccesses.push_back({
            QString::fromStdString(event.timestamp),
            QString::fromStdString(event.filename),
            event.index,
            speakers,
            QString::fromStdString(event.sourcePreview),
            QString::fromStdString(event.translationPreview),
            QString::fromStdString(event.transby)
        });
    }

	void onRuntimeTransError(const RuntimeTransErrorEvent& event) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRuntimeErrors.push_back({
            QString::fromStdString(event.timestamp),
            QString::fromStdString(event.kind),
            QString::fromStdString(event.level),
            QString::fromStdString(event.message),
            QString::fromStdString(event.filename),
            QString::fromStdString(event.indexRange),
            event.requestCount,
            QString::fromStdString(event.model),
            event.sleepSeconds
        });
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

	~GUIController() override
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
    QVector<GuiRuntimeTransSuccessEvent> m_pendingRuntimeSuccesses;
    QVector<GuiRuntimeTransErrorEvent> m_pendingRuntimeErrors;
};

TranslatorWorker::TranslatorWorker(const fs::path& projectDir, QObject* parent)
    : QObject(parent), m_projectDir(projectDir)
{
    qRegisterMetaType<GuiRuntimeTransSuccessEvent>("GuiRuntimeTransSuccessEvent");
    qRegisterMetaType<GuiRuntimeTransErrorEvent>("GuiRuntimeTransErrorEvent");
    qRegisterMetaType<GuiRuntimeFileProgress>("GuiRuntimeFileProgress");
    qRegisterMetaType<QVector<GuiRuntimeFileProgress>>("QVector<GuiRuntimeFileProgress>");
    qRegisterMetaType<QVector<GuiRuntimeTransSuccessEvent>>("QVector<GuiRuntimeTransSuccessEvent>");
    qRegisterMetaType<QVector<GuiRuntimeTransErrorEvent>>("QVector<GuiRuntimeTransErrorEvent>");
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
        Q_EMIT writeLogSignal(tr("[GalTransl++ 系统错误] %1").arg(QString::fromStdString(e.what())));
        Q_EMIT translationFinishedSignal(-2);
        return;
    }
    catch (const std::invalid_argument& e) {
        controller->flush();
        Q_EMIT writeLogSignal(tr("[GalTransl++ 参数错误] %1").arg(QString::fromStdString(e.what())));
        Q_EMIT translationFinishedSignal(-2);
        return;
    }
    catch (const std::runtime_error& e) {
        controller->flush();
        Q_EMIT writeLogSignal(tr("[GalTransl++ 运行时错误] %1").arg(QString::fromStdString(e.what())));
        Q_EMIT translationFinishedSignal(-2);
        return;
    }
    catch (const std::exception& e) {
        controller->flush();
        Q_EMIT writeLogSignal(tr("[GalTransl++ 标准错误] %1").arg(QString::fromStdString(e.what())));
        Q_EMIT translationFinishedSignal(-2);
        return;
    }
    catch (...) {
        controller->flush();
        Q_EMIT writeLogSignal(tr("[GalTransl++ 未知错误]"));
        Q_EMIT translationFinishedSignal(-2);
        return;
    }

    controller->writeLog(tr("[GalTransl++ info] 翻译任务%1。").arg(m_shouldStop ? tr("已正常停止") : tr("已正常完成")).toStdString());
    controller->flush();
    Q_EMIT translationFinishedSignal(m_shouldStop ? 1 : 0);
}

void TranslatorWorker::stopTranslation()
{
    m_shouldStop = true;
}
