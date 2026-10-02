#pragma once

#include "BasePage.h"
#include <toml.hpp>

class HomePage : public BasePage
{
    Q_OBJECT

public:
	explicit HomePage(toml::ordered_value& globalConfig, QWidget* parent = nullptr);

private:
    void setupUi();

    toml::ordered_value& m_globalConfig;
};
