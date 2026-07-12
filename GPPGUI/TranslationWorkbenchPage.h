#ifndef TRANSLATIONWORKBENCHPAGE_H
#define TRANSLATIONWORKBENCHPAGE_H

#include "BasePage.h"
#include "TranslatorWorker.h"
#include <QMap>
#include <QSet>
#include <QStandardItemModel>
#include <QVector>


class ElaListView;
class ElaPushButton;
class ElaText;
class QButtonGroup;
class QStackedWidget;

class TranslationWorkbenchPage : public BasePage
{
    Q_OBJECT

public:
    explicit TranslationWorkbenchPage(QWidget* parent = nullptr);

    void resetRuntimeFiles(const QVector<GuiRuntimeFileProgress>& files);
    void updateRuntimeFiles(const QVector<GuiRuntimeFileProgress>& files);
    void appendSuccesses(const QVector<GuiRuntimeTransSuccessEvent>& events);
    void appendErrors(const QVector<GuiRuntimeTransErrorEvent>& events);
    void updateStage(const QString& stage, const QString& currentFile);
    void clearRuntime();

private:
    void setupUi();
    void setSideTab(int index);
    void renderSuccesses();
    void renderErrors();
    void renderFiles();
    void refreshHeader();
    void trimSuccesses();
    void trimErrors();

    QVector<GuiRuntimeTransSuccessEvent> m_successes;
    QVector<GuiRuntimeTransErrorEvent> m_errors;
    QMap<QString, GuiRuntimeFileProgress> m_files;
    QSet<QString> m_successFileFilters;
    int m_successTotal{};
    int m_errorTotal{};
    QString m_stage;
    QString m_currentFile;

    QStandardItemModel* m_successModel = nullptr;
    QStandardItemModel* m_errorModel = nullptr;
    QStandardItemModel* m_fileModel = nullptr;

    ElaListView* m_successList = nullptr;
    ElaListView* m_errorList = nullptr;
    ElaListView* m_fileList = nullptr;
    ElaText* m_summaryText = nullptr;
    ElaText* m_filterText = nullptr;
    ElaPushButton* m_clearFilterButton = nullptr;
    ElaPushButton* m_errorsTabButton = nullptr;
    ElaPushButton* m_filesTabButton = nullptr;
    QButtonGroup* m_sideTabGroup = nullptr;
    QStackedWidget* m_sideStack = nullptr;
};

#endif
