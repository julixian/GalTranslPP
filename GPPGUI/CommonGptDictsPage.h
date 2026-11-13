#pragma once

#include "BasePage.h"
#include "GptDictModel.h"

#include <QList>
#include <QStackedWidget>
#include <QSharedPointer>
#include <filesystem>
#include "GppTomlFwd.h"

class ElaPlainTextEdit;
class ElaTableView;

namespace fs = std::filesystem;

struct GptTabEntry {
    QWidget* pageMainWidget{};
    QStackedWidget* stackedWidget{};
    ElaPlainTextEdit* plainTextEdit{};
    ElaTableView* tableView{};
    GptDictModel* dictModel{};
    fs::path dictPath;
    std::function<bool(bool)> saveFunc;
    QList<GuiGptDictEntry> withdrawList;
};

class CommonGptDictsPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonGptDictsPage(tomlkit::Item& globalConfig, QWidget* parent = nullptr);

Q_SIGNALS:
    void commonDictsChangedSignal();

private:

    void setupUi();

    tomlkit::Item& m_globalConfig;

    QList<QSharedPointer<GptTabEntry>> m_gptTabEntries;
};
