#pragma once

#include "ElaWindow.h"

#include <QMainWindow>
#include <QSize>
#include <QStringList>
#include "GppTomlFwd.h"
#include <filesystem>
#include <memory>

class HomePage;
class AboutDialog;
class DefaultPromptsPage;
class CommonGptDictsPage;
class CommonNormalDictsPage;
class AppSettingsPage;
class ProjectSettingsPage;
class ElaContentDialog;
class UpdateChecker;
class TaskbarController;
class QShortcut;
class QShowEvent;
class QHideEvent;
class QEvent;

namespace fs = std::filesystem;

class MainWindow : public ElaWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

public Q_SLOTS:
    void checkUpdate();

protected:
    Q_TAKEOVER_NATIVEEVENT_H
	void mouseReleaseEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

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
    ProjectSettingsPage* createProjectSettingsPage(const fs::path& projectDir, const QString& targetParentKey = {});
    ProjectSettingsPage* findProjectPageByKey(const QString& nodeKey) const;
    QStringList getOrderedProjectCategoryKeys() const;
    bool isProjectCategoryKey(const QString& nodeKey) const;
    bool hasCategoryName(const QString& name, const QString& excludeCategoryKey = {}) const;
    bool promptCategoryName(const QString& dialogTitle, const QString& initialText, const QString& placeholder, QString& outName);
    QString createProjectCategory(const QString& name);
    void handleNewProjectCategory(const QString& moveProjectKey = {});
    void handleRenameProjectCategory(const QString& categoryKey);
    void handleDeleteProjectCategory(const QString& categoryKey);
    void showProjectRootContextMenu(const QPoint& globalPos);
    void showProjectCategoryContextMenu(const QString& categoryKey, const QPoint& globalPos);
    void showProjectNodeContextMenu(const QString& nodeKey, const QPoint& globalPos);
    void updateTaskbarProgress();
    void updateTaskbarBadge();
    void syncProjectsToGlobalConfig();

    HomePage* m_homePage = nullptr;
    AboutDialog* m_aboutDialog = nullptr;
    DefaultPromptsPage* m_defaultPromptPage = nullptr;
    CommonNormalDictsPage* m_commonPreDictsPage = nullptr;
    CommonGptDictsPage* m_commonGptDictsPage = nullptr;
    CommonNormalDictsPage* m_commonPostDictsPage = nullptr;
    AppSettingsPage* m_appSettingsPage = nullptr;

    QString m_commonDictExpanderKey;
    QString m_projectExpanderKey;
    QStringList m_projectCategoryKeys;
    bool m_projectsLoaded = false;

    QString m_aboutKey;
    QString m_illustrationKey;
    QString m_appSettingsKey;

    QShortcut* m_clearLogShortcut = nullptr;

    QList<QSharedPointer<ProjectSettingsPage>> m_projectPages;

    ElaContentDialog* m_closeDialog = nullptr;
    UpdateChecker* m_updateChecker = nullptr;
    std::unique_ptr<TaskbarController> m_taskbarController;

    QSize m_windowSize;
    std::unique_ptr<tomlkit::Item> m_globalConfig;
};
