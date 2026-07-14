#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#define PYBIND11_HEADERS
#include "../GalTranslPP/GPPMacros.hpp"
#include "ElaWindow.h"

#include <QMainWindow>
#include <toml.hpp>
#include <filesystem>

class HomePage;
class AboutDialog;
class DefaultPromptsPage;
class CommonGptDictsPage;
class CommonNormalDictsPage;
class AppSettingsPage;
class ProjectSettingsPage;
class ElaContentDialog;
class UpdateChecker;
class QShortcut;

namespace fs = std::filesystem;
namespace py = pybind11;

class MainWindow : public ElaWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

public Q_SLOTS:
    void checkUpdate();

protected:
	void mouseReleaseEvent(QMouseEvent* event) override;

private Q_SLOTS:
    void onNewProjectTriggered();
    void onOpenProjectTriggered();
    void onRemoveProjectTriggered();
    void onDeleteProjectTriggered();
    void onSaveProjectTriggered();
    void onFinishTranslating(const QString& nodeKey);
    void onCloseWindowClicked(bool restart);
    void onClearLog(bool forceClear); // forceClear 为 true 时，只要处在项目的任一页面(为 false 时，如快捷键方案，需要处于 `开始翻译` 页面)，就可以触发日志清除

private:

    void initWindow();
    void initEdgeLayout();
    void initContent();
    ProjectSettingsPage* createProjectSettingsPage(const fs::path& projectDir);

    HomePage* m_homePage = nullptr;
    AboutDialog* m_aboutDialog = nullptr;
    DefaultPromptsPage* m_defaultPromptPage = nullptr;
    CommonNormalDictsPage* m_commonPreDictsPage = nullptr;
    CommonGptDictsPage* m_commonGptDictsPage = nullptr;
    CommonNormalDictsPage* m_commonPostDictsPage = nullptr;
    AppSettingsPage* m_appSettingsPage = nullptr;

    QString m_commonDictExpanderKey;
    QString m_projectExpanderKey;

    QString m_aboutKey;
    QString m_transIllustrationKey;
    QString m_appSettingsKey;

    QShortcut* m_clearLogShortcut = nullptr;

    QList<QSharedPointer<ProjectSettingsPage>> m_projectPages;

    ElaContentDialog* m_closeDialog = nullptr;
    UpdateChecker* m_updateChecker = nullptr;

    toml::ordered_value m_globalConfig;
};

#endif
