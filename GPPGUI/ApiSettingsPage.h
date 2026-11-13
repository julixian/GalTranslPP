#pragma once

#include "BasePage.h"
#include "GppTomlFwd.h"
#include <QList>
#include <QSize>

class QEvent;
class QVBoxLayout;
class ElaScrollPageArea;
class ElaIconButton;
class GppFlowList;
class GppApiCard;
class ElaToolButton;

class ApiSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit ApiSettingsPage(tomlkit::Item& globalConfig, tomlkit::Item& projectConfig, QWidget* parent = nullptr);
    ~ApiSettingsPage() override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private Q_SLOTS:
    void addApiInputRow();

private:
    static QSize s_configWidgetSize;

    // 布局模式与 GUIConfig.apiLayoutMode 对应：0 为流式，1 为列表。
    int m_layoutMode = 0;
    ElaToolButton* m_flowModeButton = nullptr;
    ElaToolButton* m_listModeButton = nullptr;
    QWidget* m_listWidget = nullptr;
    QVBoxLayout* m_listLayout = nullptr;
    GppFlowList* m_flowList = nullptr;
    tomlkit::Item& m_globalConfig;
    tomlkit::Item& m_projectConfig;

    // 用于存储动态控件的列表
    struct ApiRowControls {
        ElaScrollPageArea* container;      // 容纳该行的容器（带边框的卡片）
        GppApiCard* flowCard;
        QWidget* configWidget;
        ElaIconButton* moveUpButton;
        ElaIconButton* moveDownButton;
        std::function<bool(tomlkit::Item&)> serializeFunc;
    };
    QList<ApiRowControls> m_apiRows;

    void setupUi();
    bool serializeApis(tomlkit::Item& apis) const;
    void switchLayout(int mode);
    void refreshApiLayout();
    void deleteApiRow(ElaScrollPageArea* container);
    void duplicateApiRow(ElaScrollPageArea* container);
    void moveApiRow(ElaScrollPageArea* container, int offset);
    void updateMoveButtonStates();
    // 创建一个新的 Api 输入行（现在返回一个ElaScrollPageArea*）
    ElaScrollPageArea* createApiInputRowWidget();
    ElaScrollPageArea* createApiInputRowWidget(const tomlkit::Item& apiTblValue);
};
