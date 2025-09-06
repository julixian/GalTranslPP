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
    ElaText* updateTitle = new ElaText("2025-09-06更新", 15, this);
    ElaText* update1 = new ElaText("1、初版测试发布", 13, this);
    update1->setIsWrapAnywhere(true);

    mainLayout->addWidget(updateTitle);
    mainLayout->addWidget(update1);
    mainLayout->addStretch();
}

UpdateWidget::~UpdateWidget()
{
}
