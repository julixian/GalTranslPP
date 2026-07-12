#include "TranslationWorkbenchPage.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QHBoxLayout>
#include <QPainter>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <algorithm>

#include "ElaListView.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaTheme.h"

namespace
{
    constexpr int SuccessRole = Qt::UserRole + 1;
    constexpr int ErrorRole = Qt::UserRole + 2;
    constexpr int FileRole = Qt::UserRole + 3;
    constexpr int MaxSuccessEvents = 500;
    constexpr int MaxRenderedSuccessEvents = 100;
    constexpr int MaxErrorEvents = 80;

    QString compactPreview(QString text, int maxChars = 180)
    {
        text.replace("\r", " ");
        text.replace("\n", " ");
        text.replace("\t", " ");
        text = text.trimmed();
        if (maxChars >= 0 && text.size() > maxChars) {
            text = text.left(maxChars) + "...";
        }
        return text;
    }

    QColor themeColor(ElaThemeType::ThemeColor color)
    {
        return eTheme->getThemeColor(eTheme->getThemeMode(), color);
    }

    QString formatRuntimeTime(const QString& timestamp)
    {
        const QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
        if (!dt.isValid()) {
            return timestamp;
        }
        return dt.toLocalTime().toString("HH:mm:ss");
    }

    void drawPill(QPainter* painter, QRectF rect, const QString& text, const QColor& fill, const QColor& pen)
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRoundedRect(rect, rect.height() / 2, rect.height() / 2);
        painter->setPen(pen);
        painter->drawText(rect.adjusted(8, 0, -8, 0), Qt::AlignCenter, text);
    }

    QString compactModelLabel(const QString& model)
    {
        const QString trimmed = model.trimmed();
        const int slash = trimmed.lastIndexOf('/');
        if (slash < 0 || slash + 1 >= trimmed.size()) {
            return trimmed;
        }
        return trimmed.mid(slash + 1);
    }

    class SuccessDelegate : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            // 成功句流只保存轻量事件，实际布局在 delegate 中绘制，
            // 这样高频追加时不会为每一条创建 QWidget。
            painter->save();
            painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

            const auto event = index.data(SuccessRole).value<GuiRuntimeTransSuccessEvent>();
            const bool selected = option.state.testFlag(QStyle::State_Selected);
            const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
            QRectF card = option.rect.adjusted(6, 4, -6, -5);
            QColor cardColor = selected ? themeColor(ElaThemeType::BasicSelectedAlpha)
                : hovered ? themeColor(ElaThemeType::BasicHoverAlpha)
                : themeColor(ElaThemeType::BasicBaseAlpha);
            painter->setPen(QPen(selected ? themeColor(ElaThemeType::PrimaryNormal) : themeColor(ElaThemeType::BasicBorder), selected ? 1.3 : 1.0));
            painter->setBrush(cardColor);
            painter->drawRoundedRect(card, 7, 7);

            QRect content = card.toRect().adjusted(14, 8, -12, -8);
            const QColor textColor = themeColor(ElaThemeType::BasicText);
            const QColor detailColor = themeColor(ElaThemeType::BasicDetailsText);
            const QColor primary = themeColor(ElaThemeType::PrimaryNormal);
            const bool dark = eTheme->getThemeMode() == ElaThemeType::Dark;

            QFont tagFont = option.font;
            tagFont.setPixelSize(11);
            tagFont.setBold(true);
            painter->setFont(tagFont);
            QFontMetrics tagMetrics(tagFont);
            const QString indexText = QString("#%1").arg(event.index);
            const int indexWidth = tagMetrics.horizontalAdvance(indexText) + 18;
            drawPill(painter, QRectF(content.left(), content.top(), indexWidth, 22), indexText,
                dark ? QColor(24, 74, 47) : QColor(216, 246, 226), dark ? QColor(139, 241, 177) : QColor(19, 116, 61));

            const int fileX = content.left() + indexWidth + 8;
            const int timeWidth = 54;
            QRect fileRect(fileX, content.top(), content.right() - fileX - timeWidth - 8, 22);
            painter->setPen(primary);
            painter->drawText(fileRect, Qt::AlignVCenter | Qt::AlignLeft,
                tagMetrics.elidedText(event.filename, Qt::ElideMiddle, fileRect.width()));

            painter->setPen(detailColor);
            painter->drawText(QRect(content.right() - timeWidth, content.top(), timeWidth, 22),
                Qt::AlignRight | Qt::AlignVCenter, formatRuntimeTime(event.timestamp));

            auto drawLine = [&](int y, const QString& label, const QString& text)
                {
                    QFont labelFont = option.font;
                    labelFont.setPixelSize(11);
                    labelFont.setBold(true);
                    painter->setFont(labelFont);
                    QRectF labelRect(content.left(), y + 1, 44, 18);
                    drawPill(painter, labelRect, label, themeColor(ElaThemeType::BasicHoverAlpha), detailColor);

                    QFont bodyFont = option.font;
                    bodyFont.setPixelSize(13);
                    painter->setFont(bodyFont);
                    painter->setPen(textColor);
                    QFontMetrics fm(bodyFont);
                    QRect textRect(content.left() + 52, y, content.width() - 52, 20);
                    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                        fm.elidedText(compactPreview(text), Qt::ElideRight, textRect.width()));
                };

            const QString speaker = event.speakers.join(" / ");
            if (!speaker.isEmpty()) {
                QFont speakerFont = option.font;
                speakerFont.setPixelSize(11);
                speakerFont.setBold(true);
                painter->setFont(speakerFont);
                QFontMetrics speakerMetrics(speakerFont);
                const int speakerWidth = qMin(content.width(), speakerMetrics.horizontalAdvance(speaker) + 18);
                drawPill(painter, QRectF(content.left(), content.top() + 27, speakerWidth, 20), speaker,
                    themeColor(ElaThemeType::BasicHoverAlpha), textColor);
            }

            drawLine(content.top() + 51, "SRC", event.sourcePreview);
            drawLine(content.top() + 74, "DST", event.translationPreview);

            if (!event.transby.isEmpty()) {
                QFont modelFont = option.font;
                modelFont.setPixelSize(11);
                painter->setFont(modelFont);
                painter->setPen(detailColor);
                painter->drawText(QRect(content.left(), content.bottom() - 18, content.width(), 18),
                    Qt::AlignRight | Qt::AlignVCenter, compactModelLabel(event.transby));
            }

            painter->restore();
        }
    };

    class ErrorDelegate : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            // 错误卡片默认只展示短摘要，完整错误放 tooltip，避免 Api 长报文拖慢绘制。
            painter->save();
            painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
            const auto event = index.data(ErrorRole).value<GuiRuntimeTransErrorEvent>();
            const bool selected = option.state.testFlag(QStyle::State_Selected);
            const bool dark = eTheme->getThemeMode() == ElaThemeType::Dark;
            QRectF card = option.rect.adjusted(5, 5, -7, -7);
            painter->setPen(QPen(selected ? QColor(255, 97, 86) : themeColor(ElaThemeType::BasicBorder), selected ? 1.3 : 1.0));
            painter->setBrush(selected ? themeColor(ElaThemeType::BasicSelectedAlpha) : themeColor(ElaThemeType::BasicBaseAlpha));
            painter->drawRoundedRect(card, 7, 7);

            const QColor danger = dark ? QColor(255, 132, 132) : QColor(190, 42, 42);
            const QColor dangerFill = dark ? QColor(92, 32, 36, 170) : QColor(255, 225, 225, 210);
            QRectF accent(card.left(), card.top() + 18, 4, card.height() - 36);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(255, 112, 67));
            painter->drawRoundedRect(accent, 2, 2);

            QRect content = card.toRect().adjusted(18, 10, -12, -10);
            const QColor textColor = themeColor(ElaThemeType::BasicText);
            const QColor detailColor = themeColor(ElaThemeType::BasicDetailsText);

            QFont tagFont = option.font;
            tagFont.setPixelSize(11);
            tagFont.setBold(true);
            painter->setFont(tagFont);
            QFontMetrics tagMetrics(tagFont);
            QString kind = event.kind.isEmpty() ? "error" : event.kind;
            if (kind == "parse") {
                kind = QObject::tr("结果解析");
            }
            else if (kind == "api") {
                kind = QObject::tr("模型请求");
            }
            else if (kind == "agent") {
                kind = QObject::tr("Agent");
            }
            else if (kind == "file") {
                kind = QObject::tr("文件");
            }
            const int tagWidth = tagMetrics.horizontalAdvance(kind) + 18;
            drawPill(painter, QRectF(content.left(), content.top(), tagWidth, 22), kind, dangerFill, danger);
            int cursorX = content.left() + tagWidth + 8;
            if (event.requestCount > 0) {
                const QString requestText = QObject::tr("请求 %1").arg(event.requestCount);
                const int requestWidth = tagMetrics.horizontalAdvance(requestText) + 18;
                drawPill(painter, QRectF(cursorX, content.top(), requestWidth, 22), requestText,
                    themeColor(ElaThemeType::BasicHoverAlpha), detailColor);
                cursorX += requestWidth + 8;
            }
            painter->setPen(detailColor);
            painter->drawText(QRect(cursorX, content.top(), content.right() - cursorX, 22),
                Qt::AlignVCenter | Qt::AlignRight, formatRuntimeTime(event.timestamp));

            QFont msgFont = option.font;
            msgFont.setPixelSize(13);
            msgFont.setBold(true);
            painter->setFont(msgFont);
            painter->setPen(textColor);
            QFontMetrics msgMetrics(msgFont);
            painter->drawText(QRect(content.left(), content.top() + 32, content.width(), 24),
                Qt::AlignVCenter | Qt::AlignLeft,
                msgMetrics.elidedText(compactPreview(event.message, 100), Qt::ElideRight, content.width()));

            auto drawMetaPill = [&](int y, const QString& label, const QString& value)
                {
                    if (value.isEmpty()) {
                        return;
                    }
                    QFont metaFont = option.font;
                    metaFont.setPixelSize(11);
                    metaFont.setBold(true);
                    painter->setFont(metaFont);
                    QFontMetrics metaMetrics(metaFont);
                    const QString text = label + " " + value;
                    const int width = qMin(content.width(), metaMetrics.horizontalAdvance(text) + 22);
                    QRectF rect(content.left(), y, width, 23);
                    painter->setPen(QPen(themeColor(ElaThemeType::BasicBorder), 1));
                    painter->setBrush(themeColor(ElaThemeType::BasicHoverAlpha));
                    painter->drawRoundedRect(rect, rect.height() / 2, rect.height() / 2);
                    painter->setPen(danger);
                    painter->drawText(QRectF(rect.left() + 10, rect.top(), metaMetrics.horizontalAdvance(label), rect.height()),
                        Qt::AlignVCenter | Qt::AlignLeft, label);
                    painter->setPen(textColor);
                    QRectF valueRect(rect.left() + 10 + metaMetrics.horizontalAdvance(label + " "), rect.top(),
                        rect.width() - 20 - metaMetrics.horizontalAdvance(label + " "), rect.height());
                    painter->drawText(valueRect, Qt::AlignVCenter | Qt::AlignLeft,
                        metaMetrics.elidedText(value, Qt::ElideMiddle, (int)valueRect.width()));
                };

            const QString fileText = event.filename.isEmpty()
                ? QString()
                : event.filename + (event.indexRange.isEmpty() ? QString() : ": " + event.indexRange);
            drawMetaPill(content.top() + 64, QObject::tr("文件"), fileText);
            drawMetaPill(content.top() + 94, QObject::tr("模型"), compactModelLabel(event.model));

            painter->restore();
        }
    };

    class FileDelegate : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            // 文件进度卡片是右侧的可点击筛选入口；点击后会过滤左侧成功句流。
            painter->save();
            painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
            const auto file = index.data(FileRole).value<GuiRuntimeFileProgress>();
            QRectF card = option.rect.adjusted(5, 4, -5, -5);
            painter->setPen(QPen(themeColor(ElaThemeType::BasicBorder), 1.0));
            painter->setBrush(themeColor(ElaThemeType::BasicBaseAlpha));
            painter->drawRoundedRect(card, 7, 7);

            QRect content = card.toRect().adjusted(12, 8, -12, -8);
            const QColor textColor = themeColor(ElaThemeType::BasicText);
            const QColor detailColor = themeColor(ElaThemeType::BasicDetailsText);
            const QColor primary = themeColor(ElaThemeType::PrimaryNormal);
            const int percent = file.total > 0 ? qMin(100, qMax(0, (file.completed * 100) / file.total)) : 0;

            QFont titleFont = option.font;
            titleFont.setPixelSize(13);
            titleFont.setBold(true);
            painter->setFont(titleFont);
            QFontMetrics titleMetrics(titleFont);
            painter->setPen(textColor);
            painter->drawText(QRect(content.left(), content.top(), content.width() - 76, 22),
                Qt::AlignVCenter | Qt::AlignLeft, titleMetrics.elidedText(file.filename, Qt::ElideMiddle, content.width() - 76));
            painter->setPen(detailColor);
            painter->drawText(QRect(content.right() - 70, content.top(), 70, 22),
                Qt::AlignRight | Qt::AlignVCenter, QString("%1%").arg(percent));

            QRectF track(content.left(), content.top() + 32, content.width(), 8);
            painter->setPen(Qt::NoPen);
            painter->setBrush(themeColor(ElaThemeType::BasicHoverAlpha));
            painter->drawRoundedRect(track, 4, 4);
            QRectF fill = track;
            fill.setWidth(track.width() * percent / 100.0);
            painter->setBrush(primary);
            painter->drawRoundedRect(fill, 4, 4);

            QFont metaFont = option.font;
            metaFont.setPixelSize(12);
            painter->setFont(metaFont);
            painter->setPen(detailColor);
            painter->drawText(QRect(content.left(), content.top() + 48, content.width(), 20),
                Qt::AlignVCenter | Qt::AlignLeft,
                QString("%1/%2  ·  %3 问题").arg(file.completed).arg(file.total).arg(file.problems));
            painter->restore();
        }
    };
}

