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

#include <unicode/unistr.h>
#include <unicode/brkiter.h>
#include <unicode/uscript.h>
#include <unicode/translit.h>

#include <utf8cpp/utf8.h>

#include <cpp-base64/base64.cpp>

#pragma comment(lib, "python3.lib")
#pragma comment(lib, "python312.lib")

module Tool;

import ITranslator;

namespace fs = std::filesystem;


#ifdef _WIN32
std::string wide2Ascii(const std::wstring& wide, UINT codePage, LPBOOL usedDefaultChar) {
    int len = WideCharToMultiByte(codePage, 0, wide.data(), (int)wide.length(), 
        nullptr, 0, nullptr, usedDefaultChar);
    if (len == 0) return {};
    std::string ascii(len, '\0');
    WideCharToMultiByte(codePage, 0, wide.data(), (int)wide.length(), 
        ascii.data(), len, nullptr, nullptr);
    return ascii;
}

std::string wide2Ascii(std::wstring_view wide, UINT codePage, LPBOOL usedDefaultChar) {
    int len = WideCharToMultiByte(codePage, 0, wide.data(), (int)wide.length(),
        nullptr, 0, nullptr, usedDefaultChar);
    if (len == 0) return {};
    std::string ascii(len, '\0');
    WideCharToMultiByte(codePage, 0, wide.data(), (int)wide.length(),
        ascii.data(), len, nullptr, nullptr);
    return ascii;
}

