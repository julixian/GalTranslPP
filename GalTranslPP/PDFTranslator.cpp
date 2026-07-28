module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"
#include <toml.hpp>

module PDFTranslator;

import Tool;
import PDFTool;

namespace fs = std::filesystem;

PDFTranslator::~PDFTranslator() 
{
    m_logger->info(gppTr("PDFTranslator.~PDFTranslator", "所有任务已完成！PDFTranslator 结束").toStdString());
}

PDFTranslator::PDFTranslator(const fs::path& projectDir, const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger) :
    NormalJsonTranslator(projectDir, controller, logger,
        // m_inputDir                                                m_inputCacheDir
        // m_outputDir                                               m_outputCacheDir
        L"cache" / projectDir.filename() / L"pdf_json_input", L"cache" / projectDir.filename() / L"gt_input_cache",
        L"cache" / projectDir.filename() / L"pdf_json_output", L"cache" / projectDir.filename() / L"gt_output_cache")
{
    m_logger->info(gppTr("PDFTranslator.PDFTranslator", "GalTransl++ PDFTranslator 启动...")
        .toStdString());
}

void PDFTranslator::pdfInit()
{
    m_pdfInputDir = m_projectDir / L"gt_input";
    m_pdfOutputDir = m_projectDir / L"gt_output";

    try {
        const auto projectConfig = toml::uparse(m_projectDir / L"Config.toml");
        const auto pluginConfig = toml::uparse(filePluginConfigPath / L"PDF.toml");

        m_bilingualOutput = parseToml<bool>(projectConfig, pluginConfig, "plugins.PDF.bilingualOutput");
        m_babeldocLangOut = parseToml<std::string>(projectConfig, pluginConfig, "plugins.PDF.babeldocLangOut");

        checkPDFDependency(m_logger);
    }
    catch (const toml::exception& e) {
        throw std::runtime_error(gppTr("PDFTranslator.pdfInit", "PDF 配置文件解析失败: %1")
            .arg(e.what())
            .toStdString());
    }
}


void PDFTranslator::pdfBeforeRun()
{
    for (const auto& dir : { m_pdfInputDir, m_pdfOutputDir }) {
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
            m_logger->info(gppTr("PDFTranslator.pdfBeforeRun", "已创建目录: [%1]")
                .arg(wide2Ascii(dir))
                .toStdString());
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
        throw std::runtime_error(gppTr("PDFTranslator.pdfBeforeRun", "未找到 PDF 文件").toStdString());
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

        m_logger->info(gppTr("PDFTranslator.pdfBeforeRun", "正在提取 PDF 文件元数据: [%1]")
            .arg(wide2Ascii(relPDFPath))
            .toStdString());

        const auto& [success, message] = extractPDF(pdfFilePath, inputJsonFile, m_babeldocLangOut);

        if (success) {
            m_logger->info(gppTr("PDFTranslator.pdfBeforeRun", "成功提取 PDF 文件元数据: %1")
                .arg(message)
                .toStdString());
        }
        else {
            throw std::runtime_error(gppTr("PDFTranslator.pdfBeforeRun", "提取 PDF 文件元数据失败: %1")
                .arg(message)
                .toStdString());
        }
    }

    m_onFileProcessed = [this](const fs::path& relProcessedFile)
        {
            std::unique_lock<std::mutex> lock(m_onFileProcessedMutex);
            const auto it = m_jsonToPDFPathMap.find(relProcessedFile);
            if (it == m_jsonToPDFPathMap.end()) {
                m_logger->error(gppTr("PDFTranslator.pdfBeforeRun", "[文件 %1] 未找到对应的元数据")
                    .arg(wide2Ascii(relProcessedFile))
                    .toStdString());
                return;
            }

            const fs::path& origPDFPath = it->second;
            lock.unlock();
            const fs::path relPDFPath = fs::relative(origPDFPath, m_pdfInputDir);
            const fs::path outputPDFFile = m_pdfOutputDir / relPDFPath;
            createParent(outputPDFFile);

            m_logger->info(gppTr("PDFTranslator.pdfBeforeRun", "正在回注 PDF 文件: [%1]")
                .arg(wide2Ascii(relPDFPath))
                .toStdString());

            auto [success, message] = reinjectPDF(origPDFPath, m_outputDir / relProcessedFile,
                outputPDFFile.parent_path(), m_babeldocLangOut, m_bilingualOutput);

            if (success) {
                m_logger->info(gppTr("PDFTranslator.pdfBeforeRun", "成功回注 PDF 文件: %1")
                    .arg(message)
                    .toStdString());
            }
            else {
                throw std::runtime_error(gppTr("PDFTranslator.pdfBeforeRun", "回注 PDF 文件失败: %1")
                    .arg(message)
                    .toStdString());
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
