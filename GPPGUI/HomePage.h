#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include <toml.hpp>
#include "BasePage.h"

class HomePage : public BasePage
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit HomePage(toml::ordered_value& globalConfig, QWidget* parent = nullptr);
    ~HomePage() override;

private:
    void setupUi();

    toml::ordered_value& m_globalConfig;
};

#endif // HOMEPAGE_H
