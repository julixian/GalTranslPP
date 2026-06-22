#ifndef TF2HCFGPAGE_H
#define TF2HCFGPAGE_H

#include <toml.hpp>
#include "BasePage.h"

class TF2HCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit TF2HCfgPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~TF2HCfgPage() override;

private:
    toml::ordered_value& m_projectConfig;
};

#endif // ! TF2HCFGPAGE_H
