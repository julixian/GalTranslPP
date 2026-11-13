module;

#include "GPPMacros.hpp"
#ifdef _WIN32
#include <Windows.h>
#endif

export module Tool;

export import std;
export import ctpl_stl;
export import GPPDefines;
export import ITranslator;

export NAMESPACE_BEGIN(gpp)

namespace fs = std::filesystem;

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
#endif

// 数字段按数值比较，相同数值中前导零多的在前；忽略 ASCII 大小写，其它字符按编码单元比较。
int compareNatural(std::string_view lhs, std::string_view rhs) noexcept;
int compareNatural(std::wstring_view lhs, std::wstring_view rhs) noexcept;

#ifdef _WIN32
std::string wide2Ascii(std::wstring_view wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr);
template<typename T>
    requires(std::is_same_v<std::remove_cvref_t<T>, fs::path>)
std::string wide2Ascii(T&& path, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr) {
    return wide2Ascii(path.native(), codePage, usedDefaultChar);
}
std::wstring ascii2Wide(std::string_view ascii, UINT codePage = CP_UTF8);
#elif defined(__APPLE__)
std::string wide2Ascii(std::wstring_view wide);
template<typename T>
    requires(std::is_same_v<std::remove_cvref_t<T>, fs::path>)
std::string wide2Ascii(T&& path) {
    return path.native();
}
std::wstring ascii2Wide(std::string_view ascii);
#endif

