// CommonNormalDictPage.h

#ifndef COMMONNORMALDICTPAGE_H
#define COMMONNORMALDICTPAGE_H

#include <toml++/toml.hpp>
#include <string>
#include "BasePage.h"
#include "NormalTabEntry.h"

namespace fs = std::filesystem;

class CommonNormalDictPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonNormalDictPage(const std::string& mode, toml::table& globalConfig, QWidget* parent = nullptr);
    ~CommonNormalDictPage() override;

Q_SIGNALS:
    void commonDictsChanged();

private:

    QList<NormalDictEntry> readNormalDicts(const fs::path& dictPath);
    QString readNormalDictsStr(const fs::path& dictPath);

    void _setupUI();

    toml::table& _globalConfig;

    QList<NormalTabEntry> _normalTabEntries;
    QWidget* _mainWindow;

    std::string _modePath;
    std::string _modeConfig;
};

#endif // COMMONNORMALDICTPAGE_H
