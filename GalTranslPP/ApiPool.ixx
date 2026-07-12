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
        std::mutex m_mutex;

        std::unique_ptr<std::mt19937> m_gen;

    public:
        explicit ApiPool(const std::shared_ptr<spdlog::logger>& logger);

        void loadApis(const std::vector<TranslationApi>& apis);

        std::optional<TranslationApi> getApi(const std::string& apiStrategy);
        std::optional<TranslationApi> getFirstApi();

        void resortTokens();

        void reportProblem(const TranslationApi& badApi);

        bool isEmpty();
        size_t size();
    };

    bool checkResponse(ApiResponse& response, const std::unique_ptr<ApiPool>& apiPool, const TranslationApi& currentApi,
        const std::string& logPrefix, const fs::path& relFilePath, const std::string& apiStrategy,
        const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger,
        int& requestCount, bool checkQuota);
}
