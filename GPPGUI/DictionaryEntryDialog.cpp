#include "DictionaryEntryDialog.h"

#include <algorithm>
#include <limits>
#include <utility>

#include <QAbstractTableModel>
#include <QFont>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QPalette>
#include <QSizePolicy>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include "ElaAlignedCheckBox.h"
#include "ElaDef.h"
#include "ElaIconButton.h"
#include "ElaLineEdit.h"
#include "ElaMessageBar.h"
#include "ElaNoWheelComboBox.h"
#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include "ElaSpinBox.h"
#include "ElaText.h"
#include "ElaToolTip.h"
#include "ReorderableTableView.h"

namespace
{
    enum ConditionColumn : int
    {
        DragHandle = 0,
        Pattern,
        SentenceOffset,
        Target,
        ConditionColumnCount
    };

    const QStringList& conditionTargets()
    {
        static const QStringList targets = {
            "name",
            "names",
            "nametrans",
            "namestrans",
            "orig",
            "preproc",
            "problems",
            "transby",
            "transraw",
            "transview",
            "otherinfo"
        };
        return targets;
    }

    class NormalConditionTableModel final : public QAbstractTableModel
    {
    public:
        explicit NormalConditionTableModel(QList<NormalCondition>& conditions, QObject* parent = nullptr)
            : QAbstractTableModel(parent), m_conditions(conditions)
        {
        }

        int rowCount(const QModelIndex& parent = QModelIndex()) const override
        {
            return parent.isValid() ? 0 : m_conditions.size();
        }

        int columnCount(const QModelIndex& parent = QModelIndex()) const override
        {
            return parent.isValid() ? 0 : ConditionColumnCount;
        }

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
        {
            if (!index.isValid() || index.row() < 0 || index.row() >= m_conditions.size()) {
                return {};
            }

            const NormalCondition& condition = m_conditions.at(index.row());
            if (index.column() == DragHandle) {
                if (role == Qt::ToolTipRole) {
                    return DictionaryEntryDialog::tr("拖动调整顺序");
                }
                if (role == Qt::TextAlignmentRole) {
                    return Qt::AlignCenter;
                }
                return {};
            }

            if (role == Qt::DisplayRole || role == Qt::EditRole) {
                switch (index.column()) {
                case Pattern:
                    return condition.pattern;
                case SentenceOffset:
                    if (role == Qt::DisplayRole && condition.sentenceOffset > 0) {
                        return QString("+%1").arg(condition.sentenceOffset);
                    }
                    return condition.sentenceOffset;
                case Target:
                    return condition.target;
                default:
                    break;
                }
            }

            if (role == Qt::TextAlignmentRole && index.column() == SentenceOffset) {
                return Qt::AlignCenter;
            }
            return {};
        }

        QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
        {
            if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
                return QAbstractTableModel::headerData(section, orientation, role);
            }

            switch (section) {
            case DragHandle:
                return {};
            case Pattern:
                return DictionaryEntryDialog::tr("条件正则");
            case SentenceOffset:
                return DictionaryEntryDialog::tr("相对句");
            case Target:
                return DictionaryEntryDialog::tr("条件对象");
            default:
                return {};
            }
        }

        Qt::ItemFlags flags(const QModelIndex& index) const override
        {
            if (!index.isValid()) {
                return Qt::ItemIsDropEnabled;
            }

            Qt::ItemFlags itemFlags = QAbstractTableModel::flags(index) | Qt::ItemIsDropEnabled;
            if (index.column() == DragHandle) {
                return itemFlags | Qt::ItemIsDragEnabled;
            }
            return itemFlags | Qt::ItemIsEditable;
        }

        bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override
        {
            if (role != Qt::EditRole || !index.isValid()
                || index.row() < 0 || index.row() >= m_conditions.size()) {
                return false;
            }

            NormalCondition& condition = m_conditions[index.row()];
            switch (index.column()) {
            case Pattern: {
                const QString pattern = value.toString();
                if (condition.pattern == pattern) {
                    return false;
                }
                condition.pattern = pattern;
                break;
            }
            case SentenceOffset: {
                const int sentenceOffset = value.toInt();
                if (condition.sentenceOffset == sentenceOffset) {
                    return false;
                }
                condition.sentenceOffset = sentenceOffset;
                break;
            }
            case Target: {
                const QString target = value.toString();
                if (condition.target == target) {
                    return false;
                }
                condition.target = target;
                break;
            }
            default:
                return false;
            }

            Q_EMIT dataChanged(index, index, { Qt::DisplayRole, Qt::EditRole });
            return true;
        }

        bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
            const QModelIndex& destinationParent, int destinationChild) override
        {
            if (sourceParent.isValid() || destinationParent.isValid() || count != 1
                || sourceRow < 0 || sourceRow >= m_conditions.size()
                || destinationChild < 0 || destinationChild > m_conditions.size()
                || destinationChild == sourceRow || destinationChild == sourceRow + 1) {
                return false;
            }

            if (!beginMoveRows(sourceParent, sourceRow, sourceRow, destinationParent, destinationChild)) {
                return false;
            }
            const int targetRow = destinationChild > sourceRow ? destinationChild - 1 : destinationChild;
            m_conditions.move(sourceRow, targetRow);
            endMoveRows();
            return true;
        }

        Qt::DropActions supportedDropActions() const override
        {
            return Qt::MoveAction;
        }

        Qt::DropActions supportedDragActions() const override
        {
            return Qt::MoveAction;
        }

        bool insertCondition(int row)
        {
            if (row < 0 || row > m_conditions.size()) {
                return false;
            }

            beginInsertRows({}, row, row);
            NormalCondition condition;
            condition.target = "orig";
            m_conditions.insert(row, std::move(condition));
            endInsertRows();
            return true;
        }

        bool removeCondition(int row)
        {
            if (row < 0 || row >= m_conditions.size()) {
                return false;
            }

            beginRemoveRows({}, row, row);
            m_conditions.removeAt(row);
            endRemoveRows();
            return true;
        }

    private:
        QList<NormalCondition>& m_conditions;
    };

    class NormalConditionItemDelegate final : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex& index) const override
        {
            switch (index.column()) {
            case Pattern:
                return new ElaLineEdit(parent);
            case SentenceOffset: {
                ElaSpinBox* spinBox = new ElaSpinBox(parent);
                spinBox->setMinimumWidth(0);
                spinBox->setMaximumWidth(QWIDGETSIZE_MAX);
                spinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                spinBox->setRange(-99, 99);
                return spinBox;
            }
            case Target: {
                ElaNoWheelComboBox* comboBox = new ElaNoWheelComboBox(parent);
                QPalette editorPalette = comboBox->palette();
                editorPalette.setColor(QPalette::Window, editorPalette.color(QPalette::Base));
                comboBox->setPalette(editorPalette);
                comboBox->setAutoFillBackground(true);
                comboBox->addItems(conditionTargets());
                return comboBox;
            }
            default:
                return nullptr;
            }
        }

        void setEditorData(QWidget* editor, const QModelIndex& index) const override
        {
            switch (index.column()) {
            case Pattern:
            {
                if (ElaLineEdit* lineEdit = qobject_cast<ElaLineEdit*>(editor)) {
                    lineEdit->setText(index.data(Qt::EditRole).toString());
                    lineEdit->selectAll();
                }
                break;
            }
            case SentenceOffset: {
                if (ElaSpinBox* spinBox = qobject_cast<ElaSpinBox*>(editor)) {
                    spinBox->setValue(index.data(Qt::EditRole).toInt());
                }
                break;
            }
            case Target: {
                if (ElaComboBox* comboBox = qobject_cast<ElaComboBox*>(editor)) {
                    comboBox->setCurrentIndex(comboBox->findText(index.data(Qt::EditRole).toString()));
                }
                break;
            }
            default:
                break;
            }
        }

        void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
        {
            switch (index.column()) {
            case Pattern:
                if (ElaLineEdit* lineEdit = qobject_cast<ElaLineEdit*>(editor)) {
                    model->setData(index, lineEdit->text(), Qt::EditRole);
                }
                break;
            case SentenceOffset:
                if (ElaSpinBox* spinBox = qobject_cast<ElaSpinBox*>(editor)) {
                    model->setData(index, spinBox->value(), Qt::EditRole);
                }
                break;
            case Target:
                if (ElaComboBox* comboBox = qobject_cast<ElaComboBox*>(editor)) {
                    model->setData(index, comboBox->currentText(), Qt::EditRole);
                }
                break;
            default:
                break;
            }
        }

        void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
            const QModelIndex&) const override
        {
            editor->setGeometry(option.rect);
        }
    };

    ElaText* createFieldLabel(const QString& text, QWidget* parent)
    {
        return new ElaText(text, 14, parent);
    }
}

