#ifndef DICTIONARYSEARCHBAR_H
#define DICTIONARYSEARCHBAR_H

#include <QWidget>

class ElaLineEdit;
class ElaToolButton;
class QTableView;
class QTimer;

class DictionarySearchBar final : public QWidget
{
    Q_OBJECT

public:
    explicit DictionarySearchBar(QTableView* tableView, const QString& detailLabel,
        QWidget* parent = nullptr);

    void refresh();

private:
    void applyFilter();

    QTableView* m_tableView = nullptr;
    ElaLineEdit* m_searchEdit = nullptr;
    ElaToolButton* m_fieldButton = nullptr;
    QTimer* m_timer = nullptr;
    int m_field = 0;
};

#endif
