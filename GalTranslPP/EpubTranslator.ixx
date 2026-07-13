module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"

export module EpubTranslator;

export import NormalJsonTranslator;

namespace fs = std::filesystem;

export
{
    struct EpubTextNodeInfo {
        size_t offset; // 节点在原始文件中的字节偏移量
        size_t length; // 节点内容的字节长度
    };

    struct CallbackPattern {
        std::unique_ptr<jpc::Regex> org;
        std::unique_ptr<jpc::RegexReplace> rep;

        CallbackPattern() {
            org = std::make_unique<jpc::Regex>();
            rep = std::make_unique<jpc::RegexReplace>(org.get());
        }
    };

    struct RegexPattern {
        std::unique_ptr<jpc::Regex> org;
        std::unique_ptr<jpc::RegexReplace> rep;

        absl::btree_multimap<int, CallbackPattern> callbackPatterns;
        bool isCallback{};

        RegexPattern() {
            org = std::make_unique<jpc::Regex>();
            rep = std::make_unique<jpc::RegexReplace>(org.get());
        }
    };

    struct JsonInfo {
        std::vector<EpubTextNodeInfo> metadata;
        // 存储json文件相对路径到原始 HTML 完整路径的映射
        fs::path htmlPath;
        // 存储json文件相对路径到其所属 epub 完整路径的映射
        fs::path epubPath;
        // 存储json文件相对路径到 normal_post 完整路径的映射
        fs::path normalPostPath;
        // HTML 预处理正则替换后的内容
        std::string content;
    };

    class EpubTranslator : public NormalJsonTranslator {

        friend void pybind11_init_gpp_plugin_api(::pybind11::module_& m);
        friend class LuaManager;

    protected:
        fs::path m_epubInputDir;
        fs::path m_epubOutputDir;
        fs::path m_tempUnpackDir;
        fs::path m_tempRebuildDir;

        // EPUB 处理相关的配置
        bool m_bilingualOutput{};
        std::string m_originalTextColor;
        std::string m_originalTextScale;

        // 预处理正则和后处理正则
        std::vector<RegexPattern> m_preRegexPatterns;
        std::vector<RegexPattern> m_postRegexPatterns;

        // 存储json文件相对路径到各种元数据的映射
        absl::flat_hash_map<fs::path, JsonInfo> m_jsonToInfoMap;

        // 每个epub完整路径对应的多个json文件相对路径以及有没有处理完毕
        absl::flat_hash_map<fs::path, absl::flat_hash_map<fs::path, bool>> m_epubToJsonsMap;

        std::mutex m_onFileProcessedMutex;

    public:

        EpubTranslator(const fs::path& projectDir,
            const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger);

    	~EpubTranslator() override;

        void epubInit();
        void epubBeforeRun();

    	void run() override;
    };
}
