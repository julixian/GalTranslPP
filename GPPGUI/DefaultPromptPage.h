#ifndef DEFAULTPROMPTPAGE_H
#define DEFAULTPROMPTPAGE_H

#include "BasePage.h"
#include <toml.hpp>

class DefaultPromptPage : public BasePage
{
    Q_OBJECT
public:

    explicit DefaultPromptPage(QWidget* parent = nullptr);
    ~DefaultPromptPage() override;

private:

    toml::ordered_value m_promptConfig;

    void setupUi();
};

#endif
