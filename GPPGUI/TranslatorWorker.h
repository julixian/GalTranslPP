#ifndef TRANSLATORWORKER_H
#define TRANSLATORWORKER_H

#include <QObject>
#include <QStringList>
#include <QVector>
#include <filesystem>

namespace fs = std::filesystem;

struct GuiRuntimeTransSuccessEvent {
    QString timestamp;
    QString filename;
    int index{};
    QStringList speakers;
    QStringList problems;
    QString sourcePreview;
    QString translationPreview;
    QString transby;
};
Q_DECLARE_METATYPE(GuiRuntimeTransSuccessEvent)

struct GuiRuntimeTransErrorEvent {
    QString timestamp;
    QString kind;
    QString level;
    QString message;
    QString filename;
    QString indexRange;
    int requestCount = -1;
    QString model;
    double sleepSeconds = -1.0;
};
Q_DECLARE_METATYPE(GuiRuntimeTransErrorEvent)

struct GuiRuntimeFileProgress {
    QString filename;
    int total{};
    int completed{};
    int problems{};
};
Q_DECLARE_METATYPE(GuiRuntimeFileProgress)

class TranslatorWorker : public QObject
{
    Q_OBJECT

public:
    explicit TranslatorWorker(const fs::path& projectDir, QObject* parent = nullptr);

    bool getShouldStop() const { return m_shouldStop; }
    void setShouldStop(bool shouldStop) { m_shouldStop = shouldStop; }
    void doTranslation();
    void stopTranslation();

Q_SIGNALS:
    // 任务完成信号（无论是正常结束还是被中断）
    void translationFinishedSignal(int exitCode);

    void makeBarSignal(int totalSentences, int totalThreads);
    void writeLogSignal(const QString& log);
    void addThreadNumSignal();
    void reduceThreadNumSignal();
    void updateBarSignal(int ticks);
    void runtimeFilesResetSignal(const QVector<GuiRuntimeFileProgress>& files);
    void runtimeFileProgressBatchSignal(const QVector<GuiRuntimeFileProgress>& files);
    void runtimeTransSuccessBatchSignal(const QVector<GuiRuntimeTransSuccessEvent>& events);
    void runtimeTransErrorBatchSignal(const QVector<GuiRuntimeTransErrorEvent>& events);
    void runtimeStageChangedSignal(const QString& stage, const QString& currentFile);

private:
    fs::path m_projectDir;
    bool m_shouldStop = false;
};

#endif
