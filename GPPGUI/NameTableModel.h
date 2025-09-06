// NameTableModel.h

#ifndef NAMETABLEMODEL_H
#define NAMETABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QStringList>

// 定义一个结构体来表示一条字典记录
struct NameTableEntry
{
    QString original;
    QString translation;
    int count;
};

class NameTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit NameTableModel(QObject* parent = nullptr);

    // --- 必须重写的核心虚函数 ---
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // --- 实现可编辑性所需重写的函数 ---
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    // --- 用于操作模型的公共方法 ---
    void loadData(const QList<NameTableEntry>& entries); // 从外部加载数据
    bool insertRow(int row, const QModelIndex& parent = QModelIndex());
    bool removeRow(int row, const QModelIndex& parent = QModelIndex());
    QList<NameTableEntry> getEntries() const;

private:
    QList<NameTableEntry> _entries; // 存储所有字典条目的列表
    QStringList _headerLabels;       // 存储表头标题
};

#endif // NAMETABLEMODEL_H
