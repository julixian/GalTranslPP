module;

#include "GPPMacros.hpp"

export module ProjectConfig;

export import ApiTool;

export NAMESPACE_BEGIN(gpp)

// 预览和翻译共用解析结果，默认值只在 readTranslationSettings 中维护。
struct TranslationSettings {
    int batchSize;
    int threadsNum;
    int nameTransBatchSize;
    std::string sortMethod;
    std::string targetLang;
    std::string splitFileMethod;
    std::string problemOverviewFormat;
    int splitFileNum;
    bool reuseRepeatedBlocks;
    int repeatedBlockMinSize;
    int cacheSearchDistance;
    int saveCacheInterval;
    bool keepPreviousOtherInfo;
    std::string linebreakSymbol;
    int maxRequestCount;
    int inputBlockMaxLines;
    int problemMaxLines;
    int glossaryMaxLines;
    bool smartRetry;
    int enhanceJailbreak;
    bool checkQuota;
    bool retransAllWhenFail;
    bool agentEnabled;
    bool agentAllowPython;
    int agentMaxTurnsPerChunk;
    int agentCompactContextThresholdBytes;
    int agentSearchResultLimit;
    int agentContextLinesLimit;
    std::string projectNotePath;
};

struct ProjectApi {
    std::string name;
    size_t index;
    std::vector<TranslationApi> tokens;
};

struct ProjectDictionaryFile {
    std::string name;
    std::filesystem::path path;
    bool exists;
};

struct ProjectPrompt {
    std::string key;
    std::filesystem::path path;
    std::string text;
};

struct ProjectPrompts {
    ProjectPrompt system;
    ProjectPrompt user;
};

TranslationSettings readTranslationSettings(const toml::value& config);
void validateTranslationSettings(const TranslationSettings& config, bool agentEnabled);
bool supportsTransAgent(TransEngine engine);
bool usesTranslationApi(TransEngine engine);
std::vector<ProjectApi> readProjectApis(const toml::value& config, TransEngine engine, bool agentEnabled,
    const std::function<void(const std::string&)>& warningFunc);
std::vector<ProjectDictionaryFile> resolveProjectDictionaries(const std::filesystem::path& projectDir,
    const toml::value& config, const std::string& type);
ProjectPrompts readProjectPrompts(const std::filesystem::path& projectDir,
    TransEngine engine, bool agentEnabled);

NAMESPACE_END(gpp)
