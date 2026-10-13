#pragma once

#include "ElaContentDialog.h"
#include "ElaDialog.h"
#include "GptDictModel.h"
#include "NormalDictModel.h"
#include <QSize>

class QShowEvent;

class DictionaryEntryDialog final : public ElaDialog
{
    Q_OBJECT

public:
    explicit DictionaryEntryDialog(const GuiGptDictEntry& entry, QWidget* parent = nullptr);
    explicit DictionaryEntryDialog(const GuiNormalDictEntry& entry, QWidget* parent = nullptr);

    GuiGptDictEntry getGptEntry() const;
    GuiNormalDictEntry getNormalEntry() const;

protected:
    void showEvent(QShowEvent* event) override;

private:
    static QSize s_gptDialogSize;
    static QSize s_normalDialogSize;
    static int s_patternColumnWidth;
    static int s_sentenceOffsetColumnWidth;
    static int s_targetColumnWidth;

    QSize m_windowSize;
    GuiGptDictEntry m_gptEntry;
    GuiNormalDictEntry m_normalEntry;
};

class DictionaryEntryDeleteDialog final : public ElaContentDialog
{
    Q_OBJECT

public:
    explicit DictionaryEntryDeleteDialog(int selectedCount, QWidget* parent = nullptr);
};
