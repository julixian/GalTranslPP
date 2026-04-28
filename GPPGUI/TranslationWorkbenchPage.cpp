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

namespace {
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
        if (text.size() <= maxChars) {
            return text;
        }
        return text.left(qMax(0, maxChars - 3)) + "...";
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
            painter->save();
            painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

            const auto event = index.data(SuccessRole).value<GuiRuntimeSuccessEvent>();
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

            if (!event.translatedBy.isEmpty()) {
                QFont modelFont = option.font;
                modelFont.setPixelSize(11);
                painter->setFont(modelFont);
                painter->setPen(detailColor);
                painter->drawText(QRect(content.left(), content.bottom() - 18, content.width(), 18),
                    Qt::AlignRight | Qt::AlignVCenter, compactModelLabel(event.translatedBy));
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
            painter->save();
            painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
            const auto event = index.data(ErrorRole).value<GuiRuntimeErrorEvent>();
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
            if (event.retryCount > 0) {
                const QString retryText = QObject::tr("重试 %1").arg(event.retryCount);
                const int retryWidth = tagMetrics.horizontalAdvance(retryText) + 18;
                drawPill(painter, QRectF(cursorX, content.top(), retryWidth, 22), retryText,
                    themeColor(ElaThemeType::BasicHoverAlpha), detailColor);
                cursorX += retryWidth + 8;
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
    _setupUI();
}

void TranslationWorkbenchPage::_setupUI()
{
    QWidget* mainWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(18, 14, 16, 0);
    mainLayout->setSpacing(8);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    _summaryText = new ElaText(tr("等待翻译任务"), 13, mainWidget);
    headerLayout->addWidget(_summaryText, 1);
    _filterText = new ElaText("", 12, mainWidget);
    _filterText->setStyleSheet(QString("color:%1;").arg(themeColor(ElaThemeType::BasicDetailsText).name(QColor::HexArgb)));
    headerLayout->addWidget(_filterText);
    _clearFilterButton = new ElaPushButton(tr("清除筛选"), mainWidget);
    connect(_clearFilterButton, &ElaPushButton::clicked, this, [=]()
        {
            _successFileFilters.clear();
            _renderSuccesses();
            _refreshHeader();
        });
    headerLayout->addWidget(_clearFilterButton);
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
    _successModel = new QStandardItemModel(this);
    _successList = new ElaListView(successPane);
    _successList->setModel(_successModel);
    _successList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _successList->setUniformItemSizes(true);
    _successList->setItemHeight(126);
    _successList->setItemDelegate(new SuccessDelegate(_successList));
    successLayout->addWidget(_successList, 1);
    bodyLayout->addWidget(successPane, 5);

    QWidget* sidePane = new QWidget(mainWidget);
    QVBoxLayout* sideLayout = new QVBoxLayout(sidePane);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(5);
    QHBoxLayout* tabLayout = new QHBoxLayout();
    tabLayout->setSpacing(5);
    _sideTabGroup = new QButtonGroup(this);
    _sideTabGroup->setExclusive(true);
    _errorsTabButton = new ElaPushButton(tr("最近错误"), sidePane);
    _errorsTabButton->setCheckable(true);
    _filesTabButton = new ElaPushButton(tr("文件进度"), sidePane);
    _filesTabButton->setCheckable(true);
    _sideTabGroup->addButton(_errorsTabButton, 0);
    _sideTabGroup->addButton(_filesTabButton, 1);
    tabLayout->addWidget(_errorsTabButton);
    tabLayout->addWidget(_filesTabButton);
    sideLayout->addLayout(tabLayout);
    connect(_sideTabGroup, &QButtonGroup::idClicked, this, &TranslationWorkbenchPage::_setSideTab);

    _sideStack = new QStackedWidget(sidePane);
    _errorModel = new QStandardItemModel(this);
    _errorList = new ElaListView(_sideStack);
    _errorList->setModel(_errorModel);
    _errorList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _errorList->setUniformItemSizes(true);
    _errorList->setItemHeight(152);
    _errorList->setItemDelegate(new ErrorDelegate(_errorList));
    _sideStack->addWidget(_errorList);

    _fileModel = new QStandardItemModel(this);
    _fileList = new ElaListView(_sideStack);
    _fileList->setModel(_fileModel);
    _fileList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _fileList->setUniformItemSizes(true);
    _fileList->setItemHeight(92);
    _fileList->setItemDelegate(new FileDelegate(_fileList));
    connect(_fileList, &ElaListView::clicked, this, [=](const QModelIndex& index)
        {
            const auto file = index.data(FileRole).value<GuiRuntimeFileProgress>();
            if (!file.filename.isEmpty()) {
                if (_successFileFilters.contains(file.filename)) {
                    _successFileFilters.remove(file.filename);
                }
                else {
                    _successFileFilters.insert(file.filename);
                }
                _renderSuccesses();
                _refreshHeader();
            }
        });
    _sideStack->addWidget(_fileList);
    sideLayout->addWidget(_sideStack, 1);
    bodyLayout->addWidget(sidePane, 3);

    mainLayout->addLayout(bodyLayout, 1);
    addCentralWidget(mainWidget, true, false, 0);
    _setSideTab(0);
    _refreshHeader();
}

void TranslationWorkbenchPage::_setSideTab(int index)
{
    _sideStack->setCurrentIndex(index);
    _errorsTabButton->setChecked(index == 0);
    _filesTabButton->setChecked(index == 1);
}

void TranslationWorkbenchPage::clearRuntime()
{
    _successes.clear();
    _errors.clear();
    _files.clear();
    _successFileFilters.clear();
    _stage.clear();
    _currentFile.clear();
    _renderSuccesses();
    _renderErrors();
    _renderFiles();
    _refreshHeader();
}

void TranslationWorkbenchPage::resetRuntimeFiles(const QVector<GuiRuntimeFileProgress>& files)
{
    _files.clear();
    _successFileFilters.clear();
    for (const auto& file : files) {
        _files[file.filename] = file;
    }
    _renderFiles();
    _renderSuccesses();
    _refreshHeader();
}

void TranslationWorkbenchPage::updateRuntimeFile(const GuiRuntimeFileProgress& file)
{
    if (file.filename.isEmpty()) {
        return;
    }
    _files[file.filename] = file;
    _renderFiles();
    _refreshHeader();
}

void TranslationWorkbenchPage::updateRuntimeFiles(const QVector<GuiRuntimeFileProgress>& files)
{
    bool changed = false;
    for (const auto& file : files) {
        if (file.filename.isEmpty()) {
            continue;
        }
        _files[file.filename] = file;
        changed = true;
    }
    if (!changed) {
        return;
    }
    _renderFiles();
    _refreshHeader();
}

void TranslationWorkbenchPage::appendSuccess(const GuiRuntimeSuccessEvent& event)
{
    _successes.push_front(event);
    _trimSuccesses();
    _renderSuccesses();
    _refreshHeader();
}

void TranslationWorkbenchPage::appendSuccesses(const QVector<GuiRuntimeSuccessEvent>& events)
{
    if (events.isEmpty()) {
        return;
    }
    for (const auto& event : events) {
        _successes.push_front(event);
    }
    _trimSuccesses();
    _renderSuccesses();
    _refreshHeader();
}

void TranslationWorkbenchPage::appendError(const GuiRuntimeErrorEvent& event)
{
    _errors.push_front(event);
    _trimErrors();
    _renderErrors();
    _refreshHeader();
}

void TranslationWorkbenchPage::appendErrors(const QVector<GuiRuntimeErrorEvent>& events)
{
    if (events.isEmpty()) {
        return;
    }
    for (const auto& event : events) {
        _errors.push_front(event);
    }
    _trimErrors();
    _renderErrors();
    _refreshHeader();
}

void TranslationWorkbenchPage::updateStage(const QString& stage, const QString& currentFile)
{
    _stage = stage;
    _currentFile = currentFile;
    _refreshHeader();
}

void TranslationWorkbenchPage::_renderSuccesses()
{
    const bool stickToBottom = _successList
        && (_successList->verticalScrollBar()->maximum() - _successList->verticalScrollBar()->value() <= 24);
    if (_successList) {
        _successList->setUpdatesEnabled(false);
    }
    _successModel->clear();
    QVector<GuiRuntimeSuccessEvent> visible;
    visible.reserve(qMin(_successes.size(), MaxRenderedSuccessEvents));
    for (const GuiRuntimeSuccessEvent& event : _successes) {
        if (!_successFileFilters.isEmpty() && !_successFileFilters.contains(event.filename)) {
            continue;
        }
        visible.push_back(event);
        if (visible.size() >= MaxRenderedSuccessEvents) {
            break;
        }
    }
    std::reverse(visible.begin(), visible.end());

    for (const auto& event : visible) {
        QStandardItem* item = new QStandardItem(event.filename);
        item->setData(QVariant::fromValue(event), SuccessRole);
        item->setEditable(false);
        _successModel->appendRow(item);
    }
    if (_successList) {
        _successList->setUpdatesEnabled(true);
        if (stickToBottom) {
            _successList->scrollToBottom();
        }
    }
}

void TranslationWorkbenchPage::_renderErrors()
{
    if (_errorList) {
        _errorList->setUpdatesEnabled(false);
    }
    _errorModel->clear();
    for (const auto& event : _errors) {
        GuiRuntimeErrorEvent displayEvent = event;
        displayEvent.message = compactPreview(displayEvent.message, 240);
        QStandardItem* item = new QStandardItem(displayEvent.message);
        item->setData(QVariant::fromValue(displayEvent), ErrorRole);
        item->setToolTip(displayEvent.message);
        item->setEditable(false);
        _errorModel->appendRow(item);
    }
    if (_errorList) {
        _errorList->setUpdatesEnabled(true);
    }
}

void TranslationWorkbenchPage::_renderFiles()
{
    _fileModel->clear();
    QList<GuiRuntimeFileProgress> files = _files.values();
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b)
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
        _fileModel->appendRow(item);
    }
}

