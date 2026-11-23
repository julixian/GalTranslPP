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
        "1. 移除原有的回复预填充式改善拒答，新增挂载在「智能重试」下的「图片替换输入（改善拒答）」选项。开启后智能重试节奏为「第 3 次切换图片替换 → 第 4 次对半拆分 → 第 5 次清空上下文并沿用图片替换」（未开启图片替换时仍保持第 3 次折半、第 4 次清空上下文）。",
        "2. [GUI] 项目管理新增二级可折叠分类：支持在「项目管理」下新建、重命名、排序和删除分类，支持右键移动以及直接鼠标拖拽项目跨分类移动与排序，并保存分类折叠状态。",
        "3. [GUI] 左侧导航栏支持拖拽调宽：在展开模式下可拖动右侧边缘调节宽度。",
        "4. [GUI] 优化延迟项目初始化体验：项目各子设置页改为按需懒加载，显著减轻首次进入项目时的界面卡顿。",
    };

    mainLayout->addWidget(updateTitle);
    for (const auto& updateQStr : updateList) {
        ElaText* updateItem = new ElaText(updateQStr, 13, this);
        updateItem->setIsWrapAnywhere(true);
        mainLayout->addWidget(updateItem);
    }

    mainLayout->addStretch();
}
