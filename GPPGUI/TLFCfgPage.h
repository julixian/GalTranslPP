#ifndef TLFCFGPAGE_H
#define TLFCFGPAGE_H

#include <toml.hpp>
#include "BasePage.h"

class TLFCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit TLFCfgPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~TLFCfgPage() override;

private:
    toml::ordered_value& m_projectConfig;

};

#endif // TLFCFGPAGE_H
