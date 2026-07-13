module;

#include "GPPMacros.hpp"
#ifdef _WIN32
#include <Windows.h>
#endif
#include <ctpl_stl.h>
#include <toml.hpp>

export module Tool;

export import GPPDefines;
export import ITranslator;

namespace fs = std::filesystem;

export
{
#if __cpp_lib_string_view < 202403L
    template<class CharT, class Traits, class Alloc>
    [[nodiscard]] std::basic_string<CharT, Traits, Alloc>
        operator+(const std::basic_string<CharT, Traits, Alloc>& lhs,
            std::basic_string_view<CharT, Traits> rhs) {
        std::basic_string<CharT, Traits, Alloc> r(lhs.get_allocator());
        r.reserve(lhs.size() + rhs.size());
        return r.append(lhs).append(rhs);
    }

    template<class CharT, class Traits, class Alloc>
    [[nodiscard]] std::basic_string<CharT, Traits, Alloc>
        operator+(std::basic_string_view<CharT, Traits> lhs,
            const std::basic_string<CharT, Traits, Alloc>& rhs) {
        std::basic_string<CharT, Traits, Alloc> r(rhs.get_allocator());
        r.reserve(lhs.size() + rhs.size());
        return r.append(lhs).append(rhs);
    }

    template<class CharT, class Traits>
    [[nodiscard]] std::basic_string<CharT, Traits>
        operator+(std::basic_string_view<CharT, Traits> lhs,
            std::basic_string_view<CharT, Traits> rhs) {
        std::basic_string<CharT, Traits> r;
        r.reserve(lhs.size() + rhs.size());
        return r.append(lhs).append(rhs);
    }
#endif

    std::string wide2Ascii(std::wstring_view wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr);
    template<typename T>
    requires(std::is_same_v<std::remove_cvref_t<T>, fs::path>)
    std::string wide2Ascii(T&& path, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr) {
#ifdef _WIN32
        return wide2Ascii(path.native(), codePage, usedDefaultChar);
#else
        return wide2Ascii(path.wstring(), codePage, usedDefaultChar);
#endif
    }
    std::wstring ascii2Wide(std::string_view ascii, UINT codePage = CP_UTF8);

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

    std::optional<int> str2Int(std::string_view str);

    std::vector<std::string_view> splitStringView(std::string_view str, char delimiter);
    std::vector<std::string_view> splitStringView(std::string_view str, std::string_view delimiter);
    std::vector<std::string> splitString(std::string_view str, char delimiter);
    std::vector<std::string> splitString(std::string_view str, std::string_view delimiter);

    std::vector<std::string_view> splitTsvLineView(std::string_view line, std::span<const std::string_view> delimiters);
    std::vector<std::string> splitTsvLine(std::string_view line, std::span<const std::string_view> delimiters);
    std::vector<std::string_view> splitIntoGraphemeViews(std::string_view str);
    std::vector<std::string> splitIntoGraphemes(std::string_view str);

    size_t countGraphemes(std::string_view str);

    int countSubstring(std::string_view str, std::string_view sub);
    std::vector<double> getSubstringPositions(std::string_view str, std::string_view sub);
    std::pair<std::string_view, int> getMostCommonCharView(std::string_view str);
    std::pair<std::string, int> getMostCommonChar(std::string_view str);

    std::string& replaceStrInplace(std::string& str, std::string_view org, std::string_view rep);
    std::string replaceStr(const std::string& str, std::string_view org, std::string_view rep);

    // 轻量修复模型返回 JSON 文本中常见的未转义引号。
    std::string lightRepairJsonText(std::string_view jsonStr);

    bool hasPunctuation(std::string_view str);
    bool hasWhitespace(std::string_view str);
    std::string removePunctuation(std::string_view str);
    std::string removeWhitespace(std::string_view str);

    std::string_view truncateUtf8PrefixView(std::string_view str, size_t maxCodepoints);
    std::string truncateUtf8Prefix(std::string_view str, size_t maxCodepoints, std::string_view ellipsis = "...");
    std::string_view truncateUtf8SuffixView(std::string_view str, size_t maxCodepoints);
    std::string truncateUtf8Suffix(std::string_view str, size_t maxCodepoints, std::string_view ellipsis = "...");
    std::string maskApiKey(std::string_view apiKey);

    bool hasKatakana(std::string_view str);
    bool hasKana(std::string_view str);
    bool hasLatin(std::string_view str);
    bool hasHangul(std::string_view str);
    bool hasCJK(std::string_view str);
    std::string extractKatakana(std::string_view str);
    std::string extractKana(std::string_view str);
    std::string extractLatin(std::string_view str);
    std::string extractHangul(std::string_view str);
    std::string extractCJK(std::string_view str);
    // 不保证线程安全，但对插件来说不影响
    // 因为 m_lua/py 的 text 插件应当是同步执行的
    std::function<std::string(const std::string&)> getTraditionalChineseExtractor();


    std::string getNameString(const Sentence& se);
    std::string getNameString(const json& j);

    const std::string& chooseStringRef(Sentence* sentence, CachePart target);
    std::string chooseString(Sentence* sentence, CachePart target);
    CachePart chooseCachePart(std::string_view partName);

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

    bool isApiTranslationEngine(TransEngine transEngine);
    PluginRunTime choosePluginRunTime(std::string_view pluginNameLower, PluginRunTime defaultTime);
    void waitForThreads(ctpl::thread_pool& pool, std::vector<std::future<void>>& results);


    bool executeCommand(const std::wstring& program, const std::wstring& args, bool showWindow = true, int timeDelayAfterCommand = 5);
    int getConsoleWidth();

    bool createParent(const fs::path& path);
    bool isSameExtension(const fs::path& filePath, std::wstring_view ext);

    void extractZip(const fs::path& zipPath, const fs::path& outputDir);
    void extractFileFromZip(const fs::path& zipPath, const fs::path& outputDir, std::string_view fileName);
    void extractFilesFromZip(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& fileNames);
    void extractZipInclude(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& includePrefixes);
    void extractZipExclude(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& excludePrefixes);

    namespace toml
    {
        ::toml::value uparse(const fs::path& path);
        ::toml::ordered_value uoparse(const fs::path& path);
    }

    json parseJson(const fs::path& path, std::ifstream& ifs);
    json parseJson(const fs::path& path);
    ordered_json parseOrderedJson(const fs::path& path, std::ifstream& ifs);
    ordered_json parseOrderedJson(const fs::path& path);

    void loadTokenizeCache
    (absl::flat_hash_map<std::string, WordPosVec>& result, const fs::path& cachePath, const std::shared_ptr<spdlog::logger>& logger);
    void saveTokenizeCache
    (const absl::flat_hash_map<std::string, WordPosVec>& cache, const fs::path& cachePath, const std::shared_ptr<spdlog::logger>& logger);


    std::string currentTimestampString();
    uint64_t calculateFileCRC64(const fs::path& filePath);
    int compareVersion(std::string_view latestVer, std::string_view currentVer);

    template<typename DataType>
    void atomicOutputFile(std::ofstream& ofs, const fs::path& path, DataType&& data) {
        const fs::path tempPath = path.wstring() + L".__GPP_TEMP__";
        createParent(tempPath);
        ofs.open(tempPath, std::ios::binary);
        ofs.write((char*)data.data(), data.size());
        ofs.close();
        fs::rename(tempPath, path);
    }

    template<typename DataType>
    void atomicOutputFile(const fs::path& path, DataType&& data) {
        std::ofstream ofs;
        atomicOutputFile(ofs, path, data);
    }

    template<typename T>
    concept AbsCalculable = requires(T a, T b) {
        { a > b } -> std::convertible_to<bool>;
        { a - b } -> std::convertible_to<T>;
    };
    template<AbsCalculable T>
    T calculateAbs(T a, T b) {
        return a > b ? a - b : b - a;
    }

    // 辅助函数：在一个 TOML 表中，根据一个由键组成的路径向量来查找值
    template<typename TC>
    const toml::basic_value<TC>* findValueByPath(const toml::basic_value<TC>& table, std::span<const std::string_view> keyViews) {
        const toml::basic_value<TC>* pCurrentValue = &table;
        for (const auto& keyView : keyViews) {
            // 确保当前值是一个表 (table) 或有序表 (ordered_value)
            if (!pCurrentValue->is_table()) {
                return nullptr; // 路径中间部分不是一个表，无法继续查找
            }
            const std::string key(keyView);
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
    auto parseToml(const toml::basic_value<TC>& config, const toml::basic_value<TC2>& backup, std::string_view path,
        bool reversePriority = false) -> decltype(auto)
    {
        const std::vector<std::string_view> keyViews = splitStringView(path, '.');
        if (
            keyViews.empty() ||
            std::ranges::any_of(keyViews, [](const auto& keyView) { return keyView.empty(); })
            )
        {
            throw std::runtime_error(gppTr("parseToml", "无效的 TOML 路径: %1").arg(path).toStdString());
        }
        if (!reversePriority) {
            if (auto pValue = findValueByPath(config, keyViews)) {
                return toml::get<T>(*pValue);
            }
            if (auto pValue = findValueByPath(backup, keyViews)) {
                return toml::get<T>(*pValue);
            }
        }
        else {
            if (auto pValue = findValueByPath(backup, keyViews)) {
                return toml::get<T>(*pValue);
            }
            if (auto pValue = findValueByPath(config, keyViews)) {
                return toml::get<T>(*pValue);
            }
        }
        throw std::runtime_error(gppTr("parseToml", "无法在 TOML 中找到值: %1").arg(path).toStdString());
    }

    template<typename TC, typename T>
    toml::basic_value<TC>& insertToml(toml::basic_value<TC>& table, std::span<const std::string_view> keyViews, const T& value)
    {
        if (
            keyViews.empty() ||
            std::ranges::any_of(keyViews, [](const auto& keyView) { return keyView.empty(); }) ||
            !table.is_table()
            )
        {
            return table;
        }
        auto* pCurrentTable = &table.as_table();
        for (size_t i = 0; i < keyViews.size() - 1; i++) {
            auto& subValue = (*pCurrentTable)[std::string(keyViews[i])];
            if (!subValue.is_table()) {
                subValue = toml::table{};
            }
            pCurrentTable = &subValue.as_table();
        }
        auto& lastVal = (*pCurrentTable)[std::string(keyViews.back())];
        const auto orgComments = lastVal.comments();
        lastVal = toml::basic_value<TC>{ value };
        lastVal.comments() = orgComments;
        return table;
    }

    template<typename TC,typename T>
    toml::basic_value<TC>& insertToml(toml::basic_value<TC>& table, std::string_view path, const T& value)
    {
        const std::vector<std::string_view> keyViews = splitStringView(path, '.');
        return insertToml(table, keyViews, value);
    }


    template<typename TC>
    auto toml2Json(const toml::basic_value<TC>& value)
        -> std::conditional_t<std::is_same_v<TC, toml::ordered_type_config>, ordered_json, json>
    {
        using RetJsonType = std::conditional_t<std::is_same_v<TC, toml::ordered_type_config>, ordered_json, json>;
        if (value.is_table()) {
            RetJsonType resultMap = RetJsonType::object();
            for (const auto& [key, val] : value.as_table()) {
                resultMap[key] = toml2Json(val);
            }
            return resultMap;
        }
        else if (value.is_array()) {
            RetJsonType resultVec = RetJsonType::array();
            for (const auto& elem : value.as_array()) {
                resultVec.push_back(toml2Json(elem));
            }
            return resultVec;
        }
        else if (value.is_boolean()) {
            return value.as_boolean();
        }
        else if (value.is_integer()) {
            return value.as_integer();
        }
        else if (value.is_floating()) {
            return value.as_floating();
        }
        else if (value.is_string()) {
            return value.as_string();
        }
        throw std::runtime_error(gppTr("toml2Json", "不支持的 TOML 数据类型: %1")
            .arg(toml::to_string(value.type()))
            .toStdString());
    }

    template<typename JsonType>
    auto json2Toml(const JsonType& value)
        -> std::conditional_t<std::is_same_v<JsonType, ordered_json>, toml::ordered_value, toml::value>
    {
        using RetTomlType = std::conditional_t<std::is_same_v<JsonType, ordered_json>, toml::ordered_value, toml::value>;
        if (value.is_object()) {
            typename RetTomlType::table_type table;
            for (auto it = value.cbegin(); it != value.cend(); ++it) {
                if (!it.value().is_null()) {
                    table.insert({ it.key(), json2Toml(it.value()) });
                }
            }
            return RetTomlType{ table };
        }
        else if (value.is_array()) {
            typename RetTomlType::array_type array;
            for (const auto& child : value) {
                if (!child.is_null()) {
                    array.push_back(json2Toml(child));
                }
            }
            return RetTomlType{ array };
        }
        else if (value.is_boolean()) {
            return value.template get<bool>();
        }
        else if (value.is_number_integer()) {
            return value.template get<int64_t>();
        }
        else if (value.is_number_float()) {
            return value.template get<double>();
        }
        else if (value.is_string()) {
            return value.template get<std::string>();
        }
        throw std::runtime_error(gppTr("json2Toml", "不支持的 JSON 数据类型: %1")
            .arg(value.type_name())
            .toStdString());
    }
}
