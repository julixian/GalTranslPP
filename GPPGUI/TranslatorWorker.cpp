#include "TranslatorWorker.h"
#include <QThread>
#include <QTimer>

import std;
import ITranslator;

class GUIController : public IController
{

public:
    virtual void makeBar(int totalSentences, int totalThreads) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _makeBarCallback(totalSentences, totalThreads);
    }

    virtual void writeLog(const std::string& log) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _log += log;
    }

    virtual void addThreadNum() override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _addThreadNumCallback();
    }

    virtual void reduceThreadNum() override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _reduceThreadNumCallback();
    }

    virtual void updateBar(int ticks) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _progress += ticks;
    }

    virtual bool shouldStop() override
    {
        return _shouldStopCallback();
    }

    void flush()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_log.empty()) {
            _writeLogCallback(_log);
            _log.clear();
        }
        if (_progress > 0) {
            _updateBarCallback(_progress);
            _progress = 0;
        }
    }

    GUIController(std::function<void(int, int)> makeBarCallback, std::function<void(const std::string&)> writeLogCallback,
        std::function<void()> addThreadNumCallback, std::function<void()> reduceThreadNumCallback, std::function<void(int)> updateBarCallback, 
        std::function<bool()> shouldStopCallback) :
        _makeBarCallback{ makeBarCallback }, _writeLogCallback{ writeLogCallback }, _addThreadNumCallback{ addThreadNumCallback },
        _reduceThreadNumCallback{ reduceThreadNumCallback }, _updateBarCallback{ updateBarCallback }, _shouldStopCallback{ shouldStopCallback }
    {
        _log.reserve(1024 * 1024);
        _flushThread = std::thread([this]()
            {
                while (_controlling.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
    std::function<void(int, int)> _makeBarCallback;
    std::function<void(const std::string&)> _writeLogCallback;
    std::function<void()> _addThreadNumCallback;
    std::function<void()> _reduceThreadNumCallback;
    std::function<void(int)> _updateBarCallback;
    std::function<bool()> _shouldStopCallback;
    std::thread _flushThread;
    std::atomic<bool> _controlling = true;
    std::mutex _mutex;
    int _progress = 0;
    std::string _log;
};

TranslatorWorker::TranslatorWorker(const fs::path& projectDir, QObject* parent)
    : QObject{ parent }, _projectDir{ projectDir }
{
}

void TranslatorWorker::doTranslation()
{
    _shouldStop = false;

    auto makeBarCallback = [this](int totalSentences, int totalThreads)
        {
            Q_EMIT makeBarSignal(totalSentences, totalThreads);
        };
    auto writeLogCallback = [this](const std::string& log)
        {
            Q_EMIT writeLogSignal(QString::fromStdString(log));
        };
    auto addThreadNumCallback = [this]()
        {
            Q_EMIT addThreadNumSignal();
        };
    auto reduceThreadNumCallback = [this]()
        {
            Q_EMIT reduceThreadNumSignal();
        };
    auto updateBarCallback = [this](int ticks)
        {
            Q_EMIT updateBarSignal(ticks);
        };
    auto shouldStopCallback = [this]() -> bool
        {
            return this->_shouldStop.load();
        };
    std::shared_ptr<GUIController> controller = std::make_shared<GUIController>(makeBarCallback, writeLogCallback,
        addThreadNumCallback, reduceThreadNumCallback, updateBarCallback, shouldStopCallback);
    try {

        {
            std::unique_ptr<ITranslator> translator = createTranslator(_projectDir, controller);
            if (!translator) {
                Q_EMIT translationFinished(-1);
                return;
            }
            translator->run();
        }

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
