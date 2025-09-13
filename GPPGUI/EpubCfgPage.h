// EpubCfgPage.h

#ifndef EPUBCFGPAGE_H
#define EPUBCFGPAGE_H

#include <toml++/toml.hpp>
#include "BasePage.h"

class EpubCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit EpubCfgPage(toml::table& projectConfig, QWidget* parent = nullptr);
    ~EpubCfgPage();
    void apply2Config();

private:
    toml::table& _projectConfig;

};

#endif // EPUBCFGPAGE_H
