module;

#include <spdlog/spdlog.h>

import std;
import Tool;
export module APIPool;

export {

    class APIPool {
    private:
        std::vector<TranslationAPI> m_apis;
        std::shared_ptr<spdlog::logger> m_logger;
        std::mutex m_mutex;

        std::random_device m_rd;
        std::mt19937 m_gen;

    public:
        APIPool();

        void setLogger(std::shared_ptr<spdlog::logger> logger) {
            m_logger = logger;
        }

        void loadAPIs(const std::vector<TranslationAPI>& apis);

        std::optional<TranslationAPI> getAPI();

        std::optional<TranslationAPI> getFirstAPI();

        void resortTokens();

        void reportProblem(const TranslationAPI& badAPI);

        bool isEmpty();
    };
}


module :private;

APIPool::APIPool() : m_gen(m_rd()) {}

void APIPool::loadAPIs(const std::vector<TranslationAPI>& apis) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_apis.insert(m_apis.end(), apis.begin(), apis.end());
    m_logger->info("令牌池新加载 {} 个 API Keys， 现共有 {} 个API Keys", apis.size(), m_apis.size());
}

std::optional<TranslationAPI> APIPool::getAPI() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_apis.empty()) {
        return std::nullopt; // 没有可用的 token
    }

    // 生成一个随机索引
    std::uniform_int_distribution<> distrib(0, (int)m_apis.size() - 1);
    int index = distrib(m_gen);

    return m_apis[index];
}

std::optional<TranslationAPI> APIPool::getFirstAPI() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_apis.empty()) {
        return std::nullopt;
    }

    return m_apis.front();
}

void APIPool::resortTokens() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_apis.size() > 1) {
        std::rotate(m_apis.begin(), m_apis.begin() + 1, m_apis.end());
    }
}

void APIPool::reportProblem(const TranslationAPI& badAPI) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::erase_if(m_apis, [&](const TranslationAPI& api)
        {
            // 通过 apikey 来唯一标识一个 api
            if (api.apikey == badAPI.apikey) {
                m_logger->warn("API Key [{}] 已被标记为不可用。", api.apikey);
                return true;
            }
            return false;
        });
}

bool APIPool::isEmpty() {
    std::lock_guard<std::mutex> lock(m_mutex); // 加锁
    return m_apis.empty();
}