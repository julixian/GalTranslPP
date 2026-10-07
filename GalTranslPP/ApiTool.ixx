module;

#include "GPPMacros.hpp"

export module ApiTool;

export import ITranslator;
export import Tool;

export NAMESPACE_BEGIN(gpp)

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
    // 以下选项仅供高级 Agent 的原生 API 流程使用。
    // off 不严格，all 全部严格，commit 仅严格约束 commit_translations。
    std::string agentStrictTools = "all";
    bool agentStateful = false;
    // 在高级 Agent 请求中启用服务端按 token 阈值自动压缩，需要模型和接口支持。
    bool agentNativeAutoCompaction = false;
    // 0 不指定阈值；需要显式阈值的 Responses 路径沿用默认 100000 tokens。
    int agentCompactThresholdTokens = 0;
    bool agentGeminiInteractions = false;
};

enum class ApiErrorType {
    Unknown,
    Transport,
    InvalidKeyOrQuota,
    ModelUnavailable,
    RateLimit,
    Refusal,
    JsonParse,
    ResponseParse,
    Incomplete
};

struct ApiError {
    ApiErrorType type = ApiErrorType::Unknown;
    std::string message;
    // 原始响应单独保存，日志和界面在展示时再决定如何拼接。
    std::string rawResponse;
    long statusCode = 0;
};

// API 层统一构造错误，分别保存本地说明、原始响应和 HTTP 状态；仅 Unknown 按状态码和错误文本推断类型。
// 本函数只返回错误对象；parseApiResponse 可在内部抛出该对象，但会在自己的 catch 中转成 unexpected。
ApiError makeApiError(ApiErrorType type, const std::string& detail = {}, std::string rawResponse = {}, long statusCode = 0);
// handleApiError 和 GUI 展示时拼接错误说明、后续动作与原始响应；空原始响应显示 [GPP.响应为空]。
// 不推断错误类型，也不记录日志或决定重试。
std::string formatApiError(const ApiError& error, const std::string& actionMessage = {});
// 普通文本提取与高级 Agent 共用的响应检查：解析 JSON，检查服务端 error、协议拒答、截断和完成状态。
// 不校验译文或工具参数；成功的 JSON 再交给 parseApiContent / parseAdvancedAgentReply 各自提取。
// 内部 ApiError、JSON 解析异常和其它 std::exception 均转为 unexpected<ApiError>，保留原始响应。
// 调用方补 HTTP 状态并向上返回；是否重试由翻译流程决定。
std::expected<json, ApiError> parseApiResponse(const std::string& rawResponse, ApiProtocol protocol, bool geminiInteractions = false);

struct ApiResponse {
    std::expected<std::string, ApiError> content;
};

// ApiPool 的身份匹配与高级 Agent 的会话复用共用：比较端点、凭据、模型及请求选项，不比较健康计数。
bool isSameApi(const TranslationApi& lhs, const TranslationApi& rhs);

// 以下类型和会话操作仅供高级 Agent 使用；普通 Agent 使用 ApiResponse 返回的文本动作协议。
struct AdvancedAgentApiToolCall {
    std::string id;
    std::string name;
    // 保留原始参数，单个工具的解析错误也能按调用 id 回填给模型。
    json arguments;
};

struct AdvancedAgentApiReply {
    std::string text;
    std::vector<AdvancedAgentApiToolCall> calls;
};

struct AdvancedAgentApiResponse {
    std::expected<AdvancedAgentApiReply, ApiError> content;
};

struct AdvancedAgentApiSession {
    TranslationApi api;
    std::string systemPrompt;
    json history = json::array();
    std::string previousId;
    size_t sentCount = 0;
};

