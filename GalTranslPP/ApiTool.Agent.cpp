module;

#include "GPPMacros.hpp"

module ApiTool;

import Tool;

bool isSameApi(const TranslationApi& lhs, const TranslationApi& rhs)
{
    // 健康计数不属于会话身份；端点、凭据、模型或协议选项改变时不能沿用服务端 id 和签名。
    return lhs.protocol == rhs.protocol && lhs.apikey == rhs.apikey && lhs.apiurl == rhs.apiurl
        && lhs.modelName == rhs.modelName && lhs.thinkingLevel == rhs.thinkingLevel
        && lhs.extraHeaders == rhs.extraHeaders && lhs.extraBody == rhs.extraBody
        && lhs.temperature == rhs.temperature && lhs.topP == rhs.topP
        && lhs.frequencyPenalty == rhs.frequencyPenalty && lhs.presencePenalty == rhs.presencePenalty
        && lhs.useSystemProxy == rhs.useSystemProxy && lhs.agentStrictTools == rhs.agentStrictTools
        && lhs.agentStateful == rhs.agentStateful && lhs.agentNativeAutoCompaction == rhs.agentNativeAutoCompaction
        && lhs.agentCompactThresholdTokens == rhs.agentCompactThresholdTokens
        && lhs.agentGeminiInteractions == rhs.agentGeminiInteractions;
}

void appendAgentUserMessage(ApiAgentSession& session, const std::string& text)
{
    switch (session.api.protocol)
    {
    case ApiProtocol::Gemini:
        if (session.api.agentGeminiInteractions) {
            session.history.push_back({{"type", "user_input"}, {"content", json::array({{{"type", "text"}, {"text", text}}})}});
        }
        else {
            session.history.push_back({{"role", "user"}, {"parts", json::array({{{"text", text}}})}});
        }
        break;
    default:
        session.history.push_back({{"role", "user"}, {"content", text}});
        break;
    }
}

void appendAgentToolResults(ApiAgentSession& session, const json& results)
{
    json blocks = json::array();
    for (const auto& result : results) {
        const std::string id = result.at("id");
        const std::string name = result.at("name");
        const auto& content = result.at("result");
        switch (session.api.protocol)
        {
        case ApiProtocol::OpenAI:
            session.history.push_back({{"role", "tool"}, {"tool_call_id", id}, {"content", content.dump()}});
            break;
        case ApiProtocol::OpenAIRes:
            session.history.push_back({{"type", "function_call_output"}, {"call_id", id}, {"output", content.dump()}});
            break;
        case ApiProtocol::Claude:
            blocks.push_back({{"type", "tool_result"}, {"tool_use_id", id}, {"content", content.dump()},
                {"is_error", content.contains("error")}});
            break;
        case ApiProtocol::Gemini:
            if (session.api.agentGeminiInteractions) {
                session.history.push_back({{"type", "function_result"}, {"call_id", id}, {"name", name},
                    {"result", content}, {"is_error", content.contains("error")}});
            }
            else {
                json response = {{"name", name}, {"response", content}};
                if (!id.empty()) response["id"] = id;
                blocks.push_back({{"functionResponse", std::move(response)}});
            }
            break;
        }
    }
    if (!blocks.empty()) {
        session.history.push_back(session.api.protocol == ApiProtocol::Claude
            ? json{{"role", "user"}, {"content", std::move(blocks)}}
            : json{{"role", "user"}, {"parts", std::move(blocks)}});
    }
}

namespace
{
    json pendingAgentInput(const ApiAgentSession& session)
    {
        if (session.previousId.empty()) return session.history;
        return json(session.history.begin() + session.sentCount, session.history.end());
    }

    std::string interactionApiUrl(const TranslationApi& api)
    {
        std::string url = api.apiurl;
        while (url.ends_with('/')) url.pop_back();
        if (url.ends_with("/interactions")) return url;
        if (const auto pos = url.find("/models"); pos != std::string::npos) url.erase(pos);
        if (url.ends_with("/v1beta") || url.ends_with("/v1alpha")) url.erase(url.rfind('/'));
        return url.ends_with("/v1") ? url + "/interactions" : url + "/v1/interactions";
    }

