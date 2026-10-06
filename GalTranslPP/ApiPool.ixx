module;

#include "GPPMacros.hpp"

export module ApiPool;

export import ApiTool;
export import ITranslator;

export NAMESPACE_BEGIN(gpp)

namespace fs = std::filesystem;

class ApiPool {
private:
    std::vector<TranslationApi> m_apis;
    std::shared_ptr<spdlog::logger> m_logger;
    std::chrono::steady_clock::time_point m_lastResortTime = std::chrono::steady_clock::time_point::min();
    std::mutex m_mutex;

    std::unique_ptr<std::mt19937> m_gen;

public:
    explicit ApiPool(const std::shared_ptr<spdlog::logger>& logger);

    void loadApis(const std::vector<TranslationApi>& apis);

    std::optional<TranslationApi> getApi(const std::string& apiStrategy);
    std::optional<TranslationApi> getFirstApi();
    bool containsApi(const TranslationApi& api);

    // 成功 resort 后返回将要使用的第一个 apikey
    std::optional<std::string> resortTokens();

    void reportProblem(const TranslationApi& badApi);

    bool isEmpty();
    size_t size();
};

// 普通翻译和两种 Agent 请求失败后调用；按 API 层已分类的错误记录日志/运行时错误、更新 API 健康与重试计数。
// 模型不可用或启用 checkQuota 后的无效 key/额度错误报告到池中，不增加 requestCount；限流随机等待且不计数。
// 其它错误增加 requestCount，允许 fallback 调整顺序并等待；本函数不重新解析响应，也不直接发起重试。
void handleApiError(const ApiError& error, const std::unique_ptr<ApiPool>& apiPool, const TranslationApi& currentApi,
    const std::string& logPrefix, const fs::path& relFilePath, const std::string& apiStrategy,
    const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger,
    int& requestCount, bool checkQuota);

NAMESPACE_END(gpp)
