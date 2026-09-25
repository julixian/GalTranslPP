#ifndef DICTIONARYENTRYDIALOG_H
#define DICTIONARYENTRYDIALOG_H

#include "ElaContentDialog.h"
#include "ElaDialog.h"
#include "GptDictModel.h"
#include "NormalDictModel.h"
#include <QSize>

class DictionaryEntryDialog final : public ElaDialog
{
    Q_OBJECT

public:
    explicit DictionaryEntryDialog(const GuiGptDictEntry& entry, QWidget* parent = nullptr);
    explicit DictionaryEntryDialog(const GuiNormalDictEntry& entry, QWidget* parent = nullptr);

    GuiGptDictEntry getGptEntry() const;
    GuiNormalDictEntry getNormalEntry() const;

private:
    static QSize s_gptDialogSize;
    static QSize s_normalDialogSize;
    static int s_patternColumnWidth;
    static int s_sentenceOffsetColumnWidth;
    static int s_targetColumnWidth;

    GuiGptDictEntry m_gptEntry;
    GuiNormalDictEntry m_normalEntry;
};

class DictionaryEntryDeleteDialog final : public ElaContentDialog
{
    Q_OBJECT

public:
    explicit DictionaryEntryDeleteDialog(int selectedCount, QWidget* parent = nullptr);
};

#endif