TranslationWorkbenchPage::TranslationWorkbenchPage(QWidget* parent)
    : BasePage(parent)
{
    setWindowTitle(tr("翻译工作台"));
    setTitleVisible(false);
    setupUi();
}

void TranslationWorkbenchPage::setupUi()
{
    // 工作台分成两块：左侧是按时间流动的成功句，右侧在“最近错误”和“文件进度”之间切换。
    // 页面本身只维护内存态，所有数据都来自 TranslatorWorker 转发的 Controller 运行期事件。
    QWidget* mainWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(18, 14, 16, 0);
    mainLayout->setSpacing(8);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    m_summaryText = new ElaText(tr("等待翻译任务"), 13, mainWidget);
    headerLayout->addWidget(m_summaryText, 1);
    m_filterText = new ElaText("", 12, mainWidget);
    m_filterText->setStyleSheet(QString("color:%1;").arg(themeColor(ElaThemeType::BasicDetailsText).name(QColor::HexArgb)));
    headerLayout->addWidget(m_filterText);
    m_clearFilterButton = new ElaPushButton(tr("清除筛选"), mainWidget);
    connect(m_clearFilterButton, &ElaPushButton::clicked, this, [=]()
        {
            // 清除文件筛选后重新渲染句流；底层事件仍保留在 m_successes 中。
            m_successFileFilters.clear();
            renderSuccesses();
            refreshHeader();
        });
    headerLayout->addWidget(m_clearFilterButton);
    mainLayout->addLayout(headerLayout);

    QHBoxLayout* bodyLayout = new QHBoxLayout();
    bodyLayout->setSpacing(10);

    QWidget* successPane = new QWidget(mainWidget);
    QVBoxLayout* successLayout = new QVBoxLayout(successPane);
    successLayout->setContentsMargins(0, 0, 0, 0);
    successLayout->setSpacing(5);
    ElaText* successTitle = new ElaText(tr("成功句流"), 15, successPane);
    QFont titleFont = successTitle->font();
    titleFont.setBold(true);
    successTitle->setFont(titleFont);
    successLayout->addWidget(successTitle);
    m_successModel = new QStandardItemModel(this);
    m_successList = new ElaListView(successPane);
    m_successList->setModel(m_successModel);
    m_successList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_successList->setUniformItemSizes(true);
    m_successList->setItemHeight(126);
    m_successList->setItemDelegate(new SuccessDelegate(m_successList));
    successLayout->addWidget(m_successList, 1);
    bodyLayout->addWidget(successPane, 5);

    QWidget* sidePane = new QWidget(mainWidget);
    QVBoxLayout* sideLayout = new QVBoxLayout(sidePane);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(5);
    QHBoxLayout* tabLayout = new QHBoxLayout();
    tabLayout->setSpacing(5);
    m_sideTabGroup = new QButtonGroup(this);
    m_sideTabGroup->setExclusive(true);
    m_errorsTabButton = new ElaPushButton(tr("最近错误"), sidePane);
    m_errorsTabButton->setCheckable(true);
    m_filesTabButton = new ElaPushButton(tr("文件进度"), sidePane);
    m_filesTabButton->setCheckable(true);
    m_sideTabGroup->addButton(m_errorsTabButton, 0);
    m_sideTabGroup->addButton(m_filesTabButton, 1);
    tabLayout->addWidget(m_errorsTabButton);
    tabLayout->addWidget(m_filesTabButton);
    sideLayout->addLayout(tabLayout);
    connect(m_sideTabGroup, &QButtonGroup::idClicked, this, &TranslationWorkbenchPage::setSideTab);

    m_sideStack = new QStackedWidget(sidePane);
    m_errorModel = new QStandardItemModel(this);
    m_errorList = new ElaListView(m_sideStack);
    m_errorList->setModel(m_errorModel);
    m_errorList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_errorList->setUniformItemSizes(true);
    m_errorList->setItemHeight(152);
    m_errorList->setItemDelegate(new ErrorDelegate(m_errorList));
    m_sideStack->addWidget(m_errorList);

    m_fileModel = new QStandardItemModel(this);
    m_fileList = new ElaListView(m_sideStack);
    m_fileList->setModel(m_fileModel);
    m_fileList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fileList->setUniformItemSizes(true);
    m_fileList->setItemHeight(92);
    m_fileList->setItemDelegate(new FileDelegate(m_fileList));
    connect(m_fileList, &ElaListView::clicked, this, [=](const QModelIndex& index)
        {
            // 文件进度项同时也是筛选开关：可快速只看某个文件产生的成功句。
            const auto file = index.data(FileRole).value<GuiRuntimeFileProgress>();
            if (!file.filename.isEmpty()) {
                if (m_successFileFilters.contains(file.filename)) {
                    m_successFileFilters.remove(file.filename);
                }
                else {
                    m_successFileFilters.insert(file.filename);
                }
                renderSuccesses();
                refreshHeader();
            }
        });
    m_sideStack->addWidget(m_fileList);
    sideLayout->addWidget(m_sideStack, 1);
    bodyLayout->addWidget(sidePane, 3);

    mainLayout->addLayout(bodyLayout, 1);
    addCentralWidget(mainWidget, true, false, 0);
    setSideTab(0);
    refreshHeader();
}

