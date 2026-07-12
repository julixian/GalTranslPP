export module GPPDefines;

export import std.compat;
export import AbslContainers;
export import jpcre2;
export import nlohmann.json;
export import spdlog;
export import GPPI18n;

namespace fs = std::filesystem;

export
{
    extern const fs::path baseConfigPath;
    extern const fs::path globalConfigPath;
    extern const fs::path defaultPromptPath;
    extern const fs::path defaultDictPath;
    extern const fs::path defaultGptDictPath;
    extern const fs::path defaultPreDictPath;
    extern const fs::path defaultPostDictPath;
    extern const fs::path pluginConfigsPath;
    extern const fs::path filePluginConfigPath;
    extern const fs::path textPluginConfigPath;
    extern const std::wstring transCacheDirName;
    extern const std::wstring otherCacheDirName;

    extern const std::string defaultRegCompileModifier;
    extern const std::string defaultRegReplaceModifier;

    using json = nlohmann::json;
    using ordered_json = nlohmann::ordered_json;

    enum class NameType
    {
        None, Single, Multiple
    };

    struct SentencePosition {
        std::string file;
        int index = -1;

        friend bool operator==(const SentencePosition&, const SentencePosition&) = default;
    };

    template <typename H>
    H AbslHashValue(H h, const SentencePosition& position) {
        return H::combine(std::move(h), position.file, position.index);
    }

    struct Sentence {
        int index = -1;
        std::string fileName;

        std::string name;
        std::vector<std::string> names;
        std::string nametrans;
        std::vector<std::string> namestrans;
        std::string orig;
        std::string preproc;
        std::vector<std::string> problems;
        std::string transby;
        std::string transraw;
        std::string transview;
        std::string linebreak;
        std::map<std::string, std::string> otherinfo;

        std::optional<SentencePosition> ref;
        std::vector<SentencePosition> refBy;

        NameType nameType = NameType::None;
        Sentence* prev = nullptr;
        Sentence* next = nullptr;
        bool transCompleted = false;
        bool problemAnalyzeDisabled = false;
        bool isRefPending = false;

        std::optional<std::string> getProblemByIndex(int index) {
            if (index < 0 || index >= problems.size()) {
                return std::nullopt;
            }
            return problems[index];
        }

        bool setProblemByIndex(int index, const std::string& problem) {
            if (index < 0 || index >= problems.size()) {
                return false;
            }
            problems[index] = problem;
            return true;
        }
    };

    enum class TransEngine
    {
        None, ForGalTsv, ForNovelTsv, ForGalJson, Sakura, DumpName, NameTrans, GenDict, Rebuild, ShowNormal
    };

    enum class CachePart
    { 
        None, Name, Names, NameTrans, NamesTrans, Orig, Preproc, Problems, OtherInfo, TransBy, TransRaw, Transview
    };

    enum class ConditionType
    {
        None, Gpp, Lua, Python
    };

    enum class PluginRunTime
    {
        None, DPre, Pre, Post, DPost
    };

    std::map<PluginRunTime, std::string> pluginRunTimeNames =
    {
        { PluginRunTime::None, "none" },
        { PluginRunTime::DPre, "dprerun" },
        { PluginRunTime::Pre, "prerun" },
        { PluginRunTime::Post, "postrun" },
        { PluginRunTime::DPost, "dpostrun" }
    };

    using NLPPair = std::array<std::string, 2>;
    using WordPosVec = std::vector<NLPPair>;
    using EntityVec = std::vector<NLPPair>;
    using NLPResult = std::tuple<WordPosVec, EntityVec>;

    template<typename ...Args>
    using CheckSeCondBaseFunc = std::function<bool(const Sentence* se, Args...)>;
    using CheckSeCondNormalFunc = CheckSeCondBaseFunc<>;
    using CheckSkipProblemCondFunc = CheckSeCondBaseFunc<const std::string&>;
    // first: 要忽略的问题正则表达式，second: 对应的忽略条件
    using SkipProblemCondition = std::pair<jpc::Regex, std::optional<CheckSkipProblemCondFunc>>;

    using DictList = std::vector<std::tuple<std::string, std::string, std::string>>;

}
