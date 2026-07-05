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
    m_logger->info(gppTr("APIPool.loadApis", "令牌池新加载 %1 个 API keys， 现共有 %2 个API keys")
        .arg(apis.size())
        .arg(m_apis.size())
        .toStdString());
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
        m_logger->warn(gppTr("APIPool.reportProblem", "API key [%1] 已被标记为不可用。")
            .arg(it->apikey)
            .toStdString());
        m_apis.erase(it);
    }
}

bool APIPool::isEmpty() {
    std::lock_guard<std::mutex> lock(m_mutex); // 加锁
    return m_apis.empty();
}

namespace
{
    std::string apiLogPrefix(int threadId, const fs::path& relInputPath, const TranslationApi& currentAPI, long statusCode)
    {
        return gppTr("APIPool.apiLogPrefix", "[线程 %1] [文件 %2] [模型 %3] [HTTP %4]")
            .arg(threadId)
            .arg(wide2Ascii(relInputPath))
            .arg(currentAPI.modelName)
            .arg(statusCode)
            .toStdString();
    }

    std::string apiMessage(const ApiResponse& response, std::string_view fallback)
    {
        if (!response.content.empty()) {
            return response.content;
        }
        return std::string(fallback);
    }
}

bool checkResponse(ApiResponse& response, const std::unique_ptr<APIPool>& apiPool, const TranslationApi& currentAPI,
    const std::filesystem::path& relInputPath, const std::string& apiStrategy, 
    const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger,
    int& retryCount, int threadId, bool m_checkQuota)
{
    response.success = false;
    const std::string prefix = apiLogPrefix(threadId, relInputPath, currentAPI, response.statusCode);

    if (response.statusCode == 200) {
        if (currentAPI.stream) {
            response.success = true;
            return true;
        }

        try {
            response.content = json::parse(response.content)["choices"][0]["message"]["content"].get<std::string>();
            response.success = true;
            return true;
        }
        catch (const json::exception& e) {
            ++retryCount;
            logger->warn(gppTr("checkResponse", "%1 API 响应 JSON 解析失败，进行第 %2 次重试。错误: %3，原始响应: %4")
                .arg(prefix)
                .arg(retryCount)
                .arg(e.what())
                .arg(truncateUtf8Prefix(response.content, 4000))
                .toStdString());
            controller->recordRuntimeError(RuntimeErrorEvent{
                .kind = "api",
                .level = "warning",
                .message = gppTr("checkResponse", "API 响应 JSON 解析失败: %1")
                    .arg(e.what())
                    .toStdString(),
                .filename = wide2Ascii(relInputPath),
                .retryCount = retryCount,
                .model = currentAPI.modelName,
                .sleepSeconds = 2.0
            });
            if (apiStrategy == "fallback") {
                logger->warn(gppTr("checkResponse", "[线程 %1] 将切换到下一个 API key(如果有多个API key的话)")
                    .arg(threadId)
                    .toStdString());
                apiPool->resortTokens();
            }
            if (!controller->shouldStop()) {
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            return false;
        }
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
        logger->error(gppTr("checkResponse", "%1 API key [%2] 疑似额度用尽，短期内多次报告将从池中移除。响应: %3")
            .arg(prefix)
            .arg(currentAPI.apikey)
            .arg(truncateUtf8Prefix(response.content, 4000))
            .toStdString());
        controller->recordRuntimeError(RuntimeErrorEvent{
            .kind = "api",
            .level = "error",
            .message = gppTr("checkResponse", "API key 疑似额度用尽: %1")
                .arg(apiMessage(response, gppTr("checkResponse", "响应为空").toStdString()))
                .toStdString(),
            .filename = wide2Ascii(relInputPath),
            .model = currentAPI.modelName
        });
        apiPool->reportProblem(currentAPI);
        // 不需要增加 retryCount
        return false;
    }

    // key 没有这个模型
    if (lowerErrorMsg.contains("no available")) {
        logger->error(gppTr("checkResponse", "%1 API key [%2] 没有可用模型，短期内多次报告将从池中移除。响应: %3")
            .arg(prefix)
            .arg(currentAPI.apikey)
            .arg(truncateUtf8Prefix(response.content, 4000))
            .toStdString());
        controller->recordRuntimeError(RuntimeErrorEvent{
            .kind = "api",
            .level = "error",
            .message = gppTr("checkResponse", "API key 没有模型 %1: %2")
                .arg(currentAPI.modelName)
                .arg(apiMessage(response, gppTr("checkResponse", "响应为空").toStdString()))
                .toStdString(),
            .filename = wide2Ascii(relInputPath),
            .model = currentAPI.modelName
        });
        apiPool->reportProblem(currentAPI);
        return false;
    }

    // 情况二：频率限制 (429) 或其他可重试错误
    // 状态码 429 是最明确的信号
    if (response.statusCode == 429 || lowerErrorMsg.contains("rate limit") || lowerErrorMsg.contains("try again") || lowerErrorMsg.contains("饱和")) {
        // 429 也不加 retryCount
        // 实现指数退避与抖动
        const int maxSleepSeconds = (int)std::pow(2, 6);
        const int sleepSeconds = std::rand() % maxSleepSeconds;
        logger->warn(gppTr("checkResponse", "%1 遇到频率限制或可重试错误，将等待 %2 秒后重试。响应: %3")
            .arg(prefix)
            .arg(sleepSeconds)
            .arg(truncateUtf8Prefix(apiMessage(response, gppTr("checkResponse", "空")
                .toStdString()), 4000))
            .toStdString());
        controller->recordRuntimeError(RuntimeErrorEvent{
            .kind = "api",
            .level = "warning",
            .message = gppTr("checkResponse", "遇到频率限制或可重试错误: %1")
                .arg(apiMessage(response, gppTr("checkResponse", "响应为空").toStdString()))
                .toStdString(),
            .filename = wide2Ascii(relInputPath),
            .model = currentAPI.modelName,
            .sleepSeconds = (double)sleepSeconds
        });
        if (sleepSeconds > 0 && !controller->shouldStop()) {
            std::this_thread::sleep_for(std::chrono::seconds(sleepSeconds));
        }
        return false;
    }

    // 其他无法识别的硬性错误
    ++retryCount;
    logger->warn(gppTr("checkResponse", "%1 遇到未知 API 错误，进行第 %2 次重试。响应: %3")
        .arg(prefix)
        .arg(retryCount)
        .arg(truncateUtf8Prefix(apiMessage(response, gppTr("checkResponse", "空")
            .toStdString()), 4000))
        .toStdString());
    controller->recordRuntimeError(RuntimeErrorEvent{
        .kind = "api",
        .level = "warning",
        .message = apiMessage(response, gppTr("checkResponse", "未知 API 错误").toStdString()),
        .filename = wide2Ascii(relInputPath),
        .retryCount = retryCount,
        .model = currentAPI.modelName,
        .sleepSeconds = 2.0
    });
    if (apiStrategy == "fallback") {
        logger->warn(gppTr("checkResponse", "[线程 %1] 将切换到下一个 API key(如果有多个API key的话)")
            .arg(threadId)
            .toStdString());
        apiPool->resortTokens();
    }
    if (!controller->shouldStop()) {
        std::this_thread::sleep_for(std::chrono::seconds(2)); // 简单等待
    }
    return false;
}