void TranslationWorkbenchPage::setSideTab(int index)
{
    // ElaPushButton 分段按钮不依赖额外导航控件，切换时只同步 stacked widget 和 checked 状态。
    m_sideStack->setCurrentIndex(index);
    m_errorsTabButton->setChecked(index == 0);
    m_filesTabButton->setChecked(index == 1);
}

void TranslationWorkbenchPage::clearRuntime()
{
    // 新任务开始或手动清空时调用；页面不落盘保存历史，状态全部随 GUI 会话消失。
    m_successes.clear();
    m_errors.clear();
    m_files.clear();
    m_successFileFilters.clear();
    m_successTotal = 0;
    m_errorTotal = 0;
    m_stage.clear();
    m_currentFile.clear();
    renderSuccesses();
    renderErrors();
    renderFiles();
    refreshHeader();
}

void TranslationWorkbenchPage::resetRuntimeFiles(const QVector<GuiRuntimeFileProgress>& files)
{
    // Controller 重新注册运行文件时，右侧文件表以这批数据为准，旧筛选也要清空。
    m_files.clear();
    m_successFileFilters.clear();
    for (const auto& file : files) {
        m_files[file.filename] = file;
    }
    renderFiles();
    renderSuccesses();
    refreshHeader();
}

void TranslationWorkbenchPage::updateRuntimeFiles(const QVector<GuiRuntimeFileProgress>& files)
{
    // Worker 会批量合并高频文件进度事件，这里一次更新多项后只刷新一遍列表。
    bool changed = false;
    for (const auto& file : files) {
        if (file.filename.isEmpty()) {
            continue;
        }
        m_files[file.filename] = file;
        changed = true;
    }
    if (!changed) {
        return;
    }
    renderFiles();
    refreshHeader();
}

