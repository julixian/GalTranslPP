// NormalTabEntry.h

#ifndef NORMALTABENTRY_H
#define NORMALTABENTRY_H

#include <QStackedWidget>
#include <filesystem>
#include "NormalDictModel.h"

namespace fs = std::filesystem;

class ElaPlainTextEdit;
class ElaTableView;

struct NormalTabEntry {
    QStackedWidget* stackedWidget;
    ElaPlainTextEdit* plainTextEdit;
    ElaTableView* tableView;
    NormalDictModel* normalDictModel;
    fs::path dictPath;
};

#endif // NORMALTABENTRY_H