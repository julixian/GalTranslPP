module;

#ifdef _WIN32
#include <Windows.h>
#endif
#include <ranges>
#include <spdlog/spdlog.h>
#include <zip.h>
#include <unicode/unistr.h>
#include <unicode/uchar.h>
#include <unicode/brkiter.h>
#include <unicode/schriter.h>
#include <unicode/uscript.h>

export module Tool;

import <nlohmann/json.hpp>;
import <toml.hpp>;
export import std;
export import GPPDefines;
export import ITranslator;

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;
namespace fs = std::filesystem;

export {

#ifdef _WIN32
    std::string wide2Ascii(const std::wstring& wide, UINT CodePage = 65001, LPBOOL usedDefaultChar = nullptr);

    std::wstring ascii2Wide(const std::string& ascii, UINT CodePage = 65001);

    std::string ascii2Ascii(const std::string& ascii, UINT src, UINT dst, LPBOOL usedDefaultChar = nullptr);

    bool executeCommand(const std::string& utf8Command);

    int getConsoleWidth();
#endif

    std::string getNameString(const Sentence* se);

    std::string getNameString(const json& j);

    void createParent(const fs::path& path);

    std::wstring wstr2Lower(const std::wstring& wstr);

    std::vector<std::string> splitString(const std::string& s, char delimiter);

    std::vector<std::string> splitString(const std::string& s, const std::string& delimiter);

    std::vector<std::string> splitTsvLine(const std::string& line, const std::vector<std::string>& delimiters);

    const std::string& chooseStringRef(const Sentence* sentence, CachePart tar);

    std::string chooseString(const Sentence* sentence, CachePart tar);

    CachePart chooseCachePart(const std::string& partName);

    bool isSameExtension(const fs::path& filePath, const std::wstring& ext);

    std::string removePunctuation(const std::string& text);

    std::pair<std::string, int> getMostCommonChar(const std::string& s);

    std::vector<std::string> splitIntoGraphemes(const std::string& sourceString);

    int countSubstring(const std::string& text, const std::string& sub);

    std::vector<double> getSubstringPositions(const std::string& text, const std::string& sub);

    std::string& replaceStrInplace(std::string& str, const std::string& org, const std::string& rep);

    std::string extractCharactersByScripts(const std::string& sourceString, const std::vector<UScriptCode>& targetScripts);

    std::string extractKatakana(const std::string& sourceString);

    std::string extractKana(const std::string& sourceString);

    std::string extractLatin(const std::string& sourceString);

    std::string extractHangul(const std::string& sourceString);

    std::string extractNonGbkChars(const std::string& sourceString);

    void extractZip(const fs::path& zipPath, const fs::path& outputDir);


    template<typename T>
    T calculateAbs(T a, T b) {
        return a > b ? a - b : b - a;
    }

    // 辅助函数：在一个 TOML 表中，根据一个由键组成的路径向量来查找值
    template<typename TC>
    const toml::basic_value<TC>* findValueByPath(const toml::basic_value<TC>& table, const std::vector<std::string>& keys) {
        const toml::basic_value<TC>* pCurrentValue = &table;
        for (const auto& key : keys) {
            // 确保当前值是一个表 (table) 或有序表 (ordered_value)
            if (!pCurrentValue->is_table()) {
                return nullptr; // 路径中间部分不是一个表，无法继续查找
            }
            // 检查表中是否包含下一个键
            if (!pCurrentValue->contains(key)) {
                return nullptr; // 键不存在
            }
            // 移动到下一层
            pCurrentValue = &pCurrentValue->at(key);
        }
        return pCurrentValue;
    }

    template<typename T, typename TC, typename TC2>
    auto parseToml(const toml::basic_value<TC>& config, const toml::basic_value<TC2>& backup, const std::string& path) -> decltype(auto) {
        const std::vector<std::string> keys = splitString(path, '.');
        if (
            keys.empty() ||
            std::ranges::any_of(keys, [](const std::string& key) { return key.empty(); })
            ) {
            throw std::runtime_error("Invalid TOML path: " + path);
        }
        if (auto pValue = findValueByPath(config, keys)) {
            return toml::get<T>(*pValue);
        }
        if (auto pValue = findValueByPath(backup, keys)) {
            return toml::get<T>(*pValue);
        }
        throw std::runtime_error("Failed to find value in TOML: " + path);
    }

    template<typename TC,typename T>
    toml::basic_value<TC>& insertToml(toml::basic_value<TC>& table, const std::string& path, const T& value)
    {
        const std::vector<std::string> keys = splitString(path, '.');
        auto* pCurrentTable = &table.as_table();
        for (size_t i = 0; i < keys.size() - 1; i++) {
            auto& subTable = (*pCurrentTable)[keys[i]];
            if (!subTable.is_table()) {
                subTable = toml::table{};
            }
            pCurrentTable = &subTable.as_table();
        }
        
        auto& lastVal = (*pCurrentTable)[keys.back()];
        if constexpr (std::is_same_v<toml::basic_value<TC>, toml::ordered_value>) {
            const auto orgComments = lastVal.comments();
            lastVal = toml::ordered_value{ value };
            lastVal.comments() = orgComments;
        }
        else {
            const auto orgComments = lastVal.comments();
            lastVal = toml::value{ value };
            lastVal.comments() = orgComments;
        }
        return table;
    }

}



