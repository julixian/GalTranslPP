module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"

export module PDFTranslator;

import spdlog;
import ITranslator;
import NormalJsonTranslator;

namespace fs = std::filesystem;

export
{
    class PDFTranslator : public NormalJsonTranslator {

        friend void pybind11_init_gpp_plugin_api(::pybind11::module_& m);
        friend class LuaManager;

    protected:
        fs::path m_pdfInputDir;
        fs::path m_pdfOutputDir;

        // PDF 处理相关的配置
        bool m_bilingualOutput{};

        // 存储json文件相对路径到其所属PDF完整路径的映射
        absl::flat_hash_map<fs::path, fs::path> m_jsonToPDFPathMap;

    public:

        PDFTranslator(const fs::path& projectDir, const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger);

        virtual ~PDFTranslator() override;


        void pdfInit();
        void pdfBeforeRun();

        virtual void run() override;
    };
}
