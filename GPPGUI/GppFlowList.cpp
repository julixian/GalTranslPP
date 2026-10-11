#include "GppFlowList.h"

#include "ElaFlowLayout.h"
#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>

GppFlowList::GppFlowList(QWidget* parent) : QWidget(parent)
{
    m_layout = new ElaFlowLayout(this, 8, 8, 8);
    m_layout->setIsAnimation(false);
    setAcceptDrops(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void GppFlowList::setCards(const QList<QWidget*>& cards)
{
    while (auto* item = m_layout->takeAt(0)) {
        delete item;
    }
    m_cards = cards;
    for (QWidget* card : m_cards) {
        m_layout->addWidget(card);
        card->show();
    }
    m_layout->invalidate();
    updateGeometry();
}

void GppFlowList::setDragHandle(QWidget* card, QWidget* handle)
{
    handle->setProperty("flowCard", QVariant::fromValue(card));
    handle->setCursor(Qt::OpenHandCursor);
    handle->installEventFilter(this);
}

bool GppFlowList::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            m_dragStart = mouse->position().toPoint();
            m_dragPressed = true;
        }
        // 手柄内的点击只用于准备拖拽，不能落到父卡片的菜单按钮上。
        mouse->accept();
        return true;
    }
    else if (event->type() == QEvent::MouseButtonRelease) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) m_dragPressed = false;
        mouse->accept();
        return true;
    }
    else if (event->type() == QEvent::MouseMove) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (m_dragPressed && mouse->buttons().testFlag(Qt::LeftButton) &&
            (mouse->position().toPoint() - m_dragStart).manhattanLength() >= QApplication::startDragDistance()) {
            QWidget* card = watched->property("flowCard").value<QWidget*>();
            const int index = m_cards.indexOf(card);
            m_dragPressed = false;
            if (index < 0) return true;
            QDrag drag(this);
            auto* mime = new QMimeData();
            mime->setData("application/x-gpp-flow-card", QByteArray::number(index));
            drag.setMimeData(mime);
            drag.setPixmap(card->grab());
            drag.setHotSpot(QPoint(12, card->height() / 2));
            drag.exec(Qt::MoveAction);
            m_dropIndex = -1;
            update();
            return true;
        }
        mouse->accept();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void GppFlowList::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->source() == this && event->mimeData()->hasFormat("application/x-gpp-flow-card")) {
        event->acceptProposedAction();
    }
}

int GppFlowList::insertionIndex(const QPoint& position) const
{
    // 先按换行后的纵向位置定位，再用卡片中线区分插到前面还是后面。
    for (int i = 0; i < m_cards.size(); ++i) {
        const QRect rect = m_cards.at(i)->geometry();
        if (position.y() < rect.top() ||
            (position.y() <= rect.bottom() && position.x() < rect.center().x())) {
            return i;
        }
    }
    return m_cards.size();
}

void GppFlowList::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->source() != this) return;
    m_dropIndex = insertionIndex(event->position().toPoint());
    update();
    event->acceptProposedAction();
}

void GppFlowList::dragLeaveEvent(QDragLeaveEvent* event)
{
    m_dropIndex = -1;
    update();
    event->accept();
}

void GppFlowList::dropEvent(QDropEvent* event)
{
    if (event->source() != this) return;
    const int from = event->mimeData()->data("application/x-gpp-flow-card").toInt();
    int to = insertionIndex(event->position().toPoint());
    if (to > from) --to;
    m_dropIndex = -1;
    update();
    event->acceptProposedAction();
    if (from != to) Q_EMIT cardMoved(from, to);
}

void GppFlowList::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    if (m_dropIndex < 0 || m_cards.empty()) return;
    const bool atEnd = m_dropIndex == m_cards.size();
    const QRect rect = (atEnd ? m_cards.back() : m_cards.at(m_dropIndex))->geometry();
    const int x = atEnd ? rect.right() + 4 : rect.left() - 4;
    QPainter painter(this);
    painter.setPen(QPen(palette().color(QPalette::Highlight), 2));
    painter.drawLine(x, rect.top(), x, rect.bottom());
}
