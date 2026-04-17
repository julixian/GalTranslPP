module;

#include "GPPMacros.hpp"

module APIPool;

import Tool;

namespace fs = std::filesystem;

APIPool::APIPool(const std::shared_ptr<spdlog::logger>& logger) 
    : m_logger(logger), 
    m_gen(std::make_unique<std::mt19937>(std::random_device{}())) 
{
	
}

void APIPool::loadApis(const std::vector<TranslationApi>& apis) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_apis.insert(m_apis.end(), apis.begin(), apis.end());
    m_logger->info("令牌池新加载 {} 个 API keys， 现共有 {} 个API keys", apis.size(), m_apis.size());
}

std::optional<TranslationApi> APIPool::getApi() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_apis.empty()) {
        return std::nullopt; // 没有可用的 token
    }

    // 生成一个随机索引
    std::uniform_int_distribution<> distrib(0, (int)m_apis.size() - 1);
    const int index = distrib(*m_gen);

    return m_apis[index];
}

std::optional<TranslationApi> APIPool::getFirstApi() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_apis.empty()) {
        return std::nullopt;
    }

    return m_apis.front();
}

void APIPool::resortTokens() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_apis.size() > 1) {
        std::ranges::rotate(m_apis, m_apis.begin() + 1);
    }
}

void APIPool::reportProblem(const TranslationApi& badAPI) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto it = std::ranges::find_if(m_apis, [&](const TranslationApi& api)
        {
            return api.apikey == badAPI.apikey;
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
        m_logger->warn("API key [{}] 已被标记为不可用。", it->apikey);
        m_apis.erase(it);
    }
}

bool APIPool::isEmpty() {
    std::lock_guard<std::mutex> lock(m_mutex); // 加锁
    return m_apis.empty();
}

bool checkResponse(const ApiResponse& response, const std::unique_ptr<APIPool>& apiPool, const TranslationApi& currentAPI,
    const std::filesystem::path& relInputPath, const std::string& apiStrategy, 
    const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger,
    int& retryCount, int threadId, bool m_checkQuota)
{
    if (response.success) {
        return true;
    }

    std::string lowerErrorMsg = response.content;
    str2LowerInplace(lowerErrorMsg);

    // 情况一：额度用尽 (Quota)
    if (
        m_checkQuota &&
        (lowerErrorMsg.contains("quota") ||
            lowerErrorMsg.contains("invalid tokens"))
        )
    {
        logger->error("[线程 {}] API key [{}] 疑似额度用尽，短期内多次报告将从池中移除。", threadId, currentAPI.apikey);
        apiPool->reportProblem(currentAPI);
        // 不需要增加 retryCount
        return false;
    }

    // key 没有这个模型
    if (lowerErrorMsg.contains("no available")) {
        logger->error("[线程 {}] API key [{}] 没有 [{}] 模型，短期内多次报告将从池中移除。", threadId, currentAPI.apikey, currentAPI.modelName);
        apiPool->reportProblem(currentAPI);
        return false;
    }

    // 情况二：频率限制 (429) 或其他可重试错误
    // 状态码 429 是最明确的信号
    if (response.statusCode == 429 || lowerErrorMsg.contains("rate limit") || lowerErrorMsg.contains("try again")) {
        // 429 也不加 retryCount
        logger->warn("[线程 {}] [文件 {}] 遇到频率限制或可重试错误，进行退避等待...", threadId, wide2Ascii(relInputPath));

        // 实现指数退避与抖动
        const int maxSleepSeconds = (int)std::pow(2, 6);
        const int sleepSeconds = std::rand() % maxSleepSeconds;
        logger->debug("[线程 {}] [文件 {}] 将等待 {} 秒后重试...", threadId, wide2Ascii(relInputPath), sleepSeconds);
        if (sleepSeconds > 0 && !controller->shouldStop()) {
            std::this_thread::sleep_for(std::chrono::seconds(sleepSeconds));
        }
        return false;
    }

    // 其他无法识别的硬性错误
    ++retryCount;
    logger->warn("[线程 {}] [文件 {}] 遇到未知API错误，进行第 {} 次重试...", threadId, wide2Ascii(relInputPath), retryCount);
    if (apiStrategy == "fallback") {
        logger->warn("[线程 {}] 将切换到下一个 API key(如果有多个API key的话)", threadId);
        apiPool->resortTokens();
    }
    std::this_thread::sleep_for(std::chrono::seconds(2)); // 简单等待
    return false;
}
