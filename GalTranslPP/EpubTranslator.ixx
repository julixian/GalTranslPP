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

    struct CallbackPattern {
        std::shared_ptr<icu::RegexPattern> org;
        icu::UnicodeString rep;
    };

    struct RegexPattern {
        std::shared_ptr<icu::RegexPattern> org;
        icu::UnicodeString rep;

        bool isCallback;
        std::multimap<int, CallbackPattern> callbackPatterns;
    };

    class EpubTranslator : public NormalJsonTranslator {

    private:

        fs::path m_epubInputDir;
        fs::path m_epubOutputDir;
        fs::path m_tempUnpackDir;
        fs::path m_tempRebuildDir;

        // EPUB 处理相关的配置
        bool m_bilingualOutput;
        std::string m_originalTextColor;
        std::string m_originalTextScale;

        // 预处理正则和后处理正则
        std::vector<RegexPattern> m_preRegexPatterns;
        std::vector<RegexPattern> m_postRegexPatterns;


        // 存储json文件相对路径到句子元数据的映射
        // Key: json文件相对路径 (e.g., "dir1/book1/OEBPS/chapter1.json")
        // Value: 元数据
        std::map<fs::path, std::vector<EpubTextNodeInfo>> m_jsonToMetadataMap;

        // 存储json文件相对路径到原始HTML完整路径的映射
        std::map<fs::path, fs::path> m_jsonToHtmlPathMap;

        // 存储json文件相对路径到其所属epub完整路径的映射
        std::map<fs::path, fs::path> m_jsonToEpubPathMap;

        // 存储json文件相对路径到 normal_post 完整路径的映射
        std::map<fs::path, fs::path> m_jsonToNormalPostMap;

        // 每个epub完整路径对应的多个json文件相对路径以及有没有处理完毕
        std::map<fs::path, std::map<fs::path, bool>> m_epubToJsonsMap;

    public:

        virtual void run() override;

        EpubTranslator(const fs::path& projectDir, std::shared_ptr<IController> controller, std::shared_ptr<spdlog::logger> logger);

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

EpubTranslator::EpubTranslator(const fs::path& projectDir, std::shared_ptr<IController> controller, std::shared_ptr<spdlog::logger> logger) :
    NormalJsonTranslator(projectDir, controller, logger,
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

        auto readRegexArr = [](const toml::array& regexArr, std::vector<RegexPattern>& patterns)
            {
                for (const auto& elem : regexArr) {
                    auto regexTbl = elem.as_table();
                    if (!regexTbl) {
                        continue;
                    }
                    std::string regexOrg = regexTbl->contains("org") ? (*regexTbl)["org"].value_or("") :
                        (*regexTbl)["searchStr"].value_or("");
                    if (regexOrg.empty()) {
                        continue;
                    }

                    RegexPattern regexPattern;
                    icu::UnicodeString regexUStr = icu::UnicodeString::fromUTF8(regexOrg);
                    UErrorCode status = U_ZERO_ERROR;
                    regexPattern.org = std::shared_ptr<icu::RegexPattern>(icu::RegexPattern::compile(regexUStr, 0, status));
                    if (U_FAILURE(status)) {
                        throw std::runtime_error("预处理正则编译失败: " + regexOrg);
                    }
                    regexPattern.isCallback = regexTbl->contains("callback");

                    if (regexPattern.isCallback) {
                        auto callbackArr = (*regexTbl)["callback"].as_array();
                        if (!callbackArr) {
                            throw std::runtime_error("预处理正则回调配置错误: " + regexOrg);
                        }
                        for (const auto& callbackElem : *callbackArr) {
                            auto callbackTbl = callbackElem.as_table();
                            if (!callbackTbl) {
                                continue;
                            }
                            CallbackPattern callbackPattern;
                            int group = (*callbackTbl)["group"].value_or(0);
                            if (group == 0) {
                                continue;
                            }
                            std::string callbackOrg = callbackTbl->contains("org") ? (*callbackTbl)["org"].value_or("") :
                                (*callbackTbl)["searchStr"].value_or("");
                            if (callbackOrg.empty()) {
                                continue;
                            }
                            std::string callbackRep = callbackTbl->contains("rep") ? (*callbackTbl)["rep"].value_or("") :
                                (*callbackTbl)["replaceStr"].value_or("");
                            icu::UnicodeString callbackUStr = icu::UnicodeString::fromUTF8(callbackOrg);
                            callbackPattern.org = std::shared_ptr<icu::RegexPattern>(icu::RegexPattern::compile(callbackUStr, 0, status));
                            if (U_FAILURE(status)) {
                                throw std::runtime_error("预处理正则回调正则编译失败: " + callbackOrg);
                            }
                            callbackPattern.rep = icu::UnicodeString::fromUTF8(callbackRep);
                            regexPattern.callbackPatterns.insert(std::make_pair(group, callbackPattern));
                        }
                    }
                    else {
                        std::string regexRep = regexTbl->contains("rep") ? (*regexTbl)["rep"].value_or("") :
                            (*regexTbl)["replaceStr"].value_or("");
                        icu::UnicodeString repUStr = icu::UnicodeString::fromUTF8(regexRep);
                        regexPattern.rep = repUStr;
                    }

                    patterns.push_back(regexPattern);
                }
            };

        auto preRegexArr = parseToml<toml::array>(projectConfig, pluginConfig, "plugins.Epub.预处理正则");
        readRegexArr(preRegexArr, m_preRegexPatterns);
        auto postRegexArr = parseToml<toml::array>(projectConfig, pluginConfig, "plugins.Epub.后处理正则");
        readRegexArr(postRegexArr, m_postRegexPatterns);
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

    // 正则替换
    auto regexReplace = [this](const std::vector<RegexPattern>& regexPatterns, std::string& content)
        {
            icu::UnicodeString contentUStr = icu::UnicodeString::fromUTF8(content);
            for (const auto& reg : regexPatterns) {
                UErrorCode status = U_ZERO_ERROR;
                std::unique_ptr<icu::RegexMatcher> matcher(reg.org->matcher(contentUStr, status));
                if (U_FAILURE(status)) {
                    m_logger->error("预处理正则匹配失败");
                    continue;
                }
                if (reg.isCallback) {
                    icu::UnicodeString newContentUstr;
                    while (matcher->find() && U_SUCCESS(status)) {
                        icu::UnicodeString patternUstr;
                        for (int32_t i = 1; i < matcher->groupCount() + 1; i++) {
                            icu::UnicodeString groupUstr = matcher->group(i, status);
                            auto equalRange = reg.callbackPatterns.equal_range(i);
                            for (auto it = equalRange.first; it != equalRange.second; ++it) {
                                std::unique_ptr<icu::RegexMatcher> callbackMatcher(it->second.org->matcher(groupUstr, status));
                                if (U_FAILURE(status)) {
                                    m_logger->error("预处理正则回调匹配失败");
                                    continue;
                                }
                                groupUstr = callbackMatcher->replaceAll(it->second.rep, status);
                                if (U_FAILURE(status)) {
                                    m_logger->error("预处理正则回调替换失败");
                                    continue;
                                }
                            }
                            patternUstr.append(groupUstr);
                        }
                        matcher->appendReplacement(newContentUstr, patternUstr, status);
                    }
                    matcher->appendTail(newContentUstr);
                    contentUStr = newContentUstr;
                }
                else {
                    contentUStr = matcher->replaceAll(reg.rep, status);
                    if (U_FAILURE(status)) {
                        m_logger->error("预处理正则替换失败");
                    }
                }
            }
            content.clear();
            contentUStr.toUTF8String(content);
        };


    for (const auto& epubPath : epubFiles) {
        fs::path relEpubPath = fs::relative(epubPath, m_epubInputDir); // dir1/book1.epub
        fs::path bookUnpackPath = m_tempUnpackDir / relEpubPath.parent_path() / relEpubPath.stem(); // cache/myproject/epub_unpacked/dir1/book1
        fs::path bookRebuildPath = m_tempRebuildDir / relEpubPath.parent_path() / relEpubPath.stem(); // cache/myproject/epub_rebuild/dir1/book1

        // 解压 EPUB 文件
        m_logger->debug("解压 {} 到 {}", wide2Ascii(epubPath), wide2Ascii(bookUnpackPath));
        fs::create_directories(bookUnpackPath);
        int error = 0;
        zip* za = zip_open(wide2Ascii(epubPath).c_str(), 0, &error);
        if (!za) { 
            throw std::runtime_error("无法打开 EPUB 文件: " + wide2Ascii(epubPath));
        }
        zip_int64_t num_entries = zip_get_num_entries(za, 0);
        std::ofstream ofs;
        for (zip_int64_t i = 0; i < num_entries; i++) {
            zip_stat_t zs;
            zip_stat_index(za, i, 0, &zs);
            fs::path outpath = bookUnpackPath / ascii2Wide(zs.name);
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
                ofs.open(outpath, std::ios::binary);
                ofs.write(buffer.data(), buffer.size());
                ofs.close();
            }
        }
        zip_close(za);


        // 从html中提取json和元数据
        createParent(bookRebuildPath);
        fs::copy(bookUnpackPath, bookRebuildPath, fs::copy_options::recursive);
        fs::path relBookDir = relEpubPath.parent_path() / relEpubPath.stem(); // dir1/book1
        for (const auto& htmlEntry : fs::recursive_directory_iterator(bookUnpackPath)) {
            if (htmlEntry.is_regular_file() && (isSameExtension(htmlEntry.path(), L".html") || isSameExtension(htmlEntry.path(), L".xhtml"))) {

                std::ifstream ifs(htmlEntry.path(), std::ios::binary);
                std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

                regexReplace(m_preRegexPatterns, content);

                GumboOutput* output = gumbo_parse(content.c_str());
                std::vector<std::pair<std::string, EpubTextNodeInfo>> sentences;
                extractTextNodes(output->root, sentences);
                gumbo_destroy_output(&kGumboDefaultOptions, output);

                if (sentences.empty()) continue;

                // 创建json相对路径
                fs::path relativePath = fs::relative(htmlEntry.path(), bookUnpackPath); // OEBPS/chapter1.html
                fs::path showNormalHtmlPath = m_projectDir / L"epub_show_normal" / relBookDir / relativePath;
                fs::path showNormalPostHtmlPath = m_projectDir / L"epub_show_normal_post" / relBookDir / relativePath;
                fs::path relJsonPath = relBookDir / relativePath.replace_extension(".json"); // dir1/book1/OEBPS/chapter1.json


                // 存储映射关系
                m_jsonToHtmlPathMap[relJsonPath] = htmlEntry.path();
                m_jsonToEpubPathMap[relJsonPath] = epubPath;
                m_jsonToNormalPostMap[relJsonPath] = showNormalPostHtmlPath;
                m_epubToJsonsMap[epubPath].insert(std::make_pair(relJsonPath, false));

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
                m_jsonToMetadataMap[relJsonPath] = metadata;

                createParent(m_inputDir / relJsonPath); // cache/myproject/epub_json_input/dir1/book1/OEBPS/chapter1.json
                std::ofstream ofs;
                ofs.open(m_inputDir / relJsonPath);
                ofs << j.dump(2);
                ofs.close();

                createParent(showNormalHtmlPath);
                ofs.open(showNormalHtmlPath, std::ios::binary);
                ofs << content;
                ofs.close();
            }
        }
    }

    m_onFileProcessed = [this, &regexReplace](fs::path relProcessedFile)
        {
            if (!m_jsonToEpubPathMap.count(relProcessedFile) || !m_epubToJsonsMap.count(m_jsonToEpubPathMap[relProcessedFile])) {
                m_logger->warn("未找到与 {} 对应的元数据，跳过", wide2Ascii(relProcessedFile));
                return;
            }
            fs::path epubPath = m_jsonToEpubPathMap[relProcessedFile];
            std::map<fs::path, bool>& jsonsMap = m_epubToJsonsMap[epubPath];
            jsonsMap[relProcessedFile] = true;
            if (
                std::ranges::any_of(jsonsMap, [](const auto& p)
                    {
                        return !p.second;
                    })
                ) 
            {
                return;
            }

            // 这本epub的所有文件都翻译完毕，可以开始重组
            for (const auto& [relJsonPath, isProcessed] : jsonsMap) {
                if (!m_jsonToHtmlPathMap.count(relJsonPath) || !m_jsonToMetadataMap.count(relJsonPath)) continue;

                fs::path originalHtmlPath = m_jsonToHtmlPathMap[relJsonPath];
                fs::path rebuiltHtmlPath = m_tempRebuildDir / fs::relative(originalHtmlPath, m_tempUnpackDir);
                const auto& metadata = m_jsonToMetadataMap[relJsonPath];

                // 替换 HTML 内容的逻辑
                std::ifstream ifs(rebuiltHtmlPath, std::ios::binary);
                std::string originalContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                ifs.close();

                // 预处理正则替换
                regexReplace(m_preRegexPatterns, originalContent);

                ifs.open(m_outputDir / relJsonPath);
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
                regexReplace(m_postRegexPatterns, newContent);

                std::ofstream ofs;
                ofs.open(rebuiltHtmlPath, std::ios::binary);
                ofs << newContent;
                ofs.close();

                fs::path showNormalPostHtmlPath = m_jsonToNormalPostMap[relJsonPath];
                createParent(showNormalPostHtmlPath);
                ofs.open(showNormalPostHtmlPath, std::ios::binary);
                ofs << newContent;
                ofs.close();
            }

            fs::path relEpubPath = fs::relative(epubPath, m_epubInputDir);
            fs::path bookRebuildPath = m_tempRebuildDir / relEpubPath.parent_path() / relEpubPath.stem();

            fs::path outputEpubPath = m_epubOutputDir / relEpubPath;
            createParent(outputEpubPath);
            m_logger->debug("正在打包 {}", wide2Ascii(outputEpubPath));

            int error = 0;
            zip* za = zip_open(wide2Ascii(outputEpubPath).c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
            if (!za) {
                throw std::runtime_error("无法创建 EPUB (zip) 文件: " + std::to_string(error));
            }

            // --- 步骤一：优先处理 mimetype 文件，且不压缩 ---
            fs::path mimetypePath = bookRebuildPath / "mimetype";
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
                m_logger->warn("在源目录 {} 中未找到 mimetype 文件，生成的 EPUB 可能无效。", wide2Ascii(bookRebuildPath));
            }

            // --- 步骤二：处理其他所有文件和目录 ---
            for (const auto& entry : fs::recursive_directory_iterator(bookRebuildPath)) {
                fs::path relativePath = fs::relative(entry.path(), bookRebuildPath);
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
        };


    NormalJsonTranslator::run();
}
