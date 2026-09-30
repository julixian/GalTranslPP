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

    switch (protocol) {
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

    switch (api.protocol) {
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
    switch (api.protocol) {
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
    if (api.protocol == ApiProtocol::OpenAIRes) {
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
    std::string thinkingLevel = api.thinkingLevel;
    str2LowerInplace(thinkingLevel);
    if (thinkingLevel != "off" && !thinkingLevel.empty()) {
        if (api.protocol == ApiProtocol::Claude) {
            const int budgetTokens = thinkingLevel == "high" ? 2048 : thinkingLevel == "medium" ? 1536 : 1024;
            payload["thinking"] = {
                {"type", "enabled"},
                {"budget_tokens", budgetTokens}
            };
        }
        else if (api.protocol == ApiProtocol::Gemini) {
            const std::string geminiThinkingLevel = thinkingLevel == "high" ? "HIGH"
                : thinkingLevel == "medium" ? "MEDIUM" : "LOW";
            payload["generationConfig"]["thinkingConfig"] = {
                {"thinkingLevel", geminiThinkingLevel},
                {"includeThoughts", true}
            };
        }
        else if (api.protocol == ApiProtocol::OpenAIRes) {
            payload["reasoning"]["effort"] = thinkingLevel;
        }
        else {
            payload["reasoning_effort"] = thinkingLevel;
        }
    }
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
