module;

#include "GPPMacros.hpp"

export module ApiPool;

export import ApiTool;
export import ITranslator;

namespace fs = std::filesystem;

export
{
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

    void inferAndRecordApiError(const ApiResponse& response, const std::unique_ptr<ApiPool>& apiPool, const TranslationApi& currentApi,
        const std::string& logPrefix, const fs::path& relFilePath, const std::string& apiStrategy,
        const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger,
        int& requestCount, bool checkQuota);
}
