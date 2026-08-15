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
        "1. Api 报错检测和 Agent log 优化",
		"2. 提示词优化",
		"3. 连续引用复用在 Rebuild 模式下也会生效了，暨引用句的缓存也会绑定在被引用句上，单独改引用句缓存会被刷掉",
        "4. CachePart 新增 Index 和 FileName，对应条件对象指代名 index 和 filename",
        "5. 翻译中途暂停的情况下 问题概览/Agent建议 将仅输出保存过缓存的文件中的问题，不再一并输出旧缓存问题",
		"6. Py/Lua 接口细节变更",
        "7. 性能优化",
        "8. [GUI] 翻译过程中文件进度中的卡片顺序现在会保持固定了",
        "9. [GUI] 将启动时每个项目的挂载初始化延迟到第一次点击项目的时候进行，这样哪怕挂多个项目启动速度也不会太慢，代价是第一次点击某个项目的时候会卡一下初始化，可在『应用设置——延迟项目初始化』中恢复原来的行为",
    };

    mainLayout->addWidget(updateTitle);
    for (const auto& str : updateList) {
        ElaText* updateItem = new ElaText(str, 13, this);
        updateItem->setIsWrapAnywhere(true);
        mainLayout->addWidget(updateItem);
    }

    mainLayout->addStretch();
}
