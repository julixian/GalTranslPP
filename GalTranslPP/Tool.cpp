module;

#include "GPPMacros.hpp"

#ifdef _WIN32
#include <Windows.h>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "winhttp.lib")
#endif

#include <boost/algorithm/string.hpp>
#include <boost/crc.hpp>

#define BIT7Z_AUTO_FORMAT
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitfileextractor.hpp>

#include <ctpl_stl.h>

#include <opencc/opencc.h>

#include <toml.hpp>

#include <unicode/brkiter.h>
#include <unicode/uscript.h>
#include <unicode/translit.h>
#include <unicode/utext.h>

#include <utf8cpp/utf8.h>

#include <cpp-base64/base64.cpp>

#pragma comment(lib, "python3.lib")
#pragma comment(lib, "python312.lib")

module Tool;

import ITranslator;

namespace fs = std::filesystem;


std::string wide2Ascii(std::wstring_view wide, UINT codePage, LPBOOL usedDefaultChar) {
#ifdef _WIN32
    int len = WideCharToMultiByte(codePage, 0, wide.data(), (int)wide.length(),
        nullptr, 0, nullptr, usedDefaultChar);
    if (len == 0) return {};
    std::string ascii(len, '\0');
    WideCharToMultiByte(codePage, 0, wide.data(), (int)wide.length(),
        ascii.data(), len, nullptr, nullptr);
    return ascii;
#endif
}

std::wstring ascii2Wide(std::string_view ascii, UINT codePage) {
#ifdef _WIN32
    int len = MultiByteToWideChar(codePage, 0, ascii.data(), (int)ascii.length(),
        nullptr, 0);
    if (len == 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(codePage, 0, ascii.data(), (int)ascii.length(),
        wide.data(), len);
    return wide;
#endif
}



std::optional<int> str2Int(std::string_view str) {
    int value = 0;
    // 注意：from_chars 不会跳过前导空格！如果需要，得自己 trim 一下
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
    // ec == std::errc() 表示解析动作成功
    // ptr == str.data() + str.size() 表示整个字符串都被消耗完了（没有剩余垃圾字符）
    if (ec == std::errc() && ptr == str.data() + str.size()) {
        return value;
    }
    return std::nullopt;
}



template<typename T>
std::vector<T> splitStringImpl(auto&& str, auto&& delimiter)
{
    std::vector<T> result;
    for (auto&& subStrView : str | std::views::split(delimiter)) {
        result.emplace_back(subStrView.begin(), subStrView.end());
    }
    return result;
}
std::vector<std::string_view> splitStringView(std::string_view str, char delimiter) {
    return splitStringImpl<std::string_view>(str, delimiter);
}
std::vector<std::string_view> splitStringView(std::string_view str, std::string_view delimiter) {
    return splitStringImpl<std::string_view>(str, delimiter);
}
std::vector<std::string> splitString(std::string_view str, char delimiter) {
    return splitStringImpl<std::string>(str, delimiter);
}
std::vector<std::string> splitString(std::string_view str, std::string_view delimiter) {
    return splitStringImpl<std::string>(str, delimiter);
}



std::vector<std::string_view> splitTsvLineView(std::string_view line, std::span<const std::string_view> delimiters) {
    std::vector<std::string_view> parts;
    size_t currentPos = 0;

    if (
        std::ranges::any_of(delimiters, [](const auto& delimiter)
            {
                return delimiter.empty();
            })
        )
    {
        throw std::runtime_error(gppTr("splitTsvLine", "内部错误: TSV 行切分不允许使用空分隔符").toStdString());
    }

    while (currentPos < line.length()) {
        size_t splitPos = std::string_view::npos;
        size_t delimiterLength = 0;
        for (std::string_view delimiter : delimiters) {
            if (const size_t pos = line.find(delimiter, currentPos); pos != std::string_view::npos && pos < splitPos) {
                splitPos = pos;
                delimiterLength = delimiter.length();
            }
        }
        parts.push_back(line.substr(currentPos, splitPos - currentPos));

        if (splitPos == std::string_view::npos) {
            break;
        }
        currentPos = splitPos + delimiterLength;
    }

    return parts;
}

std::vector<std::string> splitTsvLine(std::string_view line, std::span<const std::string_view> delimiters) {
    const std::vector<std::string_view> partViews = splitTsvLineView(line, delimiters);
    std::vector<std::string> parts;
    parts.reserve(partViews.size());
    for (std::string_view part : partViews) {
        parts.emplace_back(part);
    }
    return parts;
}



class UTextWrapper {
public:
    explicit UTextWrapper(std::string_view sourceString, UErrorCode& errorCode) {
        utext_openUTF8(&m_text, sourceString.data(), (int64_t)sourceString.size(), &errorCode);
    }
    UTextWrapper(const UTextWrapper&) = delete;
    UTextWrapper& operator=(const UTextWrapper&) = delete;
    UTextWrapper(UTextWrapper&&) = delete;
    UTextWrapper& operator=(UTextWrapper&&) = delete;
    ~UTextWrapper() {
        utext_close(&m_text);
    }

    UText* get() {
        return &m_text;
    }

private:
    UText m_text = UTEXT_INITIALIZER;
};

std::vector<std::string_view> splitIntoGraphemeViews(std::string_view str) {
    std::vector<std::string_view> resultVector;

    if (str.empty()) {
        return resultVector;
    }

    UErrorCode errorCode = U_ZERO_ERROR;
    UTextWrapper uText(str, errorCode);
    if (U_FAILURE(errorCode)) {
        throw std::runtime_error(gppTr("splitIntoGraphemes", "打开 UTF-8 文本失败: %1")
            .arg(u_errorName(errorCode))
            .toStdString());
    }

    const std::unique_ptr<icu::BreakIterator> breakIterator(
        icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), errorCode)
    );

    if (U_FAILURE(errorCode)) {
        throw std::runtime_error(gppTr("splitIntoGraphemes", "创建字符边界迭代器失败: %1")
            .arg(u_errorName(errorCode))
            .toStdString());
    }

    breakIterator->setText(uText.get(), errorCode);
    if (U_FAILURE(errorCode)) {
        throw std::runtime_error(gppTr("splitIntoGraphemes", "设置字符边界迭代文本失败: %1")
            .arg(u_errorName(errorCode))
            .toStdString());
    }

    int32_t start = breakIterator->first();
    for (int32_t end = breakIterator->next(); end != icu::BreakIterator::DONE; start = end, end = breakIterator->next()) {
        resultVector.push_back(str.substr((size_t)start, (size_t)(end - start)));
    }

    return resultVector;
}

