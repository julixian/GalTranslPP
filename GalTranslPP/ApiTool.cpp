module;

#include "GPPMacros.hpp"
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif
#include <cpr/cpr.h>

module ApiTool;

import Tool;

ApiProtocol parseApiProtocol(std::string_view protocol)
{
    std::string normalized(protocol);
    str2LowerInplace(normalized);
    if (normalized == "openai") {
        return ApiProtocol::OpenAI;
    }
    if (normalized == "claude") {
        return ApiProtocol::Claude;
    }
    if (normalized == "gemini") {
        return ApiProtocol::Gemini;
    }
    if (normalized == "openaires") {
        return ApiProtocol::OpenAIRes;
    }
    throw std::invalid_argument(gppTr("parseApiProtocol", "无效的 Api 协议: %1 不在 {openai, claude, gemini, openaires} 中")
        .arg(std::string(protocol))
        .toStdString());
}

std::string apiProtocolToString(ApiProtocol protocol)
{
    switch (protocol)
	{
    case ApiProtocol::Claude:
        return "claude";
    case ApiProtocol::Gemini:
        return "gemini";
    case ApiProtocol::OpenAIRes:
        return "openaires";
    case ApiProtocol::OpenAI:
    default:
        return "openai";
    }
}

std::string cvt2StdApiUrl(const std::string& url, ApiProtocol protocol)
{
    std::string ret = url;
    while (ret.ends_with('/')) {
        ret.pop_back();
    }

    switch (protocol)
    {
    case ApiProtocol::Claude:
        if (ret.ends_with("/v1/messages") || ret.ends_with("/messages")) {
            return ret;
        }
        if (jpc::Regex(R"(/v\d+$)").match(ret) > 0) {
            return ret + "/messages";
        }
        return ret + "/v1/messages";

    case ApiProtocol::Gemini:
        return ret;

    case ApiProtocol::OpenAIRes:
        if (ret.ends_with("/responses")) {
            return ret;
        }
        if (jpc::Regex(R"(/v\d+$)").match(ret) > 0) {
            return ret + "/responses";
        }
        return ret + "/v1/responses";

    case ApiProtocol::OpenAI:
    default:
        if (ret.ends_with("/chat/completions")) {
            return ret;
        }
        if (ret.ends_with("/chat")) {
            return ret + "/completions";
        }
        if (jpc::Regex(R"(/v\d+$)").match(ret) > 0) {
            return ret + "/chat/completions";
        }
        return ret + "/v1/chat/completions";
    }
}

std::string cvt2RequestApiUrl(const TranslationApi& api)
{
    if (api.protocol != ApiProtocol::Gemini) {
        return api.apiurl;
    }

    std::string ret = api.apiurl;
    while (ret.ends_with('/')) {
        ret.pop_back();
    }

    if (ret.contains(":generateContent")) {
        return ret;
    }
    if (ret.contains("/models/")) {
        return ret + ":generateContent";
    }
    if (ret.ends_with("/models")) {
        return ret + "/" + api.modelName + ":generateContent";
    }
    if (ret.ends_with("/v1") || ret.ends_with("/v1beta") || ret.ends_with("/v1alpha")) {
        return ret + "/models/" + api.modelName + ":generateContent";
    }
    return ret + "/v1beta/models/" + api.modelName + ":generateContent";
}

