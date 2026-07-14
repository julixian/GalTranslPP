#ifndef NJCFGPAGE_H
#define NJCFGPAGE_H

#include <toml.hpp>
#include "BasePage.h"

class NJCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit NJCfgPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~NJCfgPage() override;

private:
    toml::ordered_value& m_projectConfig;
};

#endif
