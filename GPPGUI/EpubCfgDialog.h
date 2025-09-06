// EpubCfgDialog.h

#ifndef EPUBCFGDIALOG_H
#define EPUBCFGDIALOG_H

#include <toml++/toml.hpp>
#include "ElaContentDialog.h"

class EpubCfgDialog : public ElaContentDialog
{
    Q_OBJECT

public:
    explicit EpubCfgDialog(toml::table& projectConfig, QWidget* parent = nullptr);
    ~EpubCfgDialog();

private Q_SLOTS:
    virtual void onRightButtonClicked() override;
    virtual void onMiddleButtonClicked() override;
    virtual void onLeftButtonClicked() override;

private:

    toml::table& _projectConfig;
    std::function<void()> _cancelFunc;
    std::function<void()> _resetFunc;
};

#endif // EPUBCFGDIALOG_H
