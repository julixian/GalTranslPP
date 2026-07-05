#ifndef PROJECTSETTINGSPAGE_H
#define PROJECTSETTINGSPAGE_H

#include <QList>
#include <toml.hpp>
#include <filesystem>
#include "BasePage.h"

namespace fs = std::filesystem;

class ElaText;
class QStackedWidget;
class APISettingsPage;
class PluginSettingsPage;
class CommonSettingsPage;
class PASettingsPage;
class NameTableSettingsPage;
class DictSettingsPage;
class DictExSettingsPage;
class StartSettingsPage;
class OtherSettingsPage;
class PromptSettingsPage;
class ProjectCachePage;

class ProjectSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit ProjectSettingsPage(const fs::path& projectDir, toml::ordered_value& globalConfig, QWidget* parent = nullptr);
    ~ProjectSettingsPage() override;

    QString getProjectName();
    fs::path getProjectDir();
    bool getIsRunning();

    virtual void apply2Config() override;
    void refreshCommonDicts();
    void clearLog(bool forceClear);

Q_SIGNALS:
    void finishTranslatingSignal(const QString& nodeKey); // 用于加红点提示翻译完成
    void changeProjectNameSignal(const QString& nodeKey, const QString& newProjectName); // 改变项目名

private:
    fs::path m_projectDir;
    toml::ordered_value& m_globalConfig;
    toml::ordered_value m_projectConfig;

    // UI 控件
    QStackedWidget* m_stackedWidget = nullptr;

    APISettingsPage* m_apiSettingsPage = nullptr;
    PluginSettingsPage* m_pluginSettingsPage = nullptr;
    CommonSettingsPage* m_commonSettingsPage = nullptr;
    PASettingsPage* m_paSettingsPage = nullptr;
    NameTableSettingsPage* m_nameTableSettingsPage = nullptr;
    DictSettingsPage* m_dictSettingsPage = nullptr;
    DictExSettingsPage* m_dictExSettingsPage = nullptr;
    StartSettingsPage* m_startSettingsPage = nullptr;
    OtherSettingsPage* m_otherSettingsPage = nullptr;
    PromptSettingsPage* m_promptSettingsPage = nullptr;
    ProjectCachePage* m_projectCachePage = nullptr;
    ElaText* m_settingsTitle = nullptr;

    void setupUi();
    void createPages();

private Q_SLOTS:
    void onStartTranslating();
    void onFinishTranslating(const QString& transEngine, int exitCode);
    void onRefreshProjectConfig();
};

#endif // PROJECTSETTINGSPAGE_H
