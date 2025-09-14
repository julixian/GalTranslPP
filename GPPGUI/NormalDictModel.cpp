// NormalDictModel.cpp

#include "NormalDictModel.h"
#include <QFont>

NormalDictModel::NormalDictModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    // 初始化表头
    _headerLabels << "原文" << "译文" << "条件对象" << "条件正则" << "启用正则" << "优先级";
    Q_ASSERT(_headerLabels.count() == Column::ColumnCount); // 确保枚举和表头数量一致
}

// 返回数据行数
int NormalDictModel::rowCount(const QModelIndex& parent) const
{
    // 检查 parent 是否有效，对于表格模型，它应始终无效
    if (parent.isValid()) {
        return 0;
    }
    return _entries.count();
}

// 返回列数
int NormalDictModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return Column::ColumnCount;
}

// 提供数据给视图
QVariant NormalDictModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= _entries.count()) {
        return QVariant();
    }

    const NormalDictEntry& entry = _entries.at(index.row());

    // --- 处理文本显示和编辑 ---
    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch (index.column())
        {
        case Column::Original:      return entry.original;
        case Column::Translation:   return entry.translation;
        case Column::ConditionTar:   return entry.conditionTar;
        case Column::ConditionReg:   return entry.conditionReg;
        case Column::IsReg:         return entry.isReg ? "true" : "false";
        case Column::Priority:      return entry.priority;
        default: break;
        }
    }

    if (role == Qt::TextAlignmentRole)
    {
        if (index.column() == Column::Priority || index.column() == Column::IsReg)
        {
            return Qt::AlignCenter;
        }
    }

    return QVariant();
}

// 提供表头数据
QVariant NormalDictModel::headerData(int section, Qt::Orientation orientation, int role) const
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
Qt::ItemFlags NormalDictModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags defaultFlags = QAbstractTableModel::flags(index);

    switch (index.column())
    {
    case Column::IsReg:
    case Column::Original:
    case Column::Translation:
    case Column::ConditionTar:
    case Column::ConditionReg:
    case Column::Priority:
        return defaultFlags | Qt::ItemIsEditable;
    default:
        return defaultFlags;
    }
}

// 2. 实现 setData，当用户完成编辑时，视图会调用此函数
bool NormalDictModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid()) {
        return false;
    }

    NormalDictEntry& entry = _entries[index.row()];

    // --- 处理文本编辑变化 ---
    if (role == Qt::EditRole)
    {
        switch (index.column())
        {
        case Column::Original:
            if (entry.original == value.toString()) return false;
            entry.original = value.toString();
            break;
        case Column::Translation:
            if (entry.translation == value.toString()) return false;
            entry.translation = value.toString();
            break;
        case Column::ConditionTar:
            if (entry.conditionTar == value.toString()) return false;
            entry.conditionTar = value.toString();
            break;
        case Column::ConditionReg:
            if (entry.conditionReg == value.toString()) return false;
            entry.conditionReg = value.toString();
            break;
        case Column::IsReg:
        {
            bool isReg = value.toBool();
            if (entry.isReg == isReg) return false;
            entry.isReg = isReg;
            break;
        }
        case Column::Priority:
        {
            bool ok;
            int priority = value.toInt(&ok);
            // 输入验证：确保输入的是有效的数字
            if (!ok || entry.priority == priority) return false;
            entry.priority = priority;
            break;
        }
        default:
            return false;
        }
        Q_EMIT dataChanged(index, index, { role, Qt::DisplayRole });
        return true;
    }

    return false;
}

// --- 公共方法 ---

void NormalDictModel::loadData(const QList<NormalDictEntry>& entries)
{
    // 在修改底层数据结构之前，必须调用 beginResetModel()
    beginResetModel();
    _entries = entries;
    // 修改完成后，调用 endResetModel()
    endResetModel();
}

bool NormalDictModel::insertRow(int row, NormalDictEntry entry, const QModelIndex& parent)
{
    // 在插入行之前，调用 beginInsertRows()
    beginInsertRows(parent, row, row);

    _entries.insert(row, entry);

    // 插入完成后，调用 endInsertRows()
    endInsertRows();
    return true;
}

bool NormalDictModel::removeRow(int row, const QModelIndex& parent)
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

QList<NormalDictEntry> NormalDictModel::getEntries() const
{
    return _entries;
}

const QList<NormalDictEntry>& NormalDictModel::getEntriesRef() const
{
    return _entries;
}
