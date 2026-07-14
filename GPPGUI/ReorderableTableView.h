#ifndef REORDERABLETABLEVIEW_H
#define REORDERABLETABLEVIEW_H

#include "ElaTableView.h"

class QAbstractItemModel;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QMimeData;
class QMouseEvent;
class QPaintEvent;

class ReorderableTableView : public ElaTableView
{
    Q_OBJECT

public:
    static constexpr int HandleColumn = 0;
    static constexpr int HandleColumnWidth = 34;

    explicit ReorderableTableView(QWidget* parent = nullptr);

    void setModel(QAbstractItemModel* model) override;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool isHandleIndex(const QModelIndex& index) const;
    bool readSourceRow(const QMimeData* mimeData, int& sourceRow) const;
    int insertionRowAt(const QPoint& position) const;
    int insertionLineY(int insertionRow) const;
    void setDropRow(int row);
    void resetDragState();
    void updateHandleCursor(const QPoint& position, bool pressed = false);

    QPoint m_pressPosition;
    int m_dragSourceRow = -1;
    int m_dropRow = -1;
    bool m_dragArmed = false;
};

#endif
