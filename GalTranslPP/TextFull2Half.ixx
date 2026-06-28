module;

#include "GPPMacros.hpp"
#include <toml.hpp>

export module TextFull2Half;

export import GPPDefines;

namespace fs = std::filesystem;

export
{
    class TextFull2Half {
    private:
        absl::flat_hash_map<char32_t, char32_t> m_conversionMap;
        std::vector<jpc::Regex> m_notConvertRegs;
        std::string m_notConvertedCharsKey;
        std::shared_ptr<spdlog::logger> m_logger;

        bool m_replacePunctuation;
        bool m_reverseConversion;

        PluginRunTime m_runTime;

        void createConversionMap();
        std::string convertText(const std::string& text, Sentence* se, bool jumpTag);

    public:
        TextFull2Half(const toml::value& projectConfig, const std::shared_ptr<spdlog::logger>& logger, PluginRunTime runTime);
        ~TextFull2Half() = default;

        void dPreRun(Sentence* se);
        void preRun(Sentence* se);
        void postRun(Sentence* se);
        void dPostRun(Sentence* se);
    };
}
