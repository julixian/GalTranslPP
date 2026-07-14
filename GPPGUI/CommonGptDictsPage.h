#ifndef COMMONGPTDICTSPAGE_H
#define COMMONGPTDICTSPAGE_H

#include "BasePage.h"
#include "GptDictModel.h"

#include <QList>
#include <QStackedWidget>
#include <QSharedPointer>
#include <filesystem>
#include <toml.hpp>

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
    std::function<bool(bool)>saveFunc;
    QSharedPointer<QList<GptDictEntry>> withdrawList;
    GptTabEntry() : withdrawList(new QList<GptDictEntry>){}
};

class CommonGptDictsPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonGptDictsPage(toml::ordered_value& globalConfig, QWidget* parent = nullptr);

Q_SIGNALS:
    void commonDictsChangedSignal();

private:

    void setupUi();

    toml::ordered_value& m_globalConfig;

    QList<GptTabEntry> m_gptTabEntries;
};

#endif