std::vector<std::string> splitIntoGraphemes(std::string_view str) {
    const std::vector<std::string_view> graphemeViews = splitIntoGraphemeViews(str);
    std::vector<std::string> resultVector;
    resultVector.reserve(graphemeViews.size());
    for (const auto& grapheme : graphemeViews) {
        resultVector.emplace_back(grapheme);
    }
    return resultVector;
}



size_t countGraphemes(std::string_view str) {
    if (str.empty()) {
        return 0;
    }
    UErrorCode errorCode = U_ZERO_ERROR;
    UTextWrapper uText(str, errorCode);
    if (U_FAILURE(errorCode)) {
        throw std::runtime_error(gppTr("countGraphemes", "打开 UTF-8 文本失败: %1")
            .arg(u_errorName(errorCode))
            .toStdString());
    }

    const std::unique_ptr<icu::BreakIterator> breakIterator(
        icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), errorCode)
    );
    if (U_FAILURE(errorCode)) {
        throw std::runtime_error(gppTr("countGraphemes", "创建字符边界迭代器失败: %1")
            .arg(u_errorName(errorCode))
            .toStdString());
    }
    breakIterator->setText(uText.get(), errorCode);
    if (U_FAILURE(errorCode)) {
        throw std::runtime_error(gppTr("countGraphemes", "设置字符边界迭代文本失败: %1")
            .arg(u_errorName(errorCode))
            .toStdString());
    }

    breakIterator->first();
    size_t count = 0;
    for (int32_t end = breakIterator->next(); end != icu::BreakIterator::DONE; end = breakIterator->next()) {
        ++count;
    }
    return count;
}



int countSubstring(std::string_view str, std::string_view sub) {
    return (int)std::ranges::distance(str | std::views::split(sub)) - 1;
}

// 计算的是子串在删去子串后的主串中出现的位置
std::vector<double> getSubstringPositions(std::string_view str, std::string_view sub) {
    if (str.empty() || sub.empty()) return {};
    std::vector<size_t> positions;
    std::vector<double> relpositions;

    for (size_t offset = str.find(sub); offset != std::string_view::npos; offset = str.find(sub, offset + sub.length())) {
        positions.push_back(offset);
    }
    const size_t newTotalLength = str.length() - positions.size() * sub.length();
    if (newTotalLength == 0) {
        return {};
    }
    for (size_t i = 0; i < positions.size(); i++) {
        const size_t newPos = positions[i] - i * sub.length();
        relpositions.push_back((double)newPos / newTotalLength);
    }
    return relpositions;
}

