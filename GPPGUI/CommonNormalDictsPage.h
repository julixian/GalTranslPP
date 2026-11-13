#pragma once

#include "BasePage.h"
#include "NormalTabEntry.h"
#include <QSharedPointer>
#include "GppTomlFwd.h"

namespace fs = std::filesystem;

class CommonNormalDictsPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonNormalDictsPage(const std::string& mode, tomlkit::Item& globalConfig, QWidget* parent = nullptr);

Q_SIGNALS:
    void commonDictsChangedSignal();

private:

    void setupUi();

    tomlkit::Item& m_globalConfig;

    QList<QSharedPointer<NormalTabEntry>> m_normalTabEntries;

    std::string m_mode;
    std::string m_modeConfigKey;
    fs::path m_modeDictDir;
};
