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
    mainLayout->setContentsMargins(5, 10, 5, 5);
    mainLayout->setSpacing(4);

    ElaText* updateTitle = new ElaText("v" + QString::fromUtf8(GPPVERSION) + " 更新", 15, this);
    QStringList updateList = {
        "1. 加回了流式输出选项，支持在所有模式和协议下启用",
		"2. [GUI] 修复了弹出窗口比记忆高度高及主窗口高度变化的 bug",
        "v3.2.3 更新",
        "1. 增加长路径支持",
		"2. 新增 Api name 和 GUI 流式布局，默认行为可在应用设置中改回",
		"3. 高级 Agent 新增选项『允许运行 Python 代码』",
		"4. 其它各种妙妙优化",
    };

    mainLayout->addWidget(updateTitle);
    for (const auto& updateQStr : updateList) {
        ElaText* updateItem = new ElaText(updateQStr, 13, this);
        updateItem->setIsWrapAnywhere(true);
        mainLayout->addWidget(updateItem);
    }

    mainLayout->addStretch();
}
