#ifndef EPUBCFGPAGE_H
#define EPUBCFGPAGE_H

#include "BasePage.h"
#include <toml.hpp>

class EpubCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit EpubCfgPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);

private:
    toml::ordered_value& m_projectConfig;
};

#endif
