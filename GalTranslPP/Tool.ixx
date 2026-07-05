module;

#include "GPPMacros.hpp"
#ifdef _WIN32
#include <Windows.h>
#endif
#include <ctpl_stl.h>
#include <toml.hpp>
#include <unicode/uscript.h>

export module Tool;

export import GPPDefines;
export import ITranslator;

namespace fs = std::filesystem;

export
{
    std::string wide2Ascii(const std::wstring& wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr);
    std::string wide2Ascii(std::wstring_view wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr);
    std::string wide2Ascii(const wchar_t* wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr) {
        return wide2Ascii(std::wstring_view(wide), codePage, usedDefaultChar);
    }
    template<typename T>
    requires(std::is_same_v<std::remove_cvref_t<T>, fs::path>)
    std::string wide2Ascii(T&& path, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr) {
#ifdef _WIN32
        return wide2Ascii(path.native(), codePage, usedDefaultChar);
#else
        return wide2Ascii(path.wstring(), codePage, usedDefaultChar);
#endif
    }
    std::wstring ascii2Wide(const std::string& ascii, UINT codePage = CP_UTF8);
    std::wstring ascii2Wide(std::string_view ascii, UINT codePage = CP_UTF8);

    bool executeCommand(const std::wstring& program, const std::wstring& args, bool showWindow = true, int timeDelayAfterCommand = 5);

    int getConsoleWidth();


    std::string getNameString(const Sentence* se);
    std::string getNameString(const json& j);

    bool createParent(const fs::path& path);

    template <typename CharT, typename Traits, typename Alloc>
    auto& str2LowerInplace(std::basic_string<CharT, Traits, Alloc>& str) {
        for (CharT& ch : str) {
            if constexpr (std::is_same_v<CharT, wchar_t>) {
                if (ch >= L'A' && ch <= L'Z') {
                    ch = (wchar_t)(ch - L'A' + L'a');
                }
            }
#ifdef PROJECT_NO_ANSI
            else if constexpr (std::is_same_v<CharT, char>) {
                if (ch >= 'A' && ch <= 'Z') {
                    ch = (char)(ch - 'A' + 'a');
                }
            }
#endif
            else {
                static_assert(false, __FUNCTION__ ", line " LITERAL_TO_STR(__LINE__));
            }
        }
        return str;
    }
    template<typename T>
    requires(!std::is_same_v<std::remove_cvref_t<T>, fs::path>)
    auto str2Lower(T&& str) {
        if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::wstring>
            || std::constructible_from<std::wstring, T>)
        {
            std::wstring ret(str);
            str2LowerInplace(ret);
            return ret;
        }
#ifdef PROJECT_NO_ANSI
        else if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string>
            || std::constructible_from<std::string, T>)
        {
            std::string ret(str);
            str2LowerInplace(ret);
            return ret;
        }
