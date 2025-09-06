#ifndef TRANSLATORWORKER_H
#define TRANSLATORWORKER_H

#include <QObject>
#include <filesystem>

namespace fs = std::filesystem;

class TranslatorWorker : public QObject
{
    Q_OBJECT
public:
    explicit TranslatorWorker(const fs::path& projectDir, QObject* parent = nullptr);
    void stopTranslation();


public Q_SLOTS:
    // 执行翻译任务的槽函数
    void doTranslation();

Q_SIGNALS:
    // 任务完成信号（无论是正常结束还是被中断）
    void translationFinished(int exitCode);

    void makeBarSignal(int totalSentences, int totalThreads);
    void writeLogSignal(QString log);
    void addThreadNumSignal();
    void reduceThreadNumSignal();
    void updateBarSignal(int ticks);

private:
    std::atomic<bool> _shouldStop;
    fs::path _projectDir;
};

#endif // TRANSLATORWORKER_H
