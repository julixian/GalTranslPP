// ProjectSettingsPage.h

#ifndef PROJECTSETTINGSPAGE_H
#define PROJECTSETTINGSPAGE_H

#include <QList>
#include <toml++/toml.hpp>
#include <filesystem>
#include <atomic>
#include "BasePage.h"

namespace fs = std::filesystem;

class QStackedWidget;
class QVBoxLayout;
class ElaToggleButton;
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

class ProjectSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit ProjectSettingsPage(toml::table& globalConfig, const fs::path& projectDir, QWidget* parent = nullptr);
    ~ProjectSettingsPage() override;

    QString getProjectName();
    fs::path getProjectDir();

    void apply2Config();
    void refreshCommonDicts();

    bool getIsRunning();

Q_SIGNALS:
        void finishedTranslating(QString nodeKey); // 用于加红点提示翻译完成

private:
    // UI 控件
    QStackedWidget* _stackedWidget;
    fs::path _projectDir;
    toml::table _projectConfig;
    toml::table& _globalConfig;

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

    QWidget* _mainWindow;

    void _setupUI();
    void _createPages();

private Q_SLOTS:
    // 槽函数，用于响应开始翻译按钮的点击
    void _onStartTranslating();
    void _onFinishTranslating(const QString& transEngine, int exitCode);
};

#endif // PROJECTSETTINGSPAGE_H
