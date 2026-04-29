#include "ProjectCachePage_p.h"

#include <QPainter>
#include <QPlainTextEdit>
#include <QSizePolicy>
#include <QStandardItem>
#include <QStyledItemDelegate>

#include "ElaIconButton.h"
#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include "ElaTheme.h"
#include "ElaToolTip.h"

namespace ProjectCachePagePrivate {


    int sentenceIndexOf(const json& object, int fallback)
    {
        if (object.is_object() && object.contains("index") && object["index"].is_number_integer()) {
            return object["index"].get<int>();
        }
        return fallback;
    }

    QString compactPreview(QString text, int maxChars)
    {
        text.replace("\r", "\\r");
        text.replace("\n", "\\n");
        if (text.size() <= maxChars) {
            return text;
        }
        return text.left(maxChars - 3) + "...";
    }

    QColor themeColor(ElaThemeType::ThemeColor color)
    {
        return eTheme->getThemeColor(eTheme->getThemeMode(), color);
    }

    QString colorName(ElaThemeType::ThemeColor color)
    {
        return themeColor(color).name(QColor::HexArgb);
    }

    QString auxiliaryTextStyle()
    {
        return QString("color:%1;").arg(colorName(ElaThemeType::BasicDetailsText));
    }

    QString splitterStyle()
    {
        return QString(
            "QSplitter::handle:horizontal{"
            "background:transparent;"
            "border:none;"
            "border-left:1px solid %1;"
            "margin:10px 4px;"
            "}"
            "QSplitter::handle:horizontal:hover{"
            "background:%2;"
            "border-left:1px solid %3;"
            "border-radius:3px;"
            "margin:6px 2px;"
            "}")
            .arg(colorName(ElaThemeType::BasicBaseLine),
                colorName(ElaThemeType::BasicHoverAlpha),
                colorName(ElaThemeType::BasicBorderHover));
    }

    void tuneTextEdit(ElaPlainTextEdit* edit, bool readOnly, int height)
    {
        edit->setMinimumHeight(height);
        edit->setMaximumHeight(height);
        edit->setReadOnly(readOnly);
        edit->setTabChangesFocus(true);
        edit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        QFont font = edit->font();
        font.setPixelSize(BodyFontPx);
        edit->setFont(font);
    }

    void tuneNavButton(ElaPushButton* button, bool active)
    {
        if (!button) {
            return;
        }
        button->setLightDefaultColor(active ? QColor(226, 241, 255) : QColor(248, 248, 248));
        button->setLightHoverColor(active ? QColor(214, 234, 255) : QColor(241, 241, 241));
        button->setLightPressColor(QColor(205, 229, 255));
        button->setDarkDefaultColor(active ? QColor(35, 62, 92) : QColor(42, 42, 42));
        button->setDarkHoverColor(active ? QColor(42, 72, 106) : QColor(52, 52, 52));
        button->setDarkPressColor(QColor(49, 82, 118));
        button->setLightTextColor(active ? QColor(15, 103, 173) : QColor(35, 35, 35));
        button->setDarkTextColor(active ? QColor(212, 232, 255) : QColor(235, 235, 235));
        button->update();
    }

    void setModelItemFont(QStandardItem* item, int px)
    {
        QFont font = item->font();
        font.setPixelSize(px);
        item->setFont(font);
    }

    ElaIconButton* createHeaderIconButton(ElaIconType::IconName icon, const QString& toolTip, QWidget* parent)
    {
        ElaIconButton* button = new ElaIconButton(icon, 22, 42, 38, parent);
        button->setBorderRadius(6);
        ElaToolTip* tip = new ElaToolTip(button);
        tip->setToolTip(toolTip);
        return button;
    }

    class CacheEntryDelegate : public QStyledItemDelegate
    {
    public:
        explicit CacheEntryDelegate(QObject* parent = nullptr)
            : QStyledItemDelegate(parent)
        {
        }

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            // 缓存条目右侧概览完全自绘：model 只提供 role 数据，
            // delegate 负责把序号、人名、问题、模型、原文和译文画成紧凑卡片。
            painter->save();
            painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

            const bool selected = option.state.testFlag(QStyle::State_Selected);
            const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
            const bool dark = eTheme->getThemeMode() == ElaThemeType::Dark;
            const QString problem = index.data(EntryProblemRole).toString();

            QRectF cardRect = option.rect.adjusted(6, 4, -6, -5);
            QColor cardColor = themeColor(ElaThemeType::BasicBaseAlpha);
            if (selected) {
                cardColor = themeColor(ElaThemeType::BasicSelectedAlpha);
            }
            else if (hovered) {
                cardColor = themeColor(ElaThemeType::BasicHoverAlpha);
            }

