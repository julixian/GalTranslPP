module;

#include "GPPMacros.hpp"

module ApiTool;

import Tool;

NAMESPACE_BEGIN(gpp)

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

// extractApiResponseContent 在 parseApiResponse 成功后调用，按协议拼接模型文本，跳过 Gemini 思考与 Responses commentary。
// 没有匹配文本块可返回 nullopt，有文本块但字符串为空则返回空字符串；字段缺失/类型错误直接抛给调用方捕获。
std::optional<std::string> parseApiContent(const json& parsed, ApiProtocol protocol)
{
    switch (protocol)
    {
    case ApiProtocol::OpenAIRes:
    {
        std::optional<std::string> content;
        for (const auto& item : parsed.at("output")) {
            if (item.value("type", "") != "message" || item.value("phase", "") == "commentary") {
                continue;
            }
            for (const auto& block : item.at("content")) {
                if (block.value("type", "") == "output_text") {
                    if (content.has_value()) {
                        content.value() += block.at("text").get<std::string>();
                    }
                    else {
                        content = block.at("text").get<std::string>();
                    }
                }
            }
        }
        return content;
    }

    case ApiProtocol::Claude:
    {
        std::optional<std::string> content;
        for (const auto& block : parsed.at("content")) {
            if (block.contains("text")) {
                if (content.has_value()) {
                    content.value() += block.at("text").get<std::string>();
                }
                else {
                    content = block.at("text").get<std::string>();
                }
            }
        }
        return content;
    }

    case ApiProtocol::Gemini:
    {
        std::optional<std::string> content;
        for (const auto& part : parsed.at("candidates").at(0).at("content").at("parts"))
        {
            if (part.contains("text") && !part.value("thought", false)) {
                if (content.has_value()) {
                    content.value() += part.at("text").get<std::string>();
                }
                else {
                    content = part.at("text").get<std::string>();
                }
            }
        }
        return content;
    }

    case ApiProtocol::OpenAI:
    default:
        return parsed.at("choices").at(0).at("message").at("content").get<std::string>();
    }
}

// sendApiRequest 的普通文本解析入口：parseApiResponse 检查协议错误 -> parseApiContent 提取内容。
// 无文本块与空字符串分别返回 ResponseParse 错误，成功保证文本非空；不解析译文格式，也不服务于高级工具响应。
// 文本提取的 std::exception 转成带原始响应的 ApiError；调用方补 HTTP 状态并交给外层请求循环处理。
std::expected<std::string, ApiError> extractApiResponseContent(const std::string& responseContent, ApiProtocol protocol)
{
    auto parsed = parseApiResponse(responseContent, protocol);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    try {
        auto content = parseApiContent(*parsed, protocol);
        if (!content) return std::unexpected(makeApiError(ApiErrorType::ResponseParse,
            gppTr("ApiTool.extractApiResponseContent", "响应中没有文本内容").toStdString(), responseContent));
        // 普通请求的调用方都要求文本内容；高级 Agent 的工具调用使用独立解析路径。
        if (content->empty()) return std::unexpected(makeApiError(ApiErrorType::ResponseParse,
            gppTr("ApiTool.extractApiResponseContent", "[GPP.内容为空]").toStdString(), responseContent));
        return std::move(*content);
    }
    catch (const std::exception& e) {
        return std::unexpected(makeApiError(ApiErrorType::ResponseParse, e.what(), responseContent));
    }
}

NAMESPACE_END(gpp)