std::string cvt2ModelListApiUrl(const TranslationApi& api)
{
    std::string ret = api.apiurl;
    while (ret.ends_with('/')) {
        ret.pop_back();
    }

    switch (api.protocol)
    {
    case ApiProtocol::Claude:
        if (ret.ends_with("/messages")) {
            ret.resize(ret.size() - std::string_view("/messages").size());
            ret += "/models";
        }
        if (ret.ends_with("/models")) {
            return ret;
        }
        if (ret.ends_with("/v1")) {
            return ret + "/models";
        }
        return ret + "/v1/models";

    case ApiProtocol::Gemini:
        if (const size_t pos = ret.find(":generateContent"); pos != std::string::npos) {
            ret.erase(pos);
        }
        if (const size_t pos = ret.find("/models/"); pos != std::string::npos) {
            ret.erase(pos + std::string_view("/models").size());
        }
        if (ret.ends_with("/models")) {
            return ret;
        }
        if (ret.ends_with("/v1") || ret.ends_with("/v1beta") || ret.ends_with("/v1alpha")) {
            return ret + "/models";
        }
        return ret + "/v1beta/models";

    case ApiProtocol::OpenAIRes:
    case ApiProtocol::OpenAI:
    default:
        if (ret.ends_with("/responses")) {
            ret.resize(ret.size() - std::string_view("/responses").size());
            ret += "/models";
        }
        else if (ret.ends_with("/chat/completions")) {
            ret.resize(ret.size() - std::string_view("/chat/completions").size());
            ret += "/models";
        }
        else if (ret.ends_with("/chat")) {
            ret.resize(ret.size() - std::string_view("/chat").size());
            ret += "/models";
        }
        if (ret.ends_with("/models")) {
            return ret;
        }
        if (jpc::Regex(R"(/v\d+$)").match(ret) > 0) {
            return ret + "/models";
        }
        return ret + "/v1/models";
    }
}

cpr::Header makeApiHeaders(const TranslationApi& api)
{
    cpr::Header headers{ {"Content-Type", "application/json"} };
    switch (api.protocol)
    {
    case ApiProtocol::Claude:
        headers["x-api-key"] = api.apikey;
        headers["anthropic-version"] = "2023-06-01";
        break;

    case ApiProtocol::Gemini:
        headers["x-goog-api-key"] = api.apikey;
        break;

    case ApiProtocol::OpenAI:
    default:
        headers["Authorization"] = "Bearer " + api.apikey;
        break;
    }

    for (const auto& [key, value] : api.extraHeaders) {
        headers[key] = value;
    }
    return headers;
}

json makeApiTestPayload(const TranslationApi& api)
{
    const std::string testMessage = gppTr("ApiTool.makeApiTestPayload", "请用中文完整回复一句话：GPP Api 测试成功。")
        .toStdString();
    switch (api.protocol)
    {
    case ApiProtocol::Gemini:
        return {
            {"contents", json::array({
                {
                    {"role", "user"},
                    {"parts", json::array({
                        {{"text", testMessage}}
                    })}
                }
            })}
        };

    case ApiProtocol::Claude:
    case ApiProtocol::OpenAI:
    case ApiProtocol::OpenAIRes:
        return {
            {"model", api.modelName},
            {"messages", json::array({
                {
                    {"role", "user"},
                    {"content", testMessage}
                }
            })}
        };
    }
    return json::object();
}

