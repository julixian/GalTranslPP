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
    constexpr int JsonRowRole = Qt::UserRole + 1;
    constexpr int HitIndexRole = Qt::UserRole + 2;
    constexpr int ProblemTextRole = Qt::UserRole + 3;
    constexpr int EntryIndexRole = Qt::UserRole + 4;
    constexpr int EntrySpeakerRole = Qt::UserRole + 5;
    constexpr int EntryProblemRole = Qt::UserRole + 6;
    constexpr int EntryEngineRole = Qt::UserRole + 7;
    constexpr int EntrySourceRole = Qt::UserRole + 8;
    constexpr int EntryDstRole = Qt::UserRole + 9;
    constexpr int HitFileRole = Qt::UserRole + 10;
    constexpr int HitSentenceRole = Qt::UserRole + 11;
    constexpr int HitBadgesRole = Qt::UserRole + 12;
    constexpr int HitSourceRole = Qt::UserRole + 13;
    constexpr int HitDstRole = Qt::UserRole + 14;
    constexpr int HitProblemRole = Qt::UserRole + 15;

    constexpr int LabelFontPx = 12;
    constexpr int BodyFontPx = 13;
    constexpr int TitleFontPx = 15;

    int sentenceIndexOf(const json& object, int fallback);
    QString compactPreview(QString text, int maxChars = 160);

    QColor themeColor(ElaThemeType::ThemeColor color);
    QString colorName(ElaThemeType::ThemeColor color);
    QString auxiliaryTextStyle();
    QString splitterStyle();

    void tuneTextEdit(ElaPlainTextEdit* edit, bool readOnly, int height);
    void tuneNavButton(ElaPushButton* button, bool active);
    void setModelItemFont(QStandardItem* item, int px = BodyFontPx);
    ElaIconButton* createHeaderIconButton(ElaIconType::IconName icon, const QString& toolTip, QWidget* parent);

    QStyledItemDelegate* createCacheEntryDelegate(QObject* parent);
    QStyledItemDelegate* createCacheSearchDelegate(QObject* parent);
}

#endif
