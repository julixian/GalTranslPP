module;

#include "GPPMacros.hpp"

module ApiTool;

import Tool;

NAMESPACE_BEGIN(gpp)

ApiError makeApiError(ApiErrorType type, std::string detail, std::string rawResponse, long statusCode)
{
    // 只对已失败的请求推断服务端错误；明确的拒答、截断和解析错误不再按正文关键词分类。
    if (type == ApiErrorType::Unknown) {
        const auto lower = str2Lower(detail + '\n' + rawResponse);
        if (statusCode == 401 || lower.contains("quota") || lower.contains("invalid token") || lower.contains("invalid_api_key"))
            type = ApiErrorType::InvalidKeyOrQuota;
        else if (lower.contains("no available") || lower.contains("no access") || lower.contains("model_not_found"))
            type = ApiErrorType::ModelUnavailable;
        else if (statusCode == 429 || lower.contains("rate limit") || lower.contains("rate_limit") || lower.contains("try again") || lower.contains("饱和"))
            type = ApiErrorType::RateLimit;
    }

    std::string message;
    switch (type)
    {
    case ApiErrorType::Transport: message = gppTr("ApiTool.makeApiError", "Api 网络请求失败").toStdString(); break;
    case ApiErrorType::InvalidKeyOrQuota: message = gppTr("ApiTool.makeApiError", "Api key 疑似无效或额度用尽").toStdString(); break;
    case ApiErrorType::ModelUnavailable: message = gppTr("ApiTool.makeApiError", "Api 没有可用模型或没有模型访问权限").toStdString(); break;
    case ApiErrorType::RateLimit: message = gppTr("ApiTool.makeApiError", "Api 频率限制或暂时不可用").toStdString(); break;
    case ApiErrorType::Refusal: message = gppTr("ApiTool.makeApiError", "Api 拒答或内容被拦截").toStdString(); break;
    case ApiErrorType::JsonParse: message = gppTr("ApiTool.makeApiError", "Api 响应 JSON 解析失败").toStdString(); break;
    case ApiErrorType::ResponseParse: message = gppTr("ApiTool.makeApiError", "Api 响应字段解析失败").toStdString(); break;
    case ApiErrorType::Incomplete: message = gppTr("ApiTool.makeApiError", "Api 回复未完成或已截断").toStdString(); break;
    default: message = gppTr("ApiTool.makeApiError", "未知 Api 错误").toStdString(); break;
    }
    if (!detail.empty()) message += ": " + detail;
    return {type, std::move(message), std::move(rawResponse), statusCode};
}

std::string formatApiError(const ApiError& error, const std::string& actionMessage)
{
    std::string message = error.message;
    if (!actionMessage.empty()) message += '\n' + actionMessage;
    message += '\n' + gppTr("ApiTool.formatApiError", "原始响应:").toStdString() + '\n';
    message += error.rawResponse.empty() ? gppTr("ApiTool.formatApiError", "[GPP.响应为空]").toStdString() : error.rawResponse;
    return message;
}

NAMESPACE_END(gpp)