void TranslationWorkbenchPage::appendSuccesses(const QVector<GuiRuntimeTransSuccessEvent>& events)
{
    if (events.isEmpty()) {
        return;
    }
    m_successTotal += events.size();
    for (const auto& event : events) {
        m_successes.push_front(event);
    }
    // 内存保留最近 500 条，但摘要里的成功事件统计使用 m_successTotal 累计值。
    trimSuccesses();
    renderSuccesses();
    refreshHeader();
}

void TranslationWorkbenchPage::appendErrors(const QVector<GuiRuntimeTransErrorEvent>& events)
{
    if (events.isEmpty()) {
        return;
    }
    m_errorTotal += events.size();
    for (const auto& event : events) {
        m_errors.push_front(event);
    }
    // 错误只保留最近若干条，避免一次 Api 风暴把 GUI 模型撑爆。
    trimErrors();
    renderErrors();
    refreshHeader();
}

void TranslationWorkbenchPage::updateStage(const QString& stage, const QString& currentFile)
{
    m_stage = stage;
    m_currentFile = currentFile;
    refreshHeader();
}

void TranslationWorkbenchPage::renderSuccesses()
{
    // m_successes 内部是“最新在前”，渲染前取最近 N 条再反转，
    // 让视觉顺序变成从上到下递增，最新事件贴近底部。
    if (m_successList) {
        m_successList->setUpdatesEnabled(false);
    }
    m_successModel->clear();
    QVector<GuiRuntimeTransSuccessEvent> visible;
    visible.reserve(qMin(m_successes.size(), MaxRenderedSuccessEvents));
    for (const GuiRuntimeTransSuccessEvent& event : m_successes) {
        if (!m_successFileFilters.isEmpty() && !m_successFileFilters.contains(event.filename)) {
            continue;
        }
        visible.push_back(event);
        if (visible.size() >= MaxRenderedSuccessEvents) {
            break;
        }
    }
    for (const auto& event : visible) {
        QStandardItem* item = new QStandardItem(event.filename);
        item->setData(QVariant::fromValue(event), SuccessRole);
        item->setEditable(false);
        m_successModel->appendRow(item);
    }
    if (m_successList) {
        m_successList->setUpdatesEnabled(true);
    }
}

