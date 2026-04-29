module;

#include "GPPMacros.hpp"

export module DictionaryReviewAgent;

import APIPool;
import AgentSourceView;
import DictionaryReviewIndex;
import GPPDefines;
import ITranslator;

namespace fs = std::filesystem;

export {
    struct DictionaryReviewAgentConfig {
        fs::path projectDir;
        std::vector<fs::path> relInputFiles;
        std::optional<fs::path> projectNotePath;
        std::string systemPrompt;
        std::string userPrompt;
        std::string apiStrategy;
        std::string targetLang;
        int maxRetries = 5;
        int threadsNum = 1;
        int maxTurnsPerTerm = 6;
        int searchResultLimit = 80;
        int apiTimeoutMs = 120000;
        bool checkQuota = true;
    };

    class DictionaryReviewAgent {
    public:
        DictionaryReviewAgent(
            const std::shared_ptr<IController>& controller,
            const std::shared_ptr<spdlog::logger>& logger,
            const std::unique_ptr<APIPool>& apiPool,
            const std::function<std::string(std::string)>& onPerformApi,
            const DictionaryReviewAgentConfig& config
        );

        DictList review(const std::vector<DictionaryReviewTermGroup>& groups, const std::vector<AgentSourceFileView>& sourceFiles);

    private:
        const std::unique_ptr<APIPool>& m_apiPool;
        std::shared_ptr<IController> m_controller;
        std::shared_ptr<spdlog::logger> m_logger;
        const std::function<std::string(std::string)>& m_onPerformApi;
        DictionaryReviewAgentConfig m_config;
    };
}
