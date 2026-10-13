module;

#include "GPPMacros.hpp"

module ApiTool;

NAMESPACE_BEGIN(gpp)

// SSE 已由 HTTP 层完整收取；只在事件边界解析 JSON，不把网络分块当作事件。
json& streamSlot(json& array, const json& index)
{
    const int i = index.get<int>();
    if (i < 0 || i > 4096) {
        throw std::out_of_range("Invalid stream item index");
    }
    if (array.is_null()) {
        array = json::array();
    }
    return array[i];
}

void appendStreamString(json& target, const json& value)
{
    if (target.is_null()) {
        target = "";
    }
    target.get_ref<std::string&>() += value.get_ref<const std::string&>();
}

struct ApiStreamState
{
    ApiProtocol protocol;
    bool complete = false;
    json response = json::object();
    absl::btree_map<int, std::string> arguments;
    absl::btree_set<int> openBlocks;

    void chat(const json& event)
    {
        for (const auto& item : event.items()) {
            const auto& key = item.key();
            const auto& value = item.value();
            if (key != "choices") {
                response[key] = value;
            }
        }
        for (const auto& choice : event.value("choices", json::array())) {
            if (choice.value("index", 0) != 0) {
                continue;
            }
            auto& out = streamSlot(response["choices"], 0);
            out["index"] = 0;
            auto& message = out["message"];
            if (message.is_null()) {
                message = {{"role", "assistant"}, {"content", ""}};
            }
            const json delta = choice.value("delta", json::object());
            for (const auto& item : delta.items()) {
                const auto& key = item.key();
                const auto& value = item.value();
                if (value.is_null()) {
                    continue;
                }
                if (key == "tool_calls") {
                    for (const auto& call : value) {
                        auto& tool = streamSlot(message["tool_calls"], call.at("index"));
                        for (const auto& item2 : call.items()) {
                            const auto& field = item2.key();
                            const auto& fragment = item2.value();
                            if (field == "index") {
                                continue;
                            }
                            if (field == "function") {
                                for (const auto& item3 : fragment.items()) {
                                    appendStreamString(tool["function"][item3.key()], item3.value());
                                }
                            }
                            else if (field == "id") {
                                appendStreamString(tool[field], fragment);
                            }
                            else {
                                tool[field] = fragment;
                            }
                        }
                    }
                }
                else if (value.is_string() && key != "role") {
                    appendStreamString(message[key], value);
                }
                else {
                    message[key] = value;
                }
            }
            if (choice.contains("finish_reason") && !choice.at("finish_reason").is_null()) {
                out["finish_reason"] = choice.at("finish_reason");
                complete = true;
            }
        }
    }

    void claude(const std::string& type, const json& event)
    {
        if (type == "message_start") {
            response = event.at("message");
        }
        else if (type == "content_block_start") {
            const int index = event.at("index");
            streamSlot(response["content"], index) = event.at("content_block");
            openBlocks.insert(index);
        }
        else if (type == "content_block_delta") {
            const int index = event.at("index");
            auto& block = response.at("content").at(index);
            const auto& delta = event.at("delta");
            const auto kind = delta.at("type").get<std::string>();
            if (kind == "input_json_delta") {
                arguments[index] += delta.at("partial_json").get_ref<const std::string&>();
            }
            else if (kind == "citations_delta") {
                block["citations"].push_back(delta.at("citation"));
            }
            else {
                // text/thinking/signature/compaction 均保留，下一轮 Agent 历史需要这些原生字段。
                for (const auto& item : delta.items()) {
                    const auto& key = item.key();
                    const auto& value = item.value();
                    if (key != "type" && value.is_string()) {
                        appendStreamString(block[key], value);
                    }
                }
            }
        }
        else if (type == "content_block_stop") {
            openBlocks.erase(event.at("index").get<int>());
        }
        else if (type == "message_delta") {
            response.update(event.at("delta"));
            if (event.contains("usage")) {
                response["usage"].update(event.at("usage"));
            }
        }
        else if (type == "message_stop") {
            complete = response.contains("stop_reason") && !response.at("stop_reason").is_null();
        }
    }

