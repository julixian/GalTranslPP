// StartSettingsPage.h

#ifndef STARTSETTINGSPAGE_H
#define STARTSETTINGSPAGE_H

#include <QThread>
#include <toml++/toml.hpp>
#include <filesystem>
#include "BasePage.h"
#include "TranslatorWorker.h"

namespace fs = std::filesystem;

class ElaPushButton;
class ElaProgressBar;

class StartSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit StartSettingsPage(fs::path& projectDir, toml::table& projectConfig, QWidget* parent = nullptr);
    ~StartSettingsPage() override;
    void apply2Config();

Q_SIGNALS:
    void startTranslating();  // 让projectSettings去保存配置
    void finishTranslating(const QString& filePlugin); // 这两个向projectSettings页发送
    void startWork();
    void stopWork();  // 这两个向worker发送

private:

    fs::path& _projectDir;
    QThread* _workThread;
    TranslatorWorker* _worker;

    void _setupUI();
    toml::table& _projectConfig;

    ElaPushButton* _startTranslateButton;
    ElaPushButton* _stopTranslateButton;
    ElaProgressBar* _progressBar;

    QString _filePlugin;

private Q_SLOTS:

    void _onStartTranslatingClicked();
    void _onStopTranslatingClicked();

    void _workFinished(int exitCode); // worker结束了的信号
};

#endif // STARTSETTINGSPAGE_H
