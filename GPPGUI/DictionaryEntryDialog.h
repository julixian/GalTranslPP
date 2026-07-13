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
    explicit DictionaryEntryDialog(const GptDictEntry& entry, QWidget* parent = nullptr);
    explicit DictionaryEntryDialog(const NormalDictEntry& entry, QWidget* parent = nullptr);

    GptDictEntry getGptEntry() const;
    NormalDictEntry getNormalEntry() const;

private:
    static QSize s_gptDialogSize;
    static QSize s_normalDialogSize;
    static int s_patternColumnWidth;
    static int s_sentenceOffsetColumnWidth;
    static int s_targetColumnWidth;

    GptDictEntry m_gptEntry;
    NormalDictEntry m_normalEntry;
};

class DictionaryEntryDeleteDialog final : public ElaContentDialog
{
    Q_OBJECT

public:
    explicit DictionaryEntryDeleteDialog(int selectedCount, QWidget* parent = nullptr);
};

#endif // DICTIONARYENTRYDIALOG_H
