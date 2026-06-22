#ifndef SKIPTRANSCFGPAGE_H
#define SKIPTRANSCFGPAGE_H

#include <toml.hpp>
#include "BasePage.h"

class SkipTransCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit SkipTransCfgPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~SkipTransCfgPage() override;

private:
    toml::ordered_value& m_projectConfig;
};

#endif // ! SKIPTRANSCFGPAGE_H
