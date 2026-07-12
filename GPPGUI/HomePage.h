#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include "BasePage.h"
#include <toml.hpp>

class HomePage : public BasePage
{
    Q_OBJECT

public:
	explicit HomePage(toml::ordered_value& globalConfig, QWidget* parent = nullptr);
    ~HomePage() override;

private:
    void setupUi();

    toml::ordered_value& m_globalConfig;
};

#endif
