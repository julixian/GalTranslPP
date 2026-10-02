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
		"1. claude 协议现在默认会在请求体中加一个 max_tokens = 8192(claude-3-5-)/16384(其它) 并自动将 system 提示词上提到请求体中",
        "2. 完善思考设置，新增更多等级及模型判断逻辑等",
        "3. 为 fallback 切换下一个 apikey 添加 10s 的时间阈值",
        "4. GUI 模型测试成功时会附带完整 json 返回体了",
        "5. 删除设置『携带上文数量』",
        "6. 优化报错信息",
        "7. Agent 模式删除 字典审校 Agent，让开发集中在优化 TransAgent 上。新增『高级 Agent』模式及相关配置，以期适配各协议各模型最先进的 Api 功能，连带新增了只在此模式下生效的相关 Api 设置",
    };

    mainLayout->addWidget(updateTitle);
    for (const auto& updateQStr : updateList) {
        ElaText* updateItem = new ElaText(updateQStr, 13, this);
        updateItem->setIsWrapAnywhere(true);
        mainLayout->addWidget(updateItem);
    }

    mainLayout->addStretch();
}