// MostCommonChar 和 Grapheme 都遍历的是字形簇而不是码点
std::pair<std::string_view, int> getMostCommonCharView(std::string_view str) {
    if (str.empty()) {
        return { {}, 0 };
    }

    absl::btree_map<std::string_view, int> counts;
    for (const auto& grapheme : splitIntoGraphemeViews(str)) {
        ++counts[grapheme];
    }

    if (counts.empty()) {
        return { {}, 0 };
    }

    const auto maxIterator = std::ranges::max_element(counts, [](const auto& a, const auto& b)
        {
            return a.second < b.second;
        });

    return { maxIterator->first, maxIterator->second };
}

std::pair<std::string, int> getMostCommonChar(std::string_view str) {
    const auto [charView, count] = getMostCommonCharView(str);
    return { std::string(charView), count };
}



std::string& replaceStrInplace(std::string& str, std::string_view org, std::string_view rep) {
    boost::replace_all(str, org, rep);
    return str;
}

std::string replaceStr(const std::string& str, std::string_view org, std::string_view rep) {
    return boost::replace_all_copy(str, org, rep);
}



bool isEscapedJsonQuote(const std::string& text, size_t pos) {
    if (pos == 0 || pos >= text.size()) {
        return false;
    }
    size_t slashCount = 0;
    for (size_t i = pos; i > 0;) {
        --i;
        if (text[i] != '\\') {
            break;
        }
        ++slashCount;
    }
    return slashCount % 2 == 1;
}

bool isLikelyJsonKeyPosition(const std::string& text, size_t keyPos) {
    if (keyPos == std::string::npos) {
        return false;
    }
    for (size_t i = keyPos; i > 0;) {
        --i;
        const unsigned char ch = (unsigned char)text[i];
        if (std::isspace(ch)) {
            continue;
        }
        return text[i] == '{' || text[i] == ',' || text[i] == '[';
    }
    return true;
}

bool isLikelyJsonStringSuffix(const std::string& text, size_t posAfterQuote) {
    for (size_t i = posAfterQuote; i < text.size(); ++i) {
        const unsigned char ch = (unsigned char)text[i];
        if (std::isspace(ch)) {
            continue;
        }
        return text[i] == ',' || text[i] == '}' || text[i] == ']';
    }
    return true;
}

size_t findLikelyJsonStringClosingQuote(const std::string& text, size_t openingQuotePos) {
    for (size_t pos = openingQuotePos + 1; pos < text.size(); ++pos) {
        if (text[pos] != '"' || isEscapedJsonQuote(text, pos)) {
            continue;
        }
        if (isLikelyJsonStringSuffix(text, pos + 1)) {
            return pos;
        }
    }
    return std::string::npos;
}

std::string lightRepairJsonText(std::string_view jsonStr) {
    if (jsonStr.empty()) {
        return std::string(jsonStr);
    }

    static constexpr std::array<std::string_view, 15> repairableFields = {
        "\"dst\":",
        "\"note\":",
        "\"rolling_context\":",
        "\"query\":",
        "\"source_term\":",
        "\"target_term\":",
        "\"final_target\":",
        "\"final_note\":",
        "\"merge_into\":",
        "\"suggestion\":",
        "\"summary\":",
        "\"scene_state\":",
        "\"unresolved_clues\":",
        "\"relationship_updates\":",
        "\"term_hints\":"
    };

    std::string newText = std::string(jsonStr);
    for (const std::string_view& field : repairableFields) {
        size_t searchPos = 0;
        while (searchPos < newText.size()) {
            const size_t fieldPos = newText.find(field, searchPos);
            if (fieldPos == std::string::npos) {
                break;
            }
            searchPos = fieldPos + field.size();
            if (!isLikelyJsonKeyPosition(newText, fieldPos)) {
                continue;
            }

            size_t openingQuotePos = searchPos;
            while (openingQuotePos < newText.size() && std::isspace((unsigned char)newText[openingQuotePos])) {
                ++openingQuotePos;
            }
            if (openingQuotePos >= newText.size() || newText[openingQuotePos] != '"') {
                continue;
            }

            const size_t closingQuotePos = findLikelyJsonStringClosingQuote(newText, openingQuotePos);
            if (closingQuotePos == std::string::npos || closingQuotePos <= openingQuotePos) {
                continue;
            }

            std::string repairedValue;
            repairedValue.reserve(closingQuotePos - openingQuotePos);
            for (size_t pos = openingQuotePos + 1; pos < closingQuotePos; ++pos) {
                if (newText[pos] == '"' && !isEscapedJsonQuote(newText, pos)) {
                    repairedValue.push_back('\\');
                }
                repairedValue.push_back(newText[pos]);
            }

            const std::string_view originalValue = std::string_view(newText).substr(openingQuotePos + 1, closingQuotePos - openingQuotePos - 1);
            if (repairedValue != originalValue) {
                newText.replace(openingQuotePos + 1, closingQuotePos - openingQuotePos - 1, repairedValue);
                searchPos = openingQuotePos + 1 + repairedValue.size() + 1;
            }
        }
    }
    return newText;
}



