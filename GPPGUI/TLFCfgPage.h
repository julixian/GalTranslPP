#ifndef TLFCFGPAGE_H
#define TLFCFGPAGE_H

#include "BasePage.h"
#include <toml.hpp>

class TLFCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit TLFCfgPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);

private:
    toml::ordered_value& m_projectConfig;

};

#endif
