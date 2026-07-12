#include "TreeSitterHighlighter.h"

#include <QTextBlock>
#include <QTextDocument>
#include <QFont>
#include <QTimer>
#include <algorithm>

extern "C" const TSLanguage* tree_sitter_json();
extern "C" const TSLanguage* tree_sitter_toml();

namespace {
const TSLanguage* languagePtr(SyntaxLanguage language)
{
    return language == SyntaxLanguage::Json ? tree_sitter_json() : tree_sitter_toml();
}

bool isPunctuationNode(const QString& type)
{
    return type.size() == 1 && QStringLiteral("{}[](),.=:").contains(type);
}
}

TreeSitterHighlighter::TreeSitterHighlighter(QTextDocument* document)
    : QSyntaxHighlighter(document)
{
    m_parser = ts_parser_new();
    m_reparseTimer = new QTimer(this);
    m_reparseTimer->setSingleShot(true);
    m_reparseTimer->setInterval(100);
    connect(m_reparseTimer, &QTimer::timeout, this, [this]()
        {
            reparseDocument();
            rehighlight();
        });
    connect(document, &QTextDocument::contentsChange, this, [this]()
        {
            m_reparseTimer->start();
        });
}

TreeSitterHighlighter::~TreeSitterHighlighter()
{
    if (m_tree) {
        ts_tree_delete(m_tree);
    }
    if (m_parser) {
        ts_parser_delete(m_parser);
    }
}

void TreeSitterHighlighter::setLanguage(SyntaxLanguage language)
{
    m_language = language;
    if (m_parser) {
        ts_parser_set_language(m_parser, languagePtr(language));
    }
    if (m_reparseTimer) {
        m_reparseTimer->stop();
    }
    reparseDocument();
    rehighlight();
}

void TreeSitterHighlighter::highlightBlock(const QString&)
{
    const QVector<HighlightSpan> spans = m_spansByBlock.value(currentBlock().blockNumber());
    for (const HighlightSpan& span : spans) {
        setFormat(span.start, span.length, span.format);
    }
}

void TreeSitterHighlighter::reparseDocument()
{
    if (!m_parser || !document()) {
        return;
    }

    const QString text = document()->toPlainText();
    const QByteArray utf8Text = text.toUtf8();
    rebuildByteIndex(text);
    m_spansByBlock.clear();

    if (m_tree) {
        ts_tree_delete(m_tree);
        m_tree = nullptr;
    }

    m_tree = ts_parser_parse_string(m_parser, nullptr, utf8Text.constData(), (uint32_t)utf8Text.size());
    if (!m_tree) {
        return;
    }
    collectNode(ts_tree_root_node(m_tree));
}

void TreeSitterHighlighter::rebuildByteIndex(const QString& text)
{
    const QByteArray utf8Text = text.toUtf8();
    m_byteToChar.fill(text.size(), utf8Text.size() + 1);

    int byteOffset = 0;
    for (int i = 0; i < text.size();) {
        const bool surrogatePair = text[i].isHighSurrogate() && i + 1 < text.size() && text[i + 1].isLowSurrogate();
        const int charCount = surrogatePair ? 2 : 1;
        const QByteArray bytes = QStringView(text).mid(i, charCount).toString().toUtf8();

        m_byteToChar[byteOffset] = i;
        for (int j = 1; j < bytes.size(); ++j) {
            m_byteToChar[byteOffset + j] = i;
        }

        byteOffset += bytes.size();
        i += charCount;
        m_byteToChar[byteOffset] = i;
    }
}

void TreeSitterHighlighter::collectNode(TSNode node)
{
    const FormatRole role = roleForNode(node);
    if (role != FormatRole::None) {
        addNodeSpan(node, role);
    }

    const uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; ++i) {
        collectNode(ts_node_child(node, i));
    }
}

