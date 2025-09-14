module;

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>
#include <zip.h>
#include <gumbo.h>
#include <unicode/regex.h>
#include <unicode/unistr.h>

export module EpubTranslator;
import Tool;
import NormalJsonTranslator;
namespace fs = std::filesystem;
using json = nlohmann::json;

export {

    struct EpubTextNodeInfo {
        size_t offset; // 节点在原始文件中的字节偏移量
        size_t length; // 节点内容的字节长度
    };

    struct RegexPattern {
        std::shared_ptr<icu::RegexPattern> pattern;
        icu::UnicodeString rep;
    };

    class EpubTranslator : public NormalJsonTranslator {

    private:

        fs::path m_epubInputDir;
        fs::path m_epubOutputDir;
        fs::path m_tempUnpackDir;
        fs::path m_tempRebuildDir;

        // EPUB 处理相关的配置
        bool m_bilingualOutput = true;
        std::string m_originalTextColor = "#808080";
        std::string m_originalTextScale = "0.8";

        // 存储每个 HTML 文件及其包含的句子元数据
        // Key: 唯一的扁平化文件名 (e.g., "book1_OEBPS_Text_chapter1.html")
        // Value: 元数据
        std::map<std::string, std::vector<EpubTextNodeInfo>> m_htmlMetadata;

        // 存储扁平化文件名到原始相对路径的映射
        std::map<std::string, fs::path> m_flatToOriginalPathMap;

        // 预处理正则和后处理正则
        std::vector<RegexPattern> m_preRegexPatterns;
        std::vector<RegexPattern> m_postRegexPatterns;

    public:

        virtual void run() override;

        EpubTranslator(const fs::path& projectDir, TransEngine transEngine, std::shared_ptr<IController> controller);

        virtual ~EpubTranslator()
        {
            m_logger->info("所有任务已完成！EpubTranlator结束。");
        }
    };
}



module :private;

// 递归遍历 Gumbo 树以提取文本节点
void extractTextNodes(GumboNode* node, std::vector<std::pair<std::string, EpubTextNodeInfo>>& sentences) {
    if (node->type == GUMBO_NODE_TEXT) {
        std::string text = node->v.text.text;
        if (text.empty() || text.find_first_not_of(" \t\n\r") == std::string::npos) {
            return;
        }
        EpubTextNodeInfo info;
        info.offset = node->v.text.start_pos.offset;
        info.length = text.length();
        sentences.push_back({ text, info });
        return;
    }

    if (node->type != GUMBO_NODE_ELEMENT || node->v.element.tag == GUMBO_TAG_SCRIPT || node->v.element.tag == GUMBO_TAG_STYLE) {
        return;
    }

    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        extractTextNodes(static_cast<GumboNode*>(children->data[i]), sentences);
    }
}

