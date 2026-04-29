module;

#include "GPPMacros.hpp"

export module DictionaryGenerator;

import APIPool;
import AgentSourceView;
import GPPDefines;
import ITranslator;

namespace fs = std::filesystem;

export {
    struct DictionaryGeneratorReviewOptions {
        bool enabled = false;
        fs::path projectDir;
        fs::path inputDir;
        std::vector<fs::path> relInputFiles;
        std::optional<fs::path> projectNotePath;
        std::string systemPrompt;
        std::string userPrompt;
        int maxTurnsPerTerm = 20;
        int searchResultLimit = 80;
    };

    class DictionaryGenerator {

    private:
        const std::unique_ptr<APIPool>& m_apiPool;
        std::shared_ptr<IController> m_controller;
        std::shared_ptr<spdlog::logger> m_logger;

        const std::function<void(Sentence*)> m_preProcessFunc; // 临时对象，不能设置引用
        const std::function<std::string(std::string)>& m_onPerformApi;  // 由于 NormalJsonTranslator 生命周期包含了 DictionaryGenrator
        const std::function<DictList(DictList)>& m_onDictProcessed;  	// 所以从前者类成员中传递过来的 function 可设置为引用
        const std::function<NLPResult(const std::string&)>& m_tokenizeSourceLangFunc;

        std::string m_systemPrompt;
        std::string m_userPrompt;
        std::string m_apiStrategy;
        std::string m_targetLang;
        int m_threadsNum;
        int m_apiTimeoutMs;
        int m_maxRetries;
        bool m_checkQuota;
        int m_totalSentences = 0;
        DictionaryGeneratorReviewOptions m_reviewOptions;

        // 阶段一和二的结果
        fs::path m_tokenizeCachePath;
        absl::flat_hash_map<std::string, EntityVec> m_tokenizeCacheMap;

        std::vector<std::string> m_segments;
        std::vector<absl::flat_hash_set<std::string>> m_segmentWords;
        absl::flat_hash_map<std::string, int> m_wordCounter;
        absl::flat_hash_set<std::string> m_nameSet;
        std::vector<AgentSourceFileView> m_reviewSourceFiles;

        // 阶段四的结果 (线程安全)
        DictList m_finalDict;
        absl::flat_hash_map<std::string, int> m_finalCounter;
        std::mutex m_resultMutex;

        void preprocessAndTokenize(const std::vector<fs::path>& jsonFiles);
        std::vector<int> solveSentenceSelection();
        void callLLMToGenerate(int segmentIndex, int threadId);
        DictList finalizeCoarseCandidates() const;

    public:
        DictionaryGenerator(const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, const std::unique_ptr<APIPool>& apiPool, 
            const std::function<NLPResult(const std::string&)>& tokenizeFunc, const fs::path& otherCacheDir,
            const std::function<void(Sentence*)>& preProcessFunc, const std::function<std::string(std::string)>& onPerformApi, const std::function<DictList(DictList)>& onDictProcessed,
            const std::string& systemPrompt, const std::string& userPrompt, const std::string& apiStrategy, const std::string& targetLang,
            int maxRetries, int threadsNum, int apiTimeoutMs, bool checkQuota,
            const DictionaryGeneratorReviewOptions& reviewOptions = {});

        ~DictionaryGenerator();

        void generate(const std::vector<fs::path>& jsonFiles, const fs::path& outputFilePath);
    };
}
