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

std::expected<json, ApiError> parseApiResponse(const std::string& rawResponse, ApiProtocol protocol, bool geminiInteractions)
{
    try {
        json parsed = json::parse(rawResponse);
        if (parsed.contains("error") && !parsed.at("error").is_null())
            throw makeApiError(ApiErrorType::Unknown);

        // 普通 batch 与原生 Agent 共用协议错误判断，各自只负责提取文本或工具调用。
        switch (protocol)
        {
        case ApiProtocol::OpenAI:
        {
            const auto& choice = parsed.at("choices").at(0);
            const auto reason = choice.value("finish_reason", "");
            const auto& message = choice.at("message");
            if (message.contains("refusal") && !message.at("refusal").is_null())
                throw makeApiError(ApiErrorType::Refusal);
            if (reason == "content_filter") throw makeApiError(ApiErrorType::Refusal, reason);
            if (reason == "length") throw makeApiError(ApiErrorType::Incomplete, reason);
            break;
        }
        case ApiProtocol::OpenAIRes:
        {
            for (const auto& item : parsed.at("output")) {
                if (item.value("type", "") != "message") continue;
                for (const auto& block : item.at("content")) {
                    if (block.value("type", "") == "refusal")
                        throw makeApiError(ApiErrorType::Refusal);
                }
            }
            const auto status = parsed.value("status", "completed");
            if (status != "completed") {
                const auto details = parsed.find("incomplete_details");
                const auto reason = details != parsed.end() && details->is_object() ? details->value("reason", status) : status;
                throw makeApiError(reason == "content_filter" ? ApiErrorType::Refusal : ApiErrorType::Incomplete, reason);
            }
            break;
        }
        case ApiProtocol::Claude:
        {
            const auto reason = parsed.value("stop_reason", "");
            if (reason == "refusal") throw makeApiError(ApiErrorType::Refusal, reason);
            if (reason == "max_tokens") throw makeApiError(ApiErrorType::Incomplete, reason);
            break;
        }
        case ApiProtocol::Gemini:
            if (geminiInteractions) {
                // Interactions 没有专门的拒答字段，失败状态不能直接当作拒答；正文拒答由 Agent 的无工具调用次数处理。
                const auto status = parsed.value("status", "");
                if (status != "completed" && status != "requires_action")
                    throw makeApiError(ApiErrorType::Incomplete, status);
            }
            else {
                if (const auto feedback = parsed.find("promptFeedback"); feedback != parsed.end()) {
                    const auto reason = feedback->value("blockReason", "");
                    if (!reason.empty() && reason != "BLOCK_REASON_UNSPECIFIED")
                        throw makeApiError(ApiErrorType::Refusal, reason);
                }
                const auto reason = parsed.at("candidates").at(0).value("finishReason", "STOP");
                if (reason != "STOP") {
                    const bool blocked = reason == "SAFETY" || reason == "RECITATION" || reason == "BLOCKLIST" || reason == "PROHIBITED_CONTENT" || reason == "SPII" || reason == "IMAGE_SAFETY";
                    throw makeApiError(blocked ? ApiErrorType::Refusal : ApiErrorType::Incomplete, reason);
                }
            }
            break;
        }
        return parsed;
    }
    catch (ApiError& error) {
        error.rawResponse = rawResponse;
        return std::unexpected(std::move(error));
    }
    catch (const json::parse_error& e) {
        return std::unexpected(makeApiError(ApiErrorType::JsonParse, e.what(), rawResponse));
    }
    catch (const std::exception& e) {
        return std::unexpected(makeApiError(ApiErrorType::ResponseParse, e.what(), rawResponse));
    }
}

NAMESPACE_END(gpp)
