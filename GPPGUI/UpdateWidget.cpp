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
    ElaText* updateTitle = new ElaText("2025-09-12更新", 15, this);
    ElaText* update1 = new ElaText("1、修复GPT字典保存的一个小bug", 13, this);
    update1->setIsWrapAnywhere(true);
    ElaText* update2 = new ElaText("2、修复优化单文件分割时引出的无法命中缓存bug", 13, this);
    update2->setIsWrapAnywhere(true);
    ElaText* update3 = new ElaText("3、新增默认提示词管理功能", 13, this);
    update3->setIsWrapAnywhere(true);
    ElaText* update4 = new ElaText("4、给两个任务的log输出间增加了两个换行", 13, this);
    update4->setIsWrapAnywhere(true);
    ElaText* update5 = new ElaText("5、优化了翻译解析，新增智能空行检测", 13, this);
    update5->setIsWrapAnywhere(true);
    ElaText* update6 = new ElaText("6、优化了Epub重建的代码", 13, this);
    update6->setIsWrapAnywhere(true);
    ElaText* update7 = new ElaText("7、新增全角半角转化插件(by natsumerinchan)", 13, this);
    update7->setIsWrapAnywhere(true);
    ElaText* update8 = new ElaText("8、新增默认字典管理", 13, this);
    update8->setIsWrapAnywhere(true);
    ElaText* update9 = new ElaText("9、更新ElaWidgetTool版本", 13, this);
    update9->setIsWrapAnywhere(true);
    ElaText* update10 = new ElaText("10、修复插件开启状态和顺序未保存的bug", 13, this);
    update10->setIsWrapAnywhere(true);
    ElaText* update11 = new ElaText("11、将字典的正则实现从boost换成icu，实现字符级别的匹配", 13, this);
    update11->setIsWrapAnywhere(true);
    ElaText* update12 = new ElaText("12、现在保存时会保留创建了但未填写的空词条", 13, this);
    update12->setIsWrapAnywhere(true);
    ElaText* update13 = new ElaText("13、将插件设置和文件输出设置时的阴影效果作用到主窗口上", 13, this);
    update13->setIsWrapAnywhere(true);
    ElaText* update14 = new ElaText("14、开始翻译界面增加更多信息", 13, this);
    update14->setIsWrapAnywhere(true);
    ElaText* update15 = new ElaText("15、修复文件名和文件大小设置错误的bug", 13, this);
    update15->setIsWrapAnywhere(true);
    ElaText* update16 = new ElaText("16、尝试为apikey添加流式选项", 13, this);
    update16->setIsWrapAnywhere(true);

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
    mainLayout->addWidget(update13);
    mainLayout->addWidget(update14);
    mainLayout->addWidget(update15);
    mainLayout->addWidget(update16);
    mainLayout->addStretch();
}

UpdateWidget::~UpdateWidget()
{
}
