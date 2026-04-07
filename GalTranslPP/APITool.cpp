module;

#define PCRE2_HEADERS
#include "GPPMacros.hpp"
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif
#include <cpr/cpr.h>

module APITool;

import Tool;

#ifdef _WIN32
// Windows下获取系统代理的辅助函数
std::string getSystemProxyUrl() {
    std::string proxyUrl;
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxyConfig;
    // 获取当前用户的代理配置
    if (WinHttpGetIEProxyConfigForCurrentUser(&proxyConfig)) {
        if (proxyConfig.lpszProxy) {
            // lpszProxy 是宽字符 (wstring)，需要转换为 string
            proxyUrl = wide2Ascii(proxyConfig.lpszProxy);
            // 这里的 proxyUrl 可能是 "127.0.0.1:7890" 这种格式
            // 或者是 "http=127.0.0.1:7890;https=..." 这种复杂格式
            // 为了简单起见，如果包含分号，我们只取第一个
            if (size_t pos = proxyUrl.find(';'); pos != std::string::npos) {
                proxyUrl = proxyUrl.substr(0, pos);
            }
            // 如果没有协议头，补上 http:// (cpr需要)
            if (!proxyUrl.contains("://") && !proxyUrl.contains("=")) {
                proxyUrl = "http://" + proxyUrl;
            }
            GlobalFree(proxyConfig.lpszProxy);
        }
        if (proxyConfig.lpszAutoConfigUrl) GlobalFree(proxyConfig.lpszAutoConfigUrl);
        if (proxyConfig.lpszProxyBypass) GlobalFree(proxyConfig.lpszProxyBypass);
    }
    return proxyUrl;
}
#else
// Linux/Mac 下获取环境变量
std::string getSystemProxyUrl() {
    const char* proxy = std::getenv("http_proxy");
    if (proxy) return std::string(proxy);
    proxy = std::getenv("HTTP_PROXY");
    if (proxy) return std::string(proxy);
    return "";
}
#endif

ApiResponse performApiRequest(json& payload, const TranslationApi& api, const std::function<std::string(std::string)>& onPerformApi,
    const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, int threadId, int apiTimeOutMs) {
    ApiResponse apiResponse;

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

    const std::string payloadStr = onPerformApi ? onPerformApi(payload.dump()) : payload.dump();

    cpr::Proxies proxies = [&]()
	    {
            if (const std::string systemProxy = getSystemProxyUrl(); !systemProxy.empty()) {
                // 同时设置 http 和 https 代理
                logger->trace("正在使用系统代理: [{}]", systemProxy);
                return cpr::Proxies{ {"http", systemProxy}, {"https", systemProxy} };
            }
            return cpr::Proxies{};
        }();

    if (api.stream) {
        // =================================================
        // ===========   流式请求处理路径   ================
        // =================================================
        std::string concatenatedContent;
        std::string sseBuffer;

        // 1. 定义一个符合 cpr::WriteCallback 构造函数要求的 lambda
        auto callbackLambda = [&](std::string_view data, intptr_t userdata) -> bool
            {
                // 将接收到的数据块(string_view)追加到缓冲区(string)
                logger->trace("[线程 {}] 接收到流式数据块: {}", threadId, replaceStr(std::string(data), "\n", "[n]"));
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
                        try {
                            json chunk = json::parse(jsonDataStr);
                            json& contentNode = chunk["choices"][0]["delta"]["content"];
                            if (contentNode.is_string()) {
                                concatenatedContent += contentNode.get<std::string>();
                            }
                        }
                        catch (...) { }
                    }
                }
                // 继续接收数据
                return !controller->shouldStop();
            };

        // 2. 使用上面定义的 lambda 来构造一个 cpr::WriteCallback 类的实例
        cpr::WriteCallback writeCallbackInstance(callbackLambda);

        // 3. 将该实例传递给 cpr::Post
        cpr::Response response = cpr::Post(
            cpr::Url{ api.apiurl },
            cpr::Body{ payloadStr },
            cpr::Header{ {"Content-Type", "application/json"}, {"Authorization", "Bearer " + api.apikey} },
            cpr::Timeout{ apiTimeOutMs },
            writeCallbackInstance, // 传递类的实例
            proxies
        );

        apiResponse.statusCode = response.status_code;
        if (response.status_code == 200) {
            apiResponse.success = true;
            apiResponse.content = concatenatedContent;
        }
        else {
            apiResponse.success = false;
            apiResponse.content = response.text;
            logger->error("[线程 {}] API 流式请求失败，状态码: {}, 错误: {}", threadId, response.status_code, response.text);
        }
    }
    else {
        // =================================================
        // =========   非流式请求处理路径   =========
        // =================================================
        cpr::Response response = cpr::Post(
            cpr::Url{ api.apiurl },
            cpr::Body{ payloadStr },
            cpr::Header{ {"Content-Type", "application/json"}, {"Authorization", "Bearer " + api.apikey} },
            cpr::Timeout{ apiTimeOutMs },
            proxies
        );

        apiResponse.statusCode = response.status_code;
        apiResponse.content = response.text; // 先记录原始响应体

        if (response.status_code == 200) {
            try {
                // 解析完整的JSON响应
                apiResponse.content = json::parse(response.text)["choices"][0]["message"]["content"];
                apiResponse.success = true;
            }
            catch (const json::exception& e) {
                logger->error("[线程 {}] 成功响应但JSON解析失败: {}, 错误: {}", threadId, response.text, e.what());
                apiResponse.success = false;
            }
        }
        else {
            logger->error("[线程 {}] API 非流请求失败，状态码: {}, 错误: {}", threadId, response.status_code, response.text);
            apiResponse.success = false;
        }
    }

    return apiResponse;
}

std::string cvt2StdApiUrl(const std::string& url) {
    std::string ret = url;
    if (ret.ends_with("/")) {
        ret.pop_back();
    }
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