bool hasPunctuation(std::string_view str) {
    const uint8_t* s = (const uint8_t*)str.data();
    const int32_t length = (int32_t)str.length();
    int32_t i = 0;
    UChar32 c;
    while (i < length) {
        U8_NEXT(s, i, length, c);
        if (u_ispunct(c)) {
            return true;
        }
    }
    return false;
}

bool hasWhitespace(std::string_view str) {
    const uint8_t* s = (const uint8_t*)str.data();
    const int32_t length = (int32_t)str.length();
    int32_t i = 0;
    UChar32 c;
    while (i < length) {
        U8_NEXT(s, i, length, c);
        if (u_isspace(c)) {
            return true;
        }
    }
    return false;
}

bool isAllPunctuation(std::string_view str) {
    const uint8_t* s = (const uint8_t*)str.data();
    const int32_t length = (int32_t)str.length();
    int32_t i = 0;
    UChar32 c;
    while (i < length) {
        U8_NEXT(s, i, length, c);
        if (!u_ispunct(c)) {
            return false;
        }
    }
    return true;
}

bool isAllWhitespace(std::string_view str) {
    const uint8_t* s = (const uint8_t*)str.data();
    const int32_t length = (int32_t)str.length();
    int32_t i = 0;
    UChar32 c;
    while (i < length) {
        U8_NEXT(s, i, length, c);
        if (!u_isspace(c)) {
            return false;
        }
    }
    return true;
}

std::string removePunctuation(std::string_view str) {
    std::string resultString;
    const uint8_t* s = (uint8_t*)str.data();
    const int32_t length = (int32_t)str.length();
    int32_t i = 0;
    UChar32 c;
    while (i < length) {
        U8_NEXT(s, i, length, c);
        if (!u_ispunct(c)) {
            utf8::unchecked::append(c, std::back_inserter(resultString));
        }
    }
    return resultString;
}

std::string removeWhitespace(std::string_view str) {
    std::string resultString;
    const uint8_t* s = (uint8_t*)str.data();
    const int32_t length = (int32_t)str.length();
    int32_t i = 0;
    UChar32 c;
    while (i < length) {
        U8_NEXT(s, i, length, c);
        if (!u_isspace(c)) {
            utf8::unchecked::append(c, std::back_inserter(resultString));
        }
    }
    return resultString;
}



std::string_view truncateUtf8PrefixView(std::string_view str, size_t maxCodepoints) {
    if (str.empty() || str.length() <= maxCodepoints) {
        return str;
    }
    if (maxCodepoints == 0) {
        return {};
    }
    const uint8_t* s = (const uint8_t*)str.data();
    const int32_t length = (int32_t)str.size();
    int32_t i = 0;
    size_t count = 0;
    while (i < length && count < maxCodepoints) {
        U8_FWD_1(s, i, length);
        ++count;
    }
    if (i >= length) {
        return str;
    }
    return str.substr(0, (size_t)i);
}

std::string truncateUtf8Prefix(std::string_view str, size_t maxCodepoints, std::string_view ellipsis) {
    const std::string_view prefix = truncateUtf8PrefixView(str, maxCodepoints);
    if (prefix.size() == str.size()) {
        return std::string(str);
    }
    std::string ret(prefix);
    ret.append(ellipsis);
    return ret;
}

std::string_view truncateUtf8SuffixView(std::string_view str, size_t maxCodepoints) {
    if (str.empty() || str.length() < maxCodepoints) {
        return str;
    }
    if (maxCodepoints == 0) {
        return {};
    }
    const uint8_t* s = (const uint8_t*)str.data();
    int32_t i = (int32_t)str.size();
    size_t count = 0;
    while (i > 0 && count < maxCodepoints) {
        U8_BACK_1(s, 0, i);
        ++count;
    }
    if (i <= 0) {
        return str;
    }
    return str.substr(i);
}

std::string truncateUtf8Suffix(std::string_view str, size_t maxCodepoints, std::string_view ellipsis) {
    const std::string_view suffix = truncateUtf8SuffixView(str, maxCodepoints);
    if (suffix.size() == str.size()) {
        return std::string(str);
    }
    std::string ret(ellipsis);
    ret.append(suffix);
    return ret;
}

