#ifndef TRANSLATIONWORKBENCHPAGE_H
#define TRANSLATIONWORKBENCHPAGE_H

#include <QMap>
#include <QSet>
#include <QStandardItemModel>
#include <QVector>

#include "BasePage.h"
#include "TranslatorWorker.h"

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
    void updateRuntimeFile(const GuiRuntimeFileProgress& file);
    void updateRuntimeFiles(const QVector<GuiRuntimeFileProgress>& files);
    void appendSuccess(const GuiRuntimeSuccessEvent& event);
    void appendSuccesses(const QVector<GuiRuntimeSuccessEvent>& events);
    void appendError(const GuiRuntimeErrorEvent& event);
    void appendErrors(const QVector<GuiRuntimeErrorEvent>& events);
    void updateStage(const QString& stage, const QString& currentFile);
    void clearRuntime();

private:
    void _setupUI();
    void _setSideTab(int index);
    void _renderSuccesses();
    void _renderErrors();
    void _renderFiles();
    void _refreshHeader();
    void _trimSuccesses();
    void _trimErrors();

    QVector<GuiRuntimeSuccessEvent> _successes;
    QVector<GuiRuntimeErrorEvent> _errors;
    QMap<QString, GuiRuntimeFileProgress> _files;
    QSet<QString> _successFileFilters;
    int _successTotal{0};
    QString _stage;
    QString _currentFile;

    QStandardItemModel* _successModel{nullptr};
    QStandardItemModel* _errorModel{nullptr};
    QStandardItemModel* _fileModel{nullptr};

    ElaListView* _successList{nullptr};
    ElaListView* _errorList{nullptr};
    ElaListView* _fileList{nullptr};
    ElaText* _summaryText{nullptr};
    ElaText* _filterText{nullptr};
    ElaPushButton* _clearFilterButton{nullptr};
    ElaPushButton* _errorsTabButton{nullptr};
    ElaPushButton* _filesTabButton{nullptr};
    QButtonGroup* _sideTabGroup{nullptr};
    QStackedWidget* _sideStack{nullptr};
};

#endif // TRANSLATIONWORKBENCHPAGE_H
