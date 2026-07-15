module;

#include "GPPMacros.hpp"

export module DictionaryGenerator;

export import ApiPool;
export import AgentCommonSourceView;
export import GPPDefines;
export import ITranslator;

namespace fs = std::filesystem;

export
{
    class DictionaryGenerator {
    private:
        std::shared_ptr<IController> m_controller;
        std::shared_ptr<spdlog::logger> m_logger;
        const std::unique_ptr<ApiPool>& m_apiPool;

        const std::function<void(Sentence*)> m_preProcessFunc; // 临时对象，不能设置引用
        const std::function<std::string(std::string)>& m_onPerformApi;  // 由于 NormalJsonTranslator 生命周期包含了 DictionaryGenrator
        const std::function<DictList(DictList)>& m_onDictProcessed;  	// 所以从前者类成员中传递过来的 function 可设置为引用
        const NLPTokenizeFunc& m_tokenizeSourceLangFunc; // 避免 python 闭包复制时死锁

        std::string m_systemPrompt;
        std::string m_userPrompt;
        std::string m_apiStrategy;
        std::string m_targetLang;
        int m_threadsNum;
        int m_inputBlockMaxLines;
        int m_maxRequestCount;
        int m_apiTimeOutMs;
        bool m_checkQuota;
        // 由 Dictionary Generator 重新用 input files 计算
        int m_totalSentences = 0;
        bool m_agentEnabled;
        fs::path m_projectDir;
        fs::path m_inputDir;
        std::vector<fs::path> m_relJsonPaths;
        std::optional<fs::path> m_agentProjectNotePath;
        std::string m_genDictReviewSystemPrompt;
        std::string m_genDictReviewUserPrompt;
        int m_agentMaxTurnsPerChunk;
        int m_agentSearchResultLimit;
        int m_agentContextLinesLimit;

        // 阶段一和二的结果
        fs::path m_tokenizeCachePath;
        absl::flat_hash_map<std::string, EntityVec> m_tokenizeCacheMap;

        std::vector<std::string> m_segments;
        std::vector<absl::flat_hash_set<std::string>> m_segmentWords;
        absl::flat_hash_map<std::string, int> m_wordCounter;
        absl::flat_hash_set<std::string> m_nameSet;
        std::vector<AgentCommonSourceFileView> m_reviewSourceFiles;

        // 阶段四的结果 (线程安全)
        DictList m_finalDict;
        absl::flat_hash_map<std::string, int> m_finalCounter;
        std::mutex m_resultMutex;

        void preprocessAndTokenize(const std::vector<fs::path>& jsonFiles);
        void callLLMToGenerate(int segmentIndex, int batchIndex, int threadId);

    public:
        DictionaryGenerator(const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, const std::unique_ptr<ApiPool>& apiPool,
            const NLPTokenizeFunc& tokenizeSourceLangFunc, const fs::path& otherCacheDir,
            const std::function<void(Sentence*)>& preProcessFunc, const std::function<std::string(std::string)>& onPerformApi, const std::function<DictList(DictList)>& onDictProcessed,
            const std::string& systemPrompt, const std::string& userPrompt, const std::string& apiStrategy, const std::string& targetLang,
            int threadsNum, int inputBlockMaxLines, int maxRequestCount, int apiTimeOutMs, bool checkQuota,
            bool agentEnabled, const fs::path& projectDir, const fs::path& inputDir,
            const std::vector<fs::path>& relJsonPaths, const std::optional<fs::path>& agentProjectNotePath,
            const std::string& genDictReviewSystemPrompt, const std::string& genDictReviewUserPrompt,
            int agentMaxTurnsPerChunk, int agentSearchResultLimit, int agentContextLinesLimit);

        ~DictionaryGenerator();

        void generate(const fs::path& outputFilePath);
    };
}
