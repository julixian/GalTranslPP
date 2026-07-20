#include "AboutDialog.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QFont>
#include <QIcon>
#include <QImage>
#include <QUrl>

#include "ElaIconButton.h"
#include "ElaImageCard.h"
#include "ElaText.h"
#include "ElaToolTip.h"

import GPPVersion;

AboutDialog::AboutDialog(QWidget* parent)
    : ElaDialog(parent)
{
    setWindowTitle(tr("关于"));
    setWindowIcon(QIcon(":/GPPGUI/Resource/images/webIcon.jpeg"));
    setIsFixedSize(true);
    setWindowModality(Qt::ApplicationModal);
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);

    setupUi();
}

void AboutDialog::setDownloadButtonEnabled(bool enabled)
{
    m_downloadButton->setEnabled(enabled);
}

void AboutDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 25, 0, 0);

    QWidget* contentWidget = new QWidget(this);
    QHBoxLayout* contentLayout = new QHBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->addSpacing(30);

    QWidget* pixCardWidget = new QWidget(contentWidget);
    QVBoxLayout* pixCardLayout = new QVBoxLayout(pixCardWidget);
    pixCardLayout->setContentsMargins(0, 0, 0, 0);
    ElaImageCard* pixCard = new ElaImageCard(pixCardWidget);
    pixCard->setFixedSize(60, 60);
    pixCard->setIsPreserveAspectCrop(false);
    pixCard->setCardImage(QImage(":/GPPGUI/Resource/images/webIcon.jpeg"));
    pixCardLayout->addWidget(pixCard);
    pixCardLayout->addStretch();
    contentLayout->addWidget(pixCardWidget);
    contentLayout->addSpacing(30);

    QWidget* textWidget = new QWidget(contentWidget);
    QVBoxLayout* textLayout = new QVBoxLayout(textWidget);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(15);

    ElaText* versionText = new ElaText("GalTransl++ GUI v" + QString::fromUtf8(GPPVERSION), 18, textWidget);
    QFont versionTextFont = versionText->font();
    versionTextFont.setWeight(QFont::Bold);
    versionText->setFont(versionTextFont);
    versionText->setWordWrap(false);
    textLayout->addWidget(versionText);

    ElaText* licenseText = new ElaText("Apache License 2.0", 14, textWidget);
    licenseText->setWordWrap(false);
    textLayout->addWidget(licenseText);

    ElaText* copyrightText = new ElaText(tr("版权所有 © 2025-2026"), 14, textWidget);
    copyrightText->setWordWrap(false);
    textLayout->addWidget(copyrightText);

    QWidget* authorTextWidget = new QWidget(textWidget);
    QHBoxLayout* authorTextLayout = new QHBoxLayout(authorTextWidget);
    authorTextLayout->setContentsMargins(0, 0, 62, 0);
    authorTextLayout->addStretch();
    ElaText* authorText = new ElaText("julixian", 14, textWidget);
    authorText->setWordWrap(false);
    authorTextLayout->addWidget(authorText);
    textLayout->addWidget(authorTextWidget);

    auto createIconButtonWithToolTipFunc = [](ElaIconType::IconName icon, const QString& toolTip,
            QWidget* parent) -> ElaIconButton*
        {
            ElaIconButton* button = new ElaIconButton(icon, parent);
            button->setFixedWidth(40);
            ElaToolTip* buttonToolTip = new ElaToolTip(button);
            buttonToolTip->setToolTip(toolTip);
            return button;
        };

    QWidget* buttonWidget = new QWidget(textWidget);
    QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
    buttonLayout->setContentsMargins(0, 0, 30, 0);
    buttonLayout->addStretch();

    ElaIconButton* jumpToGithubButton = createIconButtonWithToolTipFunc(ElaIconType::Warehouse, tr("跳转到 Github 发布页"), textWidget);
    buttonLayout->addWidget(jumpToGithubButton);
    connect(jumpToGithubButton, &ElaIconButton::clicked, this, []()
        {
            QDesktopServices::openUrl(QUrl("https://github.com/julixian/GalTranslPP/releases"));
        });

    ElaIconButton* checkUpdateButton = createIconButtonWithToolTipFunc(ElaIconType::CheckToSlot, tr("检查更新"), textWidget);
    buttonLayout->addWidget(checkUpdateButton);
    connect(checkUpdateButton, &ElaIconButton::clicked, this, [this]()
        {
            Q_EMIT checkUpdateSignal();
        });

    m_downloadButton = createIconButtonWithToolTipFunc(ElaIconType::Download, tr("下载更新"), textWidget);
    m_downloadButton->setEnabled(false);
    buttonLayout->addWidget(m_downloadButton);
    connect(m_downloadButton, &ElaIconButton::clicked, this, [this]()
        {
            Q_EMIT downloadUpdateSignal();
        });

    textLayout->addWidget(buttonWidget);
    textLayout->addStretch();
    contentLayout->addWidget(textWidget);

    mainLayout->addWidget(contentWidget);
}
