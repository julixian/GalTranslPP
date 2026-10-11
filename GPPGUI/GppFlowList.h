#pragma once

#include <QList>
#include <QPoint>
#include <QWidget>

class ElaFlowLayout;

// 自动换行的卡片列表；拖动手柄排序，卡片中的按钮仍按普通点击处理。
class GppFlowList : public QWidget
{
    Q_OBJECT

public:
    explicit GppFlowList(QWidget* parent = nullptr);
    void setCards(const QList<QWidget*>& cards);
    void setDragHandle(QWidget* card, QWidget* handle);

Q_SIGNALS:
    void cardMoved(int from, int to);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    int insertionIndex(const QPoint& position) const;

    ElaFlowLayout* m_layout;
    QList<QWidget*> m_cards;
    QPoint m_dragStart;
    bool m_dragPressed = false;
    int m_dropIndex = -1;
};
