#pragma once

#include "BasePage.h"
#include "GppTomlFwd.h"

class ElaWidget;

class SkipTransCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit SkipTransCfgPage(tomlkit::Item& projectConfig, QWidget* parent = nullptr);
	~SkipTransCfgPage() override;

private:
    tomlkit::Item& m_projectConfig;
	ElaWidget* m_hKeysWidget = nullptr;
};
