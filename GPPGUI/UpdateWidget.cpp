#include "UpdateWidget.h"

#include <QVBoxLayout>
#include "ElaText.h"

import GPPDefines;

UpdateWidget::UpdateWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 260);
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSizeConstraint(QLayout::SetMaximumSize);
    mainLayout->setContentsMargins(5, 10, 5, 5);
    mainLayout->setSpacing(4);

    ElaText* updateTitle = new ElaText("v" + QString::fromStdString(GPPVERSION) + " 更新", 15, this);
    QStringList updateList = {
        "1. 修正时间区域记录。",
        "2. [GUI] 句子完成详情界面错误数改为与成功时间同类语义的总错误计数",
        "3. 优化预计完成时间算法",
        "v2.6.6 更新",
		"1. [GUI] 修复创建/删除项目时闪退的 bug",
		"v2.6.5 更新",
		"1. [GUI] 换回高版本 QT 风格",
		"v2.6.4 更新",
        "1. NameTrans 支持多线程",
    };

    mainLayout->addWidget(updateTitle);
    for (const auto& str : updateList) {
        ElaText* updateItem = new ElaText(str, 13, this);
        updateItem->setIsWrapAnywhere(true);
        mainLayout->addWidget(updateItem);
    }
    
    mainLayout->addStretch();
}

UpdateWidget::~UpdateWidget()
{

}
