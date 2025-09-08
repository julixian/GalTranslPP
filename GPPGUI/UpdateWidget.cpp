#include "UpdateWidget.h"

#include <QVBoxLayout>

#include "ElaText.h"

UpdateWidget::UpdateWidget(QWidget* parent)
    : QWidget{parent}
{
    setMinimumSize(200, 260);
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSizeConstraint(QLayout::SetMaximumSize);
    mainLayout->setContentsMargins(5, 10, 5, 5);
    mainLayout->setSpacing(4);
    ElaText* updateTitle = new ElaText("2025-09-08更新", 15, this);
    ElaText* update1 = new ElaText("1、继续优化额度检测", 13, this);
    update1->setIsWrapAnywhere(true);
    ElaText* update2 = new ElaText("2、继续优化缓存读取逻辑", 13, this);
    update2->setIsWrapAnywhere(true);
    ElaText* update3 = new ElaText("3、优化GPT字典保存解析逻辑", 13, this);
    update3->setIsWrapAnywhere(true);
    ElaText* update4 = new ElaText("4、新增项目提示词设置", 13, this);
    update4->setIsWrapAnywhere(true);
    ElaText* update5 = new ElaText("5、新增更新检测(仅GUI)", 13, this);
    update5->setIsWrapAnywhere(true);
    ElaText* update6 = new ElaText("6、修复主题切换保存的bug", 13, this);
    update6->setIsWrapAnywhere(true);

    mainLayout->addWidget(updateTitle);
    mainLayout->addWidget(update1);
    mainLayout->addWidget(update2);
    mainLayout->addWidget(update3);
    mainLayout->addWidget(update4);
    mainLayout->addWidget(update5);
    mainLayout->addWidget(update6);
    mainLayout->addStretch();
}

UpdateWidget::~UpdateWidget()
{
}
