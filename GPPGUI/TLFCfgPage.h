#pragma once

#include "BasePage.h"
#include "GppTomlFwd.h"

class TLFCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit TLFCfgPage(tomlkit::Item& projectConfig, QWidget* parent = nullptr);

private:
    tomlkit::Item& m_projectConfig;

};
