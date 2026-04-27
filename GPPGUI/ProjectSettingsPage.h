// ProjectSettingsPage.h

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
        void finishTranslatingSignal(QString nodeKey); // 用于加红点提示翻译完成
        void changeProjectNameSignal(QString nodeKey, QString newProjectName); // 改变项目名

private:
    // UI 控件
    QStackedWidget* _stackedWidget;
    fs::path _projectDir;
    toml::ordered_value _projectConfig;
    toml::ordered_value& _globalConfig;

    APISettingsPage* _apiSettingsPage;
    PluginSettingsPage* _pluginSettingsPage;
    CommonSettingsPage* _commonSettingsPage;
    PASettingsPage* _paSettingsPage;
    NameTableSettingsPage* _nameTableSettingsPage;
    DictSettingsPage* _dictSettingsPage;
    DictExSettingsPage* _dictExSettingsPage;
    StartSettingsPage* _startSettingsPage;
    OtherSettingsPage* _otherSettingsPage;
    PromptSettingsPage* _promptSettingsPage;
    ProjectCachePage* _projectCachePage;
    ElaText* _settingsTitle;

    QWidget* _mainWindow;

    void _setupUI();
    void _createPages();

private Q_SLOTS:
    // 槽函数，用于响应开始翻译按钮的点击
    void _onStartTranslating();
    void _onFinishTranslating(const QString& transEngine, int exitCode);
    void _onRefreshProjectConfig();
};

#endif // PROJECTSETTINGSPAGE_H
