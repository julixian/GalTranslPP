#ifndef POSTFULL2HALFCFGPAGE_H
#define POSTFULL2HALFCFGPAGE_H

#include <toml++/toml.hpp>
#include "BasePage.h"

class PostFull2HalfCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit PostFull2HalfCfgPage(toml::table& projectConfig, QWidget* parent = nullptr);
    ~PostFull2HalfCfgPage();

private:
    toml::table& _projectConfig;
};

#endif // ! POSTFULL2HALFCFGPAGE_H
