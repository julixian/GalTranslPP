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

namespace fs = std::filesystem;

export {

#ifdef _WIN32
    std::string wide2Ascii(const std::wstring& wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr);
    std::string wide2Ascii(std::wstring_view wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr);
    inline std::string wide2Ascii(const wchar_t* wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr) {
        return wide2Ascii(std::wstring_view(wide), codePage, usedDefaultChar);
    }
    template<typename T>
    requires(std::is_same_v<std::remove_cvref_t<T>, fs::path>)
    inline std::string wide2Ascii(T&& path, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr) {
        return wide2Ascii(path.native(), codePage, usedDefaultChar);
    }

    std::wstring ascii2Wide(const std::string& ascii, UINT codePage = CP_UTF8);
    std::wstring ascii2Wide(std::string_view ascii, UINT codePage = CP_UTF8);

    std::string ascii2Ascii(const std::string& ascii, UINT src = CP_UTF8, UINT dst = CP_ACP, LPBOOL usedDefaultChar = nullptr);
    std::string ascii2Ascii(std::string_view ascii, UINT src = CP_UTF8, UINT dst = CP_ACP, LPBOOL usedDefaultChar = nullptr);

    bool executeCommand(const std::wstring& program, const std::wstring& args, bool showWindow = true, int timeDelayAfterCommand = 5);

    int getConsoleWidth();
#endif


    std::string getNameString(const Sentence* se);
    std::string getNameString(const json& j);

    bool createParent(const fs::path& path);

    template<typename RetT>
    RetT str2LowerImpl(auto&& str) {
        return str | std::views::transform([](const auto c) { return std::tolower(c); }) | std::ranges::to<RetT>();
    }
    template<typename T>
        requires(!std::is_same_v<std::remove_cvref_t<T>, fs::path>)
    inline auto str2Lower(T&& str) {
        if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::wstring_view>) {
            return str2LowerImpl<std::wstring>(str);
        }
        if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string_view>) {
            return str2LowerImpl<std::string>(str);
        }
        if constexpr (std::constructible_from<std::wstring_view, T>) {
            return str2LowerImpl<std::wstring>(std::wstring_view(str));
        }
        if constexpr (std::constructible_from<std::string_view, T>) {
            return str2LowerImpl<std::string>(std::string_view(str));
        }
    }
    std::wstring str2Lower(const fs::path& path) {
#ifdef _WIN32
        return str2Lower(path.native());
#else
        return str2Lower(path.wstring());
#endif
    }
    template <typename CharT, typename Traits, typename Alloc>
    auto& str2LowerInplace(std::basic_string<CharT, Traits, Alloc>& str) {
        std::transform(str.begin(), str.end(), str.begin(), [](const auto c) { return std::tolower(c); });
        return str;
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
    std::move_only_function<std::string(const std::string&)> getTraditionalChineseExtractor(const std::shared_ptr<spdlog::logger>& logger);

    void loadTokenizeCache
    (absl::flat_hash_map<std::string, std::vector<std::vector<std::string>>>& result, const fs::path& cachePath, const std::shared_ptr<spdlog::logger>& logger);
    void saveTokenizeCache
    (const absl::flat_hash_map<std::string, std::vector<std::vector<std::string>>>& cache, const fs::path& cachePath, const std::shared_ptr<spdlog::logger>& logger);

    void extractZip(const fs::path& zipPath, const fs::path& outputDir);
    void extractFileFromZip(const fs::path& zipPath, const fs::path& outputDir, const std::string& fileName);
    void extractZipExclude(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& excludePrefixes);

    void saveJsonFile(const fs::path& path, const json& value);

    std::string nowTimestampString();

    uint64_t calculateFileCRC64(const fs::path& filePath);

    bool cmpVer(const std::string& latestVer, const std::string& currentVer, bool& isCompatible);

    PluginRunTime choosePluginRunTime(const std::string& pluginNameLower, PluginRunTime defaultTime);

    void waitForThreads(ctpl::thread_pool& pool, std::vector<std::future<void>>& results);

    template<typename T>
    T calculateAbs(T a, T b) {
        return a > b ? a - b : b - a;
    }

    namespace toml {
        toml::value uparse(const fs::path& path);
        toml::ordered_value uoparse(const fs::path& path);
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