void TreeSitterHighlighter::addNodeSpan(TSNode node, FormatRole role)
{
    const int start = byteToChar(ts_node_start_byte(node));
    const int end = byteToChar(ts_node_end_byte(node));
    if (end <= start) {
        return;
    }

    QTextBlock block = document()->findBlock(start);
    while (block.isValid() && block.position() < end) {
        const int blockStart = block.position();
        const int blockEnd = blockStart + block.length() - 1;
        const int localStart = std::max(start, blockStart) - blockStart;
        const int localEnd = std::min(end, blockEnd) - blockStart;
        if (localEnd > localStart) {
            m_spansByBlock[block.blockNumber()].push_back({ localStart, localEnd - localStart, formatForRole(role) });
        }
        block = block.next();
    }
}

int TreeSitterHighlighter::byteToChar(uint32_t byteOffset) const
{
    if (m_byteToChar.isEmpty()) {
        return 0;
    }
    if (byteOffset >= (uint32_t)m_byteToChar.size()) {
        return m_byteToChar.back();
    }
    return m_byteToChar[(int)byteOffset];
}

TreeSitterHighlighter::FormatRole TreeSitterHighlighter::roleForNode(TSNode node) const
{
    const QString type = QString::fromUtf8(ts_node_type(node));
    const QString lowerType = type.toLower();

    if (!ts_node_is_named(node)) {
        return isPunctuationNode(type) ? FormatRole::Punctuation : FormatRole::None;
    }
    if (lowerType.contains(QStringLiteral("comment"))) {
        return FormatRole::Comment;
    }
    if (lowerType.contains(QStringLiteral("key"))) {
        return FormatRole::Key;
    }
    if (lowerType == QStringLiteral("string")) {
        const TSNode parent = ts_node_parent(node);
        if (QString::fromUtf8(ts_node_type(parent)) == QStringLiteral("pair")) {
            const TSNode keyNode = ts_node_child_by_field_name(parent, "key", 3);
            if (!ts_node_is_null(keyNode) && ts_node_eq(keyNode, node)) {
                return FormatRole::Key;
            }
        }
        return FormatRole::String;
    }
    if (lowerType.contains(QStringLiteral("string"))) {
        return FormatRole::String;
    }
    if (lowerType == QStringLiteral("number") ||
        lowerType == QStringLiteral("integer") ||
        lowerType == QStringLiteral("float"))
    {
        return FormatRole::Number;
    }
    if (lowerType == QStringLiteral("true") ||
        lowerType == QStringLiteral("false") ||
        lowerType == QStringLiteral("null") ||
        lowerType == QStringLiteral("boolean"))
    {
        return FormatRole::Constant;
    }
    if (lowerType.contains(QStringLiteral("table"))) {
        return FormatRole::Section;
    }
    return FormatRole::None;
}

QTextCharFormat TreeSitterHighlighter::formatForRole(FormatRole role) const
{
    QTextCharFormat format;
    switch (role) {
    case FormatRole::Comment:
        format.setForeground(QColor(125, 135, 145));
        break;
    case FormatRole::String:
        format.setForeground(QColor(38, 128, 80));
        break;
    case FormatRole::Number:
        format.setForeground(QColor(34, 99, 180));
        break;
    case FormatRole::Constant:
        format.setForeground(QColor(178, 92, 0));
        break;
    case FormatRole::Key:
        format.setForeground(QColor(136, 84, 180));
        break;
    case FormatRole::Section:
        format.setForeground(QColor(18, 118, 160));
        format.setFontWeight(QFont::DemiBold);
        break;
    case FormatRole::Punctuation:
        format.setForeground(QColor(100, 106, 115));
        break;
    case FormatRole::None:
        break;
    }
    return format;
}

void installTreeSitterHighlighter(QTextDocument* document, SyntaxLanguage language)
{
    TreeSitterHighlighter* highlighter = new TreeSitterHighlighter(document);
    highlighter->setLanguage(language);
}