    json buildAgentPayload(const ApiAgentSession& session, const json& tools)
    {
        const auto& api = session.api;
        json payload;
        json nativeTools = json::array();

        switch (api.protocol)
        {
        case ApiProtocol::OpenAI:
            // Chat 保留完整 assistant/tool 消息，每轮必须调用工具，由模型选择查询或提交。
            payload = {{"messages", session.history}, {"tool_choice", "required"}};
            payload["messages"].insert(payload["messages"].begin(), json{{"role", "system"}, {"content", session.systemPrompt}});
            for (const auto& tool : tools) {
                json function = tool;
                function["strict"] = api.agentStrictTools;
                nativeTools.push_back({{"type", "function"}, {"function", std::move(function)}});
            }
            break;

        case ApiProtocol::OpenAIRes:
            // Responses 的 output 原样接回 input；有 previous_response_id 时只发送新增输入。
            payload = {{"input", pendingAgentInput(session)}, {"instructions", session.systemPrompt},
                {"store", api.agentStateful}, {"tool_choice", "required"}};
            if (!session.previousId.empty()) payload["previous_response_id"] = session.previousId;
            for (const auto& tool : tools) {
                json function = tool;
                function["type"] = "function";
                function["strict"] = api.agentStrictTools;
                nativeTools.push_back(std::move(function));
            }
            if (api.agentNativeAutoCompaction) {
                // Responses 由服务端判断 token 数；配置为 0 时沿用本项目默认阈值。
                payload["context_management"] = json::array({{{"type", "compaction"},
                    {"compact_threshold", api.agentCompactThresholdTokens > 0 ? api.agentCompactThresholdTokens : 100000}}});
            }
            break;

        case ApiProtocol::Claude:
            // Claude 将系统提示词置于顶层，完整保留 thinking/redacted_thinking/tool_use 块。
            // 手动思考和部分新模型不支持强制工具调用；保留 auto，由提示词要求工具提交，正文不提交。
            payload = {{"system", session.systemPrompt}, {"messages", session.history}, {"tool_choice", {{"type", "auto"}}}};
            for (const auto& tool : tools) {
                json function = {{"name", tool.at("name")}, {"description", tool.at("description")},
                    {"input_schema", tool.at("parameters")}};
                if (api.agentStrictTools) function["strict"] = true;
                nativeTools.push_back(std::move(function));
            }
            if (api.agentNativeAutoCompaction) {
                // Claude 的 trigger 可以省略，未设置阈值时采用服务端默认值。
                json edit = {{"type", "compact_20260112"}};
                if (api.agentCompactThresholdTokens > 0) {
                    edit["trigger"] = {{"type", "input_tokens"}, {"value", api.agentCompactThresholdTokens}};
                }
                payload["context_management"]["edits"] = json::array({std::move(edit)});
            }
            break;

        case ApiProtocol::Gemini:
            if (api.agentGeminiInteractions) {
                // Interactions 使用独立的步骤历史、function_result 和服务端会话 id。
                payload = {{"input", pendingAgentInput(session)}, {"system_instruction", session.systemPrompt},
                    {"store", api.agentStateful}};
                if (!session.previousId.empty()) payload["previous_interaction_id"] = session.previousId;
                for (const auto& tool : tools) {
                    json function = tool;
                    function["type"] = "function";
                    nativeTools.push_back(std::move(function));
                }
                // any 要求调用声明的函数并约束参数结构，不接受正文作为翻译提交。
                payload["generation_config"]["tool_choice"] = "any";
            }
            else {
                // generateContent 回传全部 model parts，尤其是函数调用携带的 thoughtSignature。
                payload = {{"contents", session.history},
                    {"systemInstruction", {{"parts", json::array({{{"text", session.systemPrompt}}})}}}};
                json declarations = tools;
                for (auto& tool : declarations) {
                    tool["parametersJsonSchema"] = std::move(tool.at("parameters"));
                    tool.erase("parameters");
                }
                nativeTools.push_back({{"functionDeclarations", std::move(declarations)}});
                payload["toolConfig"]["functionCallingConfig"]["mode"] = "ANY";
            }
            break;
        }

        payload["tools"] = std::move(nativeTools);
        return payload;
    }

