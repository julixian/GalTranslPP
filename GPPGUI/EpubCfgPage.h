// EpubCfgPage.h

#ifndef EPUBCFGPAGE_H
#define EPUBCFGPAGE_H

#include <toml++/toml.hpp>
#include <functional>
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
    std::function<void()> _applyFunc;
};

#endif // EPUBCFGPAGE_H
