// NormalTabEntry.h

#ifndef NORMALTABENTRY_H
#define NORMALTABENTRY_H

#include <QList>
#include <QSharedPointer>
#include <QStackedWidget>
#include <filesystem>
#include "NormalDictModel.h"

namespace fs = std::filesystem;

class ElaPlainTextEdit;
class ElaTableView;

struct NormalTabEntry {
    QWidget* pageMainWidget;
    QStackedWidget* stackedWidget;
    ElaPlainTextEdit* plainTextEdit;
    ElaTableView* tableView;
    NormalDictModel* dictModel;
    fs::path dictPath;
    QSharedPointer<QList<NormalDictEntry>> withdrawList;
    NormalTabEntry() : withdrawList(new QList<NormalDictEntry>) {}
};

#endif // NORMALTABENTRY_H