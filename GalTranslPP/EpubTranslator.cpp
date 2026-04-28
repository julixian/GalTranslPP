module;

#define PCRE2_HEADERS
#include "GPPMacros.hpp"
#include <zip.h>
#pragma  warning( push ) 
#pragma  warning( disable: 4005 ) 
#include <gumbo.h>
#pragma  warning( pop ) 
#include <toml.hpp>

module EpubTranslator;

import Tool;

namespace fs = std::filesystem;

// 递归遍历 Gumbo 树以提取文本节点
void extractTextNodes(const GumboNode* node, std::vector<std::pair<std::string, EpubTextNodeInfo>>& sentences) {
    if (node->type == GUMBO_NODE_TEXT) {
        std::string text = node->v.text.text;
        if (text.empty() || text.find_first_not_of(" \t\n\r") == std::string::npos) {
            return;
        }
        EpubTextNodeInfo info;
        info.offset = node->v.text.start_pos.offset;
        info.length = text.length();
        sentences.push_back({ std::move(text), info });
        return;
    }

    if (node->type != GUMBO_NODE_ELEMENT || node->v.element.tag == GUMBO_TAG_SCRIPT || node->v.element.tag == GUMBO_TAG_STYLE) {
        return;
    }

    const GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        extractTextNodes(static_cast<GumboNode*>(children->data[i]), sentences);
    }
}

EpubTranslator::~EpubTranslator() 
{
    m_logger->info("所有任务已完成！EpubTranslator结束。");
}

EpubTranslator::EpubTranslator(const fs::path& projectDir, const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger) :
    NormalJsonTranslator(projectDir, controller, logger,
        // m_inputDir                                                m_inputCacheDir
        // m_outputDir                                               m_outputCacheDir
        L"cache" / projectDir.filename() / L"epub_json_input", L"cache" / projectDir.filename() / L"gt_input_cache",
        L"cache" / projectDir.filename() / L"epub_json_output", L"cache" / projectDir.filename() / L"gt_output_cache")
{
    m_logger->info("GalTransl++ EpubTranslator 启动...");
}

void EpubTranslator::epubInit()
{
    m_epubInputDir = m_projectDir / L"gt_input";
    m_epubOutputDir = m_projectDir / L"gt_output";
    m_tempUnpackDir = L"cache" / m_projectDir.filename() / L"epub_unpacked";
    m_tempRebuildDir = L"cache" / m_projectDir.filename() / L"epub_rebuild";

    try {
        const auto projectConfig = toml::uparse(m_projectDir / L"config.toml");
        const auto pluginConfig = toml::uparse(filePluginConfigPath / L"Epub.toml");

        m_bilingualOutput = parseToml<bool>(projectConfig, pluginConfig, "plugins.Epub.双语显示");
        m_originalTextColor = parseToml<std::string>(projectConfig, pluginConfig, "plugins.Epub.原文颜色");
        m_originalTextScale = std::to_string(parseToml<double>(projectConfig, pluginConfig, "plugins.Epub.缩小比例"));

        auto readRegexArr = [](const toml::array& regexArr, std::vector<RegexPattern>& patterns)
            {
                for (const auto& regexTbl : regexArr) {
                    const std::string regexOrg = regexTbl.contains("org") ? toml::find_or(regexTbl, "org", "") :
                        toml::find_or(regexTbl, "searchStr", "");
                    if (regexOrg.empty()) {
                        continue;
                    }

                    RegexPattern regexPattern;
                    const std::string compileModifier = toml::find_or(regexTbl, "compile_modifier", defaultRegCompileModifier);
                    const std::string replaceModifier = toml::find_or(regexTbl, "replace_modifier", defaultRegReplaceModifier);
                    regexPattern.org->setPattern(regexOrg).setModifier(compileModifier).compile();
                    regexPattern.rep->setModifier(replaceModifier);
                    if (!regexPattern.org) {
                        throw std::runtime_error("预处理正则编译失败: " + regexOrg);
                    }
                    regexPattern.isCallback = regexTbl.contains("callback");

                    if (regexPattern.isCallback) {
                        const auto& callbackArr = toml::get<toml::array>(regexTbl.at("callback"));
                        for (const auto& callbackTbl : callbackArr) {
                            if (!callbackTbl.is_table()) {
                                continue;
                            }
                            CallbackPattern callbackPattern;
                            int group = toml::find_or(callbackTbl, "group", 0);
                            if (group == 0) {
                                continue;
                            }
                            const std::string callbackOrg = callbackTbl.contains("org") ? toml::find_or(callbackTbl, "org", "") :
                                toml::find_or(callbackTbl, "searchStr", "");
                            if (callbackOrg.empty()) {
                                continue;
                            }
                            const std::string callbackRep = callbackTbl.contains("rep") ? toml::find_or(callbackTbl, "rep", "") :
                                toml::find_or(callbackTbl, "replaceStr", "");
                            const std::string callbackCompileModifier = toml::find_or(callbackTbl, "compile_modifier", defaultRegCompileModifier);
                            const std::string callbackReplaceModifier = toml::find_or(callbackTbl, "replace_modifier", defaultRegReplaceModifier);
                            callbackPattern.org->setPattern(callbackOrg).setModifier(callbackCompileModifier).compile();
                            callbackPattern.rep->setModifier(callbackReplaceModifier);
                            if (!callbackPattern.org) {
                                throw std::runtime_error(std::format("预处理正则回调正则编译失败: [{}]", callbackOrg));
                            }
                            callbackPattern.rep->setReplaceWith(callbackRep);
                            regexPattern.callbackPatterns.insert({ group, std::move(callbackPattern) });
                        }
                    }
                    else {
                        const std::string& regexRep = regexTbl.contains("rep") ? toml::find_or(regexTbl, "rep", "") :
                            toml::find_or(regexTbl, "replaceStr", "");
                        regexPattern.rep->setReplaceWith(regexRep);
                    }

                    patterns.push_back(std::move(regexPattern));
                }
            };

        const auto& preRegexArr = parseToml<toml::array>(projectConfig, pluginConfig, "plugins.Epub.preprocRegex");
        readRegexArr(preRegexArr, m_preRegexPatterns);
        const auto& postRegexArr = parseToml<toml::array>(projectConfig, pluginConfig, "plugins.Epub.postprocRegex");
        readRegexArr(postRegexArr, m_postRegexPatterns);
    }
    catch (const toml::exception& e) {
        m_logger->critical("Epub 配置文件解析失败");
        throw std::runtime_error(e.what());
    }
}


