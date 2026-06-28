module;

#include "GPPMacros.hpp"

export module Dictionary;

export import GPPDefines;
export import PythonManager;
export import LuaManager;

namespace fs = std::filesystem;

export
{
    struct GptDictEntry {
        std::string searchStr;
        std::string replaceStr;
        std::string note;
        std::unique_ptr<std::vector<size_t>> otherEntriesWhoseSearchStrContainsThatInThisEntry;
        int priority;
    };

    class GptDictionary {
    private:
        absl::flat_hash_map<std::string, WordPosVec> m_tokenizeCacheMap;
        const std::function<NLPResult(const std::string&)>& m_tokenizeSourceLangFunc;
        fs::path m_projectDir;
        fs::path m_tokenizeCachePath;
        std::vector<GptDictEntry> m_entries;
        std::shared_ptr<spdlog::logger> m_logger;
        std::shared_mutex m_tokenizeCacheMapMutex;
        const std::unique_ptr<LuaManager>& m_luaManager;
        const std::unique_ptr<PythonManager>& m_pythonManager;

    public:

        explicit GptDictionary(const fs::path& projectDir, const fs::path& otherCacheDir, const std::function<NLPResult(const std::string&)>& tokenizeSourceLangFunc,
            const std::unique_ptr<LuaManager>&, const std::unique_ptr<PythonManager>& pythonManager, const std::shared_ptr<spdlog::logger>& logger);

        ~GptDictionary();

        void sort();

        void loadFromFile(const fs::path& filePath);

        std::string generatePrompt(std::span<Sentence*> batch, TransEngine transEngine) const;

        std::string doReplace(const Sentence* se, CachePart targetToModify) const;

        void checkDictUse(Sentence* sentence, CachePart base, CachePart check);
    };


    struct NormalDictEntry {
        std::string searchStr;
        std::string replaceStr;
        std::unique_ptr<jpc::Regex> searchReg;
        std::unique_ptr<std::string> replaceModifier;
        // 条件字典相关
        std::unique_ptr<CheckSeCondFunc> dictCondition;
        int priority;
        bool isReg;
    };

    class NormalDictionary {
    private:
        fs::path m_projectDir;
        std::vector<NormalDictEntry> m_entries;
        std::shared_ptr<spdlog::logger> m_logger;
        const std::unique_ptr<LuaManager>& m_luaManager;
        const std::unique_ptr<PythonManager>& m_pythonManager;

    public:

        NormalDictionary(const fs::path& projectDir,
            const std::unique_ptr<LuaManager>& luaManager, const std::unique_ptr<PythonManager>& pythonManager, const std::shared_ptr<spdlog::logger>& logger)
            : m_projectDir(projectDir), m_luaManager(luaManager), m_pythonManager(pythonManager), m_logger(logger) {}

        void loadFromFile(const fs::path& filePath);

        void sort();

        std::string doReplace(const Sentence* sentence, CachePart targetToModify);
    };
}
