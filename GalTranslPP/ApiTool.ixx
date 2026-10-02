module;

#include "GPPMacros.hpp"

export module ApiTool;

export import ITranslator;
export import Tool;

export
{
    enum class ApiProtocol {
        OpenAI,
        Claude,
        Gemini,
        OpenAIRes
    };

    struct TranslationApi {
        ApiProtocol protocol = ApiProtocol::OpenAI;
        std::string apikey;
        std::string apiurl;
        std::string modelName;
        std::string thinkingLevel = "off";
        std::map<std::string, std::string> extraHeaders;
        json extraBody = json::object();
        std::optional<double> temperature;
        std::optional<double> topP;
        std::optional<double> frequencyPenalty;
        std::optional<double> presencePenalty;
        std::chrono::steady_clock::time_point lastReportTime = std::chrono::steady_clock::time_point::min();
        int reportCount = 0;
        bool useSystemProxy = true;
        bool agentStrictTools = true;
        bool agentStateful = true;
        // 在普通请求中启用服务端按 token 阈值自动压缩，需要模型和接口支持。
        bool agentNativeAutoCompaction = false;
        // 0 不指定阈值；需要显式阈值的 Responses 路径沿用默认 100000 tokens。
        int agentCompactThresholdTokens = 0;
        bool agentGeminiInteractions = true;
    };

    struct ApiResponse {
        std::expected<std::string, std::string> content;
        long statusCode = 0;
    };

    struct ApiAgentToolCall {
        std::string id;
        std::string name;
        // 保留原始参数，单个工具的解析错误也能按调用 id 回填给模型。
        json arguments;
    };

    struct ApiAgentReply {
        std::string text;
        std::vector<ApiAgentToolCall> calls;
    };

    struct ApiAgentResponse {
        std::expected<ApiAgentReply, std::string> content;
        long statusCode = 0;
    };

    struct ApiAgentSession {
        TranslationApi api;
        std::string systemPrompt;
        json history = json::array();
        std::string previousId;
        size_t sentCount = 0;
    };

    bool isSameApi(const TranslationApi& lhs, const TranslationApi& rhs);
    void appendAgentUserMessage(ApiAgentSession& session, const std::string& text);
    void appendAgentToolResults(ApiAgentSession& session, const json& results);
    ApiAgentResponse performAgentApiRequest(ApiAgentSession& session, const json& tools,
        const std::function<std::string(std::string_view)>& onPerformApi,
        const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, int apiTimeOutMs);

    struct ApiTestResponse {
        bool success = false;
        std::string content;
        std::string requestBody;
        long statusCode = 0;
    };

    struct ApiModelListResponse {
        bool success = false;
        std::vector<std::string> models;
        std::string content;
        long statusCode = 0;
    };


    ApiProtocol parseApiProtocol(std::string_view protocol);
    std::string apiProtocolToString(ApiProtocol protocol);

    std::string cvt2StdApiUrl(const std::string& url, ApiProtocol protocol);

    std::string cvt2RequestApiUrl(const TranslationApi& api);
    void applyApiPayloadOptions(json& payload, const TranslationApi& api);
    ApiResponse sendApiHttpRequest(const std::string& payloadStr, const TranslationApi& api, const std::string& requestUrl,
        const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, int apiTimeOutMs);

    // For Gui
    ApiModelListResponse queryApiModels(const TranslationApi& api, int apiTimeOutMs);
    ApiTestResponse testApiConnection(const TranslationApi& api, int apiTimeOutMs);

    ApiResponse performApiRequest(json& payload, const TranslationApi& api, const std::function<std::string(std::string_view)>& onPerformApi,
        const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, int threadId, int apiTimeOutMs);

}
