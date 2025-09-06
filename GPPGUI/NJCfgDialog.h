// NJCfgDialog.h

#ifndef NJCFGDIALOG_H
#define NJCFGDIALOG_H

#include <toml++/toml.hpp>
#include "ElaContentDialog.h"

class NJCfgDialog : public ElaContentDialog
{
    Q_OBJECT

public:
    explicit NJCfgDialog(toml::table& projectConfig, QWidget* parent = nullptr);
    ~NJCfgDialog();

private Q_SLOTS:
    virtual void onRightButtonClicked() override;
    virtual void onMiddleButtonClicked() override;
    virtual void onLeftButtonClicked() override;

private:
    toml::table& _projectConfig;
    std::function<void()> _cancelFunc;
    std::function<void()> _resetFunc;
};

#endif // NJCFGDIALOG_H