void TranslationWorkbenchPage::renderErrors()
{
    // 错误列表不做复杂展开，保持短卡片 + tooltip。
    if (m_errorList) {
        m_errorList->setUpdatesEnabled(false);
    }
    m_errorModel->clear();
    for (const auto& event : m_errors) {
        GuiRuntimeTransErrorEvent displayEvent = event;
        displayEvent.message = compactPreview(displayEvent.message, 240);
        QStandardItem* item = new QStandardItem(displayEvent.message);
        item->setData(QVariant::fromValue(displayEvent), ErrorRole);
        item->setToolTip(displayEvent.message);
        item->setEditable(false);
        m_errorModel->appendRow(item);
    }
    if (m_errorList) {
        m_errorList->setUpdatesEnabled(true);
    }
}

void TranslationWorkbenchPage::renderFiles()
{
    m_fileModel->clear();
    QList<GuiRuntimeFileProgress> files = m_files.values();
    // 未完成文件排前面，文件名按 locale 排序，方便运行中扫进度。
    std::ranges::sort(files, [](const auto& a, const auto& b)
        {
            const bool aDone = a.total > 0 && a.completed >= a.total;
            const bool bDone = b.total > 0 && b.completed >= b.total;
            if (aDone != bDone) {
                return !aDone;
            }
            return a.filename.localeAwareCompare(b.filename) < 0;
        });
    for (const auto& file : files) {
        QStandardItem* item = new QStandardItem(file.filename);
        item->setData(QVariant::fromValue(file), FileRole);
        item->setEditable(false);
        m_fileModel->appendRow(item);
    }
}

