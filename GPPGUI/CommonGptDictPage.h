// CommonGptDictPage.h

#ifndef COMMONGPTDICTPAGE_H
#define COMMONGPTDICTPAGE_H

#include <QList>
#include <QStackedWidget>
#include <QSharedPointer>
#include <toml++/toml.hpp>
#include <filesystem>
#include "BasePage.h"
#include "DictionaryModel.h"

class ElaPlainTextEdit;
class ElaTableView;

namespace fs = std::filesystem;

struct GptTabEntry {
    QWidget* pageMainWidget;
    QStackedWidget* stackedWidget;
    ElaPlainTextEdit* plainTextEdit;
    ElaTableView* tableView;
    DictionaryModel* dictModel;
    fs::path dictPath;
    QSharedPointer<QList<DictionaryEntry>> withdrawList;
    GptTabEntry() : withdrawList(new QList<DictionaryEntry>){}
};

class CommonGptDictPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonGptDictPage(toml::table& globalConfig, QWidget* parent = nullptr);
    ~CommonGptDictPage() override;

Q_SIGNALS:
    void commonDictsChanged();

private:

    QList<DictionaryEntry> readGptDicts(const fs::path& dictPath);
    QString readGptDictsStr(const fs::path& dictPath);

    void _setupUI();

    toml::table& _globalConfig;

    QList<GptTabEntry> _gptTabEntries;
    QWidget* _mainWindow;
};

#endif // COMMONGPTDICTPAGE_H