EpubTranslator::EpubTranslator(const fs::path& projectDir, TransEngine transEngine, std::shared_ptr<IController> controller) :
    NormalJsonTranslator(projectDir, transEngine, controller,
        // m_inputDir                                                m_inputCacheDir
        // m_outputDir                                               m_outputCacheDir
        L"cache" / projectDir.filename() / L"epub_json_input", L"cache" / projectDir.filename() / L"gt_input_cache",
        L"cache" / projectDir.filename() / L"epub_json_output", L"cache" / projectDir.filename() / L"gt_output_cache")
{
    m_epubInputDir = m_projectDir / L"gt_input";
    m_epubOutputDir = m_projectDir / L"gt_output";
    m_tempUnpackDir = L"cache" / m_projectDir.filename() / L"epub_unpacked";
    m_tempRebuildDir = L"cache" / m_projectDir.filename() / L"epub_rebuild";

    std::ifstream ifs;

    try {
        ifs.open(m_projectDir / L"config.toml");
        auto projectConfig = toml::parse(ifs);
        ifs.close();

        ifs.open(pluginConfigsPath / "filePlugins/Epub.toml");
        auto pluginConfig = toml::parse(ifs);
        ifs.close();

        m_bilingualOutput = parseToml<bool>(projectConfig, pluginConfig, "plugins.Epub.双语显示");
        m_originalTextColor = parseToml<std::string>(projectConfig, pluginConfig, "plugins.Epub.原文颜色");
        m_originalTextScale = std::to_string(parseToml<double>(projectConfig, pluginConfig, "plugins.Epub.缩小比例"));

        auto preRegexArr = parseToml<toml::array>(projectConfig, pluginConfig, "plugins.Epub.预处理正则");
        for (const auto& elem : preRegexArr) {
            auto preRegexOpt = elem.as_table();
            if (!preRegexOpt) {
                continue;
            }
            std::string preRegexOrg = (*preRegexOpt).contains("org") ? (*preRegexOpt)["org"].value_or("") : 
                (*preRegexOpt)["searchStr"].value_or("");
            if (preRegexOrg.empty()) {
                continue;
            }
            std::string preRegexRep = (*preRegexOpt).contains("rep") ? (*preRegexOpt)["rep"].value_or("") : 
                (*preRegexOpt)["replaceStr"].value_or("");
            RegexPattern preRegex;
            icu::UnicodeString preRegexUStr = icu::UnicodeString::fromUTF8(preRegexOrg);
            icu::UnicodeString repUStr = icu::UnicodeString::fromUTF8(preRegexRep);
            UErrorCode status = U_ZERO_ERROR;
            preRegex.pattern = std::shared_ptr<icu::RegexPattern>(icu::RegexPattern::compile(preRegexUStr, 0, status));
            if (U_FAILURE(status)) {
                throw std::runtime_error("预处理正则编译失败: " + preRegexOrg);
            }
            preRegex.rep = repUStr;
            m_preRegexPatterns.push_back(preRegex);
        }

        auto postRegexArr = parseToml<toml::array>(projectConfig, pluginConfig, "plugins.Epub.后处理正则");
        for (const auto& elem : postRegexArr) {
            auto postRegexOpt = elem.as_table();
            if (!postRegexOpt) {
                continue;
            }
            std::string postRegexOrg = (*postRegexOpt).contains("org") ? (*postRegexOpt)["org"].value_or("") :
                (*postRegexOpt)["searchStr"].value_or("");
            if (postRegexOrg.empty()) {
                continue;
            }
            std::string postRegexRep = (*postRegexOpt).contains("rep") ? (*postRegexOpt)["rep"].value_or("") :
                (*postRegexOpt)["replaceStr"].value_or("");
            RegexPattern postRegex;
            icu::UnicodeString postRegexUStr = icu::UnicodeString::fromUTF8(postRegexOrg);
            icu::UnicodeString repUStr = icu::UnicodeString::fromUTF8(postRegexRep);
            UErrorCode status = U_ZERO_ERROR;
            postRegex.pattern = std::shared_ptr<icu::RegexPattern>(icu::RegexPattern::compile(postRegexUStr, 0, status));
            if (U_FAILURE(status)) {
                throw std::runtime_error("后处理正则编译失败: " + postRegexOrg);
            }
            postRegex.rep = repUStr;
            m_postRegexPatterns.push_back(postRegex);
        }

    }
    catch (const toml::parse_error& e) {
        m_logger->critical("项目配置文件解析失败, 错误位置: {}, 错误信息: {}", stream2String(e.source().begin), e.description());
        throw std::runtime_error(e.what());
    }
}


