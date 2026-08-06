#include "UpdateWidget.h"

#include <QVBoxLayout>
#include "ElaText.h"

import GPPVersion;

UpdateWidget::UpdateWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 260);
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSizeConstraint(QLayout::SetMaximumSize);
    mainLayout->setContentsMargins(5, 10, 5, 5);
    mainLayout->setSpacing(4);

    ElaText* updateTitle = new ElaText("v" + QString::fromUtf8(GPPVERSION) + " 更新", 15, this);
    QStringList updateList = {
		"1. 修复问题概览在 翻译模式 和 Rebuild 中顺序不一样的 bug",
        "v3.0.5 更新",
        "1. 修复 重复块引用 在 Agent 模式下失效的 bug",
        "2. [GUI] 补上人名替换表纯文本的高亮",
    };

    mainLayout->addWidget(updateTitle);
    for (const auto& str : updateList) {
        ElaText* updateItem = new ElaText(str, 13, this);
        updateItem->setIsWrapAnywhere(true);
        mainLayout->addWidget(updateItem);
    }

    mainLayout->addStretch();
}
