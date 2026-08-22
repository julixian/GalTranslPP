#include "ReorderableTableView.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QDataStream>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFont>
#include <QHeaderView>
#include <QIODevice>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>

#include "ElaDef.h"
#include "ElaTheme.h"

namespace
{
    constexpr auto kRowMimeType = "application/x-galtranslpp-table-row";

    class DragHandleDelegate final : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QStyleOptionViewItem backgroundOption(option);
            initStyleOption(&backgroundOption, index);
            backgroundOption.text.clear();
            backgroundOption.icon = {};

            const QStyle* style = backgroundOption.widget
                ? backgroundOption.widget->style()
                : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &backgroundOption, painter, backgroundOption.widget);

            painter->save();
            painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
            QFont iconFont("ElaAwesome");
            iconFont.setPixelSize(15);
            painter->setFont(iconFont);
            painter->setPen(index.flags().testFlag(Qt::ItemIsEnabled)
                ? ElaThemeColor(eTheme->getThemeMode(), BasicDetailsText)
                : ElaThemeColor(eTheme->getThemeMode(), BasicTextDisable));
            painter->drawText(option.rect, Qt::AlignCenter,
                QChar((unsigned short)ElaIconType::GripVertical));
            painter->restore();
        }

        QWidget* createEditor(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const override
        {
            return nullptr;
        }
    };
}

ReorderableTableView::ReorderableTableView(QWidget* parent)
    : ElaTableView(parent)
{
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setDragDropOverwriteMode(false);
    setDropIndicatorShown(false);
    setAutoScroll(true);
    setItemDelegateForColumn(kHandleColumn, new DragHandleDelegate(this));
}

void ReorderableTableView::setModel(QAbstractItemModel* model)
{
    ElaTableView::setModel(model);
    if (model && model->columnCount() > kHandleColumn) {
        horizontalHeader()->setSectionResizeMode(kHandleColumn, QHeaderView::Fixed);
        setColumnWidth(kHandleColumn, kHandleColumnWidth);
    }
}

void ReorderableTableView::mousePressEvent(QMouseEvent* event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    m_dragArmed = event->button() == Qt::LeftButton && isHandleIndex(index);
    m_dragSourceRow = m_dragArmed ? index.row() : -1;
    m_pressPosition = event->position().toPoint();

    ElaTableView::mousePressEvent(event);

    if (m_dragArmed) {
        viewport()->setCursor(Qt::ClosedHandCursor);
    }
}

void ReorderableTableView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragArmed && event->buttons().testFlag(Qt::LeftButton)) {
        if ((event->position().toPoint() - m_pressPosition).manhattanLength()
            >= QApplication::startDragDistance()) {
            startDrag(Qt::MoveAction);
        }
        return;
    }

    updateHandleCursor(event->position().toPoint());
    ElaTableView::mouseMoveEvent(event);
}

void ReorderableTableView::mouseReleaseEvent(QMouseEvent* event)
{
    ElaTableView::mouseReleaseEvent(event);
    m_dragArmed = false;
    m_dragSourceRow = -1;
    updateHandleCursor(event->position().toPoint());
}

void ReorderableTableView::startDrag(Qt::DropActions supportedActions)
{
    if (!m_dragArmed || !(supportedActions & Qt::MoveAction) || !model()
        || m_dragSourceRow < 0 || m_dragSourceRow >= model()->rowCount()) {
        return;
    }

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream << m_dragSourceRow;

    auto* mimeData = new QMimeData();
    mimeData->setData(kRowMimeType, payload);

    QDrag drag(this);
    drag.setMimeData(mimeData);
    drag.exec(Qt::MoveAction, Qt::MoveAction);

    resetDragState();
}

void ReorderableTableView::dragEnterEvent(QDragEnterEvent* event)
{
    ElaTableView::dragEnterEvent(event);
    int sourceRow = -1;
    if (event->source() == this && readSourceRow(event->mimeData(), sourceRow)) {
        event->setDropAction(Qt::MoveAction);
        event->accept();
        viewport()->setCursor(Qt::ClosedHandCursor);
    }
}

