#pragma once

#include <toml++/toml.hpp>
#include "ElaContentDialog.h"
#include "ElaWidgetTools/ElaToggleSwitch.h"
#include "ElaWidgetTools/ElaText.h"
#include "ElaWidgetTools/ElaComboBox.h"
#include <QVBoxLayout>
#include <QTableWidget>
#include <QPushButton>

import Tool;

class PostFull2HalfCfgDialog : public ElaContentDialog
{
    Q_OBJECT

public:
    explicit PostFull2HalfCfgDialog(toml::table& projectConfig, QWidget* parent = nullptr);
    ~PostFull2HalfCfgDialog();

private slots:
    void onRightButtonClicked();
    void onMiddleButtonClicked();
    void onLeftButtonClicked();

private:
    toml::table& _projectConfig;
    std::function<void()> _resetFunc;
    std::function<void()> _cancelFunc;

    // UI控件
    ElaToggleSwitch* _punctuationSwitch;
    ElaToggleSwitch* _reverseSwitch;
};