template <typename CharT, typename Traits, typename Alloc>
auto& str2LowerInplace(std::basic_string<CharT, Traits, Alloc>& str) {
    for (CharT& ch : str) {
        if constexpr (std::is_same_v<CharT, wchar_t>) {
            if (ch >= L'A' && ch <= L'Z') {
                ch = (wchar_t)(ch - L'A' + L'a');
            }
        }
#if (PROJECT_NO_ANSI)
        else if constexpr (std::is_same_v<CharT, char>) {
            if (ch >= 'A' && ch <= 'Z') {
                ch = (char)(ch - 'A' + 'a');
            }
        }
#endif
        else {
            static_assert(false, "GPP_INVALID_CODE_PATH");
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
#if (PROJECT_NO_ANSI)
    else if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string>
        || std::constructible_from<std::string, T>)
    {
        std::string ret(str);
        str2LowerInplace(ret);
        return ret;
    }
#endif
    else {
        static_assert(false, "GPP_INVALID_CODE_PATH");
    }
}
std::wstring str2Lower(const fs::path& path) {
#ifdef _WIN32
    return str2Lower(path.native());
#elif defined(__APPLE__)
    return str2Lower(ascii2Wide(path.native()));
#endif
}

// 按文件通配符匹配路径；spec 使用平台原生字符类型。
bool matchFileSpec(const fs::path::string_type& path, const fs::path::string_type& spec);

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
bool isAllPunctuation(std::string_view str);
bool isAllWhitespace(std::string_view str);
std::string removePunctuation(std::string_view str);
std::string removeWhitespace(std::string_view str);

std::string_view truncateUtf8PrefixView(std::string_view str, size_t maxCodepoints);
std::string truncateUtf8Prefix(std::string_view str, size_t maxCodepoints, std::string_view ellipsis = "...");
std::string_view truncateUtf8SuffixView(std::string_view str, size_t maxCodepoints);
std::string truncateUtf8Suffix(std::string_view str, size_t maxCodepoints, std::string_view ellipsis = "...");
std::string maskApikey(std::string_view apikey);

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
std::function<std::string(std::string_view)> getTraditionalChineseExtractor();


std::string getNameString(const Sentence& se);
std::string getNameString(const json& item);

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

PluginRunTime choosePluginRunTime(std::string_view pluginNameLower, PluginRunTime defaultTime);
void waitForTransThreads(const std::shared_ptr<IController>& controller, ctpl::thread_pool& pool, std::vector<std::future<void>>& results);


void initializeProcessJob();
bool executeCommand(const std::wstring& program, const std::wstring& args, bool showWindow = true, int timeDelayAfterCommand = 5);
int getConsoleWidth();

bool createParent(const fs::path& path);
bool isSameExtension(const fs::path& filePath, std::wstring_view ext);

void extractZip(const fs::path& zipPath, const fs::path& outputDir);
void extractFileFromZip(const fs::path& zipPath, const fs::path& outputDir, std::string_view fileName);
void extractFilesFromZip(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& fileNames);
void extractZipInclude(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& includePrefixes);
void extractZipExclude(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& excludePrefixes);

toml parseToml(const fs::path& path);

json parseJson(const fs::path& path, std::ifstream& ifs);
json parseJson(const fs::path& path);
ordered_json parseOrderedJson(const fs::path& path, std::ifstream& ifs);
ordered_json parseOrderedJson(const fs::path& path);

void loadTokenizeCache
(absl::flat_hash_map<std::string, WordPosVec>& result, const fs::path& cachePath, const std::shared_ptr<spdlog::logger>& logger);
void saveTokenizeCache
(const absl::flat_hash_map<std::string, WordPosVec>& cache, const fs::path& cachePath, const std::shared_ptr<spdlog::logger>& logger);


std::string currentTimestampString();
std::uint64_t calculateFileCRC64(const fs::path& filePath);
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
const toml* findValueByPath(const toml& table, std::span<const std::string_view> keyViews);

template<typename T>
decltype(auto) parsePluginToml(const toml& config, const toml& backup, std::string_view path,
    bool reversePriority = false)
{
    const std::vector<std::string_view> keyViews = splitStringView(path, '.');
    if (
        keyViews.empty() ||
        std::ranges::any_of(keyViews, [](const std::string_view keyView) { return keyView.empty(); })
        )
    {
        throw std::runtime_error(gppTr("parsePluginToml", "无效的 TOML 路径: %1").arg(path).toStdString());
    }
    auto extractVal = [](const toml& item) -> decltype(auto)
        {
            if constexpr (std::is_same_v<std::remove_cvref_t<T>, toml>) {
                return (const toml&)item;
            }
            else {
                return item.get<T>();
            }
        };
    if (!reversePriority) {
        if (auto pValue = findValueByPath(config, keyViews)) {
            return extractVal(*pValue);
        }
        if (auto pValue = findValueByPath(backup, keyViews)) {
            return extractVal(*pValue);
        }
    }
    else {
        if (auto pValue = findValueByPath(backup, keyViews)) {
            return extractVal(*pValue);
        }
        if (auto pValue = findValueByPath(config, keyViews)) {
            return extractVal(*pValue);
        }
    }
    throw std::runtime_error(gppTr("parsePluginToml", "无法在 TOML 中找到值: %1").arg(path).toStdString());
}

template<typename T>
toml& insertToml(toml& table, std::span<const std::string_view> keyViews, const T& value)
{
    if (
        keyViews.empty() ||
        std::ranges::any_of(keyViews, [](const std::string_view keyView) { return keyView.empty(); }) ||
        !table.isTable()
        )
    {
        return table;
    }
    toml* pCurrentTable = &table;
    for (size_t i = 0; i < keyViews.size() - 1; i++) {
        toml& subValue = (*pCurrentTable)[keyViews[i]];
        if (!subValue.isTable()) {
            subValue = toml::table();
        }
        pCurrentTable = &subValue;
    }
    // toml::operator= 自动保留原有前置注释、行尾注释及键空白格式
    (*pCurrentTable)[keyViews.back()] = value;
    return table;
}

template<typename T>
toml& insertToml(toml& table, std::string_view path, const T& value)
{
    const std::vector<std::string_view> keyViews = splitStringView(path, '.');
    return insertToml(table, keyViews, value);
}


template<typename RetJsonType = json>
RetJsonType toml2Json(const toml& value)
{
    if (value.isTable()) {
        RetJsonType resultMap = RetJsonType::object();
        for (const auto [key, val] : value.tableItems()) {
            resultMap[key.name()] = toml2Json<RetJsonType>(val);
        }
        return resultMap;
    }
    else if (value.isArray()) {
        RetJsonType resultVec = RetJsonType::array();
        for (const auto& elem : value) {
            resultVec.push_back(toml2Json<RetJsonType>(elem));
        }
        return resultVec;
    }
    else if (value.isBoolean()) {
        return value.asBoolean();
    }
    else if (value.isInteger()) {
        return value.asInteger();
    }
    else if (value.isFloat()) {
        return value.asFloat();
    }
    else if (value.isString()) {
        return value.asString();
    }
    throw std::runtime_error(gppTr("toml2Json", "不支持的 TOML 数据类型: %1")
        .arg(value.kindName())
        .toStdString());
}

ordered_json toml2OrderedJson(const toml& value);

template<typename JsonType>
toml json2Toml(const JsonType& value, tomlkit::TableStyle objectTableStyle = tomlkit::TableStyle::Inline)
{
    if (value.is_object()) {
        toml table = (objectTableStyle == tomlkit::TableStyle::Inline)
            ? toml::inlineTable()
            : toml::table(objectTableStyle);
        for (auto it = value.cbegin(); it != value.cend(); ++it) {
            if (!it.value().is_null()) {
                table[it.key()] = json2Toml(it.value(), tomlkit::TableStyle::Inline);
            }
        }
        return table;
    }
    else if (value.is_array()) {
        toml array = toml::array();
        for (const auto& child : value) {
            if (!child.is_null()) {
                array.push_back(json2Toml(child, tomlkit::TableStyle::Inline));
            }
        }
        return array;
    }
    else if (value.is_boolean()) {
        return toml(value.template get<bool>());
    }
    else if (value.is_number_integer()) {
        return toml(value.template get<std::int64_t>());
    }
    else if (value.is_number_unsigned()) {
        return toml(value.template get<std::uint64_t>());
    }
    else if (value.is_number_float()) {
        return toml(value.template get<double>());
    }
    else if (value.is_string()) {
        return toml(value.template get<std::string>());
    }
    throw std::runtime_error(gppTr("json2Toml", "不支持的 JSON 数据类型: %1")
        .arg(value.type_name())
        .toStdString());
}

NAMESPACE_END(gpp)