std::wstring ascii2Wide(const std::string& ascii, UINT codePage) {
    int len = MultiByteToWideChar(codePage, 0, ascii.data(), (int)ascii.length(), nullptr, 0);
    if (len == 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(codePage, 0, ascii.data(), (int)ascii.length(), wide.data(), len);
    return wide;
}

std::wstring ascii2Wide(std::string_view ascii, UINT codePage) {
    int len = MultiByteToWideChar(codePage, 0, ascii.data(), (int)ascii.length(), nullptr, 0);
    if (len == 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(codePage, 0, ascii.data(), (int)ascii.length(), wide.data(), len);
    return wide;
}

bool executeCommand(const std::wstring& program, const std::wstring& args, bool showWindow, int timeDelayAfterCommand) {

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
}

int getConsoleWidth() {
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
}
#endif



std::string names2String(const std::vector<std::string>& names) {
    std::string result;
    for (const auto& name : names) {
        result += name + "|";
    }
    if (!result.empty()) {
        result.pop_back();
    }
    return result;
}

std::string names2String(const json& j) {
    std::string result;
    for (const auto& name : j) {
        result += name.get<std::string>() + "|";
    }
    if (!result.empty()) {
        result.pop_back();
    }
    return result;
}

std::string getNameString(const Sentence* se) {
    if (se->nameType == NameType::Single) {
        return se->name;
    }
    if (se->nameType == NameType::Multiple) {
        return names2String(se->names);
    }
    return {};
}

std::string getNameString(const json& j) {
    if (j.contains("name")) {
        return j["name"].get<std::string>();
    }
    if (j.contains("names")) {
        return names2String(j["names"]);
    }
    return {};
}

bool createParent(const fs::path& path) {
    if (path.has_parent_path() && !fs::exists(path.parent_path())) {
        return fs::create_directories(path.parent_path());
    }
    return false;
}

std::optional<int> str2Int(std::string_view sv) {
    int value = 0;
    // 注意：from_chars 不会跳过前导空格！如果需要，得自己 trim 一下
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    // ec == std::errc() 表示解析动作成功
    // ptr == sv.data() + sv.size() 表示整个字符串都被消耗完了（没有剩余垃圾字符）
    if (ec == std::errc() && ptr == sv.data() + sv.size()) {
        return value;
    }
    return std::nullopt;
}

std::vector<std::string> splitTsvLine(const std::string& line, const std::vector<std::string>& delimiters) {
    std::vector<std::string> parts;
    size_t currentPos = 0;

    if (
        std::ranges::any_of(delimiters, [](const std::string& delimiter)
            {
                return delimiter.empty();
            })
        )
    {
        throw std::runtime_error(gppTr("splitTsvLine", "TSV 行切分不允许使用空分隔符"));
    }

    while (currentPos < line.length()) {
        size_t splitPos = std::string::npos;
        size_t delimiterLength = 0;
        for (const auto& delimiter : delimiters) {
            if (const size_t pos = line.find(delimiter, currentPos); pos != std::string::npos && pos < splitPos) {
                splitPos = pos;
                delimiterLength = delimiter.length();
            }
        }
        std::string part = line.substr(currentPos, splitPos - currentPos);
        parts.push_back(std::move(part));

        if (splitPos == std::string::npos) {
            break;
        }
        currentPos = splitPos + delimiterLength;
    }

    return parts;
}


const std::string& chooseStringRef(const Sentence* sentence, CachePart tar) {
    switch (tar) {
    case CachePart::Name:
        return sentence->name;
        break;
    case CachePart::NamePreview:
        return sentence->name_preview;
        break;
    case CachePart::OrigText:
        return sentence->original_text;
        break;
    case CachePart::PreprocText:
        return sentence->pre_processed_text;
        break;
    case CachePart::PretransText:
        return sentence->pre_translated_text;
        break;
    case CachePart::TranslatedBy:
        return sentence->translated_by;
        break;
    case CachePart::TransPreview:
        return sentence->translated_preview;
        break;
    case CachePart::None:
        throw std::runtime_error(gppTr("chooseStringRef", "无效的条件目标: None"));
    default:
        throw std::runtime_error(gppTr("chooseStringRef", "无法获取字符串的无效条件目标: %1", (int)tar));
    }
    return {};
}

std::string chooseString(const Sentence* sentence, CachePart tar) {
    return chooseStringRef(sentence, tar);
}

CachePart chooseCachePart(std::string_view partName) {
    CachePart part;
    if (partName == "name") {
        part = CachePart::Name;
    }
    else if (partName == "names") {
        part = CachePart::Names;
    }
    else if (partName == "name_preview") {
        part = CachePart::NamePreview;
    }
    else if (partName == "names_preview") {
        part = CachePart::NamesPreview;
    }
    else if (partName == "orig_text") {
        part = CachePart::OrigText;
    }
    else if (partName == "preproc_text") {
        part = CachePart::PreprocText;
    }
    else if (partName == "pretrans_text") {
        part = CachePart::PretransText;
    }
    else if (partName == "problems") {
        part = CachePart::Problems;
    }
    else if (partName == "other_info") {
        part = CachePart::OtherInfo;
    }
    else if (partName == "translated_by") {
        part = CachePart::TranslatedBy;
    }
    else if (partName == "trans_preview") {
        part = CachePart::TransPreview;
    }
    else {
        throw std::invalid_argument(gppTr("chooseCachePart", "无效的 CachePart: %1", std::string(partName)));
    }
    return part;
}

bool isSameExtension(const fs::path& filePath, const std::wstring& ext) {
    return str2Lower(filePath.extension()) == str2Lower(ext);
}


// MostCommonChar 和 Grapheme 都遍历的是字形簇而不是码点
std::pair<std::string, int> getMostCommonChar(const std::string& s) {
    if (s.empty()) {
        return { {}, 0 };
    }

    UErrorCode errorCode = U_ZERO_ERROR;
    const icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(s);

    absl::btree_map<icu::UnicodeString, int> counts;

    const std::unique_ptr<icu::BreakIterator> boundary(icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), errorCode));
    if (U_FAILURE(errorCode)) {
        throw std::runtime_error(gppTr("getMostCommonChar", "创建字符边界迭代器失败: %1", u_errorName(errorCode)));
    }
    boundary->setText(ustr);

    int32_t start = boundary->first();
    for (int32_t end = boundary->next(); end != icu::BreakIterator::DONE; start = end, end = boundary->next()) {
        icu::UnicodeString grapheme;
        ustr.extract(start, end - start, grapheme);
        ++counts[grapheme];
    }

    if (counts.empty()) {
        return { {}, 0 };
    }

    const auto maxIterator = std::ranges::max_element(counts, [](const auto& a, const auto& b) 
        {
            return a.second < b.second;
        });

    const icu::UnicodeString mostCommonGrapheme = maxIterator->first;
    int maxCount = maxIterator->second;

    std::string resultStr;

    return { mostCommonGrapheme.toUTF8String(resultStr), maxCount };
}