void TranslationWorkbenchPage::refreshHeader()
{
    // 顶部摘要从运行期文件进度和错误集合即时汇总，不依赖进度条信号本身。
    int total = 0;
    int completed = 0;
    int problems = 0;
    for (const auto& file : m_files) {
        total += file.total;
        completed += file.completed;
        problems += file.problems;
    }
    const QString stage = m_stage.isEmpty() ? tr("空闲") : m_stage;
    const QString current = m_currentFile.isEmpty() ? QString() : tr(" · 当前文件: %1").arg(m_currentFile);
    m_summaryText->setText(tr("%1%2 · %3/%4 句 · %5 问题 · %6 成功事件 · %7 错误")
        .arg(stage, current)
        .arg(completed).arg(total).arg(problems).arg(m_successTotal).arg(m_errorTotal));
    if (m_successFileFilters.isEmpty()) {
        m_filterText->clear();
        m_clearFilterButton->setVisible(false);
    }
    else {
        m_filterText->setText(tr("已筛选文件: %1").arg(QStringList(m_successFileFilters.values()).join(", ")));
        m_clearFilterButton->setVisible(true);
    }
    m_errorsTabButton->setText(m_errorTotal <= 0 ? tr("最近错误") : tr("最近错误 (%1)").arg(m_errorTotal));
    const int unfinished = std::ranges::count_if(m_files, [](const auto& file)
        {
            return file.total <= 0 || file.completed < file.total;
        });
    m_filesTabButton->setText(unfinished <= 0 ? tr("文件进度") : tr("文件进度 (%1)").arg(unfinished));
}

void TranslationWorkbenchPage::trimSuccesses()
{
    // 保留上限控制在追加阶段做；m_successTotal 不裁剪，用于展示本轮累计进入句流的总数。
    while (m_successes.size() > MaxSuccessEvents) {
        m_successes.pop_back();
    }
}

void TranslationWorkbenchPage::trimErrors()
{
    // 错误多为诊断入口，保留最近即可。
    while (m_errors.size() > MaxErrorEvents) {
        m_errors.pop_back();
    }
}
