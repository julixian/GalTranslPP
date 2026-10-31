module;

#include "GPPMacros.hpp"

module ProjectConfig;

NAMESPACE_BEGIN(gpp)

#define GPP_REQUIRE_CONFIG(CONDITION, KEY, VALUE, REQUIREMENT)                                                         \
    do {                                                                                                               \
        if (!(CONDITION)) {                                                                                            \
            const std::string gppConfigValue = std::format("{}", VALUE);                                               \
            const std::string gppRequirement = std::format("{}", REQUIREMENT);                                         \
            throw std::invalid_argument(gppTr("validateNormalJsonCoreConfig", "配置项 %1 无效: 当前值 %2，要求%3")     \
                    .arg(KEY)                                                                                          \
                    .arg(gppConfigValue)                                                                               \
                    .arg(gppRequirement)                                                                               \
                    .toStdString());                                                                                   \
        }                                                                                                              \
    } while (false)

void validateTranslationSettings(const TranslationSettings& config, bool agentEnabled)
{
    auto isOneOf = [](std::string_view value, std::initializer_list<std::string_view> candidates)
        {
            return std::ranges::find(candidates, value) != candidates.end();
        };

    GPP_REQUIRE_CONFIG(config.batchSize > 0,
        "common.numPerRequestTranslate",
        config.batchSize,
        gppTr("validateNormalJsonCoreConfig", "大于 0").toStdString());
    GPP_REQUIRE_CONFIG(config.threadsNum > 0,
        "common.threadsNum",
        config.threadsNum,
        gppTr("validateNormalJsonCoreConfig", "大于 0").toStdString());
    GPP_REQUIRE_CONFIG(config.nameTransBatchSize > 0,
        "common.numPerRequestNameTranslate",
        config.nameTransBatchSize,
        gppTr("validateNormalJsonCoreConfig", "大于 0").toStdString());
    GPP_REQUIRE_CONFIG(isOneOf(config.sortMethod, { "name", "size" }),
        "common.sortMethod",
        config.sortMethod,
        gppTr("validateNormalJsonCoreConfig", "为 name 或 size").toStdString());
    GPP_REQUIRE_CONFIG(isOneOf(config.splitFileMethod, { "No", "Num", "Equal" }),
        "common.split.method",
        config.splitFileMethod,
        gppTr("validateNormalJsonCoreConfig", "为 No、Num 或 Equal").toStdString());
    GPP_REQUIRE_CONFIG(isOneOf(config.problemOverviewFormat, { "toml", "json" }),
        "common.problemOverviewFormat",
        config.problemOverviewFormat,
        gppTr("validateNormalJsonCoreConfig", "为 toml 或 json").toStdString());
    GPP_REQUIRE_CONFIG(config.splitFileNum > 0,
        "common.split.num",
        config.splitFileNum,
        gppTr("validateNormalJsonCoreConfig", "大于 0").toStdString());
    GPP_REQUIRE_CONFIG(config.repeatedBlockMinSize >= 2,
        "common.repeatedBlock.minSize",
        config.repeatedBlockMinSize,
        gppTr("validateNormalJsonCoreConfig", "大于等于 2").toStdString());
    GPP_REQUIRE_CONFIG(config.cacheSearchDistance >= 0,
        "common.split.cacheSearchDistance",
        config.cacheSearchDistance,
        gppTr("validateNormalJsonCoreConfig", "大于等于 0").toStdString());
    GPP_REQUIRE_CONFIG(config.saveCacheInterval > 0,
        "common.saveCacheInterval",
        config.saveCacheInterval,
        gppTr("validateNormalJsonCoreConfig", "大于 0").toStdString());
    GPP_REQUIRE_CONFIG(config.maxRequestCount > 0,
        "common.maxRequestCount",
        config.maxRequestCount,
        gppTr("validateNormalJsonCoreConfig", "大于 0").toStdString());
    GPP_REQUIRE_CONFIG(config.inputBlockMaxLines > 0,
        "common.log.inputBlockMaxLines",
        config.inputBlockMaxLines,
        gppTr("validateNormalJsonCoreConfig", "大于 0").toStdString());
    GPP_REQUIRE_CONFIG(config.problemMaxLines > 0,
        "common.log.problemMaxLines",
        config.problemMaxLines,
        gppTr("validateNormalJsonCoreConfig", "大于 0").toStdString());
    GPP_REQUIRE_CONFIG(config.glossaryMaxLines > 0,
        "common.log.glossaryMaxLines",
        config.glossaryMaxLines,
        gppTr("validateNormalJsonCoreConfig", "大于 0").toStdString());

    if (!agentEnabled) {
        return;
    }

    GPP_REQUIRE_CONFIG(config.agentMaxTurnsPerChunk > 0,
        "common.agent.maxTurnsPerChunk",
        config.agentMaxTurnsPerChunk,
        gppTr("validateNormalJsonCoreConfig", "大于 0").toStdString());
    GPP_REQUIRE_CONFIG(config.agentSearchResultLimit >= 1,
        "common.agent.searchResultLimit",
        config.agentSearchResultLimit,
        gppTr("validateNormalJsonCoreConfig", "大于等于 1").toStdString());
    GPP_REQUIRE_CONFIG(config.agentContextLinesLimit >= 0,
        "common.agent.contextLinesLimit",
        config.agentContextLinesLimit,
        gppTr("validateNormalJsonCoreConfig", "大于等于 0").toStdString());
    GPP_REQUIRE_CONFIG(config.agentCompactContextThresholdBytes > 0,
        "common.agent.compactContextThresholdBytes",
        config.agentCompactContextThresholdBytes,
        gppTr("validateNormalJsonCoreConfig", "大于 0").toStdString());
}

