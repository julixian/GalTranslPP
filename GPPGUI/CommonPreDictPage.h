// CommonPreDictPage.h

#ifndef COMMONPREDICTPAGE_H
#define COMMONPREDICTPAGE_H

#include <QList>
#include <toml++/toml.hpp>
#include "BasePage.h"
#include "NormalTabEntry.h"

namespace fs = std::filesystem;

class CommonPreDictPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonPreDictPage(toml::table& globalConfig, QWidget* parent = nullptr);
    ~CommonPreDictPage() override;

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

#endif // COMMONPREDICTPAGE_H
