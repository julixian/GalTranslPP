module;

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

namespace
{
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
}

EpubTranslator::~EpubTranslator() 
{
    m_logger->info(gppTr("EpubTranslator.~EpubTranslator", "所有任务已完成！EpubTranslator 结束")
        .toStdString());
}

EpubTranslator::EpubTranslator(const fs::path& projectDir, const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger) :
    NormalJsonTranslator(projectDir, controller, logger,
        // m_inputDir                                                m_inputCacheDir
        // m_outputDir                                               m_outputCacheDir
        L"cache" / projectDir.filename() / L"epub_json_input", L"cache" / projectDir.filename() / L"gt_input_cache",
        L"cache" / projectDir.filename() / L"epub_json_output", L"cache" / projectDir.filename() / L"gt_output_cache")
{
    m_logger->info(gppTr("EpubTranslator.EpubTranslator", "GalTransl++ EpubTranslator 启动...")
        .toStdString());
}

void EpubTranslator::epubInit()
{
    m_epubInputDir = m_projectDir / L"gt_input";
    m_epubOutputDir = m_projectDir / L"gt_output";
    m_tempUnpackDir = L"cache" / m_projectDir.filename() / L"epub_unpacked";
    m_tempRebuildDir = L"cache" / m_projectDir.filename() / L"epub_rebuild";

    try {
        const auto projectConfig = toml::uparse(m_projectDir / L"Config.toml");
        const auto pluginConfig = toml::uparse(filePluginConfigPath / L"Epub.toml");

        m_bilingualOutput = parseToml<bool>(projectConfig, pluginConfig, "plugins.Epub.bilingualOutput");
        m_originalTextColor = parseToml<std::string>(projectConfig, pluginConfig, "plugins.Epub.originalTextColor");
        m_originalTextScale = std::to_string(parseToml<double>(projectConfig, pluginConfig, "plugins.Epub.originalTextScale"));

        auto readRegexArr = [](const toml::array& regexArr, std::vector<RegexPattern>& patterns)
            {
                for (const auto& regexTbl : regexArr) {
                    const std::string regexOrg = toml::find_or(regexTbl, "org", "");
                    if (regexOrg.empty()) {
                        continue;
                    }

                    RegexPattern regexPattern;
                    const std::string compileModifier = toml::find_or(regexTbl, "compileModifier", defaultRegCompileModifier);
                    const std::string replaceModifier = toml::find_or(regexTbl, "replaceModifier", defaultRegReplaceModifier);
                    regexPattern.org->setPattern(regexOrg).setModifier(compileModifier).compile();
                    regexPattern.rep->setModifier(replaceModifier);
                    if (!regexPattern.org) {
                        throw std::runtime_error(gppTr("EpubTranslator.epubInit", "预处理正则 `%1` 编译失败")
                            .arg(regexOrg)
                            .toStdString());
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
                            const std::string callbackOrg = toml::find_or(callbackTbl, "org", "");
                            if (callbackOrg.empty()) {
                                continue;
                            }
                            const std::string callbackRep = toml::find_or(callbackTbl, "rep", "");
                            const std::string callbackCompileModifier = toml::find_or(callbackTbl, "compileModifier", defaultRegCompileModifier);
                            const std::string callbackReplaceModifier = toml::find_or(callbackTbl, "replaceModifier", defaultRegReplaceModifier);
                            callbackPattern.org->setPattern(callbackOrg).setModifier(callbackCompileModifier).compile();
                            callbackPattern.rep->setModifier(callbackReplaceModifier);
                            if (!callbackPattern.org) {
                                throw std::runtime_error(gppTr(
                                    "EpubTranslator.epubInit",
                                    "预处理正则回调正则 `%1` 编译失败")
                                    .arg(callbackOrg)
                                    .toStdString());
                            }
                            callbackPattern.rep->setReplaceWith(callbackRep);
                            regexPattern.callbackPatterns.insert({ group, std::move(callbackPattern) });
                        }
                    }
                    else {
                        const std::string& regexRep = toml::find_or(regexTbl, "rep", "");
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
        throw std::runtime_error(gppTr("EpubTranslator.epubInit", "Epub 配置文件解析失败: %1")
            .arg(e.what())
            .toStdString());
    }
}


void EpubTranslator::epubBeforeRun()
{
    for (const auto& dir : { m_epubInputDir, m_epubOutputDir }) {
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
            m_logger->info(gppTr("EpubTranslator.epubBeforeRun", "已创建目录: [%1]")
                .arg(wide2Ascii(dir))
                .toStdString());
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
        throw std::runtime_error(gppTr("EpubTranslator.epubBeforeRun", "未找到 EPUB 文件")
            .toStdString());
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
        m_logger->info(gppTr("EpubTranslator.epubBeforeRun", "正在解压 [%1] 到 [%2]")
            .arg(wide2Ascii(epubPath))
            .arg(wide2Ascii(bookUnpackPath))
            .toStdString());
        fs::create_directories(bookUnpackPath);
        extractZip(epubPath, bookUnpackPath);

        // 从html中提取json和元数据
        createParent(bookRebuildPath);
        fs::copy(bookUnpackPath, bookRebuildPath, fs::copy_options::recursive);
        const fs::path relBookDir = relEpubPath.parent_path() / relEpubPath.stem(); // dir1/book1
        static constexpr std::array<std::wstring_view, 4> extensionsToProcess{ L".html", L".xhtml", L".htm", L".xhtm" };
        for (const auto& htmlEntry : fs::recursive_directory_iterator(bookUnpackPath)) {
            if (htmlEntry.is_regular_file() &&
                std::ranges::any_of(extensionsToProcess, [&](const auto& ext)
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

                if (sentences.empty()) {
                    continue;
                }

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
                    j.push_back({ {"message", s} });
                    metadata.push_back(m);
                }
                info.metadata = std::move(metadata);

                std::ofstream ofs;
                atomicOutputFile(ofs, m_inputDir / relJsonPath, j.dump(2));
                atomicOutputFile(ofs, showNormalHtmlPath, info.content);
            }
        }
    }

    m_onFileProcessed = [this, regexReplace](fs::path relProcessedFile)
        {
            std::unique_lock<std::mutex> lock(m_onFileProcessedMutex);
            const auto it = m_jsonToInfoMap.find(relProcessedFile);
            if (it == m_jsonToInfoMap.end()) {
                m_logger->error(gppTr("EpubTranslator.epubBeforeRun", "[文件 %1] 未找到对应的元数据")
                    .arg(wide2Ascii(relProcessedFile))
                    .toStdString());
                return;
            }
            const JsonInfo& info = it->second;
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

            lock.unlock();
            // 这本epub的所有文件都翻译完毕，可以开始重组
            std::ifstream ifs;
            for (const auto& relJsonPath : jsonsMap | std::views::keys) {

                const JsonInfo& jsonInfo = m_jsonToInfoMap[relJsonPath];
                const fs::path& originalHtmlPath = jsonInfo.htmlPath;
                const fs::path rebuiltHtmlPath = m_tempRebuildDir / fs::relative(originalHtmlPath, m_tempUnpackDir);
                const auto& metadatas = jsonInfo.metadata;

                // 替换 HTML 内容的逻辑
                const std::string& originalContent = jsonInfo.content;

                const json translatedDatas = parseJson(m_outputDir / relJsonPath, ifs);

                if (metadatas.size() != translatedDatas.size()) {
                    throw std::runtime_error(gppTr(
                        "EpubTranslator.epubBeforeRun",
                        "[文件 %1] 元数据和翻译数据数量不匹配，无法重组 (%2 meta / %3 trans)")
                        .arg(wide2Ascii(rebuiltHtmlPath))
                        .arg(metadatas.size())
                        .arg(translatedDatas.size())
                        .toStdString());
                }

                std::string newContent;
                newContent.reserve(originalContent.length() * 2);
                size_t lastPos = 0;

                for (auto [metadata, translatedData] : std::views::zip(metadatas, translatedDatas)) {
                    newContent.append(originalContent.c_str() + lastPos, metadata.offset - lastPos);
                    const std::string translatedMessage = translatedData["message"].get<std::string>();
                    const std::string replacement = m_bilingualOutput ?
                        std::format("{}<br/><span style=\"color:{}; font-size:{}em;\">{}</span>",
                            translatedMessage, m_originalTextColor, m_originalTextScale,
                            std::string_view(originalContent.data() + metadata.offset, metadata.length))
                        : translatedMessage;
                    newContent.append(replacement);
                    lastPos = metadata.offset + metadata.length;
                }
                if (lastPos < originalContent.length()) {
                    newContent.append(originalContent.c_str() + lastPos, originalContent.length() - lastPos);
                }

                // 后处理正则替换
                regexReplace(m_postRegexPatterns, newContent);

                std::ofstream ofs;
                atomicOutputFile(ofs, rebuiltHtmlPath, newContent);

                const fs::path& showNormalPostHtmlPath = jsonInfo.normalPostPath;
                atomicOutputFile(ofs, showNormalPostHtmlPath, newContent);
            }

            const fs::path relEpubPath = fs::relative(epubPath, m_epubInputDir);
            const fs::path bookRebuildPath = m_tempRebuildDir / relEpubPath.parent_path() / relEpubPath.stem();

            const fs::path outputEpubPath = m_epubOutputDir / relEpubPath;
            createParent(outputEpubPath);
            m_logger->debug(gppTr("EpubTranslator.epubBeforeRun", "正在打包 %1")
                .arg(wide2Ascii(outputEpubPath))
                .toStdString());

            int error = 0;
            zip* za = zip_open(wide2Ascii(outputEpubPath).c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
            if (!za) {
                throw std::runtime_error(gppTr(
                    "EpubTranslator.epubBeforeRun",
                    "无法创建 EPUB (zip) 文件: [%1]")
                    .arg(error)
                    .toStdString());
            }

            // --- 步骤一：优先处理 mimetype 文件，且不压缩 ---
            if (const fs::path mimetypePath = bookRebuildPath / "mimetype"; fs::exists(mimetypePath)) {
                zip_source_t* s = zip_source_file(za, wide2Ascii(mimetypePath).c_str(), 0, 0);
                if (!s) {
                    zip_close(za);
                    throw std::runtime_error(gppTr(
                        "EpubTranslator.epubBeforeRun",
                        "无法为 mimetype 创建 zip_source_file")
                        .toStdString());
                }

                zip_int64_t idx = zip_file_add(za, "mimetype", s, ZIP_FL_ENC_UTF_8);
                if (idx < 0) {
                    zip_source_free(s);
                    zip_close(za);
                    throw std::runtime_error(gppTr(
                        "EpubTranslator.epubBeforeRun",
                        "无法将 mimetype 添加到 zip")
                        .toStdString());
                }

                if (zip_set_file_compression(za, idx, ZIP_CM_STORE, 0) < 0) {
                    zip_source_free(s);
                    zip_close(za);
                    throw std::runtime_error(gppTr(
                        "EpubTranslator.epubBeforeRun",
                        "无法将 mimetype 设置为不压缩模式。")
                        .toStdString());
                }
            }
            else {
                m_logger->warn(gppTr(
                    "EpubTranslator.epubBeforeRun",
                    "在源目录 [%1] 中未找到 mimetype 文件，生成的 EPUB 可能无效")
                    .arg(wide2Ascii(bookRebuildPath))
                    .toStdString());
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
                        throw std::runtime_error(gppTr(
                            "EpubTranslator.epubBeforeRun",
                            "无法为文件 [%1] 创建 zip_source_file")
                            .arg(entryName)
                            .toStdString());
                    }
                    if (zip_file_add(za, entryName.c_str(), s, ZIP_FL_ENC_UTF_8) < 0) {
                        zip_source_free(s);
                        zip_close(za);
                        throw std::runtime_error(gppTr(
                            "EpubTranslator.epubBeforeRun",
                            "无法将文件 [%1] 添加到 zip")
                            .arg(entryName)
                            .toStdString());
                    }
                }
            }

            // 所有 source 都在 zip_close 中被 libzip 自动管理和释放
            if (zip_close(za) < 0) {
                throw std::runtime_error(gppTr("EpubTranslator.epubBeforeRun", "关闭 zip 存档时出错: %1")
                    .arg(zip_strerror(za))
                    .toStdString());
            }

            m_logger->info(gppTr("EpubTranslator.epubBeforeRun", "已重建 EPUB 文件: [%1]")
                .arg(wide2Ascii(outputEpubPath))
                .toStdString());
        };
}

void EpubTranslator::run() {
    NormalJsonTranslator::normalJsonInit();
    EpubTranslator::epubInit();
    EpubTranslator::epubBeforeRun();
    NormalJsonTranslator::normalJsonBeforeRun();
    NormalJsonTranslator::normalJsonProcess();
    NormalJsonTranslator::normalJsonAfterRun();
}
