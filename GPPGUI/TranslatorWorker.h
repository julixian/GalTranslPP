#ifndef TRANSLATORWORKER_H
#define TRANSLATORWORKER_H

#include <QObject>
#include <filesystem>
#include <QStringList>
#include <QVector>

namespace fs = std::filesystem;

struct GuiRuntimeSuccessEvent {
    QString timestamp;
    QString filename;
    int index{0};
    QStringList speakers;
    QString sourcePreview;
    QString translationPreview;
    QString translatedBy;
};
Q_DECLARE_METATYPE(GuiRuntimeSuccessEvent)

struct GuiRuntimeErrorEvent {
    QString timestamp;
    QString kind;
    QString level;
    QString message;
    QString filename;
    QString indexRange;
    int retryCount{-1};
    QString model;
    double sleepSeconds{-1.0};
};
Q_DECLARE_METATYPE(GuiRuntimeErrorEvent)

struct GuiRuntimeFileProgress {
    QString filename;
    int total{0};
    int completed{0};
    int problems{0};
};
Q_DECLARE_METATYPE(GuiRuntimeFileProgress)

class TranslatorWorker : public QObject
{
    Q_OBJECT
public:
    explicit TranslatorWorker(const fs::path& projectDir, QObject* parent = nullptr);

    void stopTranslation();
    bool getShouldStop() const { return _shouldStop; }


public Q_SLOTS:
    // 执行翻译任务的槽函数
    void doTranslation();

Q_SIGNALS:
    // 任务完成信号（无论是正常结束还是被中断）
    void translationFinished(int exitCode);

    void makeBarSignal(int totalSentences, int totalThreads);
    void writeLogSignal(const QString& log);
    void addThreadNumSignal();
    void reduceThreadNumSignal();
    void updateBarSignal(int ticks);
    void runtimeFilesResetSignal(const QVector<GuiRuntimeFileProgress>& files);
    void runtimeFileProgressBatchSignal(const QVector<GuiRuntimeFileProgress>& files);
    void runtimeSuccessBatchSignal(const QVector<GuiRuntimeSuccessEvent>& events);
    void runtimeErrorBatchSignal(const QVector<GuiRuntimeErrorEvent>& events);
    void runtimeStageChangedSignal(const QString& stage, const QString& currentFile);

private:
    fs::path _projectDir;
    bool _shouldStop = false;
};

#endif // TRANSLATORWORKER_H
