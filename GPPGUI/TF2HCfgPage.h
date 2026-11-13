#pragma once

#include "BasePage.h"
#include "GppTomlFwd.h"

class TF2HCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit TF2HCfgPage(tomlkit::Item& projectConfig, QWidget* parent = nullptr);

private:
    tomlkit::Item& m_projectConfig;
};
