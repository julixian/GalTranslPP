#pragma once

#include "BasePage.h"
#include "GppTomlFwd.h"

class PDFCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit PDFCfgPage(tomlkit::Item& projectConfig, QWidget* parent = nullptr);

private:
    tomlkit::Item& m_projectConfig;
};