    ApiAgentReply parseAgentReply(const json& parsed, ApiAgentSession& session)
    {
        ApiAgentReply reply;
        json additions = json::array();
        bool compacted = false;
        std::string previousId;

        switch (session.api.protocol)
        {
        case ApiProtocol::OpenAI:
        {
            const auto& choice = parsed.at("choices").at(0);
            const auto& message = choice.at("message");
            if (message.contains("content") && message["content"].is_string()) reply.text = message.at("content");
            for (const auto& call : message.value("tool_calls", json::array())) {
                reply.calls.push_back({call.at("id"), call.at("function").at("name"), call.at("function").at("arguments")});
            }
            additions.push_back(message);
            break;
        }

        case ApiProtocol::OpenAIRes:
        {
            const auto& output = parsed.at("output");
            auto retainedBegin = output.begin();
            for (auto it = output.begin(); it != output.end(); ++it) {
                if (it->value("type", "") != "compaction") continue;
                const auto encrypted = it->find("encrypted_content");
                // 最新有效压缩项已承载之前的上下文；只清理本地副本，服务端续接 id 仍正常保留。
                if (encrypted != it->end() && encrypted->is_string() && !encrypted->get_ref<const std::string&>().empty()) {
                    retainedBegin = it;
                    compacted = true;
                }
            }
            for (auto it = retainedBegin; it != output.end(); ++it) {
                const auto& item = *it;
                if (item.value("type", "") == "function_call") {
                    reply.calls.push_back({item.at("call_id"), item.at("name"), item.at("arguments")});
                }
                else if (item.value("type", "") == "message") {
                    for (const auto& block : item.at("content")) {
                        if (block.value("type", "") == "output_text" && item.value("phase", "") != "commentary")
                        {
                            reply.text += block.at("text").get<std::string>();
                        }
                    }
                }
                // 推理、加密推理、压缩项和 phase 均由服务端定义，不在这里重构。
                additions.push_back(item);
            }
            if (session.api.agentStateful) previousId = parsed.at("id");
            break;
        }

        case ApiProtocol::Claude:
        {
            const auto& content = parsed.at("content");
            auto retainedBegin = content.begin();
            for (auto it = content.begin(); it != content.end(); ++it) {
                if (it->value("type", "") != "compaction") continue;
                const auto summary = it->find("content");
                // 压缩失败可能返回空摘要，只有有效压缩块才能替代它之前的历史。
                if (summary != it->end() && summary->is_string() && !summary->get_ref<const std::string&>().empty()) {
                    retainedBegin = it;
                    compacted = true;
                }
            }
            for (auto it = retainedBegin; it != content.end(); ++it) {
                const auto& block = *it;
                if (block.value("type", "") == "tool_use") reply.calls.push_back({block.at("id"), block.at("name"), block.at("input")});
                else if (block.value("type", "") == "text") reply.text += block.at("text").get<std::string>();
            }
            additions.push_back({{"role", "assistant"}, {"content", json(retainedBegin, content.end())}});
            break;
        }

        case ApiProtocol::Gemini:
            if (session.api.agentGeminiInteractions) {
                for (const auto& step : parsed.at("steps")) {
                    if (step.value("type", "") == "function_call") reply.calls.push_back({step.at("id"), step.at("name"), step.at("arguments")});
                    else if (step.value("type", "") == "model_output") {
                        for (const auto& block : step.at("content")) {
                            if (block.value("type", "") == "text") reply.text += block.at("text").get<std::string>();
                        }
                    }
                    additions.push_back(step);
                }
                if (session.api.agentStateful) previousId = parsed.at("id");
            }
            else {
                const auto& candidate = parsed.at("candidates").at(0);
                const auto& content = candidate.at("content");
                for (const auto& part : content.at("parts")) {
                    if (part.contains("functionCall")) {
                        const auto& call = part.at("functionCall");
                        reply.calls.push_back({call.value("id", ""), call.at("name"), call.at("args")});
                    }
                    else if (part.contains("text") && !part.value("thought", false)) reply.text += part.at("text").get<std::string>();
                }
                additions.push_back(content);
            }
            break;
        }

        if (reply.text.empty() && reply.calls.empty())
            throw std::runtime_error(gppTr("ApiTool.parseAgentReply", "响应中没有文本内容或工具调用").toStdString());
        // 整个响应解析成功后才更新会话，失败重试不会留下半截工具调用。
        if (compacted) session.history = std::move(additions);
        else session.history.insert(session.history.end(), additions.begin(), additions.end());
        session.previousId = std::move(previousId);
        session.sentCount = session.history.size();
        return reply;
    }
}

