module;

#include "GPPMacros.hpp"

module ApiPool;

import NormalJsonTranslatorHelperTool;
import Tool;

namespace fs = std::filesystem;

ApiPool::ApiPool(const std::shared_ptr<spdlog::logger>& logger) 
    : m_logger(logger),  m_gen(std::make_unique<std::mt19937>(std::random_device{}())) 
{
	
}

void ApiPool::loadApis(const std::vector<TranslationApi>& apis) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_apis.insert_range(m_apis.end(), apis);
    m_logger->info(gppTr("ApiPool.loadApis", "令牌池新加载 %1 个 Api keys， 现共有 %2 个 Api keys")
        .arg(apis.size())
        .arg(m_apis.size())
        .toStdString());
}

std::optional<TranslationApi> ApiPool::getApi(const std::string& apiStrategy) {
    if (apiStrategy == "fallback") {
        return getFirstApi();
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_apis.empty()) {
        return std::nullopt; // 没有可用的 token
    }
    if (m_apis.size() == 1) {
        return m_apis[0];
    }
    // 生成一个随机索引
    std::uniform_int_distribution<> distrib(0, (int)m_apis.size() - 1);
    const int index = distrib(*m_gen);
    return m_apis[index];
}

std::optional<TranslationApi> ApiPool::getFirstApi() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_apis.empty()) {
        return std::nullopt;
    }
    return m_apis.front();
}

bool ApiPool::containsApi(const TranslationApi& api) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::ranges::any_of(m_apis, [&](const TranslationApi& item) { return isSameApi(item, api); });
}

std::optional<std::string> ApiPool::resortTokens() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_apis.size() > 1) {
        const auto now = std::chrono::steady_clock::now();
        const auto durationInSec = std::chrono::duration_cast<std::chrono::seconds>
            (now - m_lastResortTime).count();
        m_lastResortTime = now;
        if (durationInSec > 10) {
            std::ranges::rotate(m_apis, m_apis.begin() + 1);
            return maskApikey(m_apis.front().apikey);
        }
    }
    return std::nullopt;
}

void ApiPool::reportProblem(const TranslationApi& badApi) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = std::ranges::find_if(m_apis, [&](const TranslationApi& api)
        {
            return isSameApi(api, badApi);
        });
    if (it == m_apis.end()) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const auto durationInSec = std::chrono::duration_cast<std::chrono::seconds>
        (now - it->lastReportTime).count();
    it->lastReportTime = now;
    if (durationInSec < 10) {
        ++(it->reportCount);
    }
    else {
        it->reportCount = 1;
    }
    if (it->reportCount >= 30) {
        m_logger->warn(gppTr("ApiPool.reportProblem", "Api key [%1] 已被标记为不可用")
            .arg(maskApikey(it->apikey))
            .toStdString());
        m_apis.erase(it);
    }
}

bool ApiPool::isEmpty() {
    std::lock_guard<std::mutex> lock(m_mutex); // 加锁
    return m_apis.empty();
}

size_t ApiPool::size() {
    std::lock_guard<std::mutex> lock(m_mutex); // 加锁
    return m_apis.size();
}

