#include "HomePage.h"

#include <array>
#include <optional>

#include <QDesktopServices>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>

#include "ElaAcrylicUrlCard.h"
#include "ElaFlowLayout.h"
#include "ElaPopularCard.h"
#include "ElaScrollArea.h"
#include "ElaText.h"
#include "ElaToolTip.h"

import Tool;

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

    constexpr std::array<DefaultPopularCard, 7> kDefaultPopularCards = {
        DefaultPopularCard{
            "https://github.com/julixian/LocaleEmulatorPlus",
            "LocaleEmulatorPlus",
            "转区工具",
            ":/GPPGUI/Resource/images/lep.png",
            "julixian",
            "Windows 区域、代码页与时区模拟工具（支持 x86/x64）",
            ":/GPPGUI/Resource/images/IARC_7+.svg.png"
        },
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

    class HeroImageCard final : public QWidget
    {
    public:
        explicit HeroImageCard(QImage image, QWidget* parent = nullptr)
            : QWidget(parent), m_image(std::move(image))
        {
            setMinimumSize(350, 260);
        }

        void setBorderRadius(int borderRadius) noexcept
        {
            m_borderRadius = borderRadius;
            update();
        }

    protected:
        void paintEvent(QPaintEvent* event) override
        {
            Q_UNUSED(event);
            if (m_image.isNull() || width() <= 0 || height() <= 0) {
                return;
            }

            QPainter painter(this);
            painter.save();
            painter.setPen(Qt::NoPen);
            painter.setRenderHints(QPainter::SmoothPixmapTransform | QPainter::Antialiasing);

            QPainterPath path;
            path.addRoundedRect(rect(), m_borderRadius, m_borderRadius);
            painter.setClipPath(path);

            const qreal cardAspect = static_cast<qreal>(width()) / height();
            const qreal imageAspect = static_cast<qreal>(m_image.width()) / m_image.height();
            int srcWidth = m_image.width();
            int srcHeight = m_image.height();
            if (cardAspect < imageAspect) {
                srcWidth = qRound(m_image.width() * cardAspect / imageAspect);
            }
            else {
                srcHeight = qRound(m_image.height() * imageAspect / cardAspect);
            }

            const int srcX = (m_image.width() - srcWidth) / 2;
            // 原图少女头部紧贴顶端，ElaImageCard 固定 50% 垂直居中裁剪会在宽屏下切掉上半张脸；偏上取景以完整露出头部与面部。
            constexpr qreal kVerticalCropRatio = 0.06;
            const int srcY = qRound((m_image.height() - srcHeight) * kVerticalCropRatio);
            painter.drawImage(rect(), m_image, QRect(srcX, srcY, srcWidth, srcHeight));
            painter.restore();
        }

    private:
        QImage m_image;
        int m_borderRadius = 10;
    };

    QWidget* createHeroCard(QWidget* parent)
    {
        ElaText* titleText = new ElaText("GalTransl++ GUI", parent);
        titleText->setTextPixelSize(35);

        ElaText* descriptionText = new ElaText("Translate your favorite galgames", parent);
        descriptionText->setTextPixelSize(18);

        QWidget* titleWidget = new QWidget(parent);
        QVBoxLayout* titleLayout = new QVBoxLayout(titleWidget);
        titleLayout->setContentsMargins(30, 60, 0, 0);
        titleLayout->addWidget(titleText);
        titleLayout->addWidget(descriptionText);

        HeroImageCard* backgroundCard = new HeroImageCard(QImage(":/GPPGUI/Resource/images/homebackground.png"), parent);
        backgroundCard->setBorderRadius(10);
        backgroundCard->setFixedHeight(400);

        ElaScrollArea* cardScrollArea = new ElaScrollArea(parent);
        cardScrollArea->setWidgetResizable(true);
        cardScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        cardScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        cardScrollArea->setIsGrabGesture(true, 0);
        cardScrollArea->setIsOverShoot(Qt::Horizontal, true);

        QWidget* cardScrollAreaWidget = new QWidget(parent);
        cardScrollAreaWidget->setStyleSheet("background-color:transparent;");
        cardScrollArea->setWidget(cardScrollAreaWidget);

        QWidget* urlCardWidget = new QWidget(cardScrollAreaWidget);
        QHBoxLayout* urlCardLayout = new QHBoxLayout(urlCardWidget);
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
        cardScrollAreaWidgetLayout->addWidget(urlCardWidget);

        QVBoxLayout* backgroundLayout = new QVBoxLayout(backgroundCard);
        backgroundLayout->setContentsMargins(0, 0, 0, 0);
        backgroundLayout->addWidget(titleWidget);
        backgroundLayout->addWidget(cardScrollArea);

        return backgroundCard;
    }

    bool hasPopularCardsArray(const tomlkit::Item& globalConfig)
    {
        return globalConfig.contains("popularCards") && globalConfig["popularCards"].isArray();
    }

    const tomlkit::Item* getPopularCardConfig(const tomlkit::Item& globalConfig, size_t index)
    {
        if (!hasPopularCardsArray(globalConfig) || index >= globalConfig["popularCards"].size()) {
            return nullptr;
        }

        const auto& popularCardsArr = globalConfig["popularCards"];
        if (!popularCardsArr[index].isTable()) {
            return nullptr;
        }

        return &popularCardsArr[index];
    }

    void connectCardUrl(const ElaPopularCard* card, const QUrl& url, const QObject* receiver)
    {
        QObject::connect(card, &ElaPopularCard::popularCardButtonClicked, receiver, [url]()
            {
                QDesktopServices::openUrl(url);
            });
    }

    void applyPopularCardConfig(ElaPopularCard* homeCard, const tomlkit::Item& card, const QObject* receiver)
    {
        const QString pathOrUrl = QString::fromStdString(tomlkit::findOr(card, "pathOrUrl", ""));
        const bool fromLocal = tomlkit::findOr(card, "fromLocal", false);
        homeCard->setCardButtonText(fromLocal ? QObject::tr("启动") : QObject::tr("获取"));

        if (!pathOrUrl.isEmpty()) {
            const QUrl url = fromLocal ? QUrl::fromLocalFile(pathOrUrl) : QUrl(pathOrUrl);
            connectCardUrl(homeCard, url, receiver);
        }

        homeCard->setTitle(QString::fromStdString(tomlkit::findOr(card, "title", "")));
        homeCard->setSubTitle(QString::fromStdString(tomlkit::findOr(card, "subTitle", "")));

        if (const QString pixmapPath = QString::fromStdString(tomlkit::findOr(card, "cardPixmap", "")); !pixmapPath.isEmpty()) {
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

        homeCard->setInteractiveTips(QString::fromStdString(tomlkit::findOr(card, "interactiveTips", "")));
        homeCard->setDetailedText(QString::fromStdString(tomlkit::findOr(card, "detailedText", "")));
        homeCard->setCardFloatPixmap(QPixmap(QString::fromStdString(tomlkit::findOr(card, "cardFloatPixmap", ""))));
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

    ElaPopularCard* createPopularCard(size_t index, const tomlkit::Item& globalConfig, QWidget* parent, const QObject* receiver)
    {
        ElaPopularCard* homeCard = new ElaPopularCard(parent);
        if (const auto* card = getPopularCardConfig(globalConfig, index)) {
            applyPopularCardConfig(homeCard, *card, receiver);
        }
        else if (index < kDefaultPopularCards.size()) {
            applyDefaultPopularCard(homeCard, kDefaultPopularCards[index], receiver);
        }
        return homeCard;
    }

    ElaFlowLayout* createPopularCardsLayout(const tomlkit::Item& globalConfig, QWidget* parent, QWidget* cardFloatArea, const QObject* receiver)
    {
        ElaFlowLayout* flowLayout = new ElaFlowLayout(0, 5, 5);
        flowLayout->setContentsMargins(18, 0, 0, 0);
        flowLayout->setIsAnimation(true);

        for (size_t i = 0; i < kDefaultPopularCards.size(); ++i) {
            ElaPopularCard* popularCard = createPopularCard(i, globalConfig, parent, receiver);
            popularCard->setCardFloatArea(cardFloatArea);
            flowLayout->addWidget(popularCard);
        }

        if (!hasPopularCardsArray(globalConfig)) {
            return flowLayout;
        }

        const auto& cards = globalConfig["popularCards"];
        for (size_t i = kDefaultPopularCards.size(); i < cards.size(); ++i) {
            if (const auto* card = getPopularCardConfig(globalConfig, i)) {
                ElaPopularCard* popularCard = new ElaPopularCard(parent);
                applyPopularCardConfig(popularCard, *card, receiver);
                popularCard->setCardFloatArea(cardFloatArea);
                flowLayout->addWidget(popularCard);
            }
        }

        return flowLayout;
    }
}

HomePage::HomePage(tomlkit::Item& globalConfig, QWidget* parent)
    : BasePage(parent), m_globalConfig(globalConfig)
{
    setWindowTitle(tr("主页"));
    setTitleVisible(false);
    setContentsMargins(2, 2, 0, 0);

    setupUi();
}

void HomePage::setupUi()
{
    QWidget* centralWidget = new QWidget(this);
    centralWidget->setWindowTitle(tr("主页"));
    QVBoxLayout* centerLayout = new QVBoxLayout(centralWidget);
    centerLayout->setSpacing(0);
    centerLayout->setContentsMargins(0, 0, 0, 0);

    ElaText* flowText = new ElaText("Useful Tools", centralWidget);
    flowText->setTextPixelSize(20);

    QWidget* flowTextWidget = new QWidget(centralWidget);
    QHBoxLayout* flowTextLayout = new QHBoxLayout(flowTextWidget);
    flowTextLayout->setSpacing(centerLayout->spacing());
    flowTextLayout->setContentsMargins(25, 0, 0, 0);
    flowTextLayout->addWidget(flowText);

    centerLayout->addWidget(createHeroCard(centralWidget));
    centerLayout->addSpacing(20);
    centerLayout->addWidget(flowTextWidget);
    centerLayout->addSpacing(10);
    centerLayout->addLayout(createPopularCardsLayout(m_globalConfig, centralWidget,this, this));
    centerLayout->addStretch();

    addCentralWidget(centralWidget);
}
