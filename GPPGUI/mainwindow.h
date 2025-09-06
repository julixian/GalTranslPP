#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ElaWindow.h"

#include <QMainWindow>
#include <toml++/toml.hpp>
class HomePage;
class AboutDialog;
class CommonDictPage;
class SettingPage;
class ProjectSettingsPage;
class ElaContentDialog;

class MainWindow : public ElaWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    virtual void mouseReleaseEvent(QMouseEvent* event);

private Q_SLOTS:
    void _on_newProject_triggered();
    void _on_openProject_triggered();
    void _on_removeProject_triggered();
    void _on_deleteProject_triggered();

    void _on_closeWindow_clicked();

private:

    void initWindow();
    void initEdgeLayout();
    void initContent();

    HomePage* _homePage{nullptr};
    AboutDialog* _aboutPage{nullptr};
    CommonDictPage* _commonDictPage{nullptr};
    SettingPage* _settingPage{nullptr};

    QString _projectExpanderKey{""};

    QString _aboutKey{""};
    QString _settingKey{""};

    QList<QSharedPointer<ProjectSettingsPage>> _projectPages{};

    ElaContentDialog* _closeDialog{ nullptr };

    toml::table _globalConfig;
};
#endif // MAINWINDOW_H
