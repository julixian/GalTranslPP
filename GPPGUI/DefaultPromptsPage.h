#ifndef DEFAULTPROMPTSPAGE_H
#define DEFAULTPROMPTSPAGE_H

#include "BasePage.h"
#include <toml.hpp>

class DefaultPromptsPage : public BasePage
{
    Q_OBJECT
public:

    explicit DefaultPromptsPage(QWidget* parent = nullptr);

private:

    toml::ordered_value m_promptConfig;

    void setupUi();
};

#endif
