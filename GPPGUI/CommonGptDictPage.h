#ifndef COMMONGPTDICTPAGE_H
#define COMMONGPTDICTPAGE_H

#include <QList>
#include <QStackedWidget>
#include <QSharedPointer>
#include <toml.hpp>
#include <filesystem>
#include "BasePage.h"
#include "GptDictModel.h"

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

class CommonGptDictPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonGptDictPage(toml::ordered_value& globalConfig, QWidget* parent = nullptr);
    ~CommonGptDictPage() override;

Q_SIGNALS:
    void commonDictsChangedSignal();

private:

    void setupUi();

    toml::ordered_value& m_globalConfig;

    QList<GptTabEntry> m_gptTabEntries;
};

#endif // COMMONGPTDICTPAGE_H