#endif
        else {
            static_assert(false, __FUNCTION__ ", line " LITERAL_TO_STR(__LINE__));
        }
    }
    std::wstring str2Lower(const fs::path& path) {
#ifdef _WIN32
        return str2Lower(path.native());
#else
        return str2Lower(path.wstring());
#endif
    }

    auto splitStringImpl(auto&& str, auto&& delimiter) -> decltype(auto)
    {
        std::vector<std::remove_cvref_t<decltype(str)>> result;
        for (auto&& subStrView : str | std::views::split(delimiter)) {
            result.emplace_back(subStrView.begin(), subStrView.end());
        }
        return result;
    }
    std::vector<std::string> splitString(const std::string& str, char delimiter) { return splitStringImpl(str, delimiter); }
    std::vector<std::string> splitString(const std::string& str, std::string_view delimiter) { return splitStringImpl(str, delimiter); }
    std::vector<std::string_view> splitStringView(std::string_view strv, char delimiter) { return splitStringImpl(strv, delimiter); }
    std::vector<std::string_view> splitStringView(std::string_view strv, std::string_view delimiter) { return splitStringImpl(strv, delimiter); }

    std::optional<int> str2Int(std::string_view sv);

    std::vector<std::string> splitTsvLine(const std::string& line, const std::vector<std::string>& delimiters);

    const std::string& chooseStringRef(const Sentence* sentence, CachePart tar);
    std::string chooseString(const Sentence* sentence, CachePart tar);

    CachePart chooseCachePart(std::string_view partName);

    bool isSameExtension(const fs::path& filePath, const std::wstring& ext);

    std::string removePunctuation(const std::string& sourceString);
    std::string removeWhitespace(const std::string& sourceString);

    std::pair<std::string, int> getMostCommonChar(const std::string& s);

    std::vector<std::string> splitIntoTokens(const WordPosVec& wordPosVec, const std::string& text);

    std::vector<std::string> splitIntoGraphemes(const std::string& sourceString);

    size_t countGraphemes(std::string_view sourceString);
    size_t countGraphemes(const std::string& sourceString);
    std::string truncateUtf8Prefix(std::string_view text, size_t maxCodepoints, std::string_view ellipsis = "...");
    std::string truncateUtf8Suffix(std::string_view text, size_t maxCodepoints, std::string_view ellipsis = "...");

    int countSubstring(const std::string& text, std::string_view sub);

    std::vector<double> getSubstringPositions(const std::string& text, std::string_view sub);

    std::string& replaceStrInplace(std::string& str, std::string_view org, std::string_view rep);
    std::string replaceStr(const std::string& str, std::string_view org, std::string_view rep);

    std::string extractCharactersByScripts(const std::string& sourceString, const std::vector<UScriptCode>& targetScripts);
    std::string extractKatakana(const std::string& sourceString);
    std::string extractKana(const std::string& sourceString);
    std::string extractLatin(const std::string& sourceString);
    std::string extractHangul(const std::string& sourceString);
    std::string extractCJK(const std::string& sourceString);
    std::function<std::string(const std::string&)> getTraditionalChineseExtractor(const std::shared_ptr<spdlog::logger>& logger);

    void loadTokenizeCache
    (absl::flat_hash_map<std::string, std::vector<std::vector<std::string>>>& result, const fs::path& cachePath, const std::shared_ptr<spdlog::logger>& logger);
    void saveTokenizeCache
    (const absl::flat_hash_map<std::string, std::vector<std::vector<std::string>>>& cache, const fs::path& cachePath, const std::shared_ptr<spdlog::logger>& logger);

    void extractZip(const fs::path& zipPath, const fs::path& outputDir);
    void extractFileFromZip(const fs::path& zipPath, const fs::path& outputDir, const std::string& fileName);
    void extractFilesFromZip(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& fileNames);
    void extractZipInclude(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& includePrefixes);
    void extractZipExclude(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& excludePrefixes);

    void saveJsonFile(const fs::path& path, const json& value);

    std::string nowTimestampString();

    uint64_t calculateFileCRC64(const fs::path& filePath);

    bool cmpVer(const std::string& latestVer, const std::string& currentVer, bool& isCompatible);

    PluginRunTime choosePluginRunTime(const std::string& pluginNameLower, PluginRunTime defaultTime);

    void waitForThreads(ctpl::thread_pool& pool, std::vector<std::future<void>>& results);

    bool isApiTranslationEngine(TransEngine transEngine);

    class ActiveWorkerGuard {
    public:
        explicit ActiveWorkerGuard(const std::shared_ptr<IController>& controller);
        ActiveWorkerGuard(const ActiveWorkerGuard&) = delete;
        ActiveWorkerGuard& operator=(const ActiveWorkerGuard&) = delete;
        ActiveWorkerGuard(ActiveWorkerGuard&&) = delete;
        ActiveWorkerGuard& operator=(ActiveWorkerGuard&&) = delete;
        ~ActiveWorkerGuard();

    private:
        std::shared_ptr<IController> m_controller;
    };

    template<typename T>
    T calculateAbs(T a, T b) {
        return a > b ? a - b : b - a;
    }

    namespace toml
    {
        ::toml::value uparse(const fs::path& path);
        ::toml::ordered_value uoparse(const fs::path& path);
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
            throw std::runtime_error(gppTr("parseToml", "无效的 TOML 路径: %1").arg(path).toStdString());
        }
        if (auto pValue = findValueByPath(config, keys)) {
            return toml::get<T>(*pValue);
        }
        if (auto pValue = findValueByPath(backup, keys)) {
            return toml::get<T>(*pValue);
        }
        throw std::runtime_error(gppTr("parseToml", "无法在 TOML 中找到值: %1").arg(path).toStdString());
    }

    template<typename T, typename TC>
    auto parseToml(const toml::basic_value<TC>& config, const std::vector<std::string>& keys) -> std::optional<T> {
        if (
            keys.empty() ||
            std::ranges::any_of(keys, [](const std::string& key) { return key.empty(); })
            ) {
            return std::nullopt;
        }
        if (auto pValue = findValueByPath(config, keys)) {
            return toml::get<T>(*pValue);
        }
        return std::nullopt;
    }

    template<typename T, typename TC>
    auto parseToml(const toml::basic_value<TC>& config, const std::string& path) -> std::optional<T> {
        const std::vector<std::string> keys = splitString(path, '.');
        return parseToml<T>(config, keys);
    }

    template<typename TC>
    size_t eraseToml(toml::basic_value<TC>& table, const std::vector<std::string>& keys) {
        if (
            keys.empty() ||
            std::ranges::any_of(keys, [](const std::string& key) { return key.empty(); })
            ) {
            return 0;
        }
        auto* pCurrentTable = &table.as_table();
        for (size_t i = 0; i < keys.size() - 1; i++) {
            auto& subTable = (*pCurrentTable)[keys[i]];
            if (!subTable.is_table()) {
                return 0;
            }
            pCurrentTable = &subTable.as_table();
        }
        return (*pCurrentTable).erase(keys.back());
    }

    template<typename TC>
    size_t eraseToml(toml::basic_value<TC>& table, const std::string& path) {
        const std::vector<std::string> keys = splitString(path, '.');
        return eraseToml(table, keys);
    }

    template<typename TC, typename T>
    toml::basic_value<TC>& insertToml(toml::basic_value<TC>& table, const std::vector<std::string>& keys, const T& value)
    {
        if (
            keys.empty() ||
            std::ranges::any_of(keys, [](const std::string& key) { return key.empty(); })
            )
        {
            return table;
        }
        auto* pCurrentTable = &table.as_table();
        for (size_t i = 0; i < keys.size() - 1; i++) {
            auto& subTable = (*pCurrentTable)[keys[i]];
            if (!subTable.is_table()) {
                subTable = toml::table{};
            }
            pCurrentTable = &subTable.as_table();
        }
        auto& lastVal = (*pCurrentTable)[keys.back()];
        const auto orgComments = lastVal.comments();
        if constexpr (std::is_same_v<toml::basic_value<TC>, toml::ordered_value>) {
            lastVal = toml::ordered_value{ value };
        }
        else {
            lastVal = toml::value{ value };
        }
        lastVal.comments() = orgComments;
        return table;
    }

    template<typename TC,typename T>
    toml::basic_value<TC>& insertToml(toml::basic_value<TC>& table, const std::string& path, const T& value)
    {
        const std::vector<std::string> keys = splitString(path, '.');
        return insertToml(table, keys, value);
    }
}
