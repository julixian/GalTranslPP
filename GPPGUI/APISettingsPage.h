#ifndef APISETTINGSPAGE_H
#define APISETTINGSPAGE_H

#include <QList>
#include <toml.hpp>
#include "BasePage.h"

class QVBoxLayout;
class ElaScrollPageArea;

class APISettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit APISettingsPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~APISettingsPage() override;
    virtual void apply2Config() override;

private Q_SLOTS:
    void addApiInputRow();
    void onDeleteApiRow();

private:
    // 成员变量
    QVBoxLayout* m_mainLayout = nullptr; // 页面主布局(用来增删APIKEY输入控件)
    toml::ordered_value& m_projectConfig;

    // 用于存储动态控件的列表
    struct ApiRowControls {
        ElaScrollPageArea* container;      // 容纳该行的容器（带边框的卡片）
        QWidget* configWidget;
        std::function<void(toml::ordered_array&)> applyFunc;
    };
    QList<ApiRowControls> m_apiRows;

    void setupUi();
    // 创建一个新的API输入行（现在返回一个ElaScrollPageArea*）
    ElaScrollPageArea* createApiInputRowWidget(const toml::value& apiTblValue = toml::table{});
};

#endif // APISETTINGSPAGE_H