module :private;

#ifdef _WIN32
std::string wide2Ascii(const std::wstring& wide, UINT CodePage, LPBOOL usedDefaultChar) {
    int len = WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, usedDefaultChar);
    if (len == 0) return {};
    std::string ascii(len, '\0');
    WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
    ascii.pop_back();
    return ascii;
}

std::wstring ascii2Wide(const std::string& ascii, UINT CodePage) {
    int len = MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, nullptr, 0);
    if (len == 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, &wide[0], len);
    wide.pop_back();
    return wide;
}

std::string ascii2Ascii(const std::string& ascii, UINT src, UINT dst, LPBOOL usedDefaultChar) {
    return wide2Ascii(ascii2Wide(ascii, src), dst, usedDefaultChar);
}

bool executeCommand(const std::string& utf8Command) {
    std::wstring utf16Command = ascii2Wide("cmd.exe /c " + utf8Command);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.dwFlags |= STARTF_USESHOWWINDOW; // 指定 wShowWindow 成员有效
    si.wShowWindow = SW_HIDE;          // 将窗口设置为隐藏
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // CreateProcessW需要一个可写的命令行缓冲区
    std::vector<wchar_t> commandLine(utf16Command.begin(), utf16Command.end());
    commandLine.push_back(L'\0');

    if (!CreateProcessW(NULL,           // lpApplicationName
        &commandLine[0], // lpCommandLine (must be writable)
        NULL,           // lpProcessAttributes
        NULL,           // lpThreadAttributes
        FALSE,          // bInheritHandles
        0,              // dwCreationFlags
        NULL,           // lpEnvironment
        NULL,           // lpCurrentDirectory
        &si,            // lpStartupInfo
        &pi)) {         // lpProcessInformation
        return false;
    }

    // 等待子进程结束
    WaitForSingleObject(pi.hProcess, INFINITE);

    // 清理句柄
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
    else if (se->nameType == NameType::Multiple) {
        return names2String(se->names);
    }
    return {};
}

std::string getNameString(const json& j) {
    if (j.contains("name")) {
        return j["name"].get<std::string>();
    }
    else if (j.contains("names")) {
        return names2String(j["names"]);
    }
    return {};
}

void createParent(const fs::path& path) {
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path());
    }
}

std::wstring wstr2Lower(const std::wstring& wstr) {
    std::wstring result = wstr;
    std::transform(result.begin(), result.end(), result.begin(), [](wchar_t wc) { return std::tolower(wc); });
    return result;
}

std::vector<std::string> splitString(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<std::string> splitString(const std::string& s, const std::string& delimiter) {
    std::vector<std::string> parts;
    for (const auto& part_view : std::views::split(s, delimiter)) {
        parts.emplace_back(part_view.begin(), part_view.end());
    }
    return parts;
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
        throw std::runtime_error("Empty delimiter is not allowed in TSV line splitting");
    }

    while (currentPos < line.length()) {
        size_t splitPos = std::string::npos;
        size_t delimiterLength = 0;
        for (const auto& delimiter : delimiters) {
            size_t pos = line.find(delimiter, currentPos);
            if (pos != std::string::npos && pos < splitPos) {
                splitPos = pos;
                delimiterLength = delimiter.length();
            }
        }
        std::string part = line.substr(currentPos, splitPos - currentPos);
        parts.push_back(part);

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
    case CachePart::TransPreview:
        return sentence->translated_preview;
        break;
    case CachePart::None:
        throw std::runtime_error("Invalid condition target: None");
    default:
        throw std::runtime_error("Invalid condition target");
    }
    return {};
}