    void gemini(const json& event)
    {
        for (const auto& item : event.items()) {
            const auto& key = item.key();
            const auto& value = item.value();
            if (key != "candidates") {
                response[key] = value;
            }
        }
        for (const auto& candidate : event.value("candidates", json::array())) {
            if (candidate.value("index", 0) != 0) {
                continue;
            }
            auto& out = streamSlot(response["candidates"], 0);
            for (const auto& item : candidate.items()) {
                const auto& key = item.key();
                const auto& value = item.value();
                if (key != "content") {
                    out[key] = value;
                }
            }
            if (candidate.contains("content")) {
                auto& content = out["content"];
                content["role"] = candidate.at("content").value("role", "model");
                auto& parts = content["parts"];
                if (parts.is_null()) {
                    parts = json::array();
                }
                for (const auto& part : candidate.at("content").value("parts", json::array())) {
                    // 文本块无需合并；保留每个 part 的 thought 标志、函数调用及 thoughtSignature。
                    parts.push_back(part);
                }
            }
            if (candidate.contains("finishReason")) {
                complete = true;
            }
        }
        if (event.contains("promptFeedback")) {
            const auto reason = event.at("promptFeedback").value("blockReason", "");
            if (!reason.empty() && reason != "BLOCK_REASON_UNSPECIFIED") {
                complete = true;
            }
        }
    }

    void interaction(const std::string& type, const json& event)
    {
        if (type == "interaction.created") {
            response = event.at("interaction");
        }
        else if (type == "step.start") {
            const int index = event.at("index");
            streamSlot(response["steps"], index) = event.at("step");
            openBlocks.insert(index);
        }
        else if (type == "step.delta") {
            const int index = event.at("index");
            auto& step = response.at("steps").at(index);
            const auto& delta = event.at("delta");
            const auto kind = delta.value("type", "");
            if (kind == "arguments_delta") {
                arguments[index] += delta.at("arguments").get_ref<const std::string&>();
            }
            else if (delta.contains("arguments_delta")) {
                arguments[index] += delta.at("arguments_delta").get_ref<const std::string&>();
            }
            else if (kind == "arguments") {
                arguments[index] += delta.at("partial_arguments").get_ref<const std::string&>();
            }
            else if (kind == "thought_summary" && delta.contains("content")) {
                step["summary"].push_back(delta.at("content"));
            }
            else if (delta.value("type", "") == "text" && step.value("type", "") == "model_output") {
                auto& content = step["content"];
                if (content.is_null()) {
                    content = json::array();
                }
                if (content.empty() || content.back().value("type", "") != "text") {
                    content.push_back({{"type", "text"}, {"text", ""}});
                }
                appendStreamString(content.back()["text"], delta.at("text"));
                if (delta.contains("annotations")) {
                    for (const auto& annotation : delta.at("annotations")) {
                        content.back()["annotations"].push_back(annotation);
                    }
                }
            }
            else {
                for (const auto& item : delta.items()) {
                    const auto& key = item.key();
                    const auto& value = item.value();
                    if (key == "type") {
                        continue;
                    }
                    if (value.is_string()) {
                        appendStreamString(step[key], value);
                    }
                    else if (value.is_array()) {
                        for (const auto& item2 : value) {
                            step[key].push_back(item2);
                        }
                    }
                    else {
                        step[key] = value;
                    }
                }
            }
        }
        else if (type == "step.stop") {
            const int index = event.at("index");
            if (event.contains("step")) {
                response.at("steps").at(index) = event.at("step");
            }
            openBlocks.erase(index);
        }
        else if (type == "interaction.completed" || type == "interaction.requires_action" || type == "interaction.failed") {
            if (event.contains("interaction")) {
                response.update(event.at("interaction"));
            }
            if (!event.contains("interaction") || !event.at("interaction").contains("status")) {
                response["status"] = type.substr(std::string_view("interaction.").size());
            }
            complete = true;
        }
    }

