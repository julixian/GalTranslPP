#include "HomePage.h"

#include <array>
#include <optional>

#include <QDesktopServices>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QIcon>
#include <QUrl>

#include "ElaAcrylicUrlCard.h"
#include "ElaFlowLayout.h"
#include "ElaImageCard.h"
#include "ElaPopularCard.h"
#include "ElaScrollArea.h"
#include "ElaText.h"
#include "ElaToolTip.h"

namespace
{
    struct DefaultPopularCard {
        const char* url;
        const char* title;
        const char* subTitle;
        const char* pixmap;
        const char* interactiveTips;
        const char* detailedText;
        const char* floatPixmap;
    };

    constexpr std::array<DefaultPopularCard, 6> DefaultPopularCards = {
        DefaultPopularCard{
            "https://github.com/satan53x/SExtractor",
            "SExTractor",
            "草佬味大，无需多盐",
            ":/GPPGUI/Resource/images/satan53x.jpg",
            "Satan53x",
            "从GalGame脚本提取和导入文本",
            ":/GPPGUI/Resource/images/IARC_7+.svg.png"
        },
        DefaultPopularCard{
            "https://github.com/morkt/GARbro",
            "GARbro",
            "无敌了",
            ":/GPPGUI/Resource/images/gar.png",
            "morkt",
            "神一样的解封包工具",
            ":/GPPGUI/Resource/images/IARC_7+.svg.png"
        },
        DefaultPopularCard{
            "https://github.com/arcusmaximus/VNTranslationTools",
            "VNTranslationTools",
            "能少用就少用",
            ":/GPPGUI/Resource/images/vnt.png",
            "arcusmaximus",
            "萌新拯救者一般的文本提取与回封工具",
            ":/GPPGUI/Resource/images/IARC_7+.svg.png"
        },
        DefaultPopularCard{
            "https://github.com/shangjiaxuan/Crass-source",
            "Crass",
            "专治老游戏",
            ":/GPPGUI/Resource/images/crass.png",
            "痴漢公賊",
            "早期Galgame解包工具，多看看它的说明文档",
            ":/GPPGUI/Resource/images/IARC_7+.svg.png"
        },
        DefaultPopularCard{
            "https://www.sublimetext.com",
            "Sublime",
            "和Em互补长短",
            ":/GPPGUI/Resource/images/sublime.png",
            "Sublime HQ",
            "高亮很好使，但编码转换不如emeditor",
            ":/GPPGUI/Resource/images/IARC_7+.svg.png"
        },
        DefaultPopularCard{
            "https://github.com/ZQF-ReVN",
            "ReVN",
            "拜见祖师爷",
            ":/GPPGUI/Resource/images/revn.png",
            "Dir-A",
            "别说你是搞机翻的",
            ":/GPPGUI/Resource/images/IARC_7+.svg.png"
        },
    };

    ElaAcrylicUrlCard* createUrlCard(const QString& url, const QString& pixmapPath, const QString& title, const QString& subTitle, QWidget* parent)
    {
        ElaAcrylicUrlCard* card = new ElaAcrylicUrlCard(parent);
        card->setCardPixmapSize(QSize(62, 62));
        card->setFixedSize(225, 225);
        card->setTitlePixelSize(17);
        card->setTitleSpacing(25);
        card->setSubTitleSpacing(13);
        card->setUrl(url);
        card->setCardPixmap(QPixmap(pixmapPath));
        card->setTitle(title);
        card->setSubTitle(subTitle);

        ElaToolTip* toolTip = new ElaToolTip(card);
        toolTip->setToolTip(url);

        return card;
    }

