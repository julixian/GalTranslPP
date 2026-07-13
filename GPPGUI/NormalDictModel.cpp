#include "NormalDictModel.h"

#include <utility>

QString serializeNormalConditionTarget(const NormalCondition& condition)
{
    QString target = condition.target;
    const QString prefix = condition.sentenceOffset < 0 ? "prev_" : "next_";
    for (int i = 0; i < qAbs(condition.sentenceOffset); ++i) {
        target.prepend(prefix);
    }
    return target;
}

QString normalConditionsSummary(const QList<NormalCondition>& conditions)
{
    QStringList parts;
    parts.reserve(conditions.size());
    for (const NormalCondition& condition : conditions) {
        parts.push_back(QString("%1->%2").arg(condition.pattern, serializeNormalConditionTarget(condition)));
    }
    return parts.join("; ");
}

NormalDictModel::NormalDictModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    m_headerLabels << QString{} << tr("原文") << tr("译文") << tr("条件") << tr("启用正则") << tr("优先级");
    Q_ASSERT(m_headerLabels.count() == Column::ColumnCount);
}

int NormalDictModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : (int)m_entries.count();
}

int NormalDictModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : Column::ColumnCount;
}

QVariant NormalDictModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.count()) {
        return {};
    }

    const NormalDictEntry& entry = m_entries.at(index.row());
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case Column::Original: return entry.original;
        case Column::Translation: return entry.translation;
        case Column::Conditions: return normalConditionsSummary(entry.conditions);
        case Column::IsReg: return entry.isReg ? "true" : "false";
        case Column::Priority: return entry.priority;
        default: return {};
        }
    }

    if (role == Qt::ToolTipRole) {
        if (index.column() == Column::DragHandle) {
            return tr("拖动调整顺序");
        }
        if (index.column() == Column::Conditions) {
            return normalConditionsSummary(entry.conditions);
        }
    }

    if (role == Qt::TextAlignmentRole
        && (index.column() == Column::DragHandle || index.column() == Column::IsReg || index.column() == Column::Priority)) {
        return Qt::AlignCenter;
    }

    return {};
}

QVariant NormalDictModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal
        && section >= 0 && section < m_headerLabels.count()) {
        return m_headerLabels.at(section);
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

Qt::ItemFlags NormalDictModel::flags(const QModelIndex& index) const
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

bool NormalDictModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.count() || role != Qt::EditRole) {
        return false;
    }

    NormalDictEntry& entry = m_entries[index.row()];
    switch (index.column()) {
    case Column::Original:
        if (entry.original == value.toString()) return false;
        entry.original = value.toString();
        break;
    case Column::Translation:
        if (entry.translation == value.toString()) return false;
        entry.translation = value.toString();
        break;
    case Column::IsReg:
    {
        const bool isReg = value.toBool();
        if (entry.isReg == isReg) return false;
        entry.isReg = isReg;
        break;
    }
    case Column::Priority:
    {
        bool ok = false;
        const int priority = value.toInt(&ok);
        if (!ok || entry.priority == priority) return false;
        entry.priority = priority;
        break;
    }
    default:
        return false;
    }

    Q_EMIT dataChanged(index, index, { Qt::DisplayRole, Qt::EditRole });
    return true;
}

bool NormalDictModel::moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
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

    QList<NormalDictEntry> movedEntries;
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

void NormalDictModel::loadData(const QList<NormalDictEntry>& entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

bool NormalDictModel::insertRow(int row, NormalDictEntry entry, const QModelIndex& parent)
{
    if (parent.isValid() || row < 0 || row > m_entries.count()) {
        return false;
    }
    beginInsertRows(parent, row, row);
    m_entries.insert(row, std::move(entry));
    endInsertRows();
    return true;
}

bool NormalDictModel::removeRow(int row, const QModelIndex& parent)
{
    if (parent.isValid() || row < 0 || row >= m_entries.count()) {
        return false;
    }
    beginRemoveRows(parent, row, row);
    m_entries.removeAt(row);
    endRemoveRows();
    return true;
}

bool NormalDictModel::setEntry(int row, NormalDictEntry entry)
{
    if (row < 0 || row >= m_entries.count() || m_entries[row] == entry) {
        return false;
    }
    m_entries[row] = std::move(entry);
    Q_EMIT dataChanged(index(row, 0), index(row, Column::ColumnCount - 1),
        { Qt::DisplayRole, Qt::EditRole, Qt::ToolTipRole });
    return true;
}

QList<NormalDictEntry> NormalDictModel::getEntries() const
{
    return m_entries;
}

const QList<NormalDictEntry>& NormalDictModel::getEntriesRef() const
{
    return m_entries;
}
