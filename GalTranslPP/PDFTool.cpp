module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"

module PDFTool;

import Tool;
import PythonManager;

namespace fs = std::filesystem;
namespace py = pybind11;

std::tuple<bool, std::string> extractPDF(const fs::path& pdfPath, const fs::path& jsonPath,
    const std::string& babeldocLangOut, bool showProgress)
{
    bool success;
    std::string message;
    auto extractTaskFunc = [&]()
        {
            std::tie(success, message) = py::module_::import("PDFConverter").attr("extract_text_to_json")
                (wide2Ascii(pdfPath), wide2Ascii(jsonPath), babeldocLangOut, showProgress).cast<std::tuple<bool, std::string>>();
        };
    PythonMainInterpreterManager::getInstance().submitTask(std::move(extractTaskFunc)).get();
    return std::make_tuple(success, message);
}


std::tuple<bool, std::string> reinjectPDF(const fs::path& orgPDFPath, const fs::path& translatedJsonPath, const fs::path& outputPDFPath,
    const std::string& babeldocLangOut, bool bilingualOutput, bool showProgress)
{
    bool success;
    std::string message;
    auto reinjectTaskFunc = [&]()
        {
            // 其实传 wstring 也可以，不过还是统一成传 U8 吧
            std::tie(success, message) = py::module_::import("PDFConverter").attr("reinject_json_to_pdf")
                (wide2Ascii(orgPDFPath), wide2Ascii(translatedJsonPath), wide2Ascii(outputPDFPath),
                    babeldocLangOut, bilingualOutput, showProgress).cast<std::tuple<bool, std::string>>();
        };
    PythonMainInterpreterManager::getInstance().submitTask(std::move(reinjectTaskFunc)).get();
    return std::make_tuple(success, message);
}

void checkPDFDependency(const std::shared_ptr<spdlog::logger>& logger) {
    checkPythonDependencies({ "babeldoc" }, logger);
}