std::string chooseString(const Sentence* sentence, CachePart tar) {
    return chooseStringRef(sentence, tar);
}

CachePart chooseCachePart(const std::string& partName) {
    CachePart part = CachePart::None;
    if (partName == "name") {
        part = CachePart::Name;
    }
    else if (partName == "name_preview") {
        part = CachePart::NamePreview;
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
    else if (partName == "trans_preview") {
        part = CachePart::TransPreview;
    }
    return part;
}

bool isSameExtension(const fs::path& filePath, const std::wstring& ext) {
    return _wcsicmp(filePath.extension().wstring().c_str(), ext.c_str()) == 0;
}

std::string removePunctuation(const std::string& text) {
    UErrorCode errorCode = U_ZERO_ERROR;
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(text);
    icu::UnicodeString result;

    // 创建一个用于遍历字形簇的 BreakIterator
    std::unique_ptr<icu::BreakIterator> boundary(icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), errorCode));
    if (U_FAILURE(errorCode)) {
        throw std::runtime_error(std::format("Failed to create a character break iterator: {}", u_errorName(errorCode)));
    }
    boundary->setText(ustr);

    int32_t start = boundary->first();
    // 遍历每个字形簇
    for (int32_t end = boundary->next(); end != icu::BreakIterator::DONE; start = end, end = boundary->next()) {
        icu::UnicodeString grapheme;
        ustr.extract(start, end - start, grapheme); // 提取一个完整的字形簇
        // 判断这个字形簇是否是标点
        // 一个标点符号通常自身就是一个单独的码点构成的字形簇。
        // 如果一个字形簇包含多个码点（如 'e' + '´'），它几乎不可能是标点。
        bool isPunctuationCluster = (grapheme.length() == 1 && u_ispunct(grapheme.char32At(0)));
        if (!isPunctuationCluster) {
            result.append(grapheme); // 如果不是标点，则保留整个字形簇
        }
    }

    std::string resultStr;
    return result.toUTF8String(resultStr);
}

