#include "HomePage.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

#include "ElaAcrylicUrlCard.h"
#include "ElaFlowLayout.h"
#include "ElaImageCard.h"
#include "ElaMenu.h"
#include "ElaMessageBar.h"
#include "ElaNavigationRouter.h"
#include "ElaPopularCard.h"
#include "ElaScrollArea.h"
#include "ElaText.h"
#include "ElaToolTip.h"

HomePage::HomePage(QWidget* parent)
    : BasePage(parent)
{
    // 预览窗口标题
    setWindowTitle("主页");

    setTitleVisible(false);
    setContentsMargins(2, 2, 0, 0);
    // 标题卡片区域
    ElaText* titleText = new ElaText("GalTransl++ GUI", this);
    
    titleText->setTextPixelSize(35);
    ElaText* descriptionText = new ElaText("Translate your favorite galgames", this);
    descriptionText->setTextPixelSize(18);

    QVBoxLayout* titleLayout = new QVBoxLayout();
    titleLayout->setContentsMargins(30, 60, 0, 0);
    titleLayout->addWidget(titleText);
    titleLayout->addWidget(descriptionText);

    ElaImageCard* backgroundCard = new ElaImageCard(this);
    backgroundCard->setBorderRadius(10);
    backgroundCard->setFixedHeight(400);
    backgroundCard->setMaximumAspectRatio(1.7);
    backgroundCard->setCardImage(QImage(":/GPPGUI/Resource/Image/homebackground.png"));

    ElaAcrylicUrlCard* urlCard1 = new ElaAcrylicUrlCard(this);
    urlCard1->setCardPixmapSize(QSize(62, 62));
    urlCard1->setFixedSize(225, 225);
    urlCard1->setTitlePixelSize(17);
    urlCard1->setTitleSpacing(25);
    urlCard1->setSubTitleSpacing(13);
    urlCard1->setUrl("https://github.com/julixian/GalTranslPP");
    urlCard1->setCardPixmap(QPixmap(":/GPPGUI/Resource/Image/github.png"));
    urlCard1->setTitle("GalTransl++\nGithub");
    urlCard1->setSubTitle("AI 自动化翻译解决方案");
    ElaToolTip* urlCard1ToolTip = new ElaToolTip(urlCard1);
    urlCard1ToolTip->setToolTip("https://github.com/julixian/GalTranslPP");
    ElaAcrylicUrlCard* urlCard2 = new ElaAcrylicUrlCard(this);
    urlCard2->setCardPixmapSize(QSize(62, 62));
    urlCard2->setFixedSize(225, 225);
    urlCard2->setTitlePixelSize(17);
    urlCard2->setTitleSpacing(25);
    urlCard2->setSubTitleSpacing(13);
    urlCard2->setUrl("https://julixian-siw.worldsystem.net/");
    urlCard2->setCardPixmap(QPixmap(":/GPPGUI/Resource/Image/webIcon.jpeg"));
    urlCard2->setTitle("julixian");
    urlCard2->setSubTitle("LAMUNATION FOREVER！！！");
    ElaToolTip* urlCard2ToolTip = new ElaToolTip(urlCard2);
    urlCard2ToolTip->setToolTip("https://julixian-siw.worldsystem.net/");

    ElaScrollArea* cardScrollArea = new ElaScrollArea(this);
    cardScrollArea->setWidgetResizable(true);
    cardScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    cardScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    cardScrollArea->setIsGrabGesture(true, 0);
    cardScrollArea->setIsOverShoot(Qt::Horizontal, true);
    QWidget* cardScrollAreaWidget = new QWidget(this);
    cardScrollAreaWidget->setStyleSheet("background-color:transparent;");
    cardScrollArea->setWidget(cardScrollAreaWidget);
    QHBoxLayout* urlCardLayout = new QHBoxLayout();
    urlCardLayout->setSpacing(15);
    urlCardLayout->setContentsMargins(30, 0, 0, 6);
    urlCardLayout->addWidget(urlCard1);
    urlCardLayout->addWidget(urlCard2);
    urlCardLayout->addStretch();
    QVBoxLayout* cardScrollAreaWidgetLayout = new QVBoxLayout(cardScrollAreaWidget);
    cardScrollAreaWidgetLayout->setContentsMargins(0, 0, 0, 0);
    cardScrollAreaWidgetLayout->addStretch();
    cardScrollAreaWidgetLayout->addLayout(urlCardLayout);

    QVBoxLayout* backgroundLayout = new QVBoxLayout(backgroundCard);
    backgroundLayout->setContentsMargins(0, 0, 0, 0);
    backgroundLayout->addLayout(titleLayout);
    backgroundLayout->addWidget(cardScrollArea);

    // 推荐卡片
    ElaText* flowText = new ElaText("Useful Tools", this);
    flowText->setTextPixelSize(20);
    QHBoxLayout* flowTextLayout = new QHBoxLayout();
    flowTextLayout->setContentsMargins(33, 0, 0, 0);
    flowTextLayout->addWidget(flowText);
    // ElaFlowLayout
    ElaPopularCard* homeCard = new ElaPopularCard(this);
    connect(homeCard, &ElaPopularCard::popularCardButtonClicked, this, [=]() {
        QDesktopServices::openUrl(QUrl("https://github.com/satan53x/SExtractor"));
    });
    homeCard->setCardPixmap(QPixmap(":/GPPGUI/Resource/Image/satan53x.jpg"));
    homeCard->setTitle("SExTractor");
    homeCard->setSubTitle("草佬味大，无需多盐");
    homeCard->setInteractiveTips("Satan53x");
    homeCard->setDetailedText("从GalGame脚本提取和导入文本");
    homeCard->setCardFloatPixmap(QPixmap(":/Resource/Image/IARC_7+.svg.png"));

    ElaPopularCard* homeCard1 = new ElaPopularCard(this);
    connect(homeCard1, &ElaPopularCard::popularCardButtonClicked, this, [=]() {
        QDesktopServices::openUrl(QUrl("https://github.com/crskycode/GARbro"));
    });
    homeCard1->setTitle("GARbro");
    homeCard1->setSubTitle("无敌了");
    homeCard1->setCardPixmap(QPixmap(":/GPPGUI/Resource/Image/gar.png"));
    homeCard1->setInteractiveTips("morkt");
    homeCard1->setDetailedText("神一样的解封包工具");
    homeCard1->setCardFloatPixmap(QPixmap(":/GPPGUI/Resource/Image/IARC_7+.svg.png"));

    ElaPopularCard* homeCard2 = new ElaPopularCard(this);
    connect(homeCard2, &ElaPopularCard::popularCardButtonClicked, this, [=]() {
        QDesktopServices::openUrl(QUrl("https://github.com/arcusmaximus/VNTranslationTools"));
    });
    homeCard2->setTitle("VNTranslationTools");
    homeCard2->setSubTitle("能少用就少用");
    homeCard2->setCardPixmap(QPixmap(":/GPPGUI/Resource/Image/vnt.png"));
    homeCard2->setInteractiveTips("arcusmaximus");
    homeCard2->setDetailedText("萌新拯救者一般的文本提取与回封工具");
    homeCard2->setCardFloatPixmap(QPixmap(":/GPPGUI/Resource/Image/IARC_7+.svg.png"));

    ElaPopularCard* homeCard3 = new ElaPopularCard(this);
    connect(homeCard3, &ElaPopularCard::popularCardButtonClicked, this, [=]() {
        QDesktopServices::openUrl(QUrl("https://github.com/shangjiaxuan/Crass-source"));
    });
    homeCard3->setTitle("Crass");
    homeCard3->setSubTitle("专治老游戏");
    homeCard3->setCardPixmap(QPixmap(":/GPPGUI/Resource/Image/crass.png"));
    homeCard3->setInteractiveTips("痴漢公賊");
    homeCard3->setDetailedText("早期Galgame解包工具，多看看它的说明文档");
    homeCard3->setCardFloatPixmap(QPixmap(":/GPPGUI/Resource/Image/IARC_7+.svg.png"));

    ElaPopularCard* homeCard4 = new ElaPopularCard(this);
    connect(homeCard4, &ElaPopularCard::popularCardButtonClicked, this, [=]() {
        QDesktopServices::openUrl(QUrl("https://www.sublimetext.com/"));
    });
    homeCard4->setTitle("Sublime");
    homeCard4->setSubTitle("和Em互补长短");
    homeCard4->setCardPixmap(QPixmap(":/GPPGUI/Resource/Image/sublime.png"));
    homeCard4->setInteractiveTips("Sublime HQ");
    homeCard4->setDetailedText("高亮很好使，但编码转换不如emeditor");
    homeCard4->setCardFloatPixmap(QPixmap(":/GPPGUI/Resource/Image/IARC_7+.svg.png"));

    ElaPopularCard* homeCard5 = new ElaPopularCard(this);
    connect(homeCard5, &ElaPopularCard::popularCardButtonClicked, this, [=]() {
        QDesktopServices::openUrl(QUrl("https://github.com/ZQF-ReVN"));
    });
    homeCard5->setTitle("ReVN");
    homeCard5->setSubTitle("拜见祖师爷");
    homeCard5->setCardPixmap(QPixmap(":/GPPGUI/Resource/Image/revn.png"));
    homeCard5->setInteractiveTips("Dir-A");
    homeCard5->setDetailedText("别说你是搞机翻的");
    homeCard5->setCardFloatPixmap(QPixmap(":/GPPGUI/Resource/Image/IARC_7+.svg.png"));

    ElaFlowLayout* flowLayout = new ElaFlowLayout(0, 5, 5);
    flowLayout->setContentsMargins(30, 0, 0, 0);
    flowLayout->setIsAnimation(true);
    flowLayout->addWidget(homeCard);
    flowLayout->addWidget(homeCard1);
    flowLayout->addWidget(homeCard2);
    flowLayout->addWidget(homeCard3);
    flowLayout->addWidget(homeCard4);
    flowLayout->addWidget(homeCard5);

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setWindowTitle("主页");
    QVBoxLayout* centerVLayout = new QVBoxLayout(centralWidget);
    centerVLayout->setSpacing(0);
    centerVLayout->setContentsMargins(0, 0, 0, 0);
    centerVLayout->addWidget(backgroundCard);
    centerVLayout->addSpacing(20);
    centerVLayout->addLayout(flowTextLayout);
    centerVLayout->addSpacing(10);
    centerVLayout->addLayout(flowLayout);
    centerVLayout->addStretch();
    addCentralWidget(centralWidget);
}

HomePage::~HomePage()
{
}
