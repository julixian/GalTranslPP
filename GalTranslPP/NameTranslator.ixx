module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"

export module NameTranslator;

export import ApiPool;
export import Dictionary;
export import ITranslator;

namespace fs = std::filesystem;

export
{
    class NameTranslator {
    private:
        std::shared_ptr<IController> m_controller;
        std::shared_ptr<spdlog::logger> m_logger;
        const std::unique_ptr<ApiPool>& m_apiPool;
        const std::unique_ptr<GptDictionary>& m_gptDictionary;

        const std::function<std::string(std::string_view)>& m_onPerformApi;

        std::string m_systemPrompt;
        std::string m_userPrompt;
        std::string m_apiStrategy;
        std::string m_targetLang;
        int m_threadsNum;
        int m_batchSize;
        int m_inputBlockMaxLines;
        int m_glossaryMaxLines;
        int m_maxRequestCount;
        int m_apiTimeoutMs;
        bool m_checkQuota;

        // 翻译一个批次，并将成功解析的译名写入共享结果。
        void translateBatch(std::span<const std::string> batchNames, int threadId, size_t batchIndex,
            absl::flat_hash_map<std::string, std::string>& translationResults, std::mutex& translationResultsMutex);

    public:
        NameTranslator(
            const std::shared_ptr<IController>& controller,
            const std::shared_ptr<spdlog::logger>& logger,
            const std::unique_ptr<ApiPool>& apiPool,
            const std::unique_ptr<GptDictionary>& gptDictionary,
            const std::function<std::string(std::string_view)>& onPerformApi,
            const std::string& systemPrompt,
            const std::string& userPrompt,
            const std::string& apiStrategy,
            const std::string& targetLang,
            int threadsNum,
            int batchSize,
            int inputBlockMaxLines,
            int glossaryMaxLines,
            int maxRequestCount,
            int apiTimeoutMs,
            bool checkQuota
        );

        void run(const fs::path& nameTablePath);
    };
}
