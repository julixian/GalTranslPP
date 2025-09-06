// DictionaryModel.cpp

#include "DictionaryModel.h"
#include <QFont>

DictionaryModel::DictionaryModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    // 初始化表头
    _headerLabels << "原文" << "译文" << "描述";
}

// 返回数据行数
int DictionaryModel::rowCount(const QModelIndex& parent) const
{
    // 检查 parent 是否有效，对于表格模型，它应始终无效
    if (parent.isValid()) {
        return 0;
    }
    return _entries.count();
}

// 返回列数
int DictionaryModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return _headerLabels.count();
}

// 提供数据给视图
QVariant DictionaryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    // 确保行和列在有效范围内
    if (index.row() >= _entries.count() || index.column() >= _headerLabels.count()) {
        return QVariant();
    }

    // --- 核心逻辑：根据 'role' 提供不同的数据 ---
    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        const DictionaryEntry& entry = _entries.at(index.row());
        switch (index.column())
        {
        case 0: return entry.original;
        case 1: return entry.translation;
        case 2: return entry.description;
        default: break;
        }
    }

    // 可以为特定单元格设置字体、颜色等
    // if (role == Qt::FontRole && index.column() == 0) {
    //     QFont font;
    //     font.setBold(true);
    //     return font;
    // }

    return QVariant();
}

// 提供表头数据
QVariant DictionaryModel::headerData(int section, Qt::Orientation orientation, int role) const
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
Qt::ItemFlags DictionaryModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    // 获取默认标志并添加 ItemIsEditable 使其可编辑
    return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
}

// 2. 实现 setData，当用户完成编辑时，视图会调用此函数
bool DictionaryModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || role != Qt::EditRole) {
        return false;
    }

    if (index.row() >= _entries.count() || index.column() >= _headerLabels.count()) {
        return false;
    }

    DictionaryEntry& entry = _entries[index.row()];
    QString textValue = value.toString();

    // 根据列更新对应的数据
    switch (index.column())
    {
    case 0:
        if (entry.original == textValue) return false;
        entry.original = textValue;
        break;
    case 1:
        if (entry.translation == textValue) return false;
        entry.translation = textValue;
        break;
    case 2:
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

// --- 公共方法 ---

void DictionaryModel::loadData(const QList<DictionaryEntry>& entries)
{
    // 在修改底层数据结构之前，必须调用 beginResetModel()
    beginResetModel();
    _entries = entries;
    // 修改完成后，调用 endResetModel()
    endResetModel();
}

bool DictionaryModel::insertRow(int row, const QModelIndex& parent)
{
    // 在插入行之前，调用 beginInsertRows()
    beginInsertRows(parent, row, row);

    DictionaryEntry emptyEntry = { "", "", "" };
    _entries.insert(row, emptyEntry);

    // 插入完成后，调用 endInsertRows()
    endInsertRows();
    return true;
}

bool DictionaryModel::removeRow(int row, const QModelIndex& parent)
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

QList<DictionaryEntry> DictionaryModel::getEntries() const
{
    return _entries;
}
