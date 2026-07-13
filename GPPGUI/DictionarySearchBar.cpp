#include "DictionarySearchBar.h"

#include <QAbstractItemModel>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QStringList>
#include <QTableView>
#include <QTimer>

#include "ElaLineEdit.h"
#include "ElaMenu.h"
#include "ElaToolButton.h"

DictionarySearchBar::DictionarySearchBar(QTableView* tableView, const QString& detailLabel,
    QWidget* parent)
    : QWidget(parent), m_tableView(tableView)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_searchEdit = new ElaLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("搜索字典..."));
    m_searchEdit->setIsClearButtonEnable(true);
    layout->addWidget(m_searchEdit, 1);

    m_fieldButton = new ElaToolButton(this);
    m_fieldButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_fieldButton->setElaIcon(ElaIconType::BarsFilter);
    m_fieldButton->setText(tr("全部"));
    m_fieldButton->setFixedWidth(100);
    m_fieldButton->setPopupMode(QToolButton::InstantPopup);
    layout->addWidget(m_fieldButton);

    ElaMenu* fieldMenu = new ElaMenu(m_fieldButton);
    fieldMenu->setFixedWidth(100);
    QActionGroup* fieldGroup = new QActionGroup(fieldMenu);
    fieldGroup->setExclusive(true);
    const QStringList labels = { tr("全部"), tr("原文"), tr("译文"), detailLabel };
    for (int field = 0; field < labels.size(); ++field) {
        QAction* action = fieldMenu->addAction(labels.at(field));
        action->setCheckable(true);
        action->setChecked(field == 0);
        action->setData(field);
        fieldGroup->addAction(action);
    }
    m_fieldButton->setMenu(fieldMenu);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(200);
    connect(m_timer, &QTimer::timeout, this, [this]() { applyFilter(); });
    connect(m_searchEdit, &ElaLineEdit::textChanged, this, [this]() { refresh(); });
    connect(fieldGroup, &QActionGroup::triggered, this, [this](QAction* action)
        {
            m_field = action->data().toInt();
            m_fieldButton->setText(action->text());
            refresh();
        });

    if (m_tableView && m_tableView->model()) {
        QAbstractItemModel* model = m_tableView->model();
        connect(model, &QAbstractItemModel::modelReset, this, [this]() { refresh(); });
        connect(model, &QAbstractItemModel::rowsInserted, this, [this]() { refresh(); });
        connect(model, &QAbstractItemModel::rowsRemoved, this, [this]() { refresh(); });
        connect(model, &QAbstractItemModel::rowsMoved, this, [this]() { refresh(); });
        connect(model, &QAbstractItemModel::dataChanged, this, [this]() { refresh(); });
    }
}

void DictionarySearchBar::refresh()
{
    m_timer->start();
}

void DictionarySearchBar::applyFilter()
{
    if (!m_tableView || !m_tableView->model()) {
        return;
    }

    const QString query = m_searchEdit->text();
    QAbstractItemModel* model = m_tableView->model();
    for (int row = 0; row < model->rowCount(); ++row) {
        bool matched = query.isEmpty();
        const int firstColumn = m_field == 0 ? 1 : m_field;
        const int lastColumn = m_field == 0 ? 3 : m_field;
        for (int column = firstColumn; !matched && column <= lastColumn; ++column) {
            matched = model->index(row, column).data(Qt::DisplayRole).toString()
                .contains(query, Qt::CaseInsensitive);
        }
        m_tableView->setRowHidden(row, !matched);
    }
}
