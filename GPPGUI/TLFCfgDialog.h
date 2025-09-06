// TLFCfgDialog.h

#ifndef TLFCFGDIALOG_H
#define TLFCFGDIALOG_H

#include <toml++/toml.hpp>
#include "ElaContentDialog.h"

class ElaLineEdit;
class ElaComboBox;

class TLFCfgDialog : public ElaContentDialog
{
    Q_OBJECT

public:
    explicit TLFCfgDialog(toml::table& projectConfig, QWidget* parent = nullptr);
    ~TLFCfgDialog();

private Q_SLOTS:
    virtual void onRightButtonClicked() override;
    virtual void onMiddleButtonClicked() override;
    virtual void onLeftButtonClicked() override;

private:
    toml::table& _projectConfig;
    std::function<void()> _resetFunc;
    std::function<void()> _cancelFunc;

};

#endif // TLFCFGDIALOG_H
