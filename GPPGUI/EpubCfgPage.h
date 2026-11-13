#pragma once

#include "BasePage.h"
#include "GppTomlFwd.h"

class EpubCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit EpubCfgPage(tomlkit::Item& projectConfig, QWidget* parent = nullptr);

private:
    tomlkit::Item& m_projectConfig;
};