            QColor borderColor = problem.isEmpty()
                ? themeColor(ElaThemeType::BasicBorder)
                : QColor(dark ? 143 : 207, dark ? 111 : 166, dark ? 45 : 65);
            painter->setPen(QPen(borderColor, problem.isEmpty() ? 1.0 : 1.3));
            painter->setBrush(cardColor);
            painter->drawRoundedRect(cardRect, 7, 7);

            if (selected) {
                QRectF indicator(cardRect.left() + 7, cardRect.top() + 14, 3, cardRect.height() - 28);
                painter->setPen(Qt::NoPen);
                painter->setBrush(themeColor(ElaThemeType::PrimaryNormal));
                painter->drawRoundedRect(indicator, 2, 2);
            }

            QRect contentRect = cardRect.toRect().adjusted(18, 8, -14, -8);
            int x = contentRect.left();
            int y = contentRect.top();

            auto drawPill = [&](QString text, const QColor& fill, const QColor& penColor, bool bold)
                {
                    if (text.trimmed().isEmpty() || x >= contentRect.right() - 24) {
                        return;
                    }
                    QFont font = option.font;
                    font.setPixelSize(12);
                    font.setBold(bold);
                    painter->setFont(font);
                    QFontMetrics fm(font);

                    const int maxWidth = contentRect.right() - x;
                    text = fm.elidedText(text, Qt::ElideRight, qMax(36, maxWidth - 12));
                    const int width = qMin(maxWidth, fm.horizontalAdvance(text) + 18);
                    QRectF pillRect(x, y, width, 23);
                    painter->setPen(Qt::NoPen);
                    painter->setBrush(fill);
                    painter->drawRoundedRect(pillRect, 11, 11);
                    painter->setPen(penColor);
                    painter->drawText(pillRect.adjusted(9, 0, -9, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
                    x += width + 8;
                };

            const QColor textColor = themeColor(ElaThemeType::BasicText);
            const QColor detailColor = themeColor(ElaThemeType::BasicDetailsText);
            QColor subtleFill = themeColor(ElaThemeType::BasicHoverAlpha);
            QColor problemFill = dark ? QColor(86, 66, 28) : QColor(247, 232, 181);
            QColor problemText = dark ? QColor(248, 219, 139) : QColor(105, 75, 15);
            QColor engineFill = themeColor(ElaThemeType::PrimaryNormal);
            engineFill.setAlpha(dark ? 65 : 32);

            drawPill(QString("#%1").arg(index.data(EntryIndexRole).toInt()), subtleFill, detailColor, true);
            drawPill(index.data(EntrySpeakerRole).toString(), subtleFill, textColor, true);
            drawPill(problem, problemFill, problemText, true);
            drawPill(index.data(EntryEngineRole).toString(), engineFill, themeColor(ElaThemeType::PrimaryNormal), true);

            auto drawLine = [&](int lineY, const QString& label, const QString& text)
                {
                    QFont labelFont = option.font;
                    labelFont.setPixelSize(12);
                    painter->setFont(labelFont);
                    painter->setPen(detailColor);
                    QRect labelRect(contentRect.left(), lineY, 42, 22);
                    painter->drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, label);

                    QFont bodyFont = option.font;
                    bodyFont.setPixelSize(BodyFontPx);
                    painter->setFont(bodyFont);
                    painter->setPen(textColor);
                    QFontMetrics fm(bodyFont);
                    QRect textRect(contentRect.left() + 46, lineY, contentRect.width() - 46, 22);
                    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                        fm.elidedText(text, Qt::ElideRight, textRect.width()));
                };

            drawLine(contentRect.top() + 29, QObject::tr("原文"), index.data(EntrySourceRole).toString());
            drawLine(contentRect.top() + 53, QObject::tr("译文"), index.data(EntryDstRole).toString());

