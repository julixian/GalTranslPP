#include "UpdateWidget.h"

#include <QVBoxLayout>
#include "ElaText.h"

import GPPVersion;

using namespace gpp;

UpdateWidget::UpdateWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 260);
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSizeConstraint(QLayout::SetMaximumSize);
    mainLayout->setContentsMargins(5, 10, 5, 0);
    mainLayout->setSpacing(4);

    ElaText* updateTitle = new ElaText("v" + QString::fromUtf8(GPPVERSION) + " 更新", 15, this);
    QStringList updateList = {
        "1. 继续优化提示词，注意一下我现在基本上都是在优化 MyCustom，基本上都是针对 gemini-4-argon 的日译中问题，有需要的请手动替换",
        "2. 优化字典发送格式，这会导致旧项目 Agent 模式的 term_ledger.json 术语表失效，请将此文件删除或将其中的 \"target_term\" 键改为 \"dst\"",
        "3. 修复自 3.2.4 起由于更新退出杀 Python 子进程导致的 Updater 无法生效的 bug",
    };

    mainLayout->addWidget(updateTitle);
    for (const auto& updateQStr : updateList) {
        ElaText* updateItem = new ElaText(updateQStr, 13, this);
        updateItem->setIsWrapAnywhere(true);
        mainLayout->addWidget(updateItem);
    }

    mainLayout->addStretch();
}
