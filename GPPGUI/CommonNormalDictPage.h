#ifndef COMMONNORMALDICTPAGE_H
#define COMMONNORMALDICTPAGE_H

#include "BasePage.h"
#include "NormalTabEntry.h"
#include <string>
#include <toml.hpp>

namespace fs = std::filesystem;

class CommonNormalDictPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonNormalDictPage(const std::string& mode, toml::ordered_value& globalConfig, QWidget* parent = nullptr);
    ~CommonNormalDictPage() override;

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
