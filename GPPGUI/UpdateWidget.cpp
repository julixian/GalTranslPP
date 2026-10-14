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
        "1. Log 准确性修正: 本地上下文 -> 上下文预估",
		"2. Agent 建议现在会写在 otherinfo[\"gpp.agent.suggestion\"] 里而不是 problems 里了",
		"3. 新增设置『保留上一次的 otherinfo』",
        "4. 新增翻译引擎 ForNovelJson，同时时隔一年再度将 Json 系列作为默认选项"
        "（时代变了，当初弃 Json 默认 TSV 是因为后者更易读更省 token，但当前的翻译模型之神 Gemini-4-argon 喜欢拒答"
        " TSV 格式和英文提示词，且无法像 3.1-pro 那样开启改善拒答）\n"
        "注: 这可能会使你现有项目 Prompt 中的内容失效，请留意！",
        "5. 删去使用批次文本协议的普通 Agent，现在的 Agent 即是上一版本的 高级 Agent"
        "（如果你的模型笨到无法使用 高级 Agent，那还不如直接上普通批次翻译，在批次翻译上使用 Agent 工具显得非常不上不下且维护困难，故删）",
        "6. Json 系列也可以使用 Agent 了",
		"7. 暂停后不再收集网络错误。api 新增基础设置『快速暂停』，开启后会立即中断网络及工具请求，不会等待批次结束",
    };

    mainLayout->addWidget(updateTitle);
    for (const auto& updateQStr : updateList) {
        ElaText* updateItem = new ElaText(updateQStr, 12, this);
        updateItem->setIsWrapAnywhere(true);
        mainLayout->addWidget(updateItem);
    }

    mainLayout->addStretch();
}
