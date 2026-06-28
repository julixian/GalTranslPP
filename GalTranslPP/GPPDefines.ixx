export module GPPDefines;

export import std.compat;
export import AbslContainers;
export import jpcre2;
export import nlohmann.json;
export import spdlog;

namespace fs = std::filesystem;

export
{
    extern const std::string GPPVERSION;
    extern const std::string PYTHONVERSION;
    extern const std::string PROMPTVERSION;
    extern const std::string DICTVERSION;
    extern const std::string QTVERSION;
    extern const std::string ICUVERSION;

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
        std::string name_preview;
        std::vector<std::string> names_preview;
        std::string original_text;
        std::string pre_processed_text;
        std::string pre_translated_text;
        std::vector<std::string> problems;
        std::string translated_by;
        std::string translated_preview;
        std::string originalLinebreak;
        std::map<std::string, std::string> other_info;
        std::optional<SentencePosition> repeatedBlockRefTo;
        std::vector<SentencePosition> repeatedBlockRefBy;

        NameType nameType = NameType::None;
        Sentence* prev = nullptr;
        Sentence* next = nullptr;
        bool complete = false;
        bool notAnalyzeProblem = false;
        bool repeatedBlockRefPending = false;

        std::optional<std::string> problems_get_by_index(int index) {
            if (index < 0 || index >= problems.size()) {
                return std::nullopt;
            }
            return problems[index];
        }

        bool problems_set_by_index(int index, const std::string& problem) {
            if (index < 0 || index >= problems.size()) {
                return false;
            }
            problems[index] = problem;
            return true;
        }
    };

    enum class TransEngine
    {
        None, ForGalJson, ForGalTsv, ForNovelTsv, DeepseekJson, Sakura, DumpName, NameTrans, GenDict, Rebuild, ShowNormal
    };

    enum class CachePart 
    { 
        None, Name, NamePreview, Names, NamesPreview, OrigText, PreprocText, PretransText, Problems, OtherInfo, TranslatedBy, TransPreview 
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

    using WordPosVec = std::vector<std::vector<std::string>>;
    using EntityVec = std::vector<std::vector<std::string>>;
    using NLPResult = std::tuple<WordPosVec, EntityVec>;

    using CheckSeCondFunc = std::function<bool(const Sentence*)>;

    using DictList = std::vector<std::tuple<std::string, std::string, std::string>>;

    using json = nlohmann::json;
    using ordered_json = nlohmann::ordered_json;

}