void applyApiPayloadOptions(json& payload, const TranslationApi& api)
{
    std::string modelName = str2Lower(api.modelName);
    std::ranges::replace(modelName, '.', '-');
    const bool claude35 = api.protocol == ApiProtocol::Claude && (modelName.contains("claude-3-5")
        || modelName.contains("claude-sonnet-3-5") || modelName.contains("claude-haiku-3-5"));

    // 提示词转化
    if (api.protocol == ApiProtocol::Claude) {
        // 3.5 系列的输出上限为 8192；其余模型默认给 16384，仍可由 extraBody 覆盖。
        payload["max_tokens"] = claude35 ? 8192 : 16384;
        json systemBlocks = json::array();
        auto& messages = payload.at("messages");
        for (auto message = messages.begin(); message != messages.end();) {
            if (message->at("role") == "system") {
                auto& content = message->at("content");
                if (content.is_string() && !content.get_ref<const std::string&>().empty()) {
                    systemBlocks.push_back({ {"type", "text"}, {"text", content} });
                }
                else {
                    systemBlocks.insert(systemBlocks.end(), content.begin(), content.end());
                }
                message = messages.erase(message);
            }
            else {
                ++message;
            }
        }
        if (!systemBlocks.empty()) {
            payload["system"] = std::move(systemBlocks);
        }
    }
    else if (api.protocol == ApiProtocol::OpenAIRes) {
        payload["input"] = std::move(payload.at("messages"));
        payload.erase("messages");
        payload["store"] = false;
    }

    payload["model"] = api.modelName;
    if (api.temperature.has_value()) {
        payload["temperature"] = api.temperature.value();
    }
    if (api.topP.has_value()) {
        payload["top_p"] = api.topP.value();
    }
    if (api.protocol != ApiProtocol::OpenAIRes && api.frequencyPenalty.has_value()) {
        payload["frequency_penalty"] = api.frequencyPenalty.value();
    }
    if (api.protocol != ApiProtocol::OpenAIRes && api.presencePenalty.has_value()) {
        payload["presence_penalty"] = api.presencePenalty.value();
    }
    if (api.stream) {
        payload["stream"] = true;
    }

    // off 或空字符串表示不传递思考参数，使用接口默认行为；none 则请求关闭或最低强度。
    if (std::string thinkingLevel = str2Lower(api.thinkingLevel); thinkingLevel != "off" && !thinkingLevel.empty()) {
        const auto isModel = [&](std::string_view name)
            {
                const auto pos = modelName.find(name);
                if (pos == std::string::npos) return false;
                const auto suffix = std::string_view(modelName).substr(pos + name.size());
                return suffix.empty() || suffix.starts_with(':') || suffix.starts_with("-20")
                    || (suffix.size() > 1 && suffix.front() == '-' && (suffix[1] < '0' || suffix[1] > '9'));
            };
        // 规则按近期官方模型维护；未识别的模型采用最新格式和档位范围。
        // OpenAI: https://developers.openai.com/api/docs/guides/reasoning
        // Claude: https://platform.claude.com/docs/en/build-with-claude/extended-thinking
        // Gemini: https://ai.google.dev/gemini-api/docs/generate-content/thinking
        switch (api.protocol)
        {
        case ApiProtocol::OpenAI:
        case ApiProtocol::OpenAIRes:
        {
            // Chat Completions 使用 reasoning_effort，Responses 使用 reasoning.effort。
            // GPT-5.1 的最高档为 high，5.2/5.4/5.5 为 xhigh，5.6 和 GPT-6 系列为 max。
            // minimal 映射到 low；支持 none 的型号保留 none，Codex 的 none 改为 low。
            // Pro 的最低档为 medium；未识别型号按最新模型处理，最低 low、最高 max。
            const bool gpt51 = isModel("gpt-5-1");
            const bool gpt52To55 = isModel("gpt-5-2") || isModel("gpt-5-4") || isModel("gpt-5-5");
            const bool pro = isModel("gpt-5-2-pro") || isModel("gpt-5-4-pro") || isModel("gpt-5-5-pro");
            const bool supportsNone = gpt51 || gpt52To55 || isModel("gpt-5-6")
                || isModel("gpt-6-sol") || isModel("gpt-6-luna");
            if (thinkingLevel == "minimal" || (thinkingLevel == "none" && !supportsNone)) {
                thinkingLevel = "low";
            }
            if (thinkingLevel == "none" && modelName.contains("-codex")) thinkingLevel = "low";
            if (pro && (thinkingLevel == "none" || thinkingLevel == "low")) thinkingLevel = "medium";
            if (gpt51 && (thinkingLevel == "xhigh" || thinkingLevel == "max")) {
                thinkingLevel = "high";
            }
            else if (gpt52To55 && thinkingLevel == "max") {
                thinkingLevel = "xhigh";
            }
            if (api.protocol == ApiProtocol::OpenAIRes) {
                payload["reasoning"]["effort"] = thinkingLevel;
            }
            else {
                payload["reasoning_effort"] = thinkingLevel;
            }
        }
        break;

        case ApiProtocol::Claude:
        {
            // 3.5 系列不支持扩展思考，任何档位均不追加 thinking 或 effort。
            // 3.7、4.0、4.1、4.5 使用 enabled + budget_tokens；4.6 及以后使用 adaptive + effort。
            // 只识别这些已知旧版本，其余型号按最新 adaptive 规范处理。
            if (claude35) break;
            const bool manualThinking = isModel("claude-3-7-sonnet") || isModel("claude-sonnet-3-7")
                || isModel("claude-sonnet-4") || isModel("claude-opus-4") || isModel("claude-opus-4-1")
                || isModel("claude-opus-4-5") || isModel("claude-sonnet-4-5") || isModel("claude-haiku-4-5");
            const bool claude46 = isModel("claude-opus-4-6") || isModel("claude-sonnet-4-6");
            const bool supportsDisabled = manualThinking || claude46 || isModel("claude-opus-4-7")
                || isModel("claude-opus-4-8") || isModel("claude-opus-5") || isModel("claude-sonnet-5");
            if (thinkingLevel == "none" && (supportsDisabled || isModel("claude-sonnet-5-5"))) {
                // 支持关闭的旧模型发送 disabled；Sonnet 5.5 用 between_tools 关闭回答前的思考。
                // 同时使用 low effort；手动预算模型中只有 Opus 4.5 支持 effort。
                payload["thinking"] = {{"type", supportsDisabled ? "disabled" : "between_tools"}};
                if (!manualThinking || isModel("claude-opus-4-5")) {
                    payload["output_config"]["effort"] = "low";
                }
            }
            else if (manualThinking) {
                // minimal/low/medium/high/xhigh/max 分别映射到 1024/2048/4096/8192/12288/15360。
                // 预算至少 1024，且小于默认 max_tokens=16384，为正文保留输出空间。
                // Opus 4.5 额外传 effort：minimal 降为 low，xhigh/max 降为 high。
                const int budgetTokens = thinkingLevel == "max" ? 15360 : thinkingLevel == "xhigh" ? 12288
                    : thinkingLevel == "high" ? 8192 : thinkingLevel == "medium" ? 4096
                    : thinkingLevel == "low" ? 2048 : 1024;
                payload["thinking"] = {{"type", "enabled"}, {"budget_tokens", budgetTokens}};
                if (isModel("claude-opus-4-5")) {
                    payload["output_config"]["effort"] = thinkingLevel == "minimal" ? "low"
                        : thinkingLevel == "xhigh" || thinkingLevel == "max" ? "high" : thinkingLevel;
                }
            }
            else {
                // adaptive 不传预算，low/medium/high/xhigh/max 直接作为 effort。
                // Opus 5.5、Fable、Mythos 及未识别型号不关闭思考，none/minimal 映射到 low。
                // 4.6 和 Mythos Preview 不支持 xhigh，映射到 high；max 保留。
                if (thinkingLevel == "none" || thinkingLevel == "minimal") thinkingLevel = "low";
                if (thinkingLevel == "xhigh" && (claude46 || isModel("claude-mythos-preview"))) thinkingLevel = "high";
                payload["thinking"] = {{"type", "adaptive"}};
                payload["output_config"]["effort"] = thinkingLevel;
            }
        }
        break;

        case ApiProtocol::Gemini:
        {
            // generateContent 的思考参数位于 generationConfig.thinkingConfig。
            // includeThoughts 只用于返回思考摘要；2.5 使用预算，3 系列及未识别型号使用等级。
            auto& thinkingConfig = payload["generationConfig"]["thinkingConfig"];
            thinkingConfig = {{"includeThoughts", true}};
            if (isModel("gemini-2-5-pro") || isModel("gemini-2-5-flash")) {
                // 2.5 Flash/Lite 的 none=0（关闭），Pro 不能关闭，none/minimal 使用最低预算 128。
                // Flash/Lite 的 minimal=512；low/medium/high/xhigh 为 2048/4096/8192/16384。
                // max 使用模型预算上限：Pro=32768，Flash/Lite=24576。
                const bool pro = isModel("gemini-2-5-pro");
                const int budgetTokens = thinkingLevel == "none" ? (pro ? 128 : 0)
                    : thinkingLevel == "minimal" ? (pro ? 128 : 512)
                    : thinkingLevel == "low" ? 2048 : thinkingLevel == "medium" ? 4096
                    : thinkingLevel == "high" ? 8192 : thinkingLevel == "xhigh" ? 16384 : (pro ? 32768 : 24576);
                thinkingConfig["thinkingBudget"] = budgetTokens;
            }
            else {
                // 3 Flash、3.1 Flash-Lite、3.5/3.6 Flash 支持 minimal，none/minimal 使用该档。
                // 其余型号（含未识别型号）最低 low；xhigh/max 统一映射到最高档 high。
                // 3 Pro 只有 low/high，medium 映射到 high；3.1 Pro 支持 medium。
                const bool supportsMinimal = isModel("gemini-3-flash") || isModel("gemini-3-1-flash-lite")
                    || isModel("gemini-3-5-flash") || isModel("gemini-3-6-flash");
                if (thinkingLevel == "none" || thinkingLevel == "minimal") {
                    thinkingLevel = supportsMinimal ? "minimal" : "low";
                }
                if (thinkingLevel == "xhigh" || thinkingLevel == "max"
                    || (thinkingLevel == "medium" && isModel("gemini-3-pro")))
                {
                    thinkingLevel = "high";
                }
                // Flash-Lite Image 只有 minimal/high 两档。
                if (isModel("gemini-3-1-flash-lite-image") && thinkingLevel != "minimal") thinkingLevel = "high";
                std::ranges::transform(thinkingLevel, thinkingLevel.begin(),
                    [](unsigned char ch) { return (char)std::toupper(ch); });
                thinkingConfig["thinkingLevel"] = thinkingLevel;
            }
        }
        break;

        }
    }

    // extraBody 覆盖
    if (api.extraBody.is_object()) {
        for (auto it = api.extraBody.cbegin(); it != api.extraBody.cend(); ++it) {
            payload[it.key()] = it.value();
        }
    }
}

