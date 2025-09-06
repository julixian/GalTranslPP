// APISettingsPage.h

#ifndef APISETTINGSPAGE_H
#define APISETTINGSPAGE_H

#include <QList>
#include <toml++/toml.hpp>
#include "BasePage.h"

class QVBoxLayout;
class ElaLineEdit;
class ElaScrollPageArea;

class APISettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit APISettingsPage(toml::table& projectConfig, QWidget* parent = nullptr);
    ~APISettingsPage() override;
    void apply2Config();

private Q_SLOTS:
    void _addApiInputRow();
    void _onDeleteApiRow();

private:
    // 成员变量
    QVBoxLayout* _mainLayout; // 页面主布局(用来增删APIKEY输入控件)
    toml::table& _projectConfig;


    // 用于存储动态控件的列表
    struct ApiRowControls {
        ElaScrollPageArea* container;      // 容纳该行的容器（带边框的卡片）
        ElaLineEdit* keyEdit;
        ElaLineEdit* urlEdit;
        ElaLineEdit* modelEdit;
    };
    QList<ApiRowControls> _apiRows;

    void _setupUI();
    // 创建一个新的API输入行（现在返回一个ElaScrollPageArea*）
    ElaScrollPageArea* _createApiInputRowWidget(const QString& key = "", const QString& url = "", const QString& model = "");
};

#endif // APISETTINGSPAGE_H