            painter->restore();
        }
    };

    class CacheSearchDelegate : public QStyledItemDelegate
    {
    public:
        explicit CacheSearchDelegate(QObject* parent = nullptr)
            : QStyledItemDelegate(parent)
        {
        }

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            // 搜索结果按 GalTransl 风格组织：
            // 问题/匹配标签 + 文件名、#index + 问题文本、原文、译文。
            painter->save();
            painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

            const bool selected = option.state.testFlag(QStyle::State_Selected);
            const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
            QRectF cardRect = option.rect.adjusted(5, 4, -5, -5);
            QColor cardColor = selected ? themeColor(ElaThemeType::BasicSelectedAlpha)
                : hovered ? themeColor(ElaThemeType::BasicHoverAlpha)
                : themeColor(ElaThemeType::BasicBaseAlpha);
            painter->setPen(QPen(selected ? themeColor(ElaThemeType::PrimaryNormal) : themeColor(ElaThemeType::BasicBorder), selected ? 1.3 : 1.0));
            painter->setBrush(cardColor);
            painter->drawRoundedRect(cardRect, 6, 6);

            QRect contentRect = cardRect.toRect().adjusted(12, 7, -10, -7);
            const bool dark = eTheme->getThemeMode() == ElaThemeType::Dark;
            const QColor textColor = themeColor(ElaThemeType::BasicText);
            const QColor detailColor = themeColor(ElaThemeType::BasicDetailsText);
            const QColor primaryColor = themeColor(ElaThemeType::PrimaryNormal);
            const QString problemText = index.data(HitProblemRole).toString();
            const QStringList badges = index.data(HitBadgesRole).toStringList();
            const bool hasProblem = !problemText.isEmpty();
            const QColor problemColor = dark ? QColor(255, 96, 96) : QColor(190, 38, 38);
            const QColor problemFill = dark ? QColor(92, 32, 36, 170) : QColor(255, 225, 225, 210);
            const QColor accentColor = dark ? QColor(249, 209, 125) : QColor(146, 93, 18);
            QColor matchFill = primaryColor;
            matchFill.setAlpha(dark ? 70 : 30);

            QFont tagFont = option.font;
            tagFont.setPixelSize(11);
            tagFont.setBold(true);
            painter->setFont(tagFont);
            QFontMetrics tagMetrics(tagFont);
            const QString tag = hasProblem ? QObject::tr("问题") : (badges.isEmpty() ? QObject::tr("匹配") : badges.first());
            const int tagWidth = tagMetrics.horizontalAdvance(tag) + 18;
            QRectF tagRect(contentRect.left(), contentRect.top() + 1, tagWidth, 22);
            painter->setPen(Qt::NoPen);
            painter->setBrush(hasProblem ? problemFill : matchFill);
            painter->drawRoundedRect(tagRect, 11, 11);
            painter->setPen(hasProblem ? problemColor : primaryColor);
            painter->drawText(tagRect, Qt::AlignCenter, tag);

            QFont titleFont = option.font;
            titleFont.setPixelSize(12);
            titleFont.setBold(true);
            painter->setFont(titleFont);
            painter->setPen(primaryColor);
            QFontMetrics titleMetrics(titleFont);
            QRect titleRect(contentRect.left() + tagWidth + 8, contentRect.top(), contentRect.width() - tagWidth - 8, 24);
            const QString title = index.data(HitFileRole).toString();
            painter->drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft,
                titleMetrics.elidedText(title, Qt::ElideMiddle, titleRect.width()));

            QFont problemFont = option.font;
            problemFont.setPixelSize(12);
            problemFont.setBold(true);
            painter->setFont(problemFont);
            QFontMetrics problemMetrics(problemFont);
            QRect problemRect(contentRect.left(), contentRect.top() + 29, contentRect.width(), 20);
            const QString problemLine = QString("#%1  %2")
                .arg(index.data(HitSentenceRole).toInt())
                .arg(hasProblem ? problemText : badges.join("/"));
            painter->setPen(hasProblem ? accentColor : detailColor);
            painter->drawText(problemRect, Qt::AlignVCenter | Qt::AlignLeft,
                problemMetrics.elidedText(problemLine, Qt::ElideRight, problemRect.width()));

            auto drawLine = [&](int lineY, const QString& label, const QString& text)
                {
                    QFont labelFont = option.font;
                    labelFont.setPixelSize(11);
                    labelFont.setBold(true);
                    painter->setFont(labelFont);
                    QRectF labelRect(contentRect.left(), lineY + 1, 34, 18);
                    painter->setPen(Qt::NoPen);
                    QColor labelFill = themeColor(ElaThemeType::BasicHoverAlpha);
                    painter->setBrush(labelFill);
                    painter->drawRoundedRect(labelRect, 5, 5);
                    painter->setPen(detailColor);
                    painter->drawText(labelRect, Qt::AlignCenter, label);

                    QFont bodyFont = option.font;
                    bodyFont.setPixelSize(12);
                    painter->setFont(bodyFont);
                    painter->setPen(textColor);
                    QFontMetrics fm(bodyFont);
                    QRect textRect(contentRect.left() + 42, lineY, contentRect.width() - 42, 20);
                    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                        fm.elidedText(text, Qt::ElideRight, textRect.width()));
                };

            drawLine(contentRect.top() + 56, QObject::tr("原文"), index.data(HitSourceRole).toString());
            drawLine(contentRect.top() + 79, QObject::tr("译文"), index.data(HitDstRole).toString());

            painter->restore();
        }
    };

    QStyledItemDelegate* createCacheEntryDelegate(QObject* parent)
    {
        return new CacheEntryDelegate(parent);
    }

    QStyledItemDelegate* createCacheSearchDelegate(QObject* parent)
    {
        return new CacheSearchDelegate(parent);
    }

}
