#ifndef PROJECTCACHEPAGE_P_H
#define PROJECTCACHEPAGE_P_H

#include <QColor>
#include <QString>
#include <Qt>

#include "ElaDef.h"
#include "ElaTheme.h"

#include <nlohmann/json.hpp>

class ElaIconButton;
class ElaPlainTextEdit;
class ElaPushButton;
class QObject;
class QStandardItem;
class QStyledItemDelegate;
class QWidget;

namespace ProjectCachePagePrivate
{
    using json = nlohmann::json;

    // Model 和自绘 delegate 共用的 role，新增展示字段时两边要一起更新。
    constexpr int kJsonRowRole = Qt::UserRole + 1;
    constexpr int kHitIndexRole = Qt::UserRole + 2;
    constexpr int kProblemTextRole = Qt::UserRole + 3;
    constexpr int kEntryIndexRole = Qt::UserRole + 4;
    constexpr int kEntrySpeakerRole = Qt::UserRole + 5;
    constexpr int kEntryProblemRole = Qt::UserRole + 6;
    constexpr int kEntryEngineRole = Qt::UserRole + 7;
    constexpr int kEntrySourceRole = Qt::UserRole + 8;
    constexpr int kEntryDstRole = Qt::UserRole + 9;
    constexpr int kHitFileRole = Qt::UserRole + 10;
    constexpr int kHitSentenceRole = Qt::UserRole + 11;
    constexpr int kHitBadgesRole = Qt::UserRole + 12;
    constexpr int kHitSourceRole = Qt::UserRole + 13;
    constexpr int kHitDstRole = Qt::UserRole + 14;
    constexpr int kHitProblemRole = Qt::UserRole + 15;

    constexpr int kLabelFontPx = 12;
    constexpr int kBodyFontPx = 13;
    constexpr int kTitleFontPx = 15;

    int sentenceIndexOf(const json& object, int fallback);
    QString compactPreview(QString text, int maxChars = 160);

    QColor themeColor(ElaThemeType::ThemeColor color);
    QString colorName(ElaThemeType::ThemeColor color);
    QString auxiliaryTextStyle();
    QString splitterStyle();

    void tuneTextEdit(ElaPlainTextEdit* edit, bool readOnly, int height);
    void tuneNavButton(ElaPushButton* button, bool active);
    void setModelItemFont(QStandardItem* item, int px = kBodyFontPx);
    ElaIconButton* createHeaderIconButton(ElaIconType::IconName icon, const QString& toolTip, QWidget* parent);

    QStyledItemDelegate* createCacheEntryDelegate(QObject* parent);
    QStyledItemDelegate* createCacheSearchDelegate(QObject* parent);
}

#endif
