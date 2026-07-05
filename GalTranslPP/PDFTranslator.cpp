module;

#include "GPPMacros.hpp"
#include <toml.hpp>

module PDFTranslator;

import Tool;
import PDFTool;

namespace fs = std::filesystem;

PDFTranslator::~PDFTranslator() 
{
    m_logger->info(gppTr("PDFTranslator.~PDFTranslator", "所有任务已完成！PDFTranslator结束。"));
}

PDFTranslator::PDFTranslator(const fs::path& projectDir, const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger) :
    NormalJsonTranslator(projectDir, controller, logger,
        // m_inputDir                                                m_inputCacheDir
        // m_outputDir                                               m_outputCacheDir
        L"cache" / projectDir.filename() / L"pdf_json_input", L"cache" / projectDir.filename() / L"gt_input_cache",
        L"cache" / projectDir.filename() / L"pdf_json_output", L"cache" / projectDir.filename() / L"gt_output_cache")
{
    m_logger->info(gppTr("PDFTranslator.PDFTranslator", "GalTransl++ PDFTranslator 启动..."));
}

void PDFTranslator::pdfInit()
{
    NormalJsonTranslator::normalJsonInit();
    m_pdfInputDir = m_projectDir / L"gt_input";
    m_pdfOutputDir = m_projectDir / L"gt_output";

    try {
        const auto projectConfig = toml::uparse(m_projectDir / L"config.toml");
        const auto pluginConfig = toml::uparse(filePluginConfigPath / L"PDF.toml");

        m_bilingualOutput = parseToml<bool>(projectConfig, pluginConfig, "plugins.PDF.输出双语翻译文件");

        checkPDFDependency(m_logger);
    }
    catch (const toml::exception& e) {
        throw std::runtime_error(gppTr("PDFTranslator.pdfInit", "PDF 配置文件解析失败: %1", e.what()));
    }
}


void PDFTranslator::pdfBeforeRun()
{
    for (const auto& dir : { m_pdfInputDir, m_pdfOutputDir }) {
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
            m_logger->info(gppTr("PDFTranslator.pdfBeforeRun", "已创建目录: %1", wide2Ascii(dir)));
        }
    }

    for (const auto& dir : { m_inputDir, m_outputDir }) {
        fs::remove_all(dir);
        fs::create_directories(dir);
    }

    std::vector<fs::path> pdfFilePaths;
    for (const auto& entry : fs::recursive_directory_iterator(m_pdfInputDir)) {
        if (entry.is_regular_file() && isSameExtension(entry.path(), L".pdf")) {
            pdfFilePaths.push_back(entry.path());
        }
    }
    if (pdfFilePaths.empty()) {
        throw std::runtime_error(gppTr("PDFTranslator.pdfBeforeRun", "未找到 PDF 文件"));
    }

    for (const auto& pdfFilePath : pdfFilePaths) {
        if (m_controller->shouldStop()) {
            return;
        }
        const fs::path relPDFPath = fs::relative(pdfFilePath, m_pdfInputDir);
        const fs::path relJsonPath = fs::path(relPDFPath).replace_extension(".json");

        m_jsonToPDFPathMap[relJsonPath] = pdfFilePath;
        const fs::path inputJsonFile = m_inputDir / relJsonPath;
        createParent(inputJsonFile);

        m_logger->info(gppTr("PDFTranslator.pdfBeforeRun", "正在提取文件: %1", wide2Ascii(relPDFPath)));

        const auto& [success, message] = extractPDF(pdfFilePath, inputJsonFile);

        if (success) {
            m_logger->info(gppTr("PDFTranslator.pdfBeforeRun", "成功提取元数据: %1", message));
        }
        else {
            throw std::runtime_error(gppTr("PDFTranslator.pdfBeforeRun", "提取元数据失败: %1", message));
        }
    }

    m_onFileProcessed = [this](fs::path relProcessedFile)
        {
            if (!m_jsonToPDFPathMap.contains(relProcessedFile)) {
                m_logger->warn(gppTr("PDFTranslator.pdfBeforeRun", "未找到与 %1 对应的元数据，跳过", wide2Ascii(relProcessedFile)));
                return;
            }

            const fs::path& origPDFPath = m_jsonToPDFPathMap[relProcessedFile];
            const fs::path relPDFPath = fs::relative(origPDFPath, m_pdfInputDir);
            const fs::path outputPDFFile = m_pdfOutputDir / relPDFPath;
            createParent(outputPDFFile);

            m_logger->info(gppTr("PDFTranslator.pdfBeforeRun", "正在回注文件: %1", wide2Ascii(relPDFPath)));

            auto [success, message] = rejectPDF(origPDFPath, m_outputDir / relProcessedFile,
                outputPDFFile.parent_path(), false, !m_bilingualOutput);

            if (success) {
                m_logger->info(gppTr("PDFTranslator.pdfBeforeRun", "成功翻译文件: %1", message));
            }
            else {
                throw std::runtime_error(gppTr("PDFTranslator.pdfBeforeRun", "翻译文件失败: %1", message));
            }
        };

}

void PDFTranslator::run() {
    NormalJsonTranslator::normalJsonInit();
    PDFTranslator::pdfInit();
    PDFTranslator::pdfBeforeRun();
    NormalJsonTranslator::normalJsonBeforeRun();
    NormalJsonTranslator::normalJsonProcess();
    NormalJsonTranslator::normalJsonAfterRun();
}