std::vector<std::string> splitIntoGraphemes(const std::string& sourceString) {
    std::vector<std::string> resultVector;

    if (sourceString.empty()) {
        return resultVector;
    }

    UErrorCode errorCode = U_ZERO_ERROR;
    const icu::UnicodeString uString = icu::UnicodeString::fromUTF8(sourceString);

    const std::unique_ptr<icu::BreakIterator> breakIterator(
        icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), errorCode)
    );

    if (U_FAILURE(errorCode)) {
        throw std::runtime_error(gppTr("splitIntoGraphemes", "创建字符边界迭代器失败: %1", u_errorName(errorCode)));
    }

    breakIterator->setText(uString);

    int32_t start = breakIterator->first();
    for (int32_t end = breakIterator->next(); end != icu::BreakIterator::DONE; start = end, end = breakIterator->next()) {
        icu::UnicodeString graphemeUString;
        uString.extract(start, end - start, graphemeUString);
        std::string graphemeStdString;
        resultVector.push_back(graphemeUString.toUTF8String(graphemeStdString));
    }

    return resultVector;
}

size_t countGraphemesImpl(auto&& sourceString)
{
    if (sourceString.empty()) {
        return 0;
    }
    UErrorCode errorCode = U_ZERO_ERROR;
    const icu::UnicodeString uString = icu::UnicodeString::fromUTF8(sourceString);
    const std::unique_ptr<icu::BreakIterator> breakIterator(
        icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), errorCode)
    );
    if (U_FAILURE(errorCode)) {
        throw std::runtime_error(gppTr("countGraphemes", "创建字符边界迭代器失败: %1", u_errorName(errorCode)));
    }
    breakIterator->setText(uString);

    int32_t start = breakIterator->first();
    size_t count = 0;
    for (int32_t end = breakIterator->next(); end != icu::BreakIterator::DONE; end = breakIterator->next()) {
        ++count;
    }
    return count;
}

size_t countGraphemes(std::string_view sourceString) {
    return countGraphemesImpl(sourceString);
}

size_t countGraphemes(const std::string& sourceString) {
    return countGraphemesImpl(sourceString);
}

std::string truncateUtf8Prefix(std::string_view text, size_t maxCodepoints, std::string_view ellipsis) {
    if (text.empty() || text.length() < maxCodepoints) {
        return std::string(text);
    }
    if (maxCodepoints == 0) {
        return std::string(ellipsis);
    }
    const uint8_t* s = (const uint8_t*)text.data();
    const int32_t length = (int32_t)text.size();
    int32_t i = 0;
    size_t count = 0;
    while (i < length && count < maxCodepoints) {
        U8_FWD_1(s, i, length);
        ++count;
    }
    if (i >= length) {
        return std::string(text);
    }
    std::string result(text.substr(0, (size_t)i));
    result += ellipsis;
    return result;
}

std::string truncateUtf8Suffix(std::string_view text, size_t maxCodepoints, std::string_view ellipsis) {
    if (text.empty() || text.length() < maxCodepoints) {
        return std::string(text);
    }
    if (maxCodepoints == 0) {
        return std::string(ellipsis);
    }
    const uint8_t* s = (const uint8_t*)text.data();
    int32_t i = (int32_t)text.size();
    size_t count = 0;
    while (i > 0 && count < maxCodepoints) {
        U8_BACK_1(s, 0, i);
        ++count;
    }
    if (i <= 0) {
        return std::string(text);
    }
    std::string result(ellipsis);
    result += text.substr((size_t)i);
    return result;
}

std::vector<std::string> splitIntoTokens(const WordPosVec& wordPosVec, const std::string& text)
{
    std::vector<std::string> tokens;

    size_t searchPos = 0; // 在原始句子中搜索的起始位置
    for (const auto& wordPos : wordPosVec) {
        const auto& token = wordPos.front();
        // 从 searchPos 开始查找当前 token
        const size_t tokenPos = text.find(token, searchPos);
        // 错误处理：如果在预期位置找不到 token，说明输入有问题
        if (tokenPos == std::string::npos) {
            throw std::runtime_error(gppTr("splitIntoTokens", "在原句剩余部分中找不到 token '%1'。", token));
        }
        // 1. 提取并添加 token 前面的空白部分
        if (tokenPos > searchPos) {
            tokens.push_back(text.substr(searchPos, tokenPos - searchPos));
        }
        // 2. 更新下一次搜索的起始位置
        searchPos = tokenPos + token.length();
        // 3. 添加 token 本身
        tokens.push_back(std::move(token));
    }
    // 4. 处理最后一个 token 后面的尾随空白
    if (searchPos < text.length()) {
        tokens.push_back(text.substr(searchPos));
    }

    return tokens;
}

// 计算子串出现次数
int countSubstring(const std::string& text, std::string_view sub) {
    return (int)std::ranges::distance(text | std::views::split(sub)) - 1;
}

