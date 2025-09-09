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
    ElaText* update1 = new ElaText("1、修复GPT字典保存的一个小bug", 13, this);
    update1->setIsWrapAnywhere(true);
    ElaText* update2 = new ElaText("2、修复优化单文件分割时引出的Rebuild无法命中缓存bug", 13, this);
    update2->setIsWrapAnywhere(true);
    ElaText* update3 = new ElaText("3、新增默认提示词管理功能", 13, this);
    update3->setIsWrapAnywhere(true);
    ElaText* update4 = new ElaText("4、给两个任务的log输出间增加了两个换行", 13, this);
    update4->setIsWrapAnywhere(true);
    ElaText* update5 = new ElaText("5、优化了翻译解析，新增智能空行检测", 13, this);
    update5->setIsWrapAnywhere(true);

    mainLayout->addWidget(updateTitle);
    mainLayout->addWidget(update1);
    mainLayout->addWidget(update2);
    mainLayout->addWidget(update3);
    mainLayout->addWidget(update4);
    mainLayout->addWidget(update5);
    mainLayout->addStretch();
}

UpdateWidget::~UpdateWidget()
{
}
