#ifndef NORMALTABENTRY_H
#define NORMALTABENTRY_H

#include "NormalDictModel.h"
#include <QList>
#include <QSharedPointer>
#include <QStackedWidget>
#include <filesystem>

namespace fs = std::filesystem;

class ElaPlainTextEdit;
class ElaTableView;

struct NormalTabEntry {
    QWidget* pageMainWidget{};
    QStackedWidget* stackedWidget{};
    ElaPlainTextEdit* plainTextEdit{};
    ElaTableView* tableView{};
    NormalDictModel* dictModel{};
    fs::path dictPath;
    std::function<bool(bool)> saveFunc;
    QSharedPointer<QList<NormalDictEntry>> withdrawList;
    NormalTabEntry() : withdrawList(new QList<NormalDictEntry>) {}
};

#endif