cpr::Proxies makeSystemProxies(const std::shared_ptr<spdlog::logger>& logger = nullptr)
{
    std::string systemProxy;
#ifdef _WIN32
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxyConfig;
    if (WinHttpGetIEProxyConfigForCurrentUser(&proxyConfig)) {
        if (proxyConfig.lpszProxy) {
            systemProxy = wide2Ascii(proxyConfig.lpszProxy);
            if (const size_t pos = systemProxy.find(';'); pos != std::string::npos) {
                systemProxy = systemProxy.substr(0, pos);
            }
            if (!systemProxy.contains("://") && !systemProxy.contains("=")) {
                systemProxy = "http://" + systemProxy;
            }
            GlobalFree(proxyConfig.lpszProxy);
        }
        if (proxyConfig.lpszAutoConfigUrl) GlobalFree(proxyConfig.lpszAutoConfigUrl);
        if (proxyConfig.lpszProxyBypass) GlobalFree(proxyConfig.lpszProxyBypass);
    }
#else
    const char* proxy = std::getenv("http_proxy");
    if (!proxy) {
        proxy = std::getenv("HTTP_PROXY");
    }
    if (proxy) {
        systemProxy = proxy;
    }
#endif

    if (!systemProxy.empty()) {
        if (logger) {
            logger->trace(gppTr("ApiTool.makeSystemProxies", "正在使用系统代理: [%1]")
                .arg(systemProxy)
                .toStdString());
        }
        return cpr::Proxies{ {"http", systemProxy}, {"https", systemProxy} };
    }
    return cpr::Proxies{};
}

