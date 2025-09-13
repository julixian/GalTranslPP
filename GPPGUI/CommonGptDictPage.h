// CommonGptDictPage.h

#ifndef COMMONGPTDICTPAGE_H
#define COMMONGPTDICTPAGE_H

#include <QList>
#include <QStackedWidget>
#include <toml++/toml.hpp>
#include <filesystem>
#include "BasePage.h"
#include "DictionaryModel.h"

class ElaPlainTextEdit;
class ElaTableView;

namespace fs = std::filesystem;

struct GptTabEntry {
    QStackedWidget* stackedWidget;
    ElaPlainTextEdit* plainTextEdit;
    ElaTableView* tableView;
    DictionaryModel* normalDictModel;
    fs::path dictPath;
};

class CommonGptDictPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonGptDictPage(toml::table& globalConfig, QWidget* parent = nullptr);
    ~CommonGptDictPage() override;

Q_SIGNALS:
    void commonDictsChanged();

public Q_SLOTS:
    void apply2Config();

private:

    QList<DictionaryEntry> readGptDicts(const fs::path& dictPath);
    QString readGptDictsStr(const fs::path& dictPath);

    void _setupUI();

    toml::table& _globalConfig;

    std::function<void()> _applyFunc;

    QList<GptTabEntry> _gptTabEntries;
    QWidget* _mainWindow;
};

#endif // COMMONGPTDICTPAGE_H
