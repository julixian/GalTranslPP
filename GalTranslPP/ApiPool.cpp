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

void handleApiError(const ApiError& error, const std::unique_ptr<ApiPool>& apiPool, const TranslationApi& currentApi,
    const std::string& logPrefix, const fs::path& relFilePath, const std::string& apiStrategy,
    const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger,
    int& requestCount, bool checkQuota)
{
    // API 层已经完成分类，这里只执行重试、健康记录和日志策略，不再匹配错误文本。
    const bool badApi = error.type == ApiErrorType::ModelUnavailable ||
        (checkQuota && error.type == ApiErrorType::InvalidKeyOrQuota);
    const bool rateLimited = error.type == ApiErrorType::RateLimit;
    int sleepSeconds = badApi ? 0 : 2;
    if (rateLimited) {
        std::mt19937 gen(std::random_device{}());
        sleepSeconds = std::uniform_int_distribution<>(1, 64)(gen);
    }

    std::string actionMessage;
    if (badApi) {
        actionMessage = gppTr("handleApiError", "Api key [%1] 短期内多次报告将从池中移除")
            .arg(maskApikey(currentApi.apikey)).toStdString();
    }
    else {
        actionMessage = gppTr("handleApiError", "等待 %1 秒后重新请求").arg(sleepSeconds).toStdString();
    }
    std::string message = formatApiError(error, actionMessage);
    const auto logMessage = gppTr("handleApiError", "%1 [HTTP %2] %3")
        .arg(logPrefix).arg(error.statusCode).arg(message).toStdString();
    if (badApi) logger->error(logMessage);
    else logger->warn(logMessage);
    controller->recordRuntimeTransError(RuntimeTransErrorEvent{
        .kind = "api",
        .level = badApi ? "error" : "warning",
        .message = std::move(message),
        .filename = wide2Ascii(relFilePath),
        .requestCount = badApi || rateLimited ? -1 : requestCount + 1,
        .model = makeTransby(currentApi.apikey, currentApi.modelName),
        .sleepSeconds = badApi ? -1.0 : (double)sleepSeconds
    });

    if (badApi) {
        apiPool->reportProblem(currentApi);
        return;
    }
    // 限流不消耗请求次数；其它错误仍按既有策略计数并允许 fallback 调整顺序。
    if (!rateLimited) {
        ++requestCount;
        if (apiStrategy == "fallback") {
            if (const auto apikey = apiPool->resortTokens()) {
                logger->warn(gppTr("handleApiError", "%1 将切换到下一个 Api key: %2")
                    .arg(logPrefix).arg(*apikey).toStdString());
            }
        }
    }
    if (!controller->shouldStop()) std::this_thread::sleep_for(std::chrono::seconds(sleepSeconds));
}