    ElaImageCard* createHeroCard(QWidget* parent)
    {
        ElaText* titleText = new ElaText("GalTransl++ GUI", parent);
        titleText->setTextPixelSize(35);

        ElaText* descriptionText = new ElaText("Translate your favorite galgames", parent);
        descriptionText->setTextPixelSize(18);

        QVBoxLayout* titleLayout = new QVBoxLayout();
        titleLayout->setContentsMargins(30, 60, 0, 0);
        titleLayout->addWidget(titleText);
        titleLayout->addWidget(descriptionText);

        ElaImageCard* backgroundCard = new ElaImageCard(parent);
        backgroundCard->setBorderRadius(10);
        backgroundCard->setFixedHeight(400);
        backgroundCard->setCardImage(QImage(":/GPPGUI/Resource/images/homebackground.png"));

        ElaScrollArea* cardScrollArea = new ElaScrollArea(parent);
        cardScrollArea->setWidgetResizable(true);
        cardScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        cardScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        cardScrollArea->setIsGrabGesture(true, 0);
        cardScrollArea->setIsOverShoot(Qt::Horizontal, true);

        QWidget* cardScrollAreaWidget = new QWidget(parent);
        cardScrollAreaWidget->setStyleSheet("background-color:transparent;");
        cardScrollArea->setWidget(cardScrollAreaWidget);

        QHBoxLayout* urlCardLayout = new QHBoxLayout();
        urlCardLayout->setSpacing(15);
        urlCardLayout->setContentsMargins(30, 0, 0, 6);
        urlCardLayout->addWidget(createUrlCard(
            "https://github.com/julixian/GalTranslPP",
            ":/GPPGUI/Resource/images/github.png",
            "GalTransl++\nGithub",
            QObject::tr("AI 自动化翻译解决方案"),
            parent));
        urlCardLayout->addWidget(createUrlCard(
            "https://julixian-siw.worldsystem.net/",
            ":/GPPGUI/Resource/images/webIcon.jpeg",
            "julixian",
            "LAMUNATION FOREVER！！！",
            parent));
        urlCardLayout->addStretch();

        QVBoxLayout* cardScrollAreaWidgetLayout = new QVBoxLayout(cardScrollAreaWidget);
        cardScrollAreaWidgetLayout->setContentsMargins(0, 0, 0, 0);
        cardScrollAreaWidgetLayout->addStretch();
        cardScrollAreaWidgetLayout->addLayout(urlCardLayout);

        QVBoxLayout* backgroundLayout = new QVBoxLayout(backgroundCard);
        backgroundLayout->setContentsMargins(0, 0, 0, 0);
        backgroundLayout->addLayout(titleLayout);
        backgroundLayout->addWidget(cardScrollArea);

        return backgroundCard;
    }

    bool hasPopularCardsArray(toml::ordered_value& globalConfig)
    {
        return globalConfig.contains("popularCards") && globalConfig["popularCards"].is_array();
    }

    std::optional<toml::ordered_value> getPopularCardConfig(toml::ordered_value& globalConfig, size_t index)
    {
        if (!hasPopularCardsArray(globalConfig) || index >= globalConfig["popularCards"].size()) {
            return std::nullopt;
        }

        auto& popularCardsArr = globalConfig["popularCards"].as_array();
        if (!popularCardsArr[index].is_table()) {
            return std::nullopt;
        }

        return popularCardsArr[index];
    }

    void connectCardUrl(const ElaPopularCard* card, const QUrl& url, const QObject* receiver)
    {
        QObject::connect(card, &ElaPopularCard::popularCardButtonClicked, receiver, [url]()
            {
                QDesktopServices::openUrl(url);
            });
    }

    void applyPopularCardConfig(ElaPopularCard* homeCard, const toml::ordered_value& card, const QObject* receiver)
    {
        const QString pathOrUrl = QString::fromStdString(toml::find_or(card, "pathOrUrl", ""));
        const bool fromLocal = toml::find_or(card, "fromLocal", false);
        homeCard->setCardButtonText(fromLocal ? QObject::tr("启动") : QObject::tr("获取"));

        if (!pathOrUrl.isEmpty()) {
            const QUrl url = fromLocal ? QUrl::fromLocalFile(pathOrUrl) : QUrl(pathOrUrl);
            connectCardUrl(homeCard, url, receiver);
        }

        homeCard->setTitle(QString::fromStdString(toml::find_or(card, "title", "")));
        homeCard->setSubTitle(QString::fromStdString(toml::find_or(card, "subTitle", "")));

        if (const QString pixmapPath = QString::fromStdString(toml::find_or(card, "cardPixmap", "")); !pixmapPath.isEmpty()) {
            homeCard->setCardPixmap(QPixmap(pixmapPath));
        }
        else if (fromLocal) {
            const QFileIconProvider iconProvider;
            const QFileInfo fileInfo(pathOrUrl);
            const QIcon icon = iconProvider.icon(fileInfo);
            if (!icon.isNull()) {
                homeCard->setCardPixmap(icon.pixmap(128, 128));
            }
        }

        homeCard->setInteractiveTips(QString::fromStdString(toml::find_or(card, "interactiveTips", "")));
        homeCard->setDetailedText(QString::fromStdString(toml::find_or(card, "detailedText", "")));
        homeCard->setCardFloatPixmap(QPixmap(QString::fromStdString(toml::find_or(card, "cardFloatPixmap", ""))));
    }