ApiAgentResponse performAgentApiRequest(ApiAgentSession& session, const json& tools,
    const std::function<std::string(std::string_view)>& onPerformApi,
    const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, int apiTimeOutMs)
{
    long statusCode = 0;
    std::string responseBody;
    try {
        auto api = session.api;
        json payload = buildAgentPayload(session, tools);
        // 保留已有模型档位规则，再把 generateContent 参数映射成 Interactions 的新规范。
        const json extraBody = api.extraBody;
        api.extraBody = json::object();
        applyApiPayloadOptions(payload, api);
        if (api.protocol == ApiProtocol::Gemini && api.agentGeminiInteractions) {
            payload["model"] = api.modelName;
            if (payload.contains("generationConfig")) {
                const auto& config = payload.at("generationConfig");
                if (config.contains("thinkingConfig")) {
                    const auto& thinking = config.at("thinkingConfig");
                    // Interactions 不使用旧的 thinkingBudget，2.5 等旧型号应选择 generateContent。
                    if (thinking.contains("thinkingLevel")) payload["generation_config"]["thinking_level"] = str2Lower(thinking.at("thinkingLevel").get<std::string>());
                }
                payload.erase("generationConfig");
            }
            payload["generation_config"]["thinking_summaries"] = "auto";
        }
        if (api.protocol == ApiProtocol::Claude && api.agentNativeAutoCompaction) {
            // 原生自动压缩目前需要 beta 头，只在用户开启后启用；其它请求不受影响。
            auto& beta = api.extraHeaders["anthropic-beta"];
            if (!beta.empty()) beta += ',';
            beta += "compact-2026-01-12";
        }
        if (extraBody.is_object()) {
            for (auto it = extraBody.begin(); it != extraBody.end(); ++it) payload[it.key()] = it.value();
        }
        const std::string body = onPerformApi ? onPerformApi(payload.dump()) : payload.dump();
        ApiResponse response = sendApiHttpRequest(body, api,
            api.protocol == ApiProtocol::Gemini && api.agentGeminiInteractions ? interactionApiUrl(api) : cvt2RequestApiUrl(api),
            controller, logger, apiTimeOutMs);
        if (!response.content) return {std::unexpected(std::move(response.content.error()))};
        statusCode = 200;
        responseBody = std::move(*response.content);
        auto parsed = parseApiResponse(responseBody, api.protocol, api.protocol == ApiProtocol::Gemini && api.agentGeminiInteractions);
        if (!parsed) {
            parsed.error().statusCode = statusCode;
            return {std::unexpected(std::move(parsed.error()))};
        }
        return {parseAgentReply(*parsed, session)};
    }
    catch (const std::exception& e) {
        return {std::unexpected(makeApiError(statusCode == 200 ? ApiErrorType::ResponseParse : ApiErrorType::Unknown,
            e.what(), std::move(responseBody), statusCode))};
    }
}
