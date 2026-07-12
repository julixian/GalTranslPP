module;

#include "GPPMacros.hpp"

export module ApiTool;

export import Tool;
export import ITranslator;

export
{
    enum class ApiProtocol {
        OpenAI,
        Claude,
        Gemini
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
        std::chrono::steady_clock::time_point lastReportTime = std::chrono::steady_clock::now();
        int reportCount = 0;
        bool stream = false;
    };

    struct ApiResponse {
        std::string content;
        long statusCode = 0;
    };

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

    std::expected<std::string, std::string> extractApiResponseContent(const std::string& responseContent, ApiProtocol protocol);

    ApiModelListResponse queryApiModels(const TranslationApi& api, int apiTimeOutMs);

    ApiTestResponse testApiConnection(const TranslationApi& api, int apiTimeOutMs);

    ApiResponse performApiRequest(json& payload, const TranslationApi& api, const std::function<std::string(std::string)>& onPerformApi,
        const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, int threadId, int apiTimeOutMs);

}