void ReorderableTableView::dragMoveEvent(QDragMoveEvent* event)
{
    ElaTableView::dragMoveEvent(event);

    int sourceRow = -1;
    if (event->source() != this || !readSourceRow(event->mimeData(), sourceRow)) {
        setDropRow(-1);
        event->ignore();
        return;
    }

    const int targetRow = insertionRowAt(event->position().toPoint());
    if (targetRow == sourceRow || targetRow == sourceRow + 1) {
        setDropRow(-1);
        event->ignore();
        return;
    }

    setDropRow(targetRow);
    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void ReorderableTableView::dragLeaveEvent(QDragLeaveEvent* event)
{
    ElaTableView::dragLeaveEvent(event);
    setDropRow(-1);
    viewport()->unsetCursor();
}

void ReorderableTableView::dropEvent(QDropEvent* event)
{
    int sourceRow = -1;
    if (event->source() != this || !model() || !readSourceRow(event->mimeData(), sourceRow)) {
        event->ignore();
        resetDragState();
        return;
    }

    const int destinationChild = insertionRowAt(event->position().toPoint());
    if (destinationChild == sourceRow || destinationChild == sourceRow + 1) {
        event->ignore();
        resetDragState();
        return;
    }

    const bool moved = model()->moveRows(QModelIndex(), sourceRow, 1,
        QModelIndex(), destinationChild);
    if (!moved) {
        event->ignore();
        resetDragState();
        return;
    }

    const int movedRow = destinationChild > sourceRow
        ? destinationChild - 1
        : destinationChild;
    const QModelIndex movedIndex = model()->index(movedRow, kHandleColumn);
    setCurrentIndex(movedIndex);
    selectRow(movedRow);
    scrollTo(movedIndex, QAbstractItemView::EnsureVisible);

    event->setDropAction(Qt::MoveAction);
    event->accept();
    resetDragState();
}

void ReorderableTableView::paintEvent(QPaintEvent* event)
{
    ElaTableView::paintEvent(event);
    if (m_dropRow < 0 || !model()) {
        return;
    }

    const int lineY = insertionLineY(m_dropRow);
    if (lineY < 0 || lineY > viewport()->height()) {
        return;
    }

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(ElaThemeColor(eTheme->getThemeMode(), PrimaryNormal), 2);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(3, lineY, qMax(3, viewport()->width() - 4), lineY);
}

bool ReorderableTableView::isHandleIndex(const QModelIndex& index) const
{
    return index.isValid() && index.column() == kHandleColumn
        && index.flags().testFlag(Qt::ItemIsEnabled);
}

bool ReorderableTableView::readSourceRow(const QMimeData* mimeData, int& sourceRow) const
{
    if (!mimeData || !mimeData->hasFormat(kRowMimeType)) {
        return false;
    }

    QByteArray payload = mimeData->data(kRowMimeType);
    QDataStream stream(&payload, QIODevice::ReadOnly);
    qint32 row = -1;
    stream >> row;
    if (stream.status() != QDataStream::Ok || !model()
        || row < 0 || row >= model()->rowCount()) {
        return false;
    }

    sourceRow = row;
    return true;
}

int ReorderableTableView::insertionRowAt(const QPoint& position) const
{
    if (!model() || model()->rowCount() == 0) {
        return 0;
    }

    const int hoveredRow = rowAt(position.y());
    if (hoveredRow >= 0) {
        const QRect rowRect = visualRect(model()->index(hoveredRow, kHandleColumn));
        return position.y() < rowRect.center().y()
            ? hoveredRow
            : hoveredRow + 1;
    }

    const QRect firstRowRect = visualRect(model()->index(0, kHandleColumn));
    if (position.y() < firstRowRect.top()) {
        return 0;
    }
    return model()->rowCount();
}

int ReorderableTableView::insertionLineY(int insertionRow) const
{
    const int rowCount = model() ? model()->rowCount() : 0;
    if (rowCount == 0) {
        return 1;
    }

    if (insertionRow <= 0) {
        return visualRect(model()->index(0, kHandleColumn)).top();
    }
    if (insertionRow >= rowCount) {
        return visualRect(model()->index(rowCount - 1, kHandleColumn)).bottom() + 1;
    }
    return visualRect(model()->index(insertionRow, kHandleColumn)).top();
}

void ReorderableTableView::setDropRow(int row)
{
    if (m_dropRow == row) {
        return;
    }
    m_dropRow = row;
    viewport()->update();
}

void ReorderableTableView::resetDragState()
{
    m_dragArmed = false;
    m_dragSourceRow = -1;
    setDropRow(-1);
    viewport()->unsetCursor();
}

void ReorderableTableView::updateHandleCursor(const QPoint& position, bool pressed)
{
    if (isHandleIndex(indexAt(position))) {
        viewport()->setCursor(pressed ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
    }
    else {
        viewport()->unsetCursor();
    }
}