// 计算的是子串在删去子串后的主串中出现的位置
std::vector<double> getSubstringPositions(const std::string& text, std::string_view sub) {
    if (text.empty() || sub.empty()) return {};
    std::vector<size_t> positions;
    std::vector<double> relpositions;

    for (size_t offset = text.find(sub); offset != std::string::npos; offset = text.find(sub, offset + sub.length())) {
        positions.push_back(offset);
    }
    const size_t newTotalLength = text.length() - positions.size() * sub.length();
    if (newTotalLength == 0) {
        return {};
    }
    for (size_t i = 0; i < positions.size(); i++) {
        const size_t newPos = positions[i] - i * sub.length();
        relpositions.push_back((double)newPos / newTotalLength);
    }
    return relpositions;
}

std::string& replaceStrInplace(std::string& str, std::string_view org, std::string_view rep) {
    boost::replace_all(str, org, rep);
    return str;
}

std::string replaceStr(const std::string& str, std::string_view org, std::string_view rep) {
    return boost::replace_all_copy(str, org, rep);
}

// 核心辅助函数
// 接受一个源字符串和一组目标脚本，返回所有匹配字符组成的UTF-8字符串
// 这个和 removeXXX 一样都是遍历的码点，所以可以用宏和 utf8cpp 来加速
std::string extractCharactersByScripts(const std::string& sourceString, const std::vector<UScriptCode>& targetScripts) {

    std::string resultString;
    UErrorCode errorCode = U_ZERO_ERROR;

    const uint8_t* s = (uint8_t*)sourceString.c_str();
    const int32_t length = (int32_t)sourceString.length();
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

std::string extractKatakana(const std::string& sourceString) {
    return extractCharactersByScripts(sourceString, { USCRIPT_KATAKANA });
}

std::string extractKana(const std::string& sourceString) {
    return extractCharactersByScripts(sourceString, { USCRIPT_HIRAGANA, USCRIPT_KATAKANA });
}

std::string extractLatin(const std::string& sourceString) {
    return extractCharactersByScripts(sourceString, { USCRIPT_LATIN });
}

std::string extractHangul(const std::string& sourceString) {
    return extractCharactersByScripts(sourceString, { USCRIPT_HANGUL });
}

std::string extractCJK(const std::string& sourceString) {
    return extractCharactersByScripts(sourceString, { USCRIPT_HAN });
}

std::string removePunctuation(const std::string& sourceString) {
    std::string resultString;
    const uint8_t* s = (uint8_t*)sourceString.c_str();
    const int32_t length = (int32_t)sourceString.length();
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

std::string removeWhitespace(const std::string& sourceString) {
    std::string resultString;
    const uint8_t* s = (uint8_t*)sourceString.c_str();
    const int32_t length = (int32_t)sourceString.length();
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

std::function<std::string(const std::string&)> getTraditionalChineseExtractor(const std::shared_ptr<spdlog::logger>& logger)
{
    // 是否需要线程安全？(似乎是不需要)
    try {
        auto converter = std::make_shared<opencc::SimpleConverter>("BaseConfig/opencc/t2s.json");
        absl::btree_set<std::string_view> excludeList = {
            "乾", "阪", "篠", "塚"
        };
        std::function<std::string(const std::string&)> result = [excludeListR = std::move(excludeList), converterR = std::move(converter)](const std::string& sourceString)
            {
                if (const std::string simplified = converterR->Convert(sourceString); simplified == sourceString) {
                    return std::string{};
                }
                std::string resultStr;
                for (const auto& grapheme : splitIntoGraphemes(sourceString)
                    | std::views::filter([&](const std::string& g) { return !excludeListR.contains(g); }))
                {
                    if (const std::string simplified = converterR->Convert(grapheme); simplified != grapheme)
                    {
                        resultStr += grapheme;
                    }
                }
                return resultStr;
            };
        logger->info(gppTr("getTraditionalChineseExtractor", "使用 OpenCC 进行繁体中文检测"));
        return result;
    }
    catch (...) {
        logger->error(gppTr("getTraditionalChineseExtractor", "OpenCC 不可用，尝试回退到基于 ICU 的繁体中文检测"));
        UErrorCode status = U_ZERO_ERROR;
        auto toSimplified = std::shared_ptr<icu::Transliterator>(icu::Transliterator::createInstance("Traditional-Simplified", UTRANS_FORWARD, status));
        if (U_FAILURE(status)) {
            throw std::runtime_error(gppTr("getTraditionalChineseExtractor", "基于 ICU 的繁体中文检测不可用"));
        }
        auto toTraditional = std::shared_ptr<icu::Transliterator>(icu::Transliterator::createInstance("Simplified-Traditional", UTRANS_FORWARD, status));
        if (U_FAILURE(status)) {
            throw std::runtime_error(gppTr("getTraditionalChineseExtractor", "基于 ICU 的简体中文检测不可用"));
        }

        // 白名单/排除列表：用于解决简繁转换中的歧义问题。
        // "著" (U+8457) 是一个典型例子，它在简体中文里也是合法字符，但T->S的转换规则可能导致误判。
        absl::btree_set<UChar32> excludeList = {
            U'著', U'乾', U'阪', U'篠', U'塚'
        };
        std::function<std::string(const std::string&)>result = [excludeListR = std::move(excludeList), toSimplifiedR = std::move(toSimplified),
            toTraditionalR = std::move(toTraditional)](const std::string& sourceString)
            {
				{
                    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(sourceString);
                    icu::UnicodeString ustrS = ustr;
                    toSimplifiedR->transliterate(ustrS);
                    if (ustrS == ustr) {
                        return std::string{};
                    }
				}

                absl::btree_set<UChar32> traditionalChars;

                const uint8_t* s = (uint8_t*)sourceString.c_str();
                const int32_t length = (int32_t)sourceString.length();
                int32_t i = 0;
                UChar32 c;

                while (i < length) {

                    U8_NEXT(s, i, length, c);

                    // 1. 必须是汉字
                    UErrorCode scriptErr = U_ZERO_ERROR;
                    if (uscript_getScript(c, &scriptErr) != USCRIPT_HAN || U_FAILURE(scriptErr)) {
                        continue;
                    }

                    // 2. 检查是否在排除列表中
                    if (excludeListR.contains(c)) {
                        continue;
                    }

                    const icu::UnicodeString uCharSource(c);

                    icu::UnicodeString uSimplified = uCharSource;
                    toSimplifiedR->transliterate(uSimplified);
                    // 3. 繁体转简体后必须有变化
                    if (uCharSource == uSimplified) {
                        continue;
                    }

                    // 4. 核心双向检查：简体转回繁体，必须能得到原始字符，确保转换是明确且可逆的
                    icu::UnicodeString uReTraditional = uSimplified;
                    toTraditionalR->transliterate(uReTraditional);

                    if (uReTraditional == uCharSource) {
                        traditionalChars.insert(c);
                    }
                }

                icu::UnicodeString resultUStr;
                for (const UChar32 ch : traditionalChars) {
                    resultUStr.append(ch);
                }

                std::string resultStr;
                return resultUStr.toUTF8String(resultStr);
            };
        logger->info(gppTr("getTraditionalChineseExtractor", "使用基于 ICU 的繁体中文检测"));
        return result;
    }
    return {};
}


void loadTokenizeCache
(absl::flat_hash_map<std::string, std::vector<std::vector<std::string>>>& result, const fs::path& cachePath, const std::shared_ptr<spdlog::logger>& logger) {
    try {
        if (fs::exists(cachePath)) {
            std::ifstream ifs(cachePath, std::ios::binary);
            json::parse(ifs).get_to(result);
        }
        else {
            logger->debug(gppTr("loadTokenizeCache", "未找到分词缓存 %1", wide2Ascii(cachePath)));
        }
    }
    catch (const json::parse_error& e) {
        logger->error(gppTr("loadTokenizeCache", "读取分词缓存 %1 失败: %2", wide2Ascii(cachePath), e.what()));
    }
}

void saveTokenizeCache
(const absl::flat_hash_map<std::string, std::vector<std::vector<std::string>>>& cache, const fs::path& cachePath, const std::shared_ptr<spdlog::logger>& logger) {
    try {
        createParent(cachePath);
        const json j = cache;
        std::ofstream ofs(cachePath, std::ios::binary);
        ofs << j.dump(2);
        ofs.close();
        logger->debug(gppTr("saveTokenizeCache", "分词缓存已保存到 %1", wide2Ascii(cachePath)));
    }
    catch (...) {
        logger->error(gppTr("saveTokenizeCache", "分词缓存 %1 保存失败", wide2Ascii(cachePath)));
    }
}

void extractFileFromZip(const fs::path& zipPath, const fs::path& outputDir, const std::string& fileName) {
    const bit7z::Bit7zLibrary library{ "7z.dll" };
    bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::Auto };
    extractor.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    extractor.extractMatching(wide2Ascii(zipPath), fileName, wide2Ascii(outputDir));
}

void extractZip(const fs::path& zipPath, const fs::path& outputDir) {
    const bit7z::Bit7zLibrary library{ "7z.dll" };
    bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::Auto };
    extractor.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    extractor.extract(wide2Ascii(zipPath), wide2Ascii(outputDir));
}

void extractZipExclude(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& excludePrefixes) {
    const bit7z::Bit7zLibrary library{ "7z.dll" };
    std::vector<uint32_t> indices;

    bit7z::BitArchiveReader archive{ library, wide2Ascii(zipPath) };
    for (const auto& item : archive) {
        if (
            std::ranges::any_of(excludePrefixes, [&](const std::string& prefix) { return item.path().starts_with(prefix); })
            )
        {
            continue;
        }
        indices.push_back(item.index());
    }

    bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::Auto };
    extractor.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    extractor.extractItems(wide2Ascii(zipPath), indices, wide2Ascii(outputDir));
}

