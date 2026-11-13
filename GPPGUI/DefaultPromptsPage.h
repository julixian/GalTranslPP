#pragma once

#include "BasePage.h"
#include "GppTomlFwd.h"
#include <memory>

class DefaultPromptsPage : public BasePage
{
    Q_OBJECT
public:

    explicit DefaultPromptsPage(QWidget* parent = nullptr);
    ~DefaultPromptsPage() override;

private:

    std::unique_ptr<tomlkit::Item> m_promptConfig;

    void setupUi();
};
