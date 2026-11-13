#pragma once

#include "BasePage.h"
#include "GppTomlFwd.h"

class HomePage : public BasePage
{
    Q_OBJECT

public:
	explicit HomePage(tomlkit::Item& globalConfig, QWidget* parent = nullptr);

private:
    void setupUi();

    tomlkit::Item& m_globalConfig;
};
