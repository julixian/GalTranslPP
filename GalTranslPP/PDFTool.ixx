module;

#include "GPPMacros.hpp"

export module PDFTool;

export import GPPDefines;

namespace fs = std::filesystem;

export
{
    std::tuple<bool, std::string> extractPDF(const fs::path& pdfPath, const fs::path& jsonPath,
        const std::string& babeldocLangOut = "zh-CN", bool showProgress = true);

    std::tuple<bool, std::string> reinjectPDF(const fs::path& orgPDFPath, const fs::path& translatedJsonPath, const fs::path& outputPDFPath,
        const std::string& babeldocLangOut = "zh-CN", bool bilingualOutput = true, bool showProgress = true);

    void checkPDFDependency(const std::shared_ptr<spdlog::logger>& logger);
}