std::string maskApikey(std::string_view apikey) {
    constexpr size_t prefixLength = 6;
    constexpr size_t suffixLength = 3;
    if (apikey.size() <= prefixLength + suffixLength + 2) {
        apikey.remove_suffix(1);
        return std::string(apikey) + '*';
    }
    return std::format("{}**{}", apikey.substr(0, prefixLength), apikey.substr(apikey.size() - suffixLength));
}



bool hasCharactersByScripts(std::string_view sourceString, std::span<const UScriptCode> targetScripts) {
    UErrorCode errorCode = U_ZERO_ERROR;

    const uint8_t* s = (const uint8_t*)sourceString.data();
    const int32_t length = (int32_t)sourceString.length();
    int32_t i = 0;
    UChar32 c;

    while (i < length) {
        U8_NEXT(s, i, length, c);
        const UScriptCode script = uscript_getScript(c, &errorCode);

        if (U_SUCCESS(errorCode) && std::ranges::contains(targetScripts, script)) {
            return true;
        }
    }

    return false;
}
bool hasKatakana(std::string_view sourceString) {
    static constexpr std::array targetScripts = { USCRIPT_KATAKANA };
    return hasCharactersByScripts(sourceString, targetScripts);
}
bool hasKana(std::string_view sourceString) {
    static constexpr std::array targetScripts = { USCRIPT_HIRAGANA, USCRIPT_KATAKANA };
    return hasCharactersByScripts(sourceString, targetScripts);
}
bool hasLatin(std::string_view sourceString) {
    static constexpr std::array targetScripts = { USCRIPT_LATIN };
    return hasCharactersByScripts(sourceString, targetScripts);
}
bool hasHangul(std::string_view sourceString) {
    static constexpr std::array targetScripts = { USCRIPT_HANGUL };
    return hasCharactersByScripts(sourceString, targetScripts);
}
bool hasCJK(std::string_view sourceString) {
    static constexpr std::array targetScripts = { USCRIPT_HAN };
    return hasCharactersByScripts(sourceString, targetScripts);
}

// 核心辅助函数
// 接受一个源字符串和一组目标脚本，返回所有匹配字符组成的UTF-8字符串
// 这个和 removeXXX 一样都是遍历的码点，所以可以用宏和 utf8cpp 来加速
std::string extractCharactersByScripts(std::string_view str, std::span<const UScriptCode> targetScripts) {

    std::string resultString;
    UErrorCode errorCode = U_ZERO_ERROR;

    const uint8_t* s = (uint8_t*)str.data();
    const int32_t length = (int32_t)str.length();
    int32_t i = 0;
    UChar32 c;

    while (i < length) {
        U8_NEXT(s, i, length, c);
        const UScriptCode script = uscript_getScript(c, &errorCode);

        if (U_SUCCESS(errorCode)) {
            if (std::ranges::contains(targetScripts, script)) {
                utf8::unchecked::append(c, std::back_inserter(resultString));
            }
        }
    }

    return resultString;
}
std::string extractKatakana(std::string_view sourceString) {
    static constexpr std::array targetScripts = { USCRIPT_KATAKANA };
    return extractCharactersByScripts(sourceString, targetScripts);
}
std::string extractKana(std::string_view sourceString) {
    static constexpr std::array targetScripts = { USCRIPT_HIRAGANA, USCRIPT_KATAKANA };
    return extractCharactersByScripts(sourceString, targetScripts);
}
std::string extractLatin(std::string_view sourceString) {
    static constexpr std::array targetScripts = { USCRIPT_LATIN };
    return extractCharactersByScripts(sourceString, targetScripts);
}
std::string extractHangul(std::string_view sourceString) {
    static constexpr std::array targetScripts = { USCRIPT_HANGUL };
    return extractCharactersByScripts(sourceString, targetScripts);
}
std::string extractCJK(std::string_view sourceString) {
    static constexpr std::array targetScripts = { USCRIPT_HAN };
    return extractCharactersByScripts(sourceString, targetScripts);
}

std::function<std::string(std::string_view)> getTraditionalChineseExtractor()
{
    static const absl::btree_set<std::string_view> excludeList = {
        "乾", "阪", "篠", "塚"
    };
    auto converter = std::make_shared<opencc::SimpleConverter>("BaseConfig/opencc/t2s.json");
    std::function<std::string(std::string_view)> result = [converterR = std::move(converter)](std::string_view str)
        {
            if (const std::string simplified = converterR->Convert(str.data(), str.size());
                simplified == str)
            {
                return std::string{};
            }
            std::string resultStr;
            for (const auto& grapheme : splitIntoGraphemeViews(str)
                | std::views::filter([&](const auto& g) { return !excludeList.contains(g); }))
            {
                if (const std::string simplified = converterR->Convert(grapheme.data(), grapheme.size());
                    simplified != grapheme)
                {
                    resultStr += grapheme;
                }
            }
            return resultStr;
        };
    return result;
}