std::expected<std::string, std::string> parseApiContent(const json& parsed, ApiProtocol protocol, bool stream = false)
{
    switch (protocol)
    {
    case ApiProtocol::OpenAIRes:
    {
        const std::string type = parsed.value("type", "");
        const std::string status = parsed.value("status", "");
        if ((parsed.contains("error") && !parsed["error"].is_null()) ||
            type == "error" || type == "response.failed" || type == "response.incomplete" ||
            status == "failed" || status == "incomplete") {
            return std::unexpected(parsed.dump());
        }
        if (stream) {
            if (type == "response.completed") {
                return parseApiContent(parsed.at("response"), protocol);
            }
            if (type == "response.output_text.delta") {
                return parsed.at("delta").get<std::string>();
            }
            return std::string{};
        }
        std::string content;
        for (const auto& item : parsed.at("output")) {
            if (item.value("type", "") != "message" || item.value("phase", "") == "commentary") {
                continue;
            }
            for (const auto& block : item.at("content")) {
                if (block.value("type", "") == "output_text") {
                    content += block.at("text").get<std::string>();
                }
            }
        }
        return content;
    }

    case ApiProtocol::Claude:
    {
        if (stream) {
            const std::string type = parsed.value("type", "");
            if (type == "content_block_delta" && parsed.at("delta").value("type", "") == "text_delta") {
                return parsed.at("delta").at("text").get<std::string>();
            }
            return std::string{};
        }
        std::string content;
        for (const auto& block : parsed.at("content")) {
            if (block.contains("text")) {
                content += block.at("text").get<std::string>();
            }
        }
        return content;
    }

    case ApiProtocol::Gemini:
    {
        if (stream && (!parsed.contains("candidates") || parsed["candidates"].empty() ||
            !parsed["candidates"].at(0).contains("content"))) {
            return std::string{};
        }
        std::string content;
        for (const auto& part : parsed.at("candidates").at(0).at("content").at("parts")) {
            if (part.contains("text")) {
                content += part.at("text").get<std::string>();
            }
        }
        return content;
    }

    case ApiProtocol::OpenAI:
    default:
        if (stream) {
            const auto& choices = parsed.at("choices");
            if (choices.empty()) {
                return std::string{};
            }
            const auto& delta = choices.at(0).at("delta");
            if (!delta.contains("content") || delta["content"].is_null()) {
                return std::string{};
            }
            return delta.at("content").get<std::string>();
        }
        return parsed.at("choices").at(0).at("message").at("content").get<std::string>();
    }
}

