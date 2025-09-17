#include "AboutDialog.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QVBoxLayout>

#include "ElaImageCard.h"
#include "ElaMenuBar.h"
#include "ElaText.h"

import Tool;

AboutDialog::AboutDialog(QWidget* parent)
    : ElaDialog(parent)
{
    setWindowTitle("关于..");
    setWindowIcon(QIcon(":/GPPGUI/Resource/Image/webIcon.jpeg"));
    this->setIsFixedSize(true);
    setWindowModality(Qt::ApplicationModal);
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    ElaImageCard* pixCard = new ElaImageCard(this);
    pixCard->setFixedSize(60, 60);
    pixCard->setIsPreserveAspectCrop(false);
    pixCard->setCardImage(QImage(":/GPPGUI/Resource/Image/webIcon.jpeg"));

    QVBoxLayout* pixCardLayout = new QVBoxLayout();
    pixCardLayout->addWidget(pixCard);
    pixCardLayout->addStretch();

    ElaText* versionText = new ElaText("GalTransl++ GUI v" + QString::fromStdString(GPPVERSION), this);
    QFont versionTextFont = versionText->font();
    versionTextFont.setWeight(QFont::Bold);
    versionText->setFont(versionTextFont);
    versionText->setWordWrap(false);
    versionText->setTextPixelSize(18);

    ElaText* licenseText = new ElaText("Apache License 2.0", this);
    licenseText->setWordWrap(false);
    licenseText->setTextPixelSize(14);
    ElaText* copyrightText = new ElaText("版权所有 © 2025 julixian", this);
    copyrightText->setWordWrap(false);
    copyrightText->setTextPixelSize(14);

    ElaMenuBar* menuBar = new ElaMenuBar(this);
    QAction* checkUpdateAction = menuBar->addElaIconAction(ElaIconType::CheckToSlot, "检查更新");
    connect(checkUpdateAction, &QAction::triggered, this, [=]()
        {
            Q_EMIT checkUpdateSignal();
        });
    QHBoxLayout* checkUpdateLayout = new QHBoxLayout();
    checkUpdateLayout->addStretch();
    checkUpdateLayout->addWidget(menuBar);

    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->setSpacing(15);
    textLayout->addWidget(versionText);
    textLayout->addWidget(licenseText);
    textLayout->addWidget(copyrightText);
    textLayout->addLayout(checkUpdateLayout);
    textLayout->addStretch();

    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->addSpacing(30);
    contentLayout->addLayout(pixCardLayout);
    contentLayout->addSpacing(30);
    contentLayout->addLayout(textLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 25, 0, 0);
    mainLayout->addLayout(contentLayout);
}

AboutDialog::~AboutDialog()
{
}
