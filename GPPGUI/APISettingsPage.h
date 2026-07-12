#ifndef APISETTINGSPAGE_H
#define APISETTINGSPAGE_H

#include "BasePage.h"
#include <QList>
#include <toml.hpp>

class QVBoxLayout;
class ElaScrollPageArea;
class ElaIconButton;

class ApiSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit ApiSettingsPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);
	void apply2Config() override;

private Q_SLOTS:
    void addApiInputRow();
    void onDeleteApiRow();

private:
    // 成员变量
    QVBoxLayout* m_mainLayout = nullptr; // 页面主布局(用来增删 Api key 输入控件)
    toml::ordered_value& m_projectConfig;

    // 用于存储动态控件的列表
    struct ApiRowControls {
        ElaScrollPageArea* container;      // 容纳该行的容器（带边框的卡片）
        QWidget* configWidget;
        ElaIconButton* moveUpButton;
        ElaIconButton* moveDownButton;
        std::function<void(toml::ordered_array&)> applyFunc;
    };
    QList<ApiRowControls> m_apiRows;

    void setupUi();
    void moveApiRow(ElaScrollPageArea* container, int offset);
    void updateMoveButtonStates();
    // 创建一个新的 Api 输入行（现在返回一个ElaScrollPageArea*）
    ElaScrollPageArea* createApiInputRowWidget(const toml::value& apiTblValue = toml::table{});
};

#endif