    void accept(const std::string& name, const std::string& data)
    {
        if (data == "[DONE]") {
            return; // 必须先收到协议完成状态，单独的 DONE 不使半截回复成功。
        }
        const auto event = json::parse(data);
        const auto type = event.value("event_type", event.value("type", name));
        if (type == "error" || (event.contains("error") && !event.at("error").is_null())) {
            throw makeApiError(ApiErrorType::Unknown, {}, event.dump());
        }
        switch (protocol)
        {
        case ApiProtocol::OpenAI:
            chat(event);
            break;
        case ApiProtocol::Claude:
            claude(type, event);
            break;
        case ApiProtocol::Gemini:
            gemini(event);
            break;
        case ApiProtocol::GeminiInterAction:
            interaction(type, event);
            break;
        case ApiProtocol::OpenAIRes:
            // 最终事件携带完整 response，包括原生推理、压缩、工具调用和会话 id。
            if (type == "response.completed" || type == "response.incomplete" || type == "response.failed") {
                response = event.at("response");
                complete = true;
            }
            break;
        }
    }
};

std::expected<std::string, ApiError> collectApiStreamResponse(const std::string& rawResponse, ApiProtocol protocol)
{
    try {
        std::string_view remaining(rawResponse);
        if (remaining.starts_with("\xEF\xBB\xBF")) {
            remaining.remove_prefix(3);
        }
        ApiStreamState state{protocol};
        std::string name, data;
        bool hasData = false;
        while (!remaining.empty()) {
            const auto end = remaining.find_first_of("\r\n");
            // 未终止的最后一行不派发；SSE 以空行终止事件。
            if (end == std::string_view::npos) {
                break;
            }
            const auto line = remaining.substr(0, end);
            const size_t separator = remaining[end] == '\r' && end + 1 < remaining.size() && remaining[end + 1] == '\n' ? 2 : 1;
            remaining.remove_prefix(end + separator);
            if (line.empty()) {
                if (hasData) {
                    state.accept(name, data);
                }
                name.clear();
                data.clear();
                hasData = false;
            }
            else if (line.starts_with("data:") || line.starts_with("event:")) {
                const auto colon = line.find(':');
                auto value = line.substr(colon + 1);
                if (value.starts_with(' ')) {
                    value.remove_prefix(1);
                }
                if (line.starts_with("event:")) {
                    name = value;
                }
                else {
                    if (hasData) {
                        data += '\n';
                    }
                    data += value;
                    hasData = true;
                }
            }
        }
        if (!state.complete || hasData || !remaining.empty()) {
            return std::unexpected(makeApiError(ApiErrorType::Incomplete, "SSE stream ended before a complete response", rawResponse));
        }
        // 先检查拒答/截断，再解析完整工具参数；截断 JSON 不掩盖原始的 Incomplete 错误。
        auto checked = parseApiResponse(state.response.dump(), protocol);
        if (!checked) {
            checked.error().rawResponse = rawResponse;
            return std::unexpected(std::move(checked.error()));
        }
        if (!state.openBlocks.empty()) {
            return std::unexpected(makeApiError(ApiErrorType::Incomplete, "Unfinished stream content block", rawResponse));
        }
        for (const auto& [index, args] : state.arguments) {
            switch (protocol)
            {
            case ApiProtocol::Claude:
                state.response.at("content").at(index)["input"] = json::parse(args);
                break;
            case ApiProtocol::GeminiInterAction:
            {
                auto& block = state.response.at("steps").at(index);
                // step.stop/最终交互快照可携带完整参数，优先使用服务端完整值。
                if (!block.contains("arguments") || block.at("arguments").empty()) {
                    block["arguments"] = json::parse(args);
                }
                break;
            }
            default:
                break;
            }
        }
        return state.response.dump();
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