std::expected<std::string, std::string> extractApiResponseContent(const std::string& responseContent, ApiProtocol protocol)
{
    try {
        return parseApiContent(json::parse(responseContent), protocol);
    }
    catch (const std::exception& e) {
        return std::unexpected(std::string(e.what()) + "\n" + responseContent);
    }
}

std::vector<std::string> extractApiModelNames(const json& parsed, ApiProtocol protocol)
{
    std::vector<std::string> models;
    std::unordered_set<std::string> seen;
    auto pushModelFunc = [&](std::string model)
        {
            if (model.starts_with("models/")) {
                model = model.substr(std::string_view("models/").size());
            }
            if (!model.empty() && seen.insert(model).second) {
                models.push_back(std::move(model));
            }
        };

    switch (protocol)
	{
    case ApiProtocol::Gemini:
        for (const auto& item : parsed.at("models")) {
            if (item.contains("name") && item["name"].is_string()) {
                pushModelFunc(item["name"].get<std::string>());
            }
        }
        break;

    case ApiProtocol::Claude:
    case ApiProtocol::OpenAI:
    case ApiProtocol::OpenAIRes:
        for (const auto& item : parsed.at("data")) {
            if (item.contains("id") && item["id"].is_string()) {
                pushModelFunc(item["id"].get<std::string>());
            }
            else if (item.contains("name") && item["name"].is_string()) {
                pushModelFunc(item["name"].get<std::string>());
            }
            else if (item.contains("model") && item["model"].is_string()) {
                pushModelFunc(item["model"].get<std::string>());
            }
        }
        break;
    }

    return models;
}



