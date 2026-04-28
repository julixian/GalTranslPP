#include "TranslatorWorker.h"
#include <QMetaType>
#include <QThread>
#include <QTimer>

import std;
import ITranslator;

namespace {
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
            qstr(event.id),
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
            qstr(event.id),
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
        std::lock_guard<std::mutex> lock(_mutex);
        _log += log;
    }

    virtual bool shouldStop() override
    {
        return _worker->getShouldStop();
    }

    virtual void flush() override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_log.isEmpty()) {
            QStringList lines = _log.split('\n');
            for (QString& line : lines) {
                if (line.length() > 256) {
                    line.truncate(256);
                    line += "(... truncated)";
                }
            }
            _log = lines.join('\n');
            Q_EMIT _worker->writeLogSignal(_log);
            _log.clear();
        }
        Q_EMIT _worker->updateBarSignal(_progress);
        _progress = 0;
        if (!_pendingRuntimeFiles.empty()) {
            QVector<GuiRuntimeFileProgress> files;
            files.reserve((qsizetype)_pendingRuntimeFiles.size());
            for (const auto& [filename, file] : _pendingRuntimeFiles) {
                files.push_back(file);
            }
            Q_EMIT _worker->runtimeFileProgressBatchSignal(files);
            _pendingRuntimeFiles.clear();
        }
        if (!_pendingRuntimeSuccesses.empty()) {
            Q_EMIT _worker->runtimeSuccessBatchSignal(_pendingRuntimeSuccesses);
            _pendingRuntimeSuccesses.clear();
        }
        if (!_pendingRuntimeErrors.empty()) {
            Q_EMIT _worker->runtimeErrorBatchSignal(_pendingRuntimeErrors);
            _pendingRuntimeErrors.clear();
        }
    }

protected:
    virtual void onMakeBar(int totalSentences, int totalThreads) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        Q_EMIT _worker->makeBarSignal(totalSentences, totalThreads);
    }

    virtual void onAddThreadNum(int) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        Q_EMIT _worker->addThreadNumSignal();
    }

    virtual void onReduceThreadNum(int) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        Q_EMIT _worker->reduceThreadNumSignal();
    }

    virtual void onUpdateBar(int ticks, int, int) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _progress += ticks;
    }

    virtual void onRuntimeFilesReset(const std::vector<RuntimeFileProgress>& files) override
    {
        QVector<GuiRuntimeFileProgress> guiFiles;
        guiFiles.reserve((qsizetype)files.size());
        for (const RuntimeFileProgress& file : files) {
            guiFiles.push_back(toGuiFileProgress(file));
        }
        Q_EMIT _worker->runtimeFilesResetSignal(guiFiles);
    }

    virtual void onRuntimeStageChanged(const std::string& stage, const std::string& currentFile) override
    {
        Q_EMIT _worker->runtimeStageChangedSignal(qstr(stage), qstr(currentFile));
    }

    virtual void onRuntimeFileProgress(const RuntimeFileProgress& file) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        GuiRuntimeFileProgress guiFile = toGuiFileProgress(file);
        _pendingRuntimeFiles[guiFile.filename.toStdString()] = std::move(guiFile);
    }

    virtual void onRuntimeSuccess(const RuntimeSuccessEvent& event) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _pendingRuntimeSuccesses.push_back(toGuiSuccessEvent(event));
    }

    virtual void onRuntimeError(const RuntimeErrorEvent& event) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _pendingRuntimeErrors.push_back(toGuiErrorEvent(event));
    }

public:
    explicit GUIController(TranslatorWorker* worker)
	    : _worker(worker)
    {
        _log.reserve(1024 * 1024);
        _flushThread = std::thread([this]()
            {
                while (_controlling) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    flush();
                }
            });
    }

    virtual ~GUIController() override
    {
        _controlling = false;
        if (_flushThread.joinable()) {
            _flushThread.join();
        }
    }

private:
    std::mutex _mutex;
    QString _log;
    std::thread _flushThread;
    TranslatorWorker* _worker;
    bool _controlling = true;
    int _progress = 0;
    std::map<std::string, GuiRuntimeFileProgress> _pendingRuntimeFiles;
    QVector<GuiRuntimeSuccessEvent> _pendingRuntimeSuccesses;
    QVector<GuiRuntimeErrorEvent> _pendingRuntimeErrors;
};

TranslatorWorker::TranslatorWorker(const fs::path& projectDir, QObject* parent)
    : QObject(parent), _projectDir(projectDir)
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
    _shouldStop = false;

    const auto controller = std::make_shared<GUIController>(this);

    try {

        const std::unique_ptr<ITranslator> translator = createTranslator(_projectDir, controller);
        if (!translator) {
            Q_EMIT translationFinished(-1);
            return;
        }
        translator->run();

    }
    catch (const std::system_error& e) {
        controller->flush();
        Q_EMIT writeLogSignal("[系统错误] " + QString::fromStdString(e.what()));
        Q_EMIT translationFinished(-2);
        return;
    }
    catch (const std::invalid_argument& e) {
        controller->flush();
        Q_EMIT writeLogSignal("[参数错误] " + QString::fromStdString(e.what()));
        Q_EMIT translationFinished(-2);
        return;
    }
    catch (const std::runtime_error& e) {
        controller->flush();
        Q_EMIT writeLogSignal("[运行时错误] " + QString::fromStdString(e.what()));
        Q_EMIT translationFinished(-2);
        return;
    }
    catch (const std::exception& e) {
        controller->flush();
        Q_EMIT writeLogSignal("[标准错误] " + QString::fromStdString(e.what()));
        Q_EMIT translationFinished(-2);
        return;
    }
    catch (...) {
        controller->flush();
        Q_EMIT writeLogSignal("[未知错误]");
        Q_EMIT translationFinished(-2);
        return;
    }

    controller->writeLog(std::string("翻译任务") + (_shouldStop ? "已停止" : "已完成") + "。");
    controller->flush();
    Q_EMIT translationFinished(_shouldStop ? 1 : 0);
}

void TranslatorWorker::stopTranslation()
{
    _shouldStop = true;
}
