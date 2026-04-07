module;

#include "GPPMacros.hpp"

export module APITool;

import Tool;
import ITranslator;

export {
    struct TranslationApi {
        std::string apikey;
        std::string apiurl;
        std::string modelName;
        std::optional<double> temperature;
        std::optional<double> topP;
        std::optional<double> frequencyPenalty;
        std::optional<double> presencePenalty;
        std::chrono::steady_clock::time_point lastReportTime = std::chrono::steady_clock::now();
        int reportCount = 0;
        bool stream;
    };

    struct ApiResponse {
        bool success = false;
        std::string content; // 成功时的内容 或 失败时的错误信息
        long statusCode = 0;   // HTTP 状态码
    };

    ApiResponse performApiRequest(json& payload, const TranslationApi& api, const std::function<std::string(std::string)>& onPerformApi,
        const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, int threadId, int apiTimeOutMs);

    std::string cvt2StdApiUrl(const std::string& url);
}
