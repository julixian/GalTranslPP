#ifndef SKIPTRANSCFGPAGE_H
#define SKIPTRANSCFGPAGE_H

#include "BasePage.h"
#include <toml.hpp>

class ElaWidget;

class SkipTransCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit SkipTransCfgPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);
	~SkipTransCfgPage() override;

private:
    toml::ordered_value& m_projectConfig;
	ElaWidget* m_hKeysWidget = nullptr;
};

#endif