#undef GPP_REQUIRE_CONFIG

TranslationSettings readTranslationSettings(const toml::value& configData)
{
    TranslationSettings settings;
    settings.batchSize = toml::find_or(configData, "common", "numPerRequestTranslate", 16);
    settings.threadsNum = toml::find_or(configData, "common", "threadsNum", 5);
    settings.nameTransBatchSize = toml::find_or(configData, "common", "numPerRequestNameTranslate", 50);
    settings.sortMethod = toml::find_or(configData, "common", "sortMethod", "size");
    settings.targetLang = toml::find_or(configData, "common", "targetLang", "zh-cn");
    settings.splitFileMethod = toml::find_or(configData, "common", "split", "method", "No");
    settings.problemOverviewFormat = toml::find_or(configData, "common", "problemOverviewFormat", "json");
    settings.splitFileNum = toml::find_or(configData, "common", "split", "num", 10);
    settings.reuseRepeatedBlocks = toml::find_or(configData, "common", "repeatedBlock", "enabled", false);
    settings.repeatedBlockMinSize = toml::find_or(configData, "common", "repeatedBlock", "minSize", 5);
    settings.cacheSearchDistance = toml::find_or(configData, "common", "split", "cacheSearchDistance", 5);
    settings.saveCacheInterval = toml::find_or(configData, "common", "saveCacheInterval", 1);
    settings.keepPreviousOtherInfo = toml::find_or(configData, "common", "keepPreviousOtherInfo", false);
    settings.linebreakSymbol = toml::find_or(configData, "common", "linebreakSymbol", "auto");
    settings.maxRequestCount = toml::find_or(configData, "common", "maxRequestCount", 4);
    settings.inputBlockMaxLines = toml::find_or(configData, "common", "log", "inputBlockMaxLines", 10);
    settings.problemMaxLines = toml::find_or(configData, "common", "log", "problemMaxLines", 3);
    settings.glossaryMaxLines = toml::find_or(configData, "common", "log", "glossaryMaxLines", 5);
    settings.smartRetry = toml::find_or(configData, "common", "smartRetry", false);
    settings.enhanceJailbreak = toml::find_or(configData, "common", "enhanceJailbreak", 0);
    settings.checkQuota = toml::find_or(configData, "common", "checkQuota", true);
    settings.retransAllWhenFail = toml::find_or(configData, "common", "retransAllWhenFail", false);
    settings.agentEnabled = toml::find_or(configData, "common", "agent", "enabled", false);
    settings.agentAllowPython = toml::find_or(configData, "common", "agent", "allowPython", false);
    settings.agentMaxTurnsPerChunk = toml::find_or(configData, "common", "agent", "maxTurnsPerChunk", 50);
    settings.agentCompactContextThresholdBytes =
        toml::find_or(configData, "common", "agent", "compactContextThresholdBytes", 400000);
    settings.agentSearchResultLimit = toml::find_or(configData, "common", "agent", "searchResultLimit", 80);
    settings.agentContextLinesLimit = toml::find_or(configData, "common", "agent", "contextLinesLimit", 20);
    settings.projectNotePath = toml::find_or(configData, "common", "agent", "projectNotePath", "ProjectNote.md");
    return settings;
}

