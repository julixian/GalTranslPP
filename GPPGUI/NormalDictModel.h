#ifndef NORMALDICTMODEL_H
#define NORMALDICTMODEL_H

#include <QAbstractTableModel>
#include <QList>

struct NormalCondition
{
    QString pattern;
    QString target = "orig";
    int sentenceOffset = 0;

    bool operator==(const NormalCondition&) const = default;
};

struct NormalDictEntry
{
    QString original;
    QString translation;
    QList<NormalCondition> conditions;
    int priority = 0;
    bool isReg = false;

    bool operator==(const NormalDictEntry&) const = default;
};

QString serializeNormalConditionTarget(const NormalCondition& condition);
QString normalConditionsSummary(const QList<NormalCondition>& conditions);

class NormalDictModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column : int {
        DragHandle = 0,
        Original,
        Translation,
        Conditions,
        IsReg,
        Priority,
        ColumnCount
    };

    explicit NormalDictModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
        const QModelIndex& destinationParent, int destinationChild) override;

    void loadData(const QList<NormalDictEntry>& entries);
    bool insertRow(int row, NormalDictEntry entry = {}, const QModelIndex& parent = QModelIndex());
    bool removeRow(int row, const QModelIndex& parent = QModelIndex());
    bool setEntry(int row, NormalDictEntry entry);
    QList<NormalDictEntry> getEntries() const;
    const QList<NormalDictEntry>& getEntriesRef() const;

private:
    QList<NormalDictEntry> m_entries;
    QStringList m_headerLabels;
};

#endif
