#ifndef GPTDICTMODEL_H
#define GPTDICTMODEL_H

#include <QAbstractTableModel>
#include <QList>

// 定义一个结构体来表示一条字典记录
struct GptDictEntry
{
    QString original;
    QString translation;
    QString description;
};

class GptDictModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit GptDictModel(QObject* parent = nullptr);

    // --- 必须重写的核心虚函数 ---
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // --- 实现可编辑性所需重写的函数 ---
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    // --- 用于操作模型的公共方法 ---
    void loadData(const QList<GptDictEntry>& entries); // 从外部加载数据
    bool insertRow(int row, const GptDictEntry& entry = {}, const QModelIndex& parent = QModelIndex());
    bool removeRow(int row, const QModelIndex& parent = QModelIndex());
    QList<GptDictEntry> getEntries() const;
    const QList<GptDictEntry>& getEntriesRef() const;

private:
    QList<GptDictEntry> m_entries; // 存储所有字典条目的列表
    QStringList m_headerLabels;       // 存储表头标题
};

#endif