bool supportsTransAgent(TransEngine engine)
{
    return engine == TransEngine::ForGalJson || engine == TransEngine::ForNovelJson ||
           engine == TransEngine::ForGalTsv || engine == TransEngine::ForNovelTsv;
}

bool usesTranslationApi(TransEngine engine)
{
    return engine != TransEngine::DumpName && engine != TransEngine::Rebuild && engine != TransEngine::ShowNormal;
}

std::vector<ProjectApi> readProjectApis(const toml::value& configData,
    TransEngine engine,
    bool agentEnabled,
    const std::function<void(const std::string&)>& warningFunc)
{
    const auto apisArr = toml::find<std::vector<toml::table>>(configData, "backend", "apis");

    std::vector<ProjectApi> apis;
    for (const auto& [apiIndex, apiTbl] : apisArr | std::views::enumerate) {
        if (apiTbl.contains("enable") && !apiTbl.at("enable").as_boolean()) {
            continue;
        }
        ProjectApi entry{
            apiTbl.contains("name") ? apiTbl.at("name").as_string() : "自定义 Api", (size_t)apiIndex, {}
        };
        TranslationApi api;
        if (apiTbl.contains("protocol") && apiTbl.at("protocol").is_string()) {
            api.protocol = parseApiProtocol(apiTbl.at("protocol").as_string());
        }
        else {
            api.protocol = ApiProtocol::OpenAI;
            warningFunc(gppTr(
                "NormalJsonTranslator.normalJsonInit", "backend.apis[%1] 未找到 Api 协议字段，默认使用 OpenAI 协议")
                    .arg(apiIndex)
                    .toStdString());
        }
        if (agentEnabled && api.protocol == ApiProtocol::Gemini && apiTbl.contains("agentGeminiInteractions") &&
            apiTbl.at("agentGeminiInteractions").as_boolean()) {
            api.protocol = ApiProtocol::GeminiInterAction;
        }
        if (apiTbl.contains("apiurl") && !apiTbl.at("apiurl").as_string().empty()) {
            api.apiurl = cvt2StdApiUrl(apiTbl.at("apiurl").as_string(), api.protocol);
        }
        else {
            warningFunc(gppTr("NormalJsonTranslator.normalJsonInit", "backend.apis[%1] apiurl 为空，已忽略")
                    .arg(apiIndex)
                    .toStdString());
            continue;
        }
        if (apiTbl.contains("modelName") && !apiTbl.at("modelName").as_string().empty()) {
            api.modelName = apiTbl.at("modelName").as_string();
        }
        else if (engine == TransEngine::Sakura) {
            api.modelName = "sakura";
        }
        else {
            warningFunc(gppTr("NormalJsonTranslator.normalJsonInit",
                "backend.apis[%1] modelName 为空且不是 Sakura TransEngine，已忽略")
                    .arg(apiIndex)
                    .toStdString());
            continue;
        }
        if (apiTbl.contains("agentStrictTools")) {
            api.agentStrictTools = apiTbl.at("agentStrictTools").as_string();
        }
        api.agentStateful = apiTbl.contains("agentStateful") && apiTbl.at("agentStateful").as_boolean();
        api.agentNativeAutoCompaction =
            apiTbl.contains("agentNativeAutoCompaction") && apiTbl.at("agentNativeAutoCompaction").as_boolean();
        if (apiTbl.contains("agentCompactThresholdTokens")) {
            api.agentCompactThresholdTokens = (int)apiTbl.at("agentCompactThresholdTokens").as_integer();
        }
        api.useSystemProxy = !apiTbl.contains("useSystemProxy") || apiTbl.at("useSystemProxy").as_boolean();
        api.stream = apiTbl.contains("stream") && apiTbl.at("stream").as_boolean();
        api.fastPause = apiTbl.contains("fastPause") && apiTbl.at("fastPause").as_boolean();
        api.thinkingLevel = "off";
        if (apiTbl.contains("thinkingLevel")) {
            api.thinkingLevel = apiTbl.at("thinkingLevel").as_string();
        }
        if (apiTbl.contains("extraHeadersEnable") && apiTbl.at("extraHeadersEnable").as_boolean() &&
            apiTbl.contains("extraHeaders")) {
            for (const auto& [key, value] : apiTbl.at("extraHeaders").as_table()) {
                if (value.is_string()) {
                    api.extraHeaders[key] = value.as_string();
                }
                else {
                    api.extraHeaders[key] = toml2Json(value).dump();
                }
            }
        }
        if (apiTbl.contains("extraBodyEnable") && apiTbl.at("extraBodyEnable").as_boolean() &&
            apiTbl.contains("extraBody") && apiTbl.at("extraBody").is_table()) {
            api.extraBody = toml2Json(apiTbl.at("extraBody"));
        }
        if (apiTbl.contains("temperature")) {
            api.temperature = apiTbl.at("temperature").as_floating();
        }
        if (apiTbl.contains("topP")) {
            api.topP = apiTbl.at("topP").as_floating();
        }
        if (apiTbl.contains("frequencyPenalty")) {
            api.frequencyPenalty = apiTbl.at("frequencyPenalty").as_floating();
        }
        if (apiTbl.contains("presencePenalty")) {
            api.presencePenalty = apiTbl.at("presencePenalty").as_floating();
        }
        bool hasApiKey = false;
        if (apiTbl.contains("apikeys")) {
            for (const auto& apiKeyValue : apiTbl.at("apikeys").as_array()) {
                const std::string& apiKey = apiKeyValue.as_string();
                if (apiKey.empty()) {
                    continue;
                }
                TranslationApi apiWithKey = api;
                apiWithKey.apikey = apiKey;
                entry.tokens.push_back(std::move(apiWithKey));
                hasApiKey = true;
            }
        }
        if (!hasApiKey && engine == TransEngine::Sakura) {
            TranslationApi apiWithKey = api;
            apiWithKey.apikey = "sk-sakura";
            entry.tokens.push_back(std::move(apiWithKey));
        }
        if (!entry.tokens.empty()) {
            apis.push_back(std::move(entry));
        }
    }
    return apis;
}