void TranslationWorkbenchPage::_refreshHeader()
{
    int total = 0;
    int completed = 0;
    int problems = 0;
    for (const auto& file : _files) {
        total += file.total;
        completed += file.completed;
        problems += file.problems;
    }
    const QString stage = _stage.isEmpty() ? tr("空闲") : _stage;
    const QString current = _currentFile.isEmpty() ? QString() : tr(" · 当前文件: ") + _currentFile;
    _summaryText->setText(tr("%1%2 · %3/%4 句 · %5 问题 · %6 成功事件 · %7 错误")
        .arg(stage, current)
        .arg(completed).arg(total).arg(problems).arg(_successes.size()).arg(_errors.size()));
    if (_successFileFilters.isEmpty()) {
        _filterText->clear();
        _clearFilterButton->setVisible(false);
    }
    else {
        _filterText->setText(tr("已筛选文件: ") + QStringList(_successFileFilters.values()).join(", "));
        _clearFilterButton->setVisible(true);
    }
    _errorsTabButton->setText(_errors.isEmpty() ? tr("最近错误") : tr("最近错误 (%1)").arg(_errors.size()));
    const int unfinished = std::count_if(_files.begin(), _files.end(), [](const auto& file)
        {
            return file.total <= 0 || file.completed < file.total;
        });
    _filesTabButton->setText(unfinished <= 0 ? tr("文件进度") : tr("文件进度 (%1)").arg(unfinished));
}

void TranslationWorkbenchPage::_trimSuccesses()
{
    while (_successes.size() > MaxSuccessEvents) {
        _successes.pop_back();
    }
}

void TranslationWorkbenchPage::_trimErrors()
{
    while (_errors.size() > MaxErrorEvents) {
        _errors.pop_back();
    }
}