void EpubTranslator::epubBeforeRun()
{
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
    auto regexReplace = [this](std::vector<RegexPattern>& regexPatterns, std::string& content)
        {
            for (RegexPattern& reg : regexPatterns) {
                reg.rep->setSubject(&content);
                if (reg.isCallback) {
                    content = reg.rep->nreplace(jpc::MatchEvaluator([&](const jpc::NumSub& m1, void*, void*)
                        {
                            std::string result;
                            for (size_t i = 1; i < m1.size(); i++) {
                                std::string groupStr = m1[i];
                                const auto [first, last] = reg.callbackPatterns.equal_range((int)i);
                                for (auto it = first; it != last; ++it) {
                                    groupStr = it->second.rep->setSubject(&groupStr).replace();
                                }
                                result.append(groupStr);
                            }
                            return result;
                        }));
                }
                else {
                    content = reg.rep->replace();
                }
            }
        };


    for (const auto& epubPath : epubFiles) {
        const fs::path relEpubPath = fs::relative(epubPath, m_epubInputDir); // dir1/book1.epub
        const fs::path bookUnpackPath = m_tempUnpackDir / relEpubPath.parent_path() / relEpubPath.stem(); // cache/myproject/epub_unpacked/dir1/book1
        const fs::path bookRebuildPath = m_tempRebuildDir / relEpubPath.parent_path() / relEpubPath.stem(); // cache/myproject/epub_rebuild/dir1/book1

        // 解压 EPUB 文件
        m_logger->info("正在解压 {} 到 {}", wide2Ascii(epubPath), wide2Ascii(bookUnpackPath));
        fs::create_directories(bookUnpackPath);
        extractZip(epubPath, bookUnpackPath);

        // 从html中提取json和元数据
        createParent(bookRebuildPath);
        fs::copy(bookUnpackPath, bookRebuildPath, fs::copy_options::recursive);
        const fs::path relBookDir = relEpubPath.parent_path() / relEpubPath.stem(); // dir1/book1
        const std::vector<std::wstring> extensionsToProcess{ L".html", L".xhtml", L".htm", L".xhtm" };
        for (const auto& htmlEntry : fs::recursive_directory_iterator(bookUnpackPath)) {
            if (htmlEntry.is_regular_file() &&
                std::ranges::any_of(extensionsToProcess, [&](const std::wstring& ext)
	                {
                        return isSameExtension(htmlEntry.path(), ext);
	                })
                ) 
            {
                std::ifstream ifs(htmlEntry.path(), std::ios::binary);
                std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

                regexReplace(m_preRegexPatterns, content);

                GumboOutput* output = gumbo_parse(content.c_str());
                std::vector<std::pair<std::string, EpubTextNodeInfo>> sentences;
                extractTextNodes(output->root, sentences);
                gumbo_destroy_output(&kGumboDefaultOptions, output);

                if (sentences.empty()) continue;

                // 创建json相对路径
                const fs::path relativePath = fs::relative(htmlEntry.path(), bookUnpackPath); // OEBPS/chapter1.html
                const fs::path showNormalHtmlPath = m_projectDir / L"epub_show_normal" / relBookDir / relativePath;
                const fs::path showNormalPostHtmlPath = m_projectDir / L"epub_show_normal_post" / relBookDir / relativePath;
                const fs::path relJsonPath = relBookDir / fs::path(relativePath).replace_extension(L".json"); // dir1/book1/OEBPS/chapter1.json


                // 存储映射关系
                JsonInfo& info = m_jsonToInfoMap[relJsonPath];
                info.htmlPath = htmlEntry.path();
                info.epubPath = epubPath;
                info.normalPostPath = showNormalPostHtmlPath;
                info.content = std::move(content);
                m_epubToJsonsMap[epubPath].insert({ relJsonPath, false });

                // 存储元数据
                std::ranges::sort(sentences, [](const auto& a, const auto& b)
                    {
                        return a.second.offset < b.second.offset;
                    });
                std::vector<EpubTextNodeInfo> metadata;
                json j = json::array();
                for (const auto& [s, m] : sentences) {
                    // TODO: 添加可选正则区分 name 和 message
                    j.push_back({ {"message", s} });
                    metadata.push_back(m);
                }
                info.metadata = std::move(metadata);

                createParent(m_inputDir / relJsonPath); // cache/myproject/epub_json_input/dir1/book1/OEBPS/chapter1.json
                std::ofstream ofs;
                ofs.open(m_inputDir / relJsonPath, std::ios::binary);
                ofs << j.dump(2);
                ofs.close();

                createParent(showNormalHtmlPath);
                ofs.open(showNormalHtmlPath, std::ios::binary);
                ofs << info.content;
                ofs.close();
            }
        }
    }

    m_onFileProcessed = [this, regexReplace](fs::path relProcessedFile)
        {
            if (!m_jsonToInfoMap.contains(relProcessedFile)) {
                m_logger->error("[文件 {}] 未找到对应的元数据", wide2Ascii(relProcessedFile));
                return;
            }
            const JsonInfo& info = m_jsonToInfoMap[relProcessedFile];
            const fs::path& epubPath = info.epubPath;
            absl::flat_hash_map<fs::path, bool>& jsonsMap = m_epubToJsonsMap[epubPath];
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
            for (const auto& relJsonPath : jsonsMap | std::views::keys) {

                const JsonInfo& jsonInfo = m_jsonToInfoMap[relJsonPath];
                const fs::path& originalHtmlPath = jsonInfo.htmlPath;
                const fs::path rebuiltHtmlPath = m_tempRebuildDir / fs::relative(originalHtmlPath, m_tempUnpackDir);
                const auto& metadatas = jsonInfo.metadata;

                // 替换 HTML 内容的逻辑
                std::ifstream ifs;
                const std::string& originalContent = jsonInfo.content;

                ifs.open(m_outputDir / relJsonPath, std::ios::binary);
                json translatedDatas = json::parse(ifs);
                ifs.close();

                if (metadatas.size() != translatedDatas.size()) {
                    throw std::runtime_error(std::format("[文件 {}] 元数据和翻译数据数量不匹配，无法重组({}meta/{}trans)", wide2Ascii(rebuiltHtmlPath), metadatas.size(), translatedDatas.size()));
                }

                std::string newContent;
                newContent.reserve(originalContent.length() * 2);
                size_t lastPos = 0;

                for (auto [metadata, translatedData] : std::views::zip(metadatas, translatedDatas)) {
                    newContent.append(originalContent.c_str() + lastPos, metadata.offset - lastPos);
                    const std::string translatedText = translatedData["message"].get<std::string>();
                    const std::string replacement = m_bilingualOutput ?
                        std::format("{}<br/><span style=\"color:{}; font-size:{}em;\">{}</span>", translatedText, m_originalTextColor, m_originalTextScale,
                            std::string_view(originalContent.data() + metadata.offset, metadata.length))
                        : translatedText;
                    newContent.append(replacement);
                    lastPos = metadata.offset + metadata.length;
                }
                if (lastPos < originalContent.length()) {
                    newContent.append(originalContent.c_str() + lastPos, originalContent.length() - lastPos);
                }

                // 后处理正则替换
                regexReplace(m_postRegexPatterns, newContent);

                std::ofstream ofs;
                ofs.open(rebuiltHtmlPath, std::ios::binary);
                ofs << newContent;
                ofs.close();

                const fs::path& showNormalPostHtmlPath = jsonInfo.normalPostPath;
                createParent(showNormalPostHtmlPath);
                ofs.open(showNormalPostHtmlPath, std::ios::binary);
                ofs << newContent;
                ofs.close();
            }

            const fs::path relEpubPath = fs::relative(epubPath, m_epubInputDir);
            const fs::path bookRebuildPath = m_tempRebuildDir / relEpubPath.parent_path() / relEpubPath.stem();

            const fs::path outputEpubPath = m_epubOutputDir / relEpubPath;
            createParent(outputEpubPath);
            m_logger->debug("正在打包 {}", wide2Ascii(outputEpubPath));

            int error = 0;
            zip* za = zip_open(wide2Ascii(outputEpubPath).c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
            if (!za) {
                throw std::runtime_error(std::format("无法创建 EPUB (zip) 文件: {}", error));
            }

            // --- 步骤一：优先处理 mimetype 文件，且不压缩 ---
            if (const fs::path mimetypePath = bookRebuildPath / "mimetype"; fs::exists(mimetypePath)) {
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
                    zip_source_free(s);
                    zip_close(za);
                    throw std::runtime_error("无法将 mimetype 设置为不压缩模式。");
                }
            }
            else {
                m_logger->warn("在源目录 {} 中未找到 mimetype 文件，生成的 EPUB 可能无效。", wide2Ascii(bookRebuildPath));
            }

            // --- 步骤二：处理其他所有文件和目录 ---
            for (const auto& entry : fs::recursive_directory_iterator(bookRebuildPath)) {
                const fs::path relativePath = fs::relative(entry.path(), bookRebuildPath);
                std::string entryName = wide2Ascii(relativePath);
                std::ranges::replace(entryName, '\\', '/');

                if (entryName == "mimetype") {
                    continue;
                }

                if (fs::is_directory(entry.path())) {
                    zip_dir_add(za, entryName.c_str(), ZIP_FL_ENC_UTF_8);
                }
                else {
                    zip_source_t* s = zip_source_file(za, wide2Ascii(entry.path()).c_str(), 0, 0);
                    if (!s) {
                        zip_close(za);
                        throw std::runtime_error(std::format("无法为文件 {} 创建 zip_source_file", entryName));
                    }
                    if (zip_file_add(za, entryName.c_str(), s, ZIP_FL_ENC_UTF_8) < 0) {
                        zip_source_free(s);
                        zip_close(za);
                        throw std::runtime_error(std::format("无法将文件 {} 添加到 zip", entryName));
                    }
                }
            }

            // 所有 source 都在 zip_close 中被 libzip 自动管理和释放
            if (zip_close(za) < 0) {
                throw std::runtime_error("关闭 zip 存档时出错: " + std::string(zip_strerror(za)));
            }

            m_logger->info("已重建 EPUB 文件: {}", wide2Ascii(outputEpubPath));
        };
}

void EpubTranslator::run() {
    NormalJsonTranslator::normalJsonInit();
    EpubTranslator::epubInit();
    EpubTranslator::epubBeforeRun();
    std::optional<std::vector<fs::path>> relFilePathsOpt = NormalJsonTranslator::normalJsonBeforeRun();
    if (!relFilePathsOpt.has_value()) {
        return;
    }
    NormalJsonTranslator::normalJsonProcess(std::move(relFilePathsOpt.value()));
    NormalJsonTranslator::normalJsonAfterRun();
}