void inferAndRecordApiError(const ApiResponse& response, const std::unique_ptr<ApiPool>& apiPool, const TranslationApi& currentApi,
    const std::string& logPrefix, const fs::path& relFilePath, const std::string& apiStrategy,
    const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger,
    int& requestCount, bool checkQuota)
{
    const std::string filename = wide2Ascii(relFilePath);
    const std::string prefix = gppTr("inferAndRecordApiError", "%1 [HTTP %2]")
        .arg(logPrefix)
        .arg(response.statusCode)
        .toStdString();

    const std::string& error = response.content.error();
    const std::string errorMessageLower = str2Lower(error);

    // 无效或额度用尽
    if (
        checkQuota &&
        (errorMessageLower.contains("quota") ||
            errorMessageLower.contains("invalid token"))
        )
    {
        logger->error(gppTr("inferAndRecordApiError", "%1 Api key [%2] 疑似无效或额度用尽，短期内多次报告将从池中移除。原始响应:\n%3")
            .arg(prefix)
            .arg(maskApikey(currentApi.apikey))
            .arg(error)
            .toStdString());
        controller->recordRuntimeTransError(RuntimeTransErrorEvent{
            .kind = "api",
            .level = "error",
            .message = gppTr("inferAndRecordApiError", "Api key 疑似失效: %1")
                .arg(error)
                .toStdString(),
            .filename = filename,
            .model = makeTransby(currentApi.apikey, currentApi.modelName)
        });
        apiPool->reportProblem(currentApi);
        // 不需要增加 requestCount
        return;
    }

    // key 没有这个模型
    if (errorMessageLower.contains("no available") || errorMessageLower.contains("no access")) {
        logger->error(gppTr("inferAndRecordApiError", "%1 Api key [%2] 没有可用模型，短期内多次报告将从池中移除。原始响应:\n%3")
            .arg(prefix)
            .arg(maskApikey(currentApi.apikey))
            .arg(error)
            .toStdString());
        controller->recordRuntimeTransError(RuntimeTransErrorEvent{
            .kind = "api",
            .level = "error",
            .message = gppTr("inferAndRecordApiError", "Api key 没有模型 %1: %2")
                .arg(currentApi.modelName)
                .arg(error)
                .toStdString(),
            .filename = filename,
            .model = makeTransby(currentApi.apikey, currentApi.modelName)
        });
        apiPool->reportProblem(currentApi);
        return;
    }

    // 频率限制或其他可再次请求错误
    // 状态码 429 是最明确的信号
    if (response.statusCode == 429 || errorMessageLower.contains("rate limit") ||
        errorMessageLower.contains("try again") || errorMessageLower.contains("饱和"))
    {
        // 429 也不加 requestCount
        // 实现指数退避与抖动
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(1, (int)std::pow(2, 6));
        const int sleepSeconds = distrib(gen);
        logger->warn(gppTr("inferAndRecordApiError", "%1 遇到频率限制或可再次请求错误，将等待 %2 秒后重新请求。原始响应:\n%3")
            .arg(prefix)
            .arg(sleepSeconds)
            .arg(error.empty()
                ? gppTr("inferAndRecordApiError", "空").toStdString()
                : error)
            .toStdString());
        controller->recordRuntimeTransError(RuntimeTransErrorEvent{
            .kind = "api",
            .level = "warning",
            .message = gppTr("inferAndRecordApiError", "遇到频率限制或可再次请求错误: %1")
                .arg(error.empty() ? gppTr("inferAndRecordApiError", "响应为空").toStdString() : error)
                .toStdString(),
            .filename = filename,
            .model = makeTransby(currentApi.apikey, currentApi.modelName),
            .sleepSeconds = (double)sleepSeconds
        });
        if (sleepSeconds > 0 && !controller->shouldStop()) {
            std::this_thread::sleep_for(std::chrono::seconds(sleepSeconds));
        }
        return;
    }

    // 其他无法识别的硬性错误
    logger->warn(gppTr("inferAndRecordApiError", "%1 遇到未知 Api 错误，原始响应:\n%2")
        .arg(prefix)
        .arg(error.empty()
            ? gppTr("inferAndRecordApiError", "空").toStdString()
            : error)
        .toStdString());
    controller->recordRuntimeTransError(RuntimeTransErrorEvent{
        .kind = "api",
        .level = "warning",
        .message = gppTr("inferAndRecordApiError", "遇到未知 Api 错误: %1")
                .arg(error.empty() ? gppTr("inferAndRecordApiError", "响应为空").toStdString() : error)
                .toStdString(),
        .filename = filename,
        .requestCount = requestCount + 1,
        .model = makeTransby(currentApi.apikey, currentApi.modelName),
        .sleepSeconds = 2.0
    });
    ++requestCount;

    if (apiStrategy == "fallback") {
        if (const std::optional<std::string> apikeyOpt = apiPool->resortTokens()) {
            logger->warn(gppTr("inferAndRecordApiError", "%1 将切换到下一个 Api key: %2")
                .arg(logPrefix)
                .arg(apikeyOpt.value())
                .toStdString());
        }
    }
    if (!controller->shouldStop()) {
        std::this_thread::sleep_for(std::chrono::seconds(2)); // 简单等待
    }
}
