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
    throw std::invalid_argument(gppTr("parseApiProtocol", "无效的 Api 协议: %1 不在 {openai, claude,  gemini} 中")
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
    case ApiProtocol::OpenAI:
    default:
        if (ret.ends_with("/chat/completions")) {
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
    payload["model"] = api.modelName;
    if (api.temperature.has_value()) {
        payload["temperature"] = api.temperature.value();
    }
    if (api.topP.has_value()) {
        payload["top_p"] = api.topP.value();
    }
    if (api.frequencyPenalty.has_value()) {
        payload["frequency_penalty"] = api.frequencyPenalty.value();
    }
    if (api.presencePenalty.has_value()) {
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

std::expected<std::string, std::string> extractApiResponseContent(const std::string& responseContent, ApiProtocol protocol)
{
    try {
        const json parsed = json::parse(responseContent);
        switch (protocol)
        {
        case ApiProtocol::Claude:
        {
            std::string content;
            for (const auto& block : parsed["content"]) {
                const json& textNode = block["text"];
                if (textNode.is_string()) {
                    content += textNode.get<std::string>();
                }
            }
            if (!content.empty()) {
                return content;
            }
            return parsed["content"][0]["text"].get<std::string>();
        }
        case ApiProtocol::Gemini:
        {
            std::string content;
            for (const auto& part : parsed["candidates"][0]["content"]["parts"]) {
                const json& textNode = part["text"];
                if (textNode.is_string()) {
                    content += textNode.get<std::string>();
                }
            }
            if (!content.empty()) {
                return content;
            }
            return parsed["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
        }
        case ApiProtocol::OpenAI:
        default:
            return parsed["choices"][0]["message"]["content"].get<std::string>();
        }
    }
    catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

std::string extractStreamApiResponseContent(const std::string& jsonDataStr, ApiProtocol protocol)
{
    try {
        const json chunk = json::parse(jsonDataStr);
        switch (protocol) {
        case ApiProtocol::Claude:
            return chunk.at("delta").at("text").get<std::string>();
        case ApiProtocol::Gemini:
        {
            std::string content;
            for (const auto& part : chunk.at("candidates").at(0).at("content").at("parts")) {
                content += part.at("text").get<std::string>();
            }
            return content;
        }
        case ApiProtocol::OpenAI:
            return chunk.at("choices").at(0).at("delta").at("content").get<std::string>();
        }
    }
    catch (const std::exception&) { }
    return {};
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



ApiResponse performApiRequest(json& payload, const TranslationApi& api, const std::function<std::string(std::string_view)>& onPerformApi,
    const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, int threadId, int apiTimeOutMs)
{
    ApiResponse apiResponse;

    const std::string requestUrl = cvt2RequestApiUrl(api); // 处理 google api 协议
    const cpr::Header headers = makeApiHeaders(api); // 加 key，extraHeaders

    applyApiPayloadOptions(payload, api); // 模型、温度、思考，extraBody
    std::string payloadStr = payload.dump();
    if (onPerformApi) {
        payloadStr = onPerformApi(payloadStr);
    }

    const cpr::Proxies proxies = api.useSystemProxy ? makeSystemProxies(logger) : cpr::Proxies{};

    if (api.stream) {
        std::string concatenatedContent;
        std::string sseBuffer;

        auto callbackLambda = [&](std::string_view data, intptr_t) -> bool
        {
            sseBuffer.append(data);
            size_t pos;
            while ((pos = sseBuffer.find('\n')) != std::string::npos) {
                std::string line = sseBuffer.substr(0, pos);
                sseBuffer.erase(0, pos + 1);

                if (line.starts_with("data: ")) {
                    std::string jsonDataStr = line.substr(6);
                    if (jsonDataStr == "[DONE]") {
                        return true;
                    }
                    concatenatedContent += extractStreamApiResponseContent(jsonDataStr, api.protocol);
                }
            }
            return !controller->shouldStop();
        };

        cpr::WriteCallback writeCallbackInstance(callbackLambda);

        cpr::Response response = cpr::Post(
            cpr::Url{ requestUrl },
            cpr::Body{ payloadStr },
            headers,
            cpr::Timeout{ apiTimeOutMs },
            writeCallbackInstance,
            proxies
        );

        apiResponse.statusCode = response.status_code;
        if (response.status_code == 200) {
            apiResponse.content = concatenatedContent;
        }
        else {
            apiResponse.content = response.text.empty() ? response.error.message : response.text;
        }
    }
    else {
        cpr::Response response = cpr::Post(
            cpr::Url{ requestUrl },
            cpr::Body{ payloadStr },
            headers,
            cpr::Timeout{ apiTimeOutMs },
            proxies
        );

        apiResponse.statusCode = response.status_code;
        apiResponse.content = response.text.empty() ? response.error.message : response.text;
    }

    return apiResponse;
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

    const std::string requestUrl = cvt2RequestApiUrl(api);
    json payload = makeApiTestPayload(api);
    applyApiPayloadOptions(payload, api);
    result.requestBody = payload.dump(2);

    const cpr::Response response = cpr::Post(
        cpr::Url{ requestUrl },
        cpr::Body{ payload.dump() },
        makeApiHeaders(api),
        cpr::Timeout{ apiTimeOutMs },
        api.useSystemProxy ? makeSystemProxies() : cpr::Proxies{}
    );

    result.statusCode = response.status_code;
    result.content = response.text.empty() ? response.error.message : response.text;
    result.success = response.status_code == 200;
    if (!result.success || api.stream) {
        return result;
    }

    std::expected<std::string, std::string> extractedContent = extractApiResponseContent(result.content, api.protocol);
    if (extractedContent.has_value()) {
        result.content = extractedContent.value();
    }
    else {
        result.success = false;
        result.content = gppTr("testApiConnection",
            "Api 响应 JSON 解析失败，%1")
            .arg(extractedContent.error())
            .toStdString();
    }
    return result;
}
