#include "UpdateWidget.h"

#include <QVBoxLayout>
#include "ElaText.h"

import Tool;

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
        "1. 规范化了 agent 模式下的一些缓存行为，修复了超过轮数时无限重试，reconcile 时单文件分割仍尝试查找其它文件缓存 的 bug",
		"2. 额外说明: agent 模式下程序会按顺序自动检索项目根目录中的以下文件，如果找到会增加一个 agentTool 给模型则并停止检索，模型将获得读取此文件的能力:",
        "脚本说明.md",
        "剧情说明.md",
        "设定补充.md",
        "script_info.md",
        "story_info.md",
        "project_note.md",

		"v2.5.0更新",
        "1. 新增实验性的 agent 模式，目前只支持 ForGalTsv/ForNovelTsv。会增加 token 消耗，理论上更推荐单线程使用，且最好中途不要改原文件/分割方式，可以大幅提高翻译一致性。增加了专门的提示词键。",
		"2. 为 ForGalJson 下英文引号的转义增加修复尝试环节",

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