// translateAdvancedBatch 按协议向本地历史追加用户输入。
void appendAdvancedAgentUserMessage(AdvancedAgentApiSession& session, const std::string& text);
// translateAdvancedBatch 将 {id, name, result} 工具结果转成各协议的回填消息，追加到本地历史。
void appendAdvancedAgentToolResults(AdvancedAgentApiSession& session, const json& results);
// 高级 Agent 的单次请求入口，由 translateAdvancedBatch 调用：构造原生工具请求 -> 应用 API 选项/插件
// -> sendApiHttpRequest -> parseApiResponse -> parseAdvancedAgentReply；响应解析成功后更新会话历史和续接位置。
// enhanceJailbreak 开启时，在本次请求末尾原样追加 assistant 自然语言预填充，不写入会话、不检查协议兼容性。
// 允许纯工具响应正文为空，不在这里解析工具参数或提交译文。std::exception 转为 ApiError：
// 拿到 HTTP 200 原始响应后归为 ResponseParse，此前归为 Unknown；已有传输/协议错误直接返回。
// 本函数不重试，调用方通过 handleApiError 处理失败并决定下一次请求。
AdvancedAgentApiResponse performAdvancedAgentApiRequest(AdvancedAgentApiSession& session, const json& tools, bool enhanceJailbreak,
    const std::function<std::string(std::string_view)>& onPerformApi,
    const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, int apiTimeOutMs);

struct ApiTestResponse {
    std::expected<std::string, ApiError> content;
    std::string requestBody;
};

struct ApiModelListResponse {
    std::expected<std::vector<std::string>, ApiError> models;
};


// 配置加载等调用，将协议名称转换成枚举；未知名称抛 invalid_argument，由调用方的配置错误处理接住。
ApiProtocol parseApiProtocol(std::string_view protocol);
std::string apiProtocolToString(ApiProtocol protocol);

std::string cvt2StdApiUrl(const std::string& url, ApiProtocol protocol);

std::string cvt2RequestApiUrl(const TranslationApi& api);
// 普通/高级请求入口及连接测试共用：原地转换协议字段，应用模型、采样、思考规则，再合并 extraBody。
// 请求结构由调用方构造；不干预预填充，不检查随后插件返回的最终 JSON。
void applyApiPayloadOptions(json& payload, const TranslationApi& api);
// 普通 sendApiRequest 与高级请求共用的非流式 HTTP POST；成功的 content 是原始响应体，还未解析 JSON。
// 检查传输错误、非 200 状态和空响应体，失败返回 ApiError；进度回调响应 controller 的停止请求。
// 直接发送最终请求体，不再检查其 JSON 结构。
ApiResponse sendApiHttpRequest(const std::string& payloadStr, const TranslationApi& api, const std::string& requestUrl,
    const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, int apiTimeOutMs);

// GUI 的模型列表查询：检查 GET 传输/HTTP 错误及服务端 error，提取并去重模型名称。
// 服务端 JSON 解析/字段异常转为 ApiError，由 GUI 展示查询结果。
ApiModelListResponse queryApiModels(const TranslationApi& api, int apiTimeOutMs);
// GUI 连接测试：构造测试消息 -> applyApiPayloadOptions -> sendApiRequest，返回请求体及原始响应/解析文本。
// 使用普通文本解析路径，不测试原生 Agent 工具；传输和解析错误通过 content 返回。
ApiTestResponse testApiConnection(const TranslationApi& api, int apiTimeOutMs);

// 普通 batch、普通 Agent、名字翻译和字典生成的单次请求入口：选项转换 -> 插件 -> sendApiRequest。
// 成功 content 为非空模型文本，译文格式和 Agent 动作交给各调用方继续解析。
// 捕获 std::exception 并返回 Unknown ApiError，已有传输/响应解析错误直接返回；不在这里重试。
// 调用方处理 content.error()，通常交给 handleApiError 后继续自己的请求循环。
ApiResponse performApiRequest(json& payload, const TranslationApi& api, const std::function<std::string(std::string_view)>& onPerformApi,
    const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, int threadId, int apiTimeOutMs);

NAMESPACE_END(gpp)
