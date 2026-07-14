#ifndef COMMONNORMALDICTSPAGE_H
#define COMMONNORMALDICTSPAGE_H

#include "BasePage.h"
#include "NormalTabEntry.h"
#include <toml.hpp>

namespace fs = std::filesystem;

class CommonNormalDictsPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonNormalDictsPage(const std::string& mode, toml::ordered_value& globalConfig, QWidget* parent = nullptr);

Q_SIGNALS:
    void commonDictsChangedSignal();

private:

    void setupUi();

    toml::ordered_value& m_globalConfig;

    QList<NormalTabEntry> m_normalTabEntries;

    std::string m_mode;
    std::string m_modeConfigKey;
    fs::path m_modeDictDir;
};

#endif