std::string names2String(const std::vector<std::string>& names) {
    return names | std::views::join_with('|')
        | std::ranges::to<std::string>();
}

std::string names2String(const json& namesItem) {
    return namesItem | std::views::transform([](const json& elem) { return elem.get<std::string>(); })
        | std::views::join_with('|')
        | std::ranges::to<std::string>();
}

std::string getNameString(const Sentence& se) {
    if (se.nameType == NameType::Single) {
        return se.name;
    }
    else if (se.nameType == NameType::Multiple) {
        return names2String(se.names);
    }
    return {};
}

std::string getNameString(const json& item) {
    if (auto it = item.find("name"); it != item.end()) {
        return it->get<std::string>();
    }
    else if (it = item.find("names"); it != item.end()) {
        return names2String(*it);
    }
    return {};
}

const std::string& chooseStringRef(Sentence* sentence, CachePart target) {
    switch (target)
	{
    case CachePart::FileName:
        return sentence->filename;
    case CachePart::Name:
        return sentence->name;
    case CachePart::NameTrans:
        return sentence->nametrans;
    case CachePart::Orig:
        return sentence->orig;
    case CachePart::Preproc:
        return sentence->preproc;
    case CachePart::TransBy:
        return sentence->transby;
    case CachePart::TransRaw:
        return sentence->transraw;
    case CachePart::Transview:
        return sentence->transview;
    case CachePart::None:
    default:
        throw std::runtime_error(gppTr("chooseStringRef", "无法获取字符串的条件目标 %1")
            .arg(std::to_underlying(target))
            .toStdString());
    }
}

std::string chooseString(Sentence* sentence, CachePart target) {
    if (target == CachePart::Index) {
        return std::to_string(sentence->index);
    }
    return chooseStringRef(sentence, target);
}

CachePart chooseCachePart(std::string_view partName) {
    if (const auto it = names2CachePart.find(partName); it != names2CachePart.end()) {
        return it->second;
    }
    throw std::invalid_argument(gppTr("chooseCachePart", "无效的 CachePart 名称: %1")
        .arg(partName)
        .toStdString());
}



ActiveWorkerGuard::ActiveWorkerGuard(const std::shared_ptr<IController>& controller)
    : m_controller(controller)
{
    if (m_controller) {
        m_controller->addThreadNum();
    }
}

ActiveWorkerGuard::~ActiveWorkerGuard()
{
    if (m_controller) {
        m_controller->reduceThreadNum();
    }
}



PluginRunTime choosePluginRunTime(std::string_view pluginNameLower, PluginRunTime defaultTime) {
    if (pluginNameLower.contains("dprerun:")) {
        return PluginRunTime::DPre;
    }
    else if (pluginNameLower.contains("dpostrun:")) {
        return PluginRunTime::DPost;
    }
    else if (pluginNameLower.contains("prerun:")) {
        return PluginRunTime::Pre;
    }
    else if (pluginNameLower.contains("postrun:")) {
        return PluginRunTime::Post;
    }
    return defaultTime;
}

void waitForTransThreads(const std::shared_ptr<IController>& controller, ctpl::thread_pool& pool, std::vector<std::future<void>>& results) {
    std::exception_ptr firstException = nullptr;
    for (auto& result : results) {
        try {
            result.get();
        }
        catch (...) {
            if (!firstException) {
                controller->setShouldStop(true);
                pool.stop();
                firstException = std::current_exception();
            }
        }
    }
    if (firstException) {
        std::rethrow_exception(firstException);
    }
}