std::string nowTimestampString() {
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    const std::chrono::zoned_time localTime{ std::chrono::current_zone(), now };
    return std::format("{:%Y-%m-%dT%H:%M:%S%Ez}", localTime);
}

void saveJsonFile(const fs::path& path, const json& value) {
    createParent(path);
    std::ofstream ofs(path, std::ios::binary);
    ofs << value.dump(2);
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

bool cmpVer(const std::string& latestVer, const std::string& currentVer, bool& isCompatible)
{
    bool isCurrentVerPre = false;
    auto removePostfix = [&](std::string v) -> std::string
        {
            while (true) {
                if (
                    !v.empty() &&
                    (v.back() < '0' || v.back() > '9')
                    ) {
                    v.pop_back();
                    isCurrentVerPre = true;
                }
                else {
                    break;
                }
            }
            return v;
        };

    const std::string fixedCurrentVer = removePostfix(currentVer);
    const std::string v1s = latestVer.find_last_of("v") == std::string::npos ? latestVer : latestVer.substr(latestVer.find_last_of("v") + 1);
    const std::string v2s = fixedCurrentVer.find_last_of("v") == std::string::npos ? fixedCurrentVer : fixedCurrentVer.substr(fixedCurrentVer.find_last_of("v") + 1);

    const std::vector<std::string> latestVerParts = splitString(v1s, '.');
    const std::vector<std::string> currentVerParts = splitString(v2s, '.');

    const size_t len = std::max(latestVerParts.size(), currentVerParts.size());

    for (size_t i = 0; i < len; i++) {
        const int latestVerPart = i < latestVerParts.size() ? str2Int(latestVerParts[i]).value_or(0) : 0;
        const int currentVerPart = i < currentVerParts.size() ? str2Int(currentVerParts[i]).value_or(0) : 0;
        if (i == 0) {
            isCompatible = latestVerPart <= currentVerPart;
        }

        if (latestVerPart > currentVerPart) {
            return true;
        }
        else if (latestVerPart < currentVerPart) {
            return false;
        }
    }

    if (isCurrentVerPre) {
        return true;
    }

    return false;
}

PluginRunTime choosePluginRunTime(const std::string& pluginNameLower, PluginRunTime defaultTime) {
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

void waitForThreads(ctpl::thread_pool& pool, std::vector<std::future<void>>& results) {
    std::exception_ptr firstException = nullptr;
    for (auto& result : results) {
        try {
            result.get();
        }
        catch (...) {
            if (!firstException) {
                pool.stop();
                firstException = std::current_exception();
            }
        }
    }
    if (firstException) {
        std::rethrow_exception(firstException);
    }
}

bool isApiTranslationEngine(TransEngine transEngine) {
    switch (transEngine)
    {
    case TransEngine::ForGalJson:
    case TransEngine::ForGalTsv:
    case TransEngine::ForNovelTsv:
    case TransEngine::DeepseekJson:
    case TransEngine::Sakura:
        return true;
    default:
        return false;
    }
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

template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const std::string& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const int& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const double& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const bool& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const toml::array& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const toml::table& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const toml::value& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const toml::ordered_array& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const toml::ordered_table& value);
template toml::ordered_value& insertToml(toml::ordered_value& table, const std::string& path, const toml::ordered_value& value);
