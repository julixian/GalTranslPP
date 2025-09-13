// CommonPostDictPage.h

#ifndef COMMONPOSTDICTPAGE_H
#define COMMONPOSTDICTPAGE_H

#include <QList>
#include <toml++/toml.hpp>
#include "BasePage.h"
#include "NormalTabEntry.h"

namespace fs = std::filesystem;

class CommonPostDictPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonPostDictPage(toml::table& globalConfig, QWidget* parent = nullptr);
    ~CommonPostDictPage() override;

Q_SIGNALS:
    void commonDictsChanged();

public Q_SLOTS:
    void apply2Config();

private:

    QList<NormalDictEntry> readNormalDicts(const fs::path& dictPath);
    QString readNormalDictsStr(const fs::path& dictPath);

    void _setupUI();

    toml::table& _globalConfig;

    std::function<void()> _applyFunc;

    QList<NormalTabEntry> _normalTabEntries;
    QWidget* _mainWindow;
};

#endif // COMMONPOSTDICTPAGE_H