bool executeCommand(const std::wstring& program, const std::wstring& args, bool showWindow, int timeDelayAfterCommand) {
#ifdef _WIN32
    std::wstring commandLineStr;

    if (showWindow) {
        // 如果需要显示窗口并延迟关闭：
        // 使用 cmd.exe /C 来执行命令。
        // 格式为: cmd.exe /C " "程序路径" 参数 & timeout /t 3 "
        // & 符号表示：无论前一个程序成功与否，都执行后面的 timeout
        // timeout /t 3 表示等待 3 秒
        // 注意：cmd /C 对引号的处理比较特殊，通常建议在最外层再包一对引号以防路径含空格出错
        commandLineStr = L"cmd.exe /C \"\"" + program + L"\" " + args + L" & timeout /t " + std::to_wstring(timeDelayAfterCommand) + L"\"";
    }
    else {
        // 如果隐藏窗口，保持原样直接运行，不需要 cmd 包装
        commandLineStr = L"\"" + program + L"\" " + args;
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = showWindow ? SW_SHOW : SW_HIDE;
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::vector<wchar_t> commandLineVec(commandLineStr.begin(), commandLineStr.end());
    commandLineVec.push_back(L'\0');

    DWORD creationFlags = 0;
    if (showWindow) {
        creationFlags |= CREATE_NEW_CONSOLE;
    }

    if (!CreateProcessW(nullptr,
        &commandLineVec[0],
        nullptr,
        nullptr,
        FALSE,
        creationFlags,
        nullptr,
        nullptr,
        &si,
        &pi)) {
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
#endif
}

int getConsoleWidth() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) {
        return 80;
    }
    // 获取控制台屏幕缓冲区信息
    if (GetConsoleScreenBufferInfo(h, &csbi)) {
        // 宽度 = 右坐标 - 左坐标 + 1
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80; // 获取失败，返回默认值
#endif
}



bool createParent(const fs::path& path) {
    if (path.has_parent_path() && !fs::exists(path.parent_path())) {
        return fs::create_directories(path.parent_path());
    }
    return false;
}

bool isSameExtension(const fs::path& filePath, std::wstring_view ext) {
    return boost::algorithm::iequals(filePath.extension().wstring(), ext);
}



void extractZip(const fs::path& zipPath, const fs::path& outputDir) {
    const bit7z::Bit7zLibrary library{ "7z.dll" };
    bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::Auto };
    extractor.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    extractor.extract(wide2Ascii(zipPath), wide2Ascii(outputDir));
}

void extractFileFromZip(const fs::path& zipPath, const fs::path& outputDir, std::string_view fileName) {
    bool extracted = false;
    const bit7z::Bit7zLibrary library{ "7z.dll" };
    bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::Auto };
    extractor.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    extractor.extractIf(wide2Ascii(zipPath), wide2Ascii(outputDir), [&](const bit7z::BitArchiveItem& item)
        {
            if (extracted) {
                return bit7z::FilterResult::AbortOperation;
            }
            if (const std::string genericPath = replaceStr(item.path(), "\\", "/"); fileName == genericPath) {
                extracted = true;
                return bit7z::FilterResult::ProcessItem;
            }
            return bit7z::FilterResult::SkipItem;
        });
}

void extractFilesFromZip(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& fileNames) {
    size_t restFileCount = fileNames.size();
    const bit7z::Bit7zLibrary library{ "7z.dll" };
    bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::Auto };
    extractor.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    extractor.extractIf(wide2Ascii(zipPath), wide2Ascii(outputDir), [&](const bit7z::BitArchiveItem& item)
        {
            if (restFileCount == 0) {
                return bit7z::FilterResult::AbortOperation;
            }
            if (const std::string genericPath = replaceStr(item.path(), "\\", "/"); fileNames.contains(genericPath)) {
                --restFileCount;
                return bit7z::FilterResult::ProcessItem;
            }
            return bit7z::FilterResult::SkipItem;
        });
}

void extractZipInclude(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& includePrefixes) {
    const bit7z::Bit7zLibrary library{ "7z.dll" };
    bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::Auto };
    extractor.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    extractor.extractIf(wide2Ascii(zipPath), wide2Ascii(outputDir), [&](const bit7z::BitArchiveItem& item)
        {
            if (const std::string genericPath = replaceStr(item.path(), "\\", "/");
                std::ranges::any_of(includePrefixes, [&](const std::string& prefix)
                    {
                        return genericPath.starts_with(prefix);
                    })
                )
            {
                return bit7z::FilterResult::ProcessItem;
            }
            return bit7z::FilterResult::SkipItem;
        });
}

void extractZipExclude(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& excludePrefixes) {
    const bit7z::Bit7zLibrary library{ "7z.dll" };
    bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::Auto };
    extractor.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    extractor.extractIf(wide2Ascii(zipPath), wide2Ascii(outputDir), [&](const bit7z::BitArchiveItem& item)
        {
            if (const std::string genericPath = replaceStr(item.path(), "\\", "/");
                std::ranges::any_of(excludePrefixes, [&](const std::string& prefix)
                    {
                        return genericPath.starts_with(prefix);
                    })
                )
            {
                return bit7z::FilterResult::SkipItem;
            }
            return bit7z::FilterResult::ProcessItem;
        });
}



namespace toml
{
    ::toml::value uparse(const fs::path& path) {
        std::ifstream ifs(path, std::ios::binary);
        return ::toml::parse(ifs, wide2Ascii(path));
    }

    ::toml::ordered_value uoparse(const fs::path& path) {
        std::ifstream ifs(path, std::ios::binary);
        return ::toml::parse<::toml::ordered_type_config>(ifs, wide2Ascii(path));
    }
}



