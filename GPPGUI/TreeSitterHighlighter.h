#ifndef TREESITTERHIGHLIGHTER_H
#define TREESITTERHIGHLIGHTER_H

#include <QHash>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>
#include <tree_sitter/api.h>

class QTimer;

enum class SyntaxLanguage {
    Toml,
    Json
};

struct HighlightSpan {
    int start = 0;
    int length = 0;
    QTextCharFormat format;
};

class TreeSitterHighlighter : public QSyntaxHighlighter
{
public:
    explicit TreeSitterHighlighter(QTextDocument* document);
    ~TreeSitterHighlighter() override;

    void setLanguage(SyntaxLanguage language);

protected:
    void highlightBlock(const QString& text) override;

private:
    enum class FormatRole {
        None,
        Comment,
        String,
        Number,
        Constant,
        Key,
        Section,
        Punctuation
    };

    void reparseDocument();
    void rebuildByteIndex(const QString& text);
    void collectNode(TSNode node);
    void addNodeSpan(TSNode node, FormatRole role);
    int byteToChar(uint32_t byteOffset) const;
    FormatRole roleForNode(TSNode node) const;
    QTextCharFormat formatForRole(FormatRole role) const;

    TSParser* m_parser = nullptr;
    TSTree* m_tree = nullptr;
    QTimer* m_reparseTimer = nullptr;
    SyntaxLanguage m_language = SyntaxLanguage::Toml;
    QVector<int> m_byteToChar;
    QHash<int, QVector<HighlightSpan>> m_spansByBlock;
};

void installTreeSitterHighlighter(QTextDocument* document, SyntaxLanguage language);

#endif
