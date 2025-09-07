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
    ElaText* updateTitle = new ElaText("2025-09-07下午更新", 15, this);
    ElaText* update1 = new ElaText("1、为GenDict模式增加进度条", 13, this);
    update1->setIsWrapAnywhere(true);
    ElaText* update2 = new ElaText("2、优化 API Key 删除逻辑", 13, this);
    update2->setIsWrapAnywhere(true);
    ElaText* update3 = new ElaText("3、增加最大重试次数设置", 13, this);
    update3->setIsWrapAnywhere(true);
    ElaText* update4 = new ElaText("4、增加携带上文数量设置", 13, this);
    update4->setIsWrapAnywhere(true);
    ElaText* update5 = new ElaText("5、修复全缓存命中的情况下 gt_output 输出没有创建递归文件夹的问题", 13, this);
    update5->setIsWrapAnywhere(true);
    ElaText* update6 = new ElaText("6、修复重翻关键词不生效的bug", 13, this);
    update6->setIsWrapAnywhere(true);
    ElaText* update7 = new ElaText("7、增加设置: 允许在项目仍在运行的情况下关闭程序(危险)", 13, this);
    update7->setIsWrapAnywhere(true);
    ElaText* update8 = new ElaText("8、降低了log输出的频率以换取流畅度和速度，"
        "同时将一些相对不重要的log调整为debug级别以避免刷屏", 13, this);
    update8->setIsWrapAnywhere(true);
    ElaText* update9 = new ElaText("9、优化了缓存读取逻辑，单文件分割不会再仅仅因重提就导致缓存全部失效", 13, this);
    update9->setIsWrapAnywhere(true);
    ElaText* update10 = new ElaText("10、优化了日志的输出滚动，现在回看log不会被新log拽回来了", 13, this);
    update10->setIsWrapAnywhere(true);
    ElaText* update11 = new ElaText("11、增加设置: 额度检测", 13, this);
    update11->setIsWrapAnywhere(true);
    ElaText* update12 = new ElaText("12、新增防多开功能", 13, this);
    update12->setIsWrapAnywhere(true);

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
    mainLayout->addWidget(update12);
    mainLayout->addStretch();
}

UpdateWidget::~UpdateWidget()
{
}
