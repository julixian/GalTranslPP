// CommonPreDictPage.h

#ifndef COMMONPREDICTPAGE_H
#define COMMONPREDICTPAGE_H

#include <QList>
#include <QStackedWidget>
#include <toml++/toml.hpp>
#include <filesystem>
#include "BasePage.h"
#include "NormalDictModel.h"

namespace fs = std::filesystem;

class ElaPlainTextEdit;
class NormalDictModel;

struct PreTabEntry {
    QStackedWidget* stackedWidget;
    ElaPlainTextEdit* plainTextEdit;
    NormalDictModel* normalDictModel;
};

class CommonPreDictPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonPreDictPage(toml::table& globalConfig, QWidget* parent = nullptr);
    ~CommonPreDictPage() override;

public Q_SLOTS:
    void apply2Config();

private:

    QList<NormalDictEntry> readNormalDicts(const fs::path& dictPath);
    QString readNormalDictsStr(const fs::path& dictPath);

    void _setupUI();

    toml::table& _globalConfig;

    std::function<void()> _applyFunc;

    QList<PreTabEntry> _preTabEntries;
};

#endif // COMMONPREDICTPAGE_H
