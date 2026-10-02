#pragma once

#include "BasePage.h"
#include <toml.hpp>

class TF2HCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit TF2HCfgPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);

private:
    toml::ordered_value& m_projectConfig;
};
