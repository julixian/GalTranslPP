module;

#include "GPPMacros.hpp"

module ApiPool;

import Tool;

namespace fs = std::filesystem;

ApiPool::ApiPool(const std::shared_ptr<spdlog::logger>& logger) 
    : m_logger(logger),  m_gen(std::make_unique<std::mt19937>(std::random_device{}())) 
{
	
}

void ApiPool::loadApis(const std::vector<TranslationApi>& apis) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_apis.insert_range(m_apis.end(), apis);
    m_logger->info(gppTr("ApiPool.loadApis", "令牌池新加载 %1 个 Api keys， 现共有 %2 个Api keys")
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

void ApiPool::resortTokens() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_apis.size() > 1) {
        std::ranges::rotate(m_apis, m_apis.begin() + 1);
    }
}

void ApiPool::reportProblem(const TranslationApi& badApi) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = std::ranges::find_if(m_apis, [&](const TranslationApi& api)
        {
            return api.apikey == badApi.apikey;
        });
    if (it == m_apis.end()) {
        return;
    }
    const auto durationInSec = std::chrono::duration_cast<std::chrono::seconds>
        (std::chrono::steady_clock::now() - it->lastReportTime).count();
    if (durationInSec < 10) {
        ++(it->reportCount);
    }
    else {
        it->reportCount = 1;
    }
    it->lastReportTime = std::chrono::steady_clock::now();
    if (it->reportCount >= 30) {
        m_logger->warn(gppTr("ApiPool.reportProblem", "Api key [%1] 已被标记为不可用")
            .arg(maskApiKey(it->apikey))
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

bool checkResponse(ApiResponse& response, const std::unique_ptr<ApiPool>& apiPool, const TranslationApi& currentApi,
    const std::string& logPrefix, const fs::path& relFilePath, const std::string& apiStrategy,
    const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger,
    int& requestCount, bool checkQuota)
{
    const std::string filename = wide2Ascii(relFilePath);
    const std::string prefix = gppTr("checkResponse", "%1 [HTTP %2]")
        .arg(logPrefix)
        .arg(response.statusCode)
        .toStdString();

    if (response.statusCode == 200) {
        if (currentApi.stream) {
            return true;
        }

        const std::expected<std::string, std::string> extractedContent = extractApiResponseContent(response.content, currentApi.protocol);
        if (extractedContent.has_value()) {
            response.content = extractedContent.value();
            return true;
        }

        logger->warn(gppTr("checkResponse", "%1 Api 响应 JSON 解析失败。错误: %2，原始响应:\n%3")
            .arg(prefix)
            .arg(extractedContent.error())
            .arg(response.content.empty()
                ? gppTr("checkResponse", "空").toStdString()
                : response.content)
            .toStdString());
        controller->recordRuntimeTransError(RuntimeTransErrorEvent{
            .kind = "api",
            .level = "warning",
            .message = gppTr("checkResponse", "Api 响应 JSON 解析失败: %1")
                .arg(extractedContent.error())
                .toStdString(),
            .filename = filename,
            .requestCount = requestCount + 1,
            .model = currentApi.modelName,
            .sleepSeconds = 2.0
        });
        ++requestCount;

        if (apiStrategy == "fallback" && apiPool->size() > 1) {
            apiPool->resortTokens();
            logger->warn(gppTr("checkResponse", "%1 切换到下一个 Api key")
                .arg(logPrefix)
                .toStdString());
        }
        if (!controller->shouldStop()) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        return false;
    }

    // response.statusCode != 200 就是有错误
    const std::string errorMessageLower = str2Lower(response.content);

    // 额度用尽 (Quota)
    if (
        checkQuota &&
        (errorMessageLower.contains("quota") ||
            errorMessageLower.contains("invalid tokens"))
        )
    {
        logger->error(gppTr("checkResponse", "%1 Api key [%2] 疑似额度用尽，短期内多次报告将从池中移除。原始响应:\n%3")
            .arg(prefix)
            .arg(maskApiKey(currentApi.apikey))
            .arg(response.content.empty()
                ? gppTr("checkResponse", "空").toStdString()
                : response.content)
            .toStdString());
        controller->recordRuntimeTransError(RuntimeTransErrorEvent{
            .kind = "api",
            .level = "error",
            .message = gppTr("checkResponse", "Api key 疑似额度用尽: %1")
                .arg(response.content.empty() ? gppTr("checkResponse", "响应为空").toStdString() : response.content)
                .toStdString(),
            .filename = filename,
            .model = currentApi.modelName
        });
        apiPool->reportProblem(currentApi);
        // 不需要增加 requestCount
        return false;
    }

    // key 没有这个模型
    if (errorMessageLower.contains("no available")) {
        logger->error(gppTr("checkResponse", "%1 Api key [%2] 没有可用模型，短期内多次报告将从池中移除。原始响应:\n%3")
            .arg(prefix)
            .arg(maskApiKey(currentApi.apikey))
            .arg(response.content.empty()
                ? gppTr("checkResponse", "空").toStdString()
                : response.content)
            .toStdString());
        controller->recordRuntimeTransError(RuntimeTransErrorEvent{
            .kind = "api",
            .level = "error",
            .message = gppTr("checkResponse", "Api key 没有模型 %1: %2")
                .arg(currentApi.modelName)
                .arg(response.content.empty() ? gppTr("checkResponse", "响应为空").toStdString() : response.content)
                .toStdString(),
            .filename = filename,
            .model = currentApi.modelName
        });
        apiPool->reportProblem(currentApi);
        return false;
    }

    // 频率限制 (429) 或其他可再次请求错误
    // 状态码 429 是最明确的信号
    if (response.statusCode == 429 || errorMessageLower.contains("rate limit") ||
        errorMessageLower.contains("try again") || errorMessageLower.contains("饱和"))
    {
        // 429 也不加 requestCount
        // 实现指数退避与抖动
        const int maxSleepSeconds = (int)std::pow(2, 6);
        const int sleepSeconds = std::rand() % maxSleepSeconds;
        logger->warn(gppTr("checkResponse", "%1 遇到频率限制或可再次请求错误，将等待 %2 秒后重新请求。原始响应:\n%3")
            .arg(prefix)
            .arg(sleepSeconds)
            .arg(response.content.empty()
                ? gppTr("checkResponse", "空").toStdString()
                : response.content)
            .toStdString());
        controller->recordRuntimeTransError(RuntimeTransErrorEvent{
            .kind = "api",
            .level = "warning",
            .message = gppTr("checkResponse", "遇到频率限制或可再次请求错误: %1")
                .arg(response.content.empty() ? gppTr("checkResponse", "响应为空").toStdString() : response.content)
                .toStdString(),
            .filename = filename,
            .model = currentApi.modelName,
            .sleepSeconds = (double)sleepSeconds
        });
        if (sleepSeconds > 0 && !controller->shouldStop()) {
            std::this_thread::sleep_for(std::chrono::seconds(sleepSeconds));
        }
        return false;
    }

    // 其他无法识别的硬性错误
    logger->warn(gppTr("checkResponse", "%1 遇到未知 Api 错误，原始响应:\n%2")
        .arg(prefix)
        .arg(response.content.empty()
            ? gppTr("checkResponse", "空").toStdString()
            : response.content)
        .toStdString());
    controller->recordRuntimeTransError(RuntimeTransErrorEvent{
        .kind = "api",
        .level = "warning",
        .message = gppTr("checkResponse", "遇到未知 Api 错误: %1")
                .arg(response.content.empty() ? gppTr("checkResponse", "响应为空").toStdString() : response.content)
                .toStdString(),
        .filename = filename,
        .requestCount = requestCount + 1,
        .model = currentApi.modelName,
        .sleepSeconds = 2.0
    });
    ++requestCount;

    if (apiStrategy == "fallback" && apiPool->size() > 1) {
        apiPool->resortTokens();
        logger->warn(gppTr("checkResponse", "%1 将切换到下一个 Api key")
            .arg(logPrefix)
            .toStdString());
    }
    if (!controller->shouldStop()) {
        std::this_thread::sleep_for(std::chrono::seconds(2)); // 简单等待
    }
    return false;
}
