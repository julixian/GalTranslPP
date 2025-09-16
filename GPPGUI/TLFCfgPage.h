// TLFCfgPage.h

#ifndef TLFCFGPAGE_H
#define TLFCFGPAGE_H

#include <toml++/toml.hpp>
#include "BasePage.h"

class TLFCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit TLFCfgPage(toml::table& projectConfig, QWidget* parent = nullptr);
    ~TLFCfgPage();

private:
    toml::table& _projectConfig;

};

#endif // TLFCFGPAGE_H
