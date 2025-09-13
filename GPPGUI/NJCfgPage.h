// NJCfgPage.h

#ifndef NJCFGPAGE_H
#define NJCFGPAGE_H

#include <toml++/toml.hpp>
#include "BasePage.h"

class NJCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit NJCfgPage(toml::table& projectConfig, QWidget* parent = nullptr);
    ~NJCfgPage();
    void apply2Config();

private:
    toml::table& _projectConfig;
};

#endif // NJCFGPAGE_H