json parseJson(const fs::path& path, std::ifstream& ifs) {
    ifs.open(path, std::ios::binary);
    struct CloseGuard {
        std::ifstream& stream;
        ~CloseGuard() { stream.close(); }
    } closeGuard{ ifs };
    return json::parse(ifs);
}

json parseJson(const fs::path& path) {
    std::ifstream ifs;
    return parseJson(path, ifs);
}

ordered_json parseOrderedJson(const fs::path& path, std::ifstream& ifs) {
    ifs.open(path, std::ios::binary);
    struct CloseGuard {
        std::ifstream& stream;
        ~CloseGuard() { stream.close(); }
    } closeGuard{ ifs };
    return ordered_json::parse(ifs);
}

ordered_json parseOrderedJson(const fs::path& path) {
    std::ifstream ifs;
    return parseOrderedJson(path, ifs);
}



void loadTokenizeCache
(absl::flat_hash_map<std::string, WordPosVec>& result, const fs::path& cachePath, const std::shared_ptr<spdlog::logger>& logger) {
    try {
        if (fs::exists(cachePath)) {
            parseJson(cachePath).get_to(result);
        }
        else {
            logger->debug(gppTr("loadTokenizeCache", "未找到分词缓存 [%1]")
                .arg(wide2Ascii(cachePath))
                .toStdString());
        }
    }
    catch (const std::exception& e) {
        logger->error(gppTr("loadTokenizeCache", "读取分词缓存 [%1] 失败: %2")
            .arg(wide2Ascii(cachePath))
            .arg(e.what())
            .toStdString());
    }
}

void saveTokenizeCache
(const absl::flat_hash_map<std::string, WordPosVec>& cache, const fs::path& cachePath, const std::shared_ptr<spdlog::logger>& logger) {
    try {
        const json j = cache;
        atomicOutputFile(cachePath, j.dump(2));
        logger->debug(gppTr("saveTokenizeCache", "分词缓存已保存到 [%1]")
            .arg(wide2Ascii(cachePath))
            .toStdString());
    }
    catch (...) {
        logger->error(gppTr("saveTokenizeCache", "分词缓存 [%1] 保存失败")
            .arg(wide2Ascii(cachePath))
            .toStdString());
    }
}



std::string currentTimestampString() {
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    const std::chrono::zoned_time localTime{ std::chrono::current_zone(), now };
    return std::format("{:%Y-%m-%dT%H:%M:%S%Ez}", localTime);
}

uint64_t calculateFileCRC64(const fs::path& filePath) {
    std::ifstream ifs(filePath, std::ios::binary);
    boost::crc_optimal<64, 0x42F0E1EBA9EA3693, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, true, true> crc;
    constexpr size_t BUFFER_SIZE = 1024 * 1024;
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    while (ifs) {
        ifs.read((char*)buffer.data(), BUFFER_SIZE);
        if (const auto readCount = ifs.gcount(); readCount > 0) {
            crc.process_bytes(buffer.data(), (size_t)readCount);
        }
    }
    return crc.checksum();
}

int compareVersion(std::string_view latestVer, std::string_view currentVer)
{
    auto parseVersion = [](std::string_view version) -> std::optional<std::array<int, 3>>
        {
            if (const size_t versionPrefixPos = version.find_last_of('v'); versionPrefixPos != std::string_view::npos) {
                version.remove_prefix(versionPrefixPos + 1);
            }
            const std::vector<std::string_view> parts = splitStringView(version, '.');
            if (parts.size() != 3) {
                return std::nullopt;
            }
            std::array<int, 3> result{};
            for (size_t i = 0; i < result.size(); ++i) {
                const std::optional<int> part = str2Int(parts[i]);
                if (!part.has_value()) {
                    return std::nullopt;
                }
                result[i] = part.value();
            }
            return result;
        };

    const std::optional<std::array<int, 3>> latestVersion = parseVersion(latestVer);
    const std::optional<std::array<int, 3>> currentVersion = parseVersion(currentVer);
    if (!latestVersion.has_value() || !currentVersion.has_value()) {
        return -2;
    }

    if (latestVersion->at(0) > currentVersion->at(0)) {
        return 2;
    }
    if (latestVersion->at(0) < currentVersion->at(0)) {
        return -1;
    }

    for (size_t i = 1; i < latestVersion->size(); ++i) {
        if (latestVersion->at(i) > currentVersion->at(i)) {
            return 1;
        }
        if (latestVersion->at(i) < currentVersion->at(i)) {
            return -1;
        }
    }
    return 0;
}