ApiResponse sendApiRequest(const std::string& payloadStr, const TranslationApi& api,
    const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, int apiTimeOutMs)
{
    const std::string requestUrl = cvt2RequestApiUrl(api);
    const cpr::Header headers = makeApiHeaders(api);
    const cpr::Proxies proxies = api.useSystemProxy ? makeSystemProxies(logger) : cpr::Proxies{};

    std::expected<std::string, std::string> content = std::string{};

    std::string rawBody;
    std::string sseBuffer;
    std::string eventData;

    auto consumeEventFunc = [&]()
        {
            if (eventData.empty()) {
                return;
            }
            if (content.has_value() && eventData != "[DONE]") {
                try {
                    const json chunk = json::parse(eventData);
                    auto extracted = parseApiContent(chunk, api.protocol, true);
                    if (!extracted) {
                        content = std::unexpected(std::move(extracted.error()));
                    }
                    else if (api.protocol == ApiProtocol::OpenAIRes && chunk.value("type", "") == "response.completed") {
                        content = std::move(extracted);
                    }
                    else {
                        content.value() += extracted.value();
                    }
                }
                catch (const std::exception& e) {
                    content = std::unexpected(std::string(e.what()) + "\n" + eventData);
                }
            }
            eventData.clear();
        };
    auto consumeLineFunc = [&](std::string_view line)
        {
            if (line.ends_with('\r')) {
                line.remove_suffix(1);
            }
            if (line.empty()) {
                consumeEventFunc();
            }
            else if (line.starts_with("data:")) {
                line.remove_prefix(5);
                if (line.starts_with(' ')) {
                    line.remove_prefix(1);
                }
                if (!eventData.empty()) {
                    eventData += '\n';
                }
                eventData.append(line);
            }
        };
    auto callbackFunc = [&](std::string_view data, intptr_t)
        {
            // 流式消息一般每个 data 返回 data: xxx\n\n
            // 结束返回 [Done]
            rawBody.append(data);
            sseBuffer.append(data);
            size_t pos;
            while ((pos = sseBuffer.find('\n')) != std::string::npos) {
                consumeLineFunc(std::string_view(sseBuffer).substr(0, pos));
                sseBuffer.erase(0, pos + 1);
            }
            return !controller || !controller->shouldStop();
        };

    const cpr::Response response = api.stream
        ? cpr::Post(cpr::Url{ requestUrl }, cpr::Body{ payloadStr }, headers,
            cpr::Timeout{ apiTimeOutMs }, cpr::WriteCallback{ callbackFunc }, proxies)
        : cpr::Post(cpr::Url{ requestUrl }, cpr::Body{ payloadStr }, headers,
            cpr::Timeout{ apiTimeOutMs }, proxies);

    if (response.status_code != 200) {
        return { std::unexpected(rawBody.empty()
            ? (response.text.empty() ? response.error.message : response.text) : rawBody), response.status_code };
    }
    if (response.error.code != cpr::ErrorCode::OK) {
        return { std::unexpected(response.error.message), response.status_code };
    }
    if (api.stream) {
        consumeLineFunc(sseBuffer);
        consumeEventFunc();
    }
    else {
        content = extractApiResponseContent(response.text, api.protocol);
    }
    return { std::move(content), response.status_code };
}

ApiResponse performApiRequest(json& payload, const TranslationApi& api, const std::function<std::string(std::string_view)>& onPerformApi,
    const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, int threadId, int apiTimeOutMs)
{
    applyApiPayloadOptions(payload, api);
    const std::string payloadStr = onPerformApi ? onPerformApi(payload.dump()) : payload.dump();
    return sendApiRequest(payloadStr, api, controller, logger, apiTimeOutMs);
}

ApiModelListResponse queryApiModels(const TranslationApi& api, int apiTimeOutMs)
{
    ApiModelListResponse result;
    const std::string requestUrl = cvt2ModelListApiUrl(api);
    const cpr::Response response = cpr::Get(
        cpr::Url{ requestUrl },
        makeApiHeaders(api),
        cpr::Timeout{ apiTimeOutMs },
        api.useSystemProxy ? makeSystemProxies() : cpr::Proxies{}
    );
    result.statusCode = response.status_code;
    result.content = response.text.empty() ? response.error.message : response.text;
    result.success = response.status_code == 200;
    if (!result.success) {
        return result;
    }

    json parsed;
    try {
        parsed = json::parse(result.content);
    }
    catch (const std::exception& e) {
        result.success = false;
        result.content = gppTr("ApiTool.queryApiModels", "模型列表响应 JSON 解析失败: %1")
            .arg(e.what())
            .toStdString();
        return result;
    }

    try {
        result.models = extractApiModelNames(parsed, api.protocol);
    }
    catch (const json::exception& e) {
        result.success = false;
        result.content = gppTr("ApiTool.queryApiModels", "模型列表响应模型字段解析失败: %1")
            .arg(e.what())
            .toStdString();
    }
    return result;
}

ApiTestResponse testApiConnection(const TranslationApi& api, int apiTimeOutMs)
{
    ApiTestResponse result;
    json payload = makeApiTestPayload(api);
    applyApiPayloadOptions(payload, api);
    result.requestBody = payload.dump(2);
    const ApiResponse response = sendApiRequest(payload.dump(), api, nullptr, nullptr, apiTimeOutMs);
    result.statusCode = response.statusCode;
    result.success = response.content.has_value();
    result.content = result.success ? response.content.value() : response.content.error();
    return result;
}
