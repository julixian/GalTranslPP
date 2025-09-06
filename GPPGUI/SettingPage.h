#ifndef SETTINGPAGE_H
#define SETTINGPAGE_H

#include <toml++/toml.hpp>
#include "BasePage.h"

class ElaRadioButton;
class ElaToggleSwitch;
class ElaComboBox;
class SettingPage : public BasePage
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit SettingPage(toml::table& globalConfig, QWidget* parent = nullptr);
    ~SettingPage() override;

private:
    ElaComboBox* _themeComboBox{nullptr};
    ElaRadioButton* _normalButton{nullptr};
    ElaRadioButton* _elaMicaButton{nullptr};

    ElaRadioButton* _micaButton{nullptr};
    ElaRadioButton* _micaAltButton{nullptr};
    ElaRadioButton* _acrylicButton{nullptr};
    ElaRadioButton* _dwmBlurnormalButton{nullptr};

    ElaRadioButton* _minimumButton{nullptr};
    ElaRadioButton* _compactButton{nullptr};
    ElaRadioButton* _maximumButton{nullptr};
    ElaRadioButton* _autoButton{nullptr};

    toml::table& _globalConfig;
};

#endif // SETTINGPAGE_H
