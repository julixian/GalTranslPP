#pragma once

#include "BasePage.h"
#include "GppTomlFwd.h"

class NJCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit NJCfgPage(tomlkit::Item& projectConfig, QWidget* parent = nullptr);
    ~NJCfgPage() override;

private:
    tomlkit::Item& m_projectConfig;
};
