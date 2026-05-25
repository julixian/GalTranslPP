module;

#include "GPPMacros.hpp"

export module APIPool;

import std;
import spdlog;
import ITranslator;
export import APITool;

export
{
    class APIPool {
    private:
        std::vector<TranslationApi> m_apis;
        std::shared_ptr<spdlog::logger> m_logger;
        std::mutex m_mutex;

        std::unique_ptr<std::mt19937> m_gen;

    public:
        explicit APIPool(const std::shared_ptr<spdlog::logger>& logger);

        void loadApis(const std::vector<TranslationApi>& apis);

        std::optional<TranslationApi> getApi();

        std::optional<TranslationApi> getFirstApi();

        void resortTokens();

        void reportProblem(const TranslationApi& badAPI);

        bool isEmpty();
    };

    bool checkResponse(const ApiResponse& response, const std::unique_ptr<APIPool>& apiPool, const TranslationApi& currentAPI,
        const std::filesystem::path& relInputPath, const std::string& apiStrategy, 
        const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger,
        int& retryCount, int threadId, bool m_checkQuota);
}
