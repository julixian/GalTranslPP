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
        "1. [GUI] 含问题概览的 log 不会再被 GUI 显示截断了",
		"2. NormalJson 新增设置『输出带引用信息』",
		"3. api 新增设置『使用系统代理』",
        "v3.0.2 更新",
		"1. [GUI] 新增特性: 更新 ProblemOverview.json/toml 后未导入并直接开始翻译会先收到提示",
		"v3.0.1 更新",
		"1. 修复翻译时某个线程抛出异常未通知其它线程导致结束缓慢的 bug",
		"v3.0.0 更新",
        "1. 配置、缓存、条件全面升级（不兼容旧版）",
        "2. 重构 API 池：支持多协议、多 Key、模型测试及自定义请求",
        "3. Agent 翻译与字典审校从实验性功能合并到正式功能",
        "4. 字典支持多条件、弹窗编辑、搜索、多选和拖拽排序",
        "5. GUI 增加 TOML/JSON 高亮并重做多处设置页",
        "6. Lua 迁移至 LuaBridge3，更新 Python/NLP 与插件示例",
    };

    mainLayout->addWidget(updateTitle);
    for (const auto& str : updateList) {
        ElaText* updateItem = new ElaText(str, 13, this);
        updateItem->setIsWrapAnywhere(true);
        mainLayout->addWidget(updateItem);
    }

    mainLayout->addStretch();
}