std::vector<ProjectDictionaryFile> resolveProjectDictionaries(
    const fs::path& projectDir, const toml::value& config, const std::string& type)
{
    std::vector<ProjectDictionaryFile> result;
    const auto names = toml::find<std::optional<std::vector<std::string>>>(config, "dictionary", type + "Dicts");
    if (!names) {
        return result;
    }
    const fs::path defaults = ascii2Wide(toml::find_or(config, "dictionary", "defaultDictFolder", "BaseConfig/Dicts"));
    for (const auto& name : *names) {
        fs::path path = ascii2Wide(name);
        if (path.is_relative()) {
            path = projectDir / path;
            if (!fs::exists(path)) {
                path = defaults / ascii2Wide(type) / ascii2Wide(name);
            }
        }
        result.push_back({ name, path, fs::exists(path) });
    }
    return result;
}

ProjectPrompts readProjectPrompts(const fs::path& projectDir, TransEngine engine, bool agentEnabled)
{
    const fs::path projectPromptPath = projectDir / L"Prompt.toml";
    const bool hasProjectPrompt = fs::exists(projectPromptPath);
    const bool hasDefaultPrompt = fs::exists(defaultPromptPath);
    if (!hasProjectPrompt && !hasDefaultPrompt) {
        throw std::runtime_error(gppTr("NormalJsonTranslator.normalJsonInit", "找不到 Prompt.toml 文件").toStdString());
    }

    const auto projectPromptData = hasProjectPrompt ? gpp::uparseToml(projectPromptPath) : toml::value{};
    const auto defaultPromptData = hasDefaultPrompt ? gpp::uparseToml(defaultPromptPath) : toml::value{};

    const auto readPromptString = [&](const std::string& key) -> ProjectPrompt
        {
            std::string prompt;
            fs::path source;
            if (hasProjectPrompt && projectPromptData.contains(key)) {
                prompt = projectPromptData.at(key).as_string();
                source = projectPromptPath;
            }
            else if (hasDefaultPrompt && defaultPromptData.contains(key)) {
                prompt = defaultPromptData.at(key).as_string();
                source = defaultPromptPath;
            }
            else {
                throw std::invalid_argument(
                    gppTr("NormalJsonTranslator.normalJsonInit", "Prompt.toml 中缺少 %1 键").arg(key).toStdString());
            }
            return { key, source, std::move(prompt) };
        };

    if (engine == TransEngine::GenDict) {
        return { readPromptString("GENDICT_SYSTEM"), readPromptString("GENDICT_USER") };
    }
    else {
        std::string systemPromptKey;
        std::string userPromptKey;

        switch (engine) {
        case TransEngine::ForGalJson:
            systemPromptKey = agentEnabled ? "FORGALJSON_AGENT_SYSTEM" : "FORGALJSON_SYSTEM";
            userPromptKey = agentEnabled ? "FORGALJSON_AGENT_USER" : "FORGALJSON_USER";
            break;
        case TransEngine::ForNovelJson:
            systemPromptKey = agentEnabled ? "FORNOVELJSON_AGENT_SYSTEM" : "FORNOVELJSON_SYSTEM";
            userPromptKey = agentEnabled ? "FORNOVELJSON_AGENT_USER" : "FORNOVELJSON_USER";
            break;
        case TransEngine::ForGalTsv:
            systemPromptKey = agentEnabled ? "FORGALTSV_AGENT_SYSTEM" : "FORGALTSV_SYSTEM";
            userPromptKey = agentEnabled ? "FORGALTSV_AGENT_USER" : "FORGALTSV_USER";
            break;
        case TransEngine::ForNovelTsv:
            systemPromptKey = agentEnabled ? "FORNOVELTSV_AGENT_SYSTEM" : "FORNOVELTSV_SYSTEM";
            userPromptKey = agentEnabled ? "FORNOVELTSV_AGENT_USER" : "FORNOVELTSV_USER";
            break;
        case TransEngine::Sakura:
            systemPromptKey = "SAKURA_SYSTEM";
            userPromptKey = "SAKURA_USER";
            break;
        case TransEngine::NameTrans:
            systemPromptKey = "NAMETRANS_SYSTEM";
            userPromptKey = "NAMETRANS_USER";
            break;
        default:
            throw std::invalid_argument(
                gppTr("NormalJsonTranslator.normalJsonInit", "内部错误: 未知的 TransEngine").toStdString());
        }

        return { readPromptString(systemPromptKey), readPromptString(userPromptKey) };
    }
}

NAMESPACE_END(gpp)
