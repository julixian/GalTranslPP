// NameTableModel.cpp

#include "NameTableModel.h"
#include <QFont>

NameTableModel::NameTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    // 初始化表头
    _headerLabels << "原名" << "译名" << "出现次数" ;
}

// 返回数据行数
int NameTableModel::rowCount(const QModelIndex& parent) const
{
    // 检查 parent 是否有效，对于表格模型，它应始终无效
    if (parent.isValid()) {
        return 0;
    }
    return _entries.count();
}

// 返回列数
int NameTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return _headerLabels.count();
}

// 提供数据给视图
QVariant NameTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= _entries.count()) {
        return QVariant();
    }

    const NameTableEntry& entry = _entries.at(index.row());

    // --- 处理文本显示和编辑 ---
    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch (index.column())
        {
        case 0:   return entry.original;
        case 1:   return entry.translation;
        case 2:   return entry.count;
        default: break;
        }
    }

    if (role == Qt::TextAlignmentRole)
    {
        if (index.column() == 2)
        {
            return Qt::AlignCenter;
        }
    }

    return QVariant();
}

// 提供表头数据
QVariant NameTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        if (section < _headerLabels.count()) {
            return _headerLabels.at(section);
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

// --- 以下是实现可编辑性的关键 ---

// 1. 设置单元格的标志 (Flags)
Qt::ItemFlags NameTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags defaultFlags = QAbstractTableModel::flags(index);

    switch (index.column())
    {
    case 0:
    case 1:
        return defaultFlags | Qt::ItemIsEditable;
    default:
        return defaultFlags;
    }
}

// 2. 实现 setData，当用户完成编辑时，视图会调用此函数
bool NameTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid()) {
        return false;
    }

    NameTableEntry& entry = _entries[index.row()];

    // --- 处理文本编辑变化 ---
    if (role == Qt::EditRole)
    {
        switch (index.column())
        {
        case 0:
            if (entry.original == value.toString()) return false;
            entry.original = value.toString();
            break;
        case 1:
            if (entry.translation == value.toString()) return false;
            entry.translation = value.toString();
            break;
        default:
            return false;
        }
        Q_EMIT dataChanged(index, index, { role, Qt::DisplayRole });
        return true;
    }

    return false;
}

// --- 公共方法 ---

void NameTableModel::loadData(const QList<NameTableEntry>& entries)
{
    // 在修改底层数据结构之前，必须调用 beginResetModel()
    beginResetModel();
    _entries = entries;
    // 修改完成后，调用 endResetModel()
    endResetModel();
}

bool NameTableModel::insertRow(int row, NameTableEntry entry, const QModelIndex& parent)
{
    // 在插入行之前，调用 beginInsertRows()
    beginInsertRows(parent, row, row);

    _entries.insert(row, entry);

    // 插入完成后，调用 endInsertRows()
    endInsertRows();
    return true;
}

bool NameTableModel::removeRow(int row, const QModelIndex& parent)
{
    if (row < 0 || row >= _entries.count()) {
        return false;
    }

    // 在移除行之前，调用 beginRemoveRows()
    beginRemoveRows(parent, row, row);
    _entries.removeAt(row);
    // 移除完成后，调用 endRemoveRows()
    endRemoveRows();
    return true;
}

QList<NameTableEntry> NameTableModel::getEntries() const
{
    return _entries;
}

const QList<NameTableEntry>& NameTableModel::getEntriesRef() const
{
    return _entries;
}