void EpubTranslator::run()
{
    m_logger->info("GalTransl++ EpubTranlator 启动...");

    for (const auto& dir : { m_epubInputDir, m_epubOutputDir }) {
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
            m_logger->info("已创建目录: {}", wide2Ascii(dir));
        }
    }

    for (const auto& dir : { m_tempUnpackDir, m_tempRebuildDir, m_inputDir, m_outputDir }) {
        fs::remove_all(dir);
        fs::create_directories(dir);
    }

    std::vector<fs::path> epubFiles;
    for (const auto& entry : fs::recursive_directory_iterator(m_epubInputDir)) {
        if (entry.is_regular_file() && isSameExtension(entry.path(), L".epub")) {
            epubFiles.push_back(entry.path());
        }
    }
    if (epubFiles.empty()) {
        throw std::runtime_error("未找到 EPUB 文件");
    }

    for (const auto& epubPath : epubFiles) {
        fs::path relEpubPath = fs::relative(epubPath, m_epubInputDir);
        fs::path bookUnpackDir = m_tempUnpackDir / relEpubPath.parent_path() / relEpubPath.stem();
        m_logger->debug("解压 {} 到 {}", wide2Ascii(epubPath), wide2Ascii(bookUnpackDir));
        fs::create_directories(bookUnpackDir);

        int error = 0;
        zip* za = zip_open(wide2Ascii(epubPath).c_str(), 0, &error);
        if (!za) { 
            throw std::runtime_error("无法打开 EPUB 文件: " + wide2Ascii(epubPath));
        }
        zip_int64_t num_entries = zip_get_num_entries(za, 0);
        for (zip_int64_t i = 0; i < num_entries; i++) {
            zip_stat_t zs;
            zip_stat_index(za, i, 0, &zs);
            fs::path outpath = bookUnpackDir / ascii2Wide(zs.name);
            if (zs.name[strlen(zs.name) - 1] == '/') {
                fs::create_directories(outpath);
            }
            else {
                zip_file* zf = zip_fopen_index(za, i, 0);
                if (!zf) {
                    throw std::runtime_error("Epub 文件解压失败: " + wide2Ascii(epubPath));
                }
                std::vector<char> buffer(zs.size);
                zip_fread(zf, buffer.data(), zs.size);
                zip_fclose(zf);
                fs::create_directories(outpath.parent_path());
                std::ofstream ofs(outpath, std::ios::binary);
                ofs.write(buffer.data(), buffer.size());
            }
        }
        zip_close(za);
    }

    for (const auto& epubPath : epubFiles) {
        fs::path relEpubPath = fs::relative(epubPath, m_epubInputDir);
        fs::path bookDirPath = m_tempUnpackDir / relEpubPath.parent_path() / relEpubPath.stem();

        std::string bookName = wide2Ascii(relEpubPath.parent_path() / relEpubPath.stem());
        for (const auto& htmlEntry : fs::recursive_directory_iterator(bookDirPath)) {
            if (htmlEntry.is_regular_file() && (isSameExtension(htmlEntry.path(), L".html") || isSameExtension(htmlEntry.path(), L".xhtml"))) {

                std::ifstream ifs(htmlEntry.path(), std::ios::binary);
                std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

                // 预处理正则替换
                icu::UnicodeString contentUStr = icu::UnicodeString::fromUTF8(content);
                for (const auto& preRegex : m_preRegexPatterns) {
                    UErrorCode status = U_ZERO_ERROR;
                    std::unique_ptr<icu::RegexMatcher> matcher(preRegex.pattern->matcher(contentUStr, status));
                    if (U_FAILURE(status)) {
                        m_logger->error("预处理正则替换失败");
                        continue;
                    }
                    contentUStr = matcher->replaceAll(preRegex.rep, status);
                    if (U_FAILURE(status)) {
                        m_logger->error("预处理正则替换失败");
                    }
                }
                content.clear();
                content = contentUStr.toUTF8String(content);

                GumboOutput* output = gumbo_parse(content.c_str());
                std::vector<std::pair<std::string, EpubTextNodeInfo>> sentences;
                extractTextNodes(output->root, sentences);
                gumbo_destroy_output(&kGumboDefaultOptions, output);

                if (sentences.empty()) continue;

                // 创建扁平化文件名
                fs::path relativePath = fs::relative(htmlEntry.path(), bookDirPath);
                std::string flatFileNameStr = bookName + "_" + wide2Ascii(relativePath);
                std::replace(flatFileNameStr.begin(), flatFileNameStr.end(), '/', '_');
                std::replace(flatFileNameStr.begin(), flatFileNameStr.end(), '\\', '_');
                fs::path flatJsonFileName = fs::path(ascii2Wide(flatFileNameStr)).replace_extension(".json");

                // 存储映射关系
                m_flatToOriginalPathMap[wide2Ascii(flatJsonFileName)] = htmlEntry.path();

                // 存储元数据
                std::ranges::sort(sentences, [](const auto& a, const auto& b)
                    {
                        return a.second.offset < b.second.offset;
                    });
                std::vector<EpubTextNodeInfo> metadata;
                json j = json::array();
                for (const auto& p : sentences) {
                    j.push_back({ {"name", ""}, {"message", p.first} });
                    metadata.push_back(p.second);
                }
                m_htmlMetadata[wide2Ascii(flatJsonFileName)] = metadata;

                std::ofstream ofs(m_inputDir / flatJsonFileName);
                ofs << j.dump(2);
            }
        }
    }

    NormalJsonTranslator::run();

    for (const auto& epubPath : epubFiles) {
        fs::path relEpubPath = fs::relative(epubPath, m_epubInputDir);
        fs::path bookDirPath = m_tempUnpackDir / relEpubPath.parent_path() / relEpubPath.stem();
        fs::path bookRebuildPath = m_tempRebuildDir / relEpubPath.parent_path() / relEpubPath.stem();
        createParent(bookRebuildPath);
        fs::copy(bookDirPath, bookRebuildPath, fs::copy_options::recursive);
    }

    for (const auto& translatedJsonEntry : fs::directory_iterator(m_outputDir)) {
        std::string flatName = wide2Ascii(translatedJsonEntry.path().filename());
        if (!m_flatToOriginalPathMap.count(flatName)) continue;

        fs::path originalHtmlPath = m_flatToOriginalPathMap[flatName];
        fs::path rebuiltHtmlPath = m_tempRebuildDir / fs::relative(originalHtmlPath, m_tempUnpackDir);
        const auto& metadata = m_htmlMetadata[flatName];

        // 替换 HTML 内容的逻辑
        std::ifstream ifs(rebuiltHtmlPath, std::ios::binary);
        std::string originalContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ifs.close();

        // 预处理正则替换
        icu::UnicodeString contentUStr = icu::UnicodeString::fromUTF8(originalContent);
        for (const auto& preRegex : m_preRegexPatterns) {
            UErrorCode status = U_ZERO_ERROR;
            std::unique_ptr<icu::RegexMatcher> matcher(preRegex.pattern->matcher(contentUStr, status));
            if (U_FAILURE(status)) {
                m_logger->error("预处理正则替换失败");
                continue;
            }
            contentUStr = matcher->replaceAll(preRegex.rep, status);
            if (U_FAILURE(status)) {
                m_logger->error("预处理正则替换失败");
            }
        }
        originalContent.clear();
        originalContent = contentUStr.toUTF8String(originalContent);

        ifs.open(translatedJsonEntry.path());
        json translatedData = json::parse(ifs);
        ifs.close();

        if (metadata.size() != translatedData.size()) {
            throw std::runtime_error(std::format("元数据和翻译数据数量不匹配，无法重组: {}", wide2Ascii(rebuiltHtmlPath)));
        }

        std::string newContent;
        newContent.reserve(originalContent.length() * 2);
        size_t lastPos = 0;

        for (size_t i = 0; i < metadata.size(); ++i) {
            std::string translatedText = translatedData[i].value("message", "");
            std::string replacement = m_bilingualOutput ? 
                (translatedText + "<br/><span style=\"color:" + m_originalTextColor + "; font-size:" + m_originalTextScale +
                    "em;\">" + originalContent.substr(metadata[i].offset, metadata[i].length) + "</span>") 
                : translatedText;
            newContent.append(originalContent.substr(lastPos, metadata[i].offset - lastPos));
            newContent.append(replacement);
            lastPos = metadata[i].offset + metadata[i].length;
        }
        if (lastPos < originalContent.length()) {
            newContent.append(originalContent.substr(lastPos));
        }

        // 后处理正则替换
        icu::UnicodeString newContentUStr = icu::UnicodeString::fromUTF8(newContent);
        for (const auto& postRegex : m_postRegexPatterns) {
            UErrorCode status = U_ZERO_ERROR;
            std::unique_ptr<icu::RegexMatcher> matcher(postRegex.pattern->matcher(newContentUStr, status));
            if (U_FAILURE(status)) {
                m_logger->error("后处理正则替换失败");
                continue;
            }
            newContentUStr = matcher->replaceAll(postRegex.rep, status);
            if (U_FAILURE(status)) {
                m_logger->error("后处理正则替换失败");
            }
        }
        newContent.clear();
        newContent = newContentUStr.toUTF8String(newContent);

        std::ofstream ofs(rebuiltHtmlPath, std::ios::binary);
        ofs << newContent;
    }


    fs::create_directories(m_epubOutputDir);
    for (const auto& epubPath : epubFiles) {
        fs::path relEpubPath = fs::relative(epubPath, m_epubInputDir);
        fs::path bookDirPath = m_tempRebuildDir / relEpubPath.parent_path() / relEpubPath.stem();

        fs::path outputEpubPath = m_epubOutputDir / relEpubPath;
        createParent(outputEpubPath);
        m_logger->debug("正在打包 {}", wide2Ascii(outputEpubPath));

        int error = 0;
        zip* za = zip_open(wide2Ascii(outputEpubPath).c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
        if (!za) {
            throw std::runtime_error("无法创建 EPUB (zip) 文件: " + std::to_string(error));
        }

        const fs::path& sourceDir = bookDirPath;

        // --- 步骤一：优先处理 mimetype 文件，且不压缩 ---
        fs::path mimetypePath = sourceDir / "mimetype";
        if (fs::exists(mimetypePath)) {
            zip_source_t* s = zip_source_file(za, wide2Ascii(mimetypePath).c_str(), 0, 0);
            if (!s) {
                zip_close(za);
                throw std::runtime_error("无法为 mimetype 创建 zip_source_file");
            }

            zip_int64_t idx = zip_file_add(za, "mimetype", s, ZIP_FL_ENC_UTF_8);
            if (idx < 0) {
                zip_source_free(s);
                zip_close(za);
                throw std::runtime_error("无法将 mimetype 添加到 zip");
            }

            if (zip_set_file_compression(za, idx, ZIP_CM_STORE, 0) < 0) {
                throw std::runtime_error("无法将 mimetype 设置为不压缩模式。");
            }
        }
        else {
            m_logger->warn("在源目录 {} 中未找到 mimetype 文件，生成的 EPUB 可能无效。", wide2Ascii(sourceDir));
        }

        // --- 步骤二：处理其他所有文件和目录 ---
        for (const auto& entry : fs::recursive_directory_iterator(sourceDir)) {
            fs::path relativePath = fs::relative(entry.path(), sourceDir);
            std::string entryName = wide2Ascii(relativePath);
            std::replace(entryName.begin(), entryName.end(), '\\', '/');

            if (entryName == "mimetype") {
                continue;
            }

            if (fs::is_directory(entry.path())) {
                zip_dir_add(za, entryName.c_str(), ZIP_FL_ENC_UTF_8);
            }
            else {
                zip_source_t* s = zip_source_file(za, wide2Ascii(entry.path()).c_str(), 0, 0);
                if (!s) {
                    throw std::runtime_error(std::format("无法为文件 {} 创建 zip_source_file", entryName));
                }
                if (zip_file_add(za, entryName.c_str(), s, ZIP_FL_ENC_UTF_8) < 0) {
                    zip_source_free(s);
                    throw std::runtime_error(std::format("无法将文件 {} 添加到 zip", entryName));
                }
            }
        }

        // 所有 source 都在 zip_close 中被 libzip 自动管理和释放
        if (zip_close(za) < 0) {
            throw std::runtime_error("关闭 zip 存档时出错: " + std::string(zip_strerror(za)));
        }
        else {
            m_logger->info("已重建 EPUB 文件: {}", wide2Ascii(outputEpubPath));
        }
    }
}