    void applyDefaultPopularCard(ElaPopularCard* homeCard, const DefaultPopularCard& card, const QObject* receiver)
    {
        connectCardUrl(homeCard, QUrl(card.url), receiver);
        homeCard->setTitle(card.title);
        homeCard->setSubTitle(card.subTitle);
        homeCard->setCardPixmap(QPixmap(card.pixmap));
        homeCard->setInteractiveTips(card.interactiveTips);
        homeCard->setDetailedText(card.detailedText);
        homeCard->setCardFloatPixmap(QPixmap(card.floatPixmap));
    }

    ElaPopularCard* createPopularCard(size_t index, toml::ordered_value& globalConfig, QWidget* parent, const QObject* receiver)
    {
        ElaPopularCard* homeCard = new ElaPopularCard(parent);
        if (const auto cardOpt = getPopularCardConfig(globalConfig, index)) {
            applyPopularCardConfig(homeCard, *cardOpt, receiver);
        }
        else if (index < DefaultPopularCards.size()) {
            applyDefaultPopularCard(homeCard, DefaultPopularCards[index], receiver);
        }
        return homeCard;
    }

    ElaFlowLayout* createPopularCardsLayout(toml::ordered_value& globalConfig, QWidget* parent, const QObject* receiver)
    {
        ElaFlowLayout* flowLayout = new ElaFlowLayout(0, 5, 5);
        flowLayout->setContentsMargins(18, 0, 0, 0);
        flowLayout->setIsAnimation(true);

        for (size_t i = 0; i < DefaultPopularCards.size(); ++i) {
            flowLayout->addWidget(createPopularCard(i, globalConfig, parent, receiver));
        }

        if (!hasPopularCardsArray(globalConfig)) {
            return flowLayout;
        }

        const auto& cards = globalConfig["popularCards"];
        for (size_t i = DefaultPopularCards.size(); i < cards.size(); ++i) {
            if (auto cardOpt = getPopularCardConfig(globalConfig, i)) {
                ElaPopularCard* popularCard = new ElaPopularCard(parent);
                applyPopularCardConfig(popularCard, *cardOpt, receiver);
                flowLayout->addWidget(popularCard);
            }
        }

        return flowLayout;
    }
}

HomePage::HomePage(toml::ordered_value& globalConfig, QWidget* parent)
    : BasePage(parent), m_globalConfig(globalConfig)
{
    setWindowTitle(tr("主页"));
    setTitleVisible(false);
    setContentsMargins(2, 2, 0, 0);

    setupUi();
}

HomePage::~HomePage() = default;

void HomePage::setupUi()
{
    QWidget* centralWidget = new QWidget(this);
    centralWidget->setWindowTitle(tr("主页"));
    QVBoxLayout* centerLayout = new QVBoxLayout(centralWidget);
    centerLayout->setSpacing(0);
    centerLayout->setContentsMargins(0, 0, 0, 0);

    ElaText* flowText = new ElaText("Useful Tools", centralWidget);
    flowText->setTextPixelSize(20);

    QHBoxLayout* flowTextLayout = new QHBoxLayout();
    flowTextLayout->setContentsMargins(25, 0, 0, 0);
    flowTextLayout->addWidget(flowText);

    centerLayout->addWidget(createHeroCard(centralWidget));
    centerLayout->addSpacing(20);
    centerLayout->addLayout(flowTextLayout);
    centerLayout->addSpacing(10);
    centerLayout->addLayout(createPopularCardsLayout(m_globalConfig, centralWidget, this));
    centerLayout->addStretch();

    addCentralWidget(centralWidget);
}