QSize DictionaryEntryDialog::s_gptDialogSize(760, 480);
QSize DictionaryEntryDialog::s_normalDialogSize(900, 680);
int DictionaryEntryDialog::s_patternColumnWidth = 470;
int DictionaryEntryDialog::s_sentenceOffsetColumnWidth = 100;
int DictionaryEntryDialog::s_targetColumnWidth = 260;

DictionaryEntryDialog::DictionaryEntryDialog(const GptDictEntry& entry, QWidget* parent)
    : ElaDialog(parent), m_gptEntry(entry)
{
    setWindowTitle(tr("编辑 GPT 字典词条"));
    setWindowModality(Qt::ApplicationModal);
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    resize(s_gptDialogSize);
    setMinimumSize(640, 420);
    connect(this, &QDialog::finished, this, [this](int)
        {
            s_gptDialogSize = size();
        });

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 0, 16, 14);
    mainLayout->setSpacing(6);

    auto addTextBlock = [this, mainLayout](const QString& label, const QString& value)
        {
            mainLayout->addWidget(createFieldLabel(label, this));
            ElaPlainTextEdit* edit = new ElaPlainTextEdit(this);
            edit->setPlainText(value);
            edit->setMinimumHeight(76);
            mainLayout->addWidget(edit, 1);
            return edit;
        };

    ElaPlainTextEdit* originalEdit = addTextBlock(tr("原文（org）"), entry.original);
    ElaPlainTextEdit* translationEdit = addTextBlock(tr("译文（rep）"), entry.translation);
    ElaPlainTextEdit* descriptionEdit = addTextBlock(tr("注释（note）"), entry.description);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 2, 0, 0);
    buttonLayout->setSpacing(8);
    buttonLayout->addStretch();

    ElaPushButton* cancelButton = new ElaPushButton(tr("取消"), this);
    cancelButton->setFixedWidth(90);
    connect(cancelButton, &ElaPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    ElaPushButton* saveButton = new ElaPushButton(tr("保存"), this);
    saveButton->setFixedWidth(90);
    connect(saveButton, &ElaPushButton::clicked, this,
        [this, originalEdit, translationEdit, descriptionEdit]()
        {
            if (originalEdit->toPlainText().trimmed().isEmpty()) {
                originalEdit->setFocus();
                ElaMessageBar::warning(ElaMessageBarType::TopLeft, tr("保存失败"),
                    tr("原文（org）不能为空"), 3000);
                return;
            }

            m_gptEntry.original = originalEdit->toPlainText();
            m_gptEntry.translation = translationEdit->toPlainText();
            m_gptEntry.description = descriptionEdit->toPlainText();
            accept();
        });
    buttonLayout->addWidget(saveButton);
    mainLayout->addLayout(buttonLayout);

    originalEdit->setFocus();
    if (parent) {
        move(parent->frameGeometry().center() - rect().center());
    }
}

