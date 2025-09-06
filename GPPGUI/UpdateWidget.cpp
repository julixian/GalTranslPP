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
    ElaText* updateTitle = new ElaText("2025-09-07更新", 15, this);
    ElaText* update1 = new ElaText("1、修复翻译过程结束后字典被清空的bug", 13, this);
    update1->setIsWrapAnywhere(true);
    ElaText* update2 = new ElaText("2、支持 gt_input 内嵌套文件夹读取", 13, this);
    update2->setIsWrapAnywhere(true);
    ElaText* update3 = new ElaText("3、修复所有一般文本框的多加换行问题", 13, this);
    update3->setIsWrapAnywhere(true);
    ElaText* update4 = new ElaText("4、将重翻关键字设定由一般设置移动到设置分析", 13, this);
    update4->setIsWrapAnywhere(true);
    ElaText* update5 = new ElaText("5、修复输出带原文选项无法取消勾选的bug", 13, this);
    update5->setIsWrapAnywhere(true);
    ElaText* update6 = new ElaText("6、修复重开后立刻关闭下次再打开缓存会减少的问题", 13, this);
    update6->setIsWrapAnywhere(true);
    ElaText* update7 = new ElaText("7、优化了 项目GPT字典 纯文本模式的输出格式", 13, this);
    update7->setIsWrapAnywhere(true);
    ElaText* update8 = new ElaText("8、保存项目配置后自动刷新人名表和字典", 13, this);
    update8->setIsWrapAnywhere(true);
    ElaText* update9 = new ElaText("9、将控制刷新的全局设置更改为『DumpName/GenDict任务完成后自动刷新人名表和字典』，并默认开启", 13, this);
    update9->setIsWrapAnywhere(true);
    ElaText* update10 = new ElaText("10、追加了日志输出文本框多加换行的修复", 13, this);
    update10->setIsWrapAnywhere(true);
    ElaText* update11 = new ElaText("11、修复了当使用纯文本模式且字典格式不正确时使用DumpName/GenDict程序会崩溃的问题，"
        "并新增非法提示", 13, this);
    update11->setIsWrapAnywhere(true);

    mainLayout->addWidget(updateTitle);
    mainLayout->addWidget(update1);
    mainLayout->addWidget(update2);
    mainLayout->addWidget(update3);
    mainLayout->addWidget(update4);
    mainLayout->addWidget(update5);
    mainLayout->addWidget(update6);
    mainLayout->addWidget(update7);
    mainLayout->addWidget(update8);
    mainLayout->addWidget(update9);
    mainLayout->addWidget(update10);
    mainLayout->addWidget(update11);
    mainLayout->addStretch();
}

UpdateWidget::~UpdateWidget()
{
}
