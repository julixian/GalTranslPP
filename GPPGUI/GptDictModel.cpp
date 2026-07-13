#include "GptDictModel.h"

#include <utility>
GptDictModel::GptDictModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    m_headerLabels << QString{} << tr("原文") << tr("译文") << tr("描述");
    Q_ASSERT(m_headerLabels.count() == Column::ColumnCount);
}

// 返回数据行数
int GptDictModel::rowCount(const QModelIndex& parent) const
{
    // 检查 parent 是否有效，对于表格模型，它应始终无效
    if (parent.isValid()) {
        return 0;
    }
    return (int)m_entries.count();
}

// 返回列数
int GptDictModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return (int)m_headerLabels.count();
}

// 提供数据给视图
QVariant GptDictModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    // 确保行和列在有效范围内
    if (index.row() >= m_entries.count() || index.column() >= m_headerLabels.count()) {
        return {};
    }

    // --- 核心逻辑：根据 'role' 提供不同的数据 ---
    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        const GptDictEntry& entry = m_entries.at(index.row());
        switch (index.column())
        {
        case Column::Original: return entry.original;
        case Column::Translation: return entry.translation;
        case Column::Description: return entry.description;
        default: break;
        }
    }

    if (role == Qt::ToolTipRole && index.column() == Column::DragHandle) {
        return tr("拖动调整顺序");
    }

    // 可以为特定单元格设置字体、颜色等
    // if (role == Qt::FontRole && index.column() == 0) {
    //     QFont font;
    //     font.setBold(true);
    //     return font;
    // }

    return {};
}

// 提供表头数据
QVariant GptDictModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        if (section < m_headerLabels.count()) {
            return m_headerLabels.at(section);
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

// --- 以下是实现可编辑性的关键 ---

// 1. 设置单元格的标志 (Flags)
Qt::ItemFlags GptDictModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags itemFlags = QAbstractTableModel::flags(index);
    if (index.column() == Column::DragHandle) {
        return itemFlags | Qt::ItemIsDragEnabled;
    }

    return itemFlags;
}

// 2. 实现 setData，当用户完成编辑时，视图会调用此函数
bool GptDictModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || role != Qt::EditRole) {
        return false;
    }

    if (index.row() >= m_entries.count() || index.column() >= m_headerLabels.count()) {
        return false;
    }

    GptDictEntry& entry = m_entries[index.row()];
    QString textValue = value.toString();

    // 根据列更新对应的数据
    switch (index.column())
    {
    case Column::Original:
        if (entry.original == textValue) return false;
        entry.original = textValue;
        break;
    case Column::Translation:
        if (entry.translation == textValue) return false;
        entry.translation = textValue;
        break;
    case Column::Description:
        if (entry.description == textValue) return false;
        entry.description = textValue;
        break;
    default:
        return false;
    }

    // *** 关键：发出 dataChanged 信号 ***
    // 通知所有连接到此模型的视图，指定单元格的数据已更改，需要重绘
    Q_EMIT dataChanged(index, index, { Qt::DisplayRole, Qt::EditRole });

    return true;
}

bool GptDictModel::moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
    const QModelIndex& destinationParent, int destinationChild)
{
    if (sourceParent.isValid() || destinationParent.isValid() || count <= 0
        || sourceRow < 0 || sourceRow + count > m_entries.size()
        || destinationChild < 0 || destinationChild > m_entries.size()
        || (destinationChild >= sourceRow && destinationChild <= sourceRow + count)) {
        return false;
    }

    if (!beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1, destinationParent, destinationChild)) {
        return false;
    }

    QList<GptDictEntry> movedEntries;
    movedEntries.reserve(count);
    for (int i = 0; i < count; ++i) {
        movedEntries.push_back(m_entries.takeAt(sourceRow));
    }
    const int insertRow = destinationChild > sourceRow ? destinationChild - count : destinationChild;
    for (int i = 0; i < movedEntries.size(); ++i) {
        m_entries.insert(insertRow + i, std::move(movedEntries[i]));
    }

    endMoveRows();
    return true;
}

// --- 公共方法 ---

void GptDictModel::loadData(const QList<GptDictEntry>& entries)
{
    // 在修改底层数据结构之前，必须调用 beginResetModel()
    beginResetModel();
    m_entries = entries;
    // 修改完成后，调用 endResetModel()
    endResetModel();
}

bool GptDictModel::insertRow(int row, const GptDictEntry& entry, const QModelIndex& parent)
{
    if (parent.isValid() || row < 0 || row > m_entries.count()) {
        return false;
    }
    // 在插入行之前，调用 beginInsertRows()
    beginInsertRows(parent, row, row);

    m_entries.insert(row, entry);

    // 插入完成后，调用 endInsertRows()
    endInsertRows();
    return true;
}

bool GptDictModel::removeRow(int row, const QModelIndex& parent)
{
    if (parent.isValid() || row < 0 || row >= m_entries.count()) {
        return false;
    }

    // 在移除行之前，调用 beginRemoveRows()
    beginRemoveRows(parent, row, row);
    m_entries.removeAt(row);
    // 移除完成后，调用 endRemoveRows()
    endRemoveRows();
    return true;
}

bool GptDictModel::setEntry(int row, const GptDictEntry& entry)
{
    if (row < 0 || row >= m_entries.count()
        || (m_entries[row].original == entry.original
            && m_entries[row].translation == entry.translation
            && m_entries[row].description == entry.description)) {
        return false;
    }
    m_entries[row] = entry;
    Q_EMIT dataChanged(index(row, 0), index(row, Column::ColumnCount - 1),
        { Qt::DisplayRole, Qt::EditRole });
    return true;
}

QList<GptDictEntry> GptDictModel::getEntries() const
{
    return m_entries;
}

const QList<GptDictEntry>& GptDictModel::getEntriesRef() const
{
    return m_entries;
}