DictionaryEntryDialog::DictionaryEntryDialog(const NormalDictEntry& entry, QWidget* parent)
    : ElaDialog(parent), m_normalEntry(entry)
{
    setWindowTitle(tr("编辑 Normal 字典词条"));
    setWindowModality(Qt::ApplicationModal);
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    resize(s_normalDialogSize);
    setMinimumSize(760, 560);
    connect(this, &QDialog::finished, this, [this](int)
        {
            s_normalDialogSize = size();
        });

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 0, 16, 14);
    mainLayout->setSpacing(8);

    QHBoxLayout* textLayout = new QHBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(10);

    QVBoxLayout* originalLayout = new QVBoxLayout();
    originalLayout->setContentsMargins(0, 0, 0, 0);
    originalLayout->setSpacing(4);
    originalLayout->addWidget(createFieldLabel(tr("原文（org）"), this));
    ElaPlainTextEdit* originalEdit = new ElaPlainTextEdit(this);
    originalEdit->setPlainText(entry.original);
    originalEdit->setMinimumHeight(88);
    originalEdit->setMaximumHeight(120);
    originalLayout->addWidget(originalEdit);
    textLayout->addLayout(originalLayout, 1);

    QVBoxLayout* translationLayout = new QVBoxLayout();
    translationLayout->setContentsMargins(0, 0, 0, 0);
    translationLayout->setSpacing(4);
    translationLayout->addWidget(createFieldLabel(tr("译文（rep）"), this));
    ElaPlainTextEdit* translationEdit = new ElaPlainTextEdit(this);
    translationEdit->setPlainText(entry.translation);
    translationEdit->setMinimumHeight(88);
    translationEdit->setMaximumHeight(120);
    translationLayout->addWidget(translationEdit);
    textLayout->addLayout(translationLayout, 1);
    mainLayout->addLayout(textLayout);

    mainLayout->addWidget(createFieldLabel(tr("匹配设置"), this));
    QGridLayout* settingsLayout = new QGridLayout();
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setHorizontalSpacing(8);
    settingsLayout->setVerticalSpacing(6);

    ElaAlignedCheckBox* isRegCheckBox = new ElaAlignedCheckBox(tr("启用正则（isReg）"), this);
    isRegCheckBox->setChecked(entry.isReg);
    settingsLayout->addWidget(isRegCheckBox, 0, 0, 1, 2);

    settingsLayout->addWidget(createFieldLabel(tr("优先级（priority）"), this), 0, 2);
    ElaSpinBox* prioritySpinBox = new ElaSpinBox(this);
    prioritySpinBox->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    prioritySpinBox->setValue(entry.priority);
    settingsLayout->addWidget(prioritySpinBox, 0, 3);

    settingsLayout->setColumnStretch(1, 1);
    settingsLayout->setColumnStretch(3, 1);
    mainLayout->addLayout(settingsLayout);

    QHBoxLayout* conditionHeaderLayout = new QHBoxLayout();
    conditionHeaderLayout->setContentsMargins(0, 0, 0, 0);
    conditionHeaderLayout->setSpacing(4);
    conditionHeaderLayout->addWidget(createFieldLabel(tr("条件（conditions）"), this));
    conditionHeaderLayout->addStretch();

    ElaIconButton* addConditionButton = new ElaIconButton(ElaIconType::Plus, 15, 30, 30, this);
    ElaToolTip* addConditionToolTip = new ElaToolTip(addConditionButton);
    addConditionToolTip->setToolTip(tr("添加条件"));
    conditionHeaderLayout->addWidget(addConditionButton);

    ElaIconButton* removeConditionButton = new ElaIconButton(ElaIconType::Minus, 15, 30, 30, this);
    ElaToolTip* removeConditionToolTip = new ElaToolTip(removeConditionButton);
    removeConditionToolTip->setToolTip(tr("删除条件"));
    conditionHeaderLayout->addWidget(removeConditionButton);
    mainLayout->addLayout(conditionHeaderLayout);

    ReorderableTableView* tableView = new ReorderableTableView(this);
    NormalConditionTableModel* model = new NormalConditionTableModel(m_normalEntry.conditions, tableView);
    tableView->setModel(model);
    tableView->setItemDelegate(new NormalConditionItemDelegate(tableView));
    tableView->verticalHeader()->setHidden(true);
    tableView->verticalHeader()->setDefaultSectionSize(36);
    tableView->setAlternatingRowColors(true);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tableView->setEditTriggers(QAbstractItemView::DoubleClicked
        | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    tableView->setMinimumHeight(240);

    QFont tableHeaderFont = tableView->horizontalHeader()->font();
    tableHeaderFont.setPixelSize(15);
    tableView->horizontalHeader()->setFont(tableHeaderFont);
    tableView->horizontalHeader()->setSectionResizeMode(DragHandle, QHeaderView::Fixed);
    tableView->horizontalHeader()->setSectionResizeMode(Pattern, QHeaderView::Interactive);
    tableView->horizontalHeader()->setSectionResizeMode(SentenceOffset, QHeaderView::Interactive);
    tableView->horizontalHeader()->setSectionResizeMode(Target, QHeaderView::Interactive);
    tableView->setColumnWidth(DragHandle, ReorderableTableView::HandleColumnWidth);
    tableView->setColumnWidth(Pattern, s_patternColumnWidth);
    tableView->setColumnWidth(SentenceOffset, s_sentenceOffsetColumnWidth);
    tableView->setColumnWidth(Target, s_targetColumnWidth);
    connect(tableView->horizontalHeader(), &QHeaderView::sectionResized, this,
        [](int logicalIndex, int, int newSize)
        {
            switch (logicalIndex) {
            case Pattern:
                s_patternColumnWidth = newSize;
                break;
            case SentenceOffset:
                s_sentenceOffsetColumnWidth = newSize;
                break;
            case Target:
                s_targetColumnWidth = newSize;
                break;
            default:
                break;
            }
        });
    mainLayout->addWidget(tableView, 1);

    connect(addConditionButton, &ElaIconButton::clicked, this, [=]()
        {
            const QModelIndex currentIndex = tableView->currentIndex();
            const int insertRow = currentIndex.isValid() ? currentIndex.row() + 1 : model->rowCount();
            if (!model->insertCondition(insertRow)) {
                return;
            }
            const QModelIndex patternIndex = model->index(insertRow, Pattern);
            tableView->setCurrentIndex(patternIndex);
            tableView->scrollTo(patternIndex);
            tableView->edit(patternIndex);
        });

    connect(removeConditionButton, &ElaIconButton::clicked, this, [=]()
        {
            QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
            std::sort(selectedRows.begin(), selectedRows.end(),
                [](const QModelIndex& left, const QModelIndex& right)
                {
                    return left.row() > right.row();
                });
            for (const QModelIndex& index : std::as_const(selectedRows)) {
                model->removeCondition(index.row());
            }
        });

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(8);
    buttonLayout->addStretch();

    ElaPushButton* cancelButton = new ElaPushButton(tr("取消"), this);
    cancelButton->setFixedWidth(90);
    connect(cancelButton, &ElaPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    ElaPushButton* saveButton = new ElaPushButton(tr("保存"), this);
    saveButton->setFixedWidth(90);
    connect(saveButton, &ElaPushButton::clicked, this,
        [this, originalEdit, translationEdit, isRegCheckBox, prioritySpinBox, tableView, model]()
        {
            tableView->setFocus();
            if (originalEdit->toPlainText().trimmed().isEmpty()) {
                originalEdit->setFocus();
                ElaMessageBar::warning(ElaMessageBarType::TopLeft, tr("保存失败"),
                    tr("原文（org）不能为空"), 3000);
                return;
            }

            for (int row = 0; row < m_normalEntry.conditions.size(); ++row) {
                const NormalCondition& condition = m_normalEntry.conditions.at(row);
                if (condition.pattern.trimmed().isEmpty()) {
                    const QModelIndex index = model->index(row, Pattern);
                    tableView->setCurrentIndex(index);
                    tableView->scrollTo(index);
                    tableView->edit(index);
                    ElaMessageBar::warning(ElaMessageBarType::TopLeft, tr("保存失败"),
                        tr("第 %1 条条件的条件正则不能为空").arg(row + 1), 3000);
                    return;
                }
                if (condition.target.trimmed().isEmpty()) {
                    const QModelIndex index = model->index(row, Target);
                    tableView->setCurrentIndex(index);
                    tableView->scrollTo(index);
                    tableView->edit(index);
                    ElaMessageBar::warning(ElaMessageBarType::TopLeft, tr("保存失败"),
                        tr("第 %1 条条件的条件对象不能为空").arg(row + 1), 3000);
                    return;
                }
            }

            m_normalEntry.original = originalEdit->toPlainText();
            m_normalEntry.translation = translationEdit->toPlainText();
            m_normalEntry.isReg = isRegCheckBox->isChecked();
            m_normalEntry.priority = prioritySpinBox->value();
            accept();
        });
    buttonLayout->addWidget(saveButton);
    mainLayout->addLayout(buttonLayout);

    originalEdit->setFocus();
    if (parent) {
        move(parent->frameGeometry().center() - rect().center());
    }
}

GptDictEntry DictionaryEntryDialog::getGptEntry() const
{
    return m_gptEntry;
}

NormalDictEntry DictionaryEntryDialog::getNormalEntry() const
{
    return m_normalEntry;
}

DictionaryEntryDeleteDialog::DictionaryEntryDeleteDialog(int selectedCount, QWidget* parent)
    : ElaContentDialog(parent)
{
    setLeftButtonText(tr("否"));
    setMiddleButtonText(tr("思考人生"));
    setRightButtonText(tr("是"));

    QWidget* widget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(15, 25, 15, 10);

    ElaText* confirmText = new ElaText(tr("你确定要删除选中的词条吗？"), widget);
    confirmText->setTextStyle(ElaTextType::Title);
    confirmText->setWordWrap(false);
    layout->addWidget(confirmText);
    layout->addSpacing(2);

    ElaText* confirmHint = new ElaText(
        tr("已选中 %1 条词条，删除后可以使用撤回按钮恢复。").arg(selectedCount), 16, widget);
    confirmHint->setTextStyle(ElaTextType::Body);
    layout->addWidget(confirmHint);
    layout->addStretch();
    setCentralWidget(widget);
}