std::pair<std::string, int> getMostCommonChar(const std::string& s) {
    if (s.empty()) {
        return { {}, 0 };
    }

    UErrorCode errorCode = U_ZERO_ERROR;
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(s);

    std::map<icu::UnicodeString, int> counts;

    std::unique_ptr<icu::BreakIterator> boundary(icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), errorCode));
    if (U_FAILURE(errorCode)) {
        throw std::runtime_error(std::format("Failed to create a character break iterator: {}", u_errorName(errorCode)));
    }
    boundary->setText(ustr);

    int32_t start = boundary->first();
    for (int32_t end = boundary->next(); end != icu::BreakIterator::DONE; start = end, end = boundary->next()) {
        icu::UnicodeString grapheme;
        ustr.extract(start, end - start, grapheme);
        counts[grapheme]++;
    }

    if (counts.empty()) {
        return { {}, 0 };
    }

    auto maxIterator = std::max_element(counts.begin(), counts.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

    icu::UnicodeString mostCommonGrapheme = maxIterator->first;
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
    icu::UnicodeString uString = icu::UnicodeString::fromUTF8(sourceString);

    std::unique_ptr<icu::BreakIterator> breakIterator(
        icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), errorCode)
    );

    if (U_FAILURE(errorCode)) {
        throw std::runtime_error(std::format("Failed to create a character break iterator: {}", u_errorName(errorCode)));
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

// 计算子串出现次数
int countSubstring(const std::string& text, const std::string& sub) {
    if (sub.empty()) return 0;
    int count = 0;
    for (size_t offset = text.find(sub); offset != std::string::npos; offset = text.find(sub, offset + sub.length())) {
        ++count;
    }
    return count;
}

// 计算的是子串在删去子串后的主串中出现的位置
std::vector<double> getSubstringPositions(const std::string& text, const std::string& sub) {
    if (text.empty() || sub.empty()) return {};
    std::vector<size_t> positions;
    std::vector<double> relpositions;

    for (size_t offset = text.find(sub); offset != std::string::npos; offset = text.find(sub, offset + sub.length())) {
        positions.push_back(offset);
    }
    size_t newTotalLength = text.length() - positions.size() * sub.length();
    if (newTotalLength == 0) {
        return {};
    }
    for (size_t i = 0; i < positions.size(); i++) {
        size_t newPos = positions[i] - i * sub.length();
        relpositions.push_back((double)newPos / newTotalLength);
    }
    return relpositions;
}

std::string& replaceStrInplace(std::string& str, const std::string& org, const std::string& rep) {
    size_t pos = 0;
    while ((pos = str.find(org, pos)) != std::string::npos) {
        str = str.replace(pos, org.length(), rep);
        pos += rep.length();
    }
    return str;
}

// 核心辅助函数
// 接受一个源字符串和一组目标脚本，返回所有匹配字符组成的UTF-8字符串
std::string extractCharactersByScripts(const std::string& sourceString, const std::vector<UScriptCode>& targetScripts) {

    icu::UnicodeString resultUString;
    icu::UnicodeString sourceUString = icu::UnicodeString::fromUTF8(sourceString);

    icu::StringCharacterIterator iter(sourceUString);
    UChar32 codePoint;
    UErrorCode errorCode = U_ZERO_ERROR;

    while (iter.hasNext()) {
        codePoint = iter.next32PostInc();
        UScriptCode script = uscript_getScript(codePoint, &errorCode);

        if (U_SUCCESS(errorCode)) {
            auto it = std::find(targetScripts.begin(), targetScripts.end(), script);
            if (it != targetScripts.end()) {
                resultUString.append(codePoint);
            }
        }
    }

    std::string resultString;
    return resultUString.toUTF8String(resultString);
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

std::string extractNonGbkChars(const std::string& sourceString) {
#ifdef _WIN32
    std::wstring wstr = ascii2Wide(sourceString);
    BOOL usedDefaultChar = FALSE;
    int len = WideCharToMultiByte(936, 0, wstr.c_str(), -1, nullptr, 0, "?", &usedDefaultChar);
    if (!usedDefaultChar) {
        return "";
    }

    std::string result;
    std::set<UChar32> seen_chars;
    icu::UnicodeString uString = icu::UnicodeString::fromUTF8(sourceString);
    icu::StringCharacterIterator iter(uString);
    UChar32 c;
    while (iter.hasNext()) {
        c = iter.next32PostInc();
        icu::UnicodeString uchar_str(c);
        std::string char_str;
        uchar_str.toUTF8String(char_str);
        std::wstring wchar_str = ascii2Wide(char_str);
        BOOL charUsedDefault = FALSE;
        WideCharToMultiByte(936, 0, wchar_str.c_str(), -1, nullptr, 0, "?", &charUsedDefault);
        if (charUsedDefault) {
            if (seen_chars.find(c) == seen_chars.end()) {
                result += char_str;
                seen_chars.insert(c);
            }
        }
    }
    return result;
#else
    return "";
#endif
}

void extractZip(const fs::path& zipPath, const fs::path& outputDir) {
    int error = 0;
    zip* za = zip_open(wide2Ascii(zipPath).c_str(), 0, &error);
    if (!za) {
        throw std::runtime_error(std::format("Failed to open zip archive: {}", wide2Ascii(zipPath)));
    }
    zip_int64_t num_entries = zip_get_num_entries(za, 0);
    for (zip_int64_t i = 0; i < num_entries; i++) {
        zip_stat_t zs;
        zip_stat_index(za, i, 0, &zs);
        fs::path outpath = outputDir / ascii2Wide(zs.name);
        if (zs.name[strlen(zs.name) - 1] == '/') {
            fs::create_directories(outpath);
        }
        else {
            zip_file* zf = zip_fopen_index(za, i, 0);
            if (!zf) {
                throw std::runtime_error(std::format("Failed to open file in zip archive: {}, file: {}", wide2Ascii(zipPath), zs.name));
            }
            std::vector<char> buffer(zs.size);
            zip_fread(zf, buffer.data(), zs.size);
            zip_fclose(zf);
            fs::create_directories(outpath.parent_path());
            std::ofstream ofs(outpath, std::ios::binary);
            ofs.write(buffer.data(), buffer.size());
        }
    }
    zip_close(za);
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