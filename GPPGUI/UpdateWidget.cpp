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
        "1. 将 toml 解析器换为自研的 tomlkitpp，大幅优化了格式保留。"
        "现在对于 GUI，只要是在数组/表内部的注释，即使后方没有非注释的项也能保留了。"
        "对于 CLI，写入问题概览的文件时间不会再修改设置文件的其余部分格式了。",
    };

    mainLayout->addWidget(updateTitle);
    for (const auto& updateQStr : updateList) {
        ElaText* updateItem = new ElaText(updateQStr, 13, this);
        updateItem->setIsWrapAnywhere(true);
        mainLayout->addWidget(updateItem);
    }

    mainLayout->addStretch();
}
