module;

#define PYBIND11_HEADERS
#define PCRE2_HEADERS
#include "GPPMacros.hpp"
#ifdef _WIN32
#include <Shlwapi.h>
#endif
#include <toml.hpp>
#include <ctpl_stl.h>
#include <sol/sol.hpp>
#include <proxy/proxy.h>

module NormalJsonTranslator;

import AgentSourceView;
import ConditionTool;
import DictionaryGenerator;
import NameTranslator;
import NormalJsonTranslatorHelperTool;
import NLPTool;
import Tool;

namespace fs = std::filesystem;
namespace py = pybind11;

json loadJsonFileOrFromDisk(const fs::path& path, const json& fallback = json::object()) {
    try {
        if (!fs::exists(path)) {
            return fallback;
        }
        std::ifstream ifs(path, std::ios::binary);
        return json::parse(ifs);
    }
    catch (...) {
        return fallback;
    }
}

AgentSourceFileView buildAgentSourceFileViewFromJson(
    const ordered_json& data,
    const std::function<void(Sentence*)>& preProcessFunc,
    const fs::path& relPath = {}
) {
    std::vector<Sentence> sentences;
    sentences.reserve(data.size());
    for (const auto& [index, item] : data | std::views::enumerate) {
        Sentence se;
        se.index = (int)index;
        if (auto jit = item.find("name"); jit != item.end()) {
            se.nameType = NameType::Single;
            jit->get_to(se.name);
        }
        else if (jit = item.find("names"); jit != item.end()) {
            se.nameType = NameType::Multiple;
            jit->get_to(se.names);
        }
        se.original_text = item.value("message", "");
        sentences.push_back(std::move(se));
    }
    for (auto [se1, se2] : std::views::adjacent<2>(sentences)) {
        se1.next = &se2;
        se2.prev = &se1;
    }

    for (Sentence& se : sentences) {
        preProcessFunc(&se);
    }
    return buildAgentSourceFileViewFromSentences(sentences, relPath);
}

std::vector<std::string> collectRelFileStrings(const std::vector<fs::path>& relFilePaths) {
    return relFilePaths
        | std::views::transform([](const fs::path& p) { return wide2Ascii(p); })
        | std::ranges::to<std::vector>();
}

bool isApiTranslationEngine(TransEngine transEngine) {
    switch (transEngine)
    {
    case TransEngine::ForGalJson:
    case TransEngine::ForGalTsv:
    case TransEngine::ForNovelTsv:
    case TransEngine::DeepseekJson:
    case TransEngine::Sakura:
        return true;
    default:
        return false;
    }
}

std::optional<std::vector<fs::path>> NormalJsonTranslator::normalJsonBeforeRun()
{
    const bool reportRuntimeWorkbench = shouldReportRuntimeWorkbench();
    int totalSentences = 0;
    std::map<std::string, int> runtimeFileTotals;
    std::map<std::string, int> originalRuntimeFileTotals;

    if (fs::exists(m_transCacheDir)) {
        try {
            fs::copy(
                m_transCacheDir,
                m_transCacheDir.parent_path() / (m_transCacheDir.filename().wstring() + L"_bak"),
                fs::copy_options::recursive | fs::copy_options::overwrite_existing
            );
        }
        catch (const fs::filesystem_error& e) {
            m_logger->error("复制缓存文件夹时出现异常: {}", e.what());
        }
    }
    for (const auto& dir : { m_inputDir, m_outputDir, m_transCacheDir }) {
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
            m_logger->debug("已创建目录: {}", wide2Ascii(dir));
        }
    }

    std::vector<fs::path> relJsonPaths;

    std::ifstream ifs;
    std::ofstream ofs;

    const fs::path nameTablePath = m_projectDir / L"人名替换表.toml";

    // 1. 扫描输入、校验 message 字段，并更新项目人名表。
    {
        absl::flat_hash_map<std::string, int> jsonNameTable;
        Sentence se;

        auto insertJsonNameTable = [&](const std::string& name)
        {
            if (!name.empty()) {
                ++jsonNameTable[name];
            }
        };

        for (const auto& entry : fs::recursive_directory_iterator(m_inputDir)) {
            if (!entry.is_regular_file() || !isSameExtension(entry.path(), L".json")) {
                continue;
            }
            const fs::path relInputPath = fs::relative(entry.path(), m_inputDir);
            try {
                ifs.open(entry.path(), std::ios::binary);
                json data = json::parse(ifs);
                ifs.close();

                for (const auto& [index, item] : data | std::views::enumerate) {
                    if (!item.contains("message")) {
                        throw std::runtime_error(std::format("[文件 {}] 第 {} 个对象缺少 message 字段。", wide2Ascii(relInputPath), index));
                    }
                    ++totalSentences;
                    if (reportRuntimeWorkbench) {
                        ++originalRuntimeFileTotals[wide2Ascii(relInputPath)];
                    }
                    if (auto jit = item.find("name"); jit != item.end()) {
                        jit->get_to(se.name);
                        if (m_usePreDictInName) {
                            se.name = m_preDictionary->doReplace(&se, CachePart::Name);
                        }
                        insertJsonNameTable(se.name);
                    }
                    else if (jit = item.find("names"); jit != item.end()) {
                        for (const auto& name : jit.value()) {
                            name.get_to(se.name);
                            if (m_usePreDictInName) {
                                se.name = m_preDictionary->doReplace(&se, CachePart::Name);
                            }
                            insertJsonNameTable(se.name);
                        }
                    }
                }

                relJsonPaths.push_back(std::move(relInputPath));
            }
            catch (const json::exception& e) {
                m_logger->critical("读取文件 {} 时出错", wide2Ascii(relInputPath));
                throw std::runtime_error(e.what());
            }
        }

        if (totalSentences == 0) {
            throw std::runtime_error("未找到有效的 Sentence");
        }
        m_lastRuntimeFileTotal = totalSentences;
        if (reportRuntimeWorkbench) {
            runtimeFileTotals = originalRuntimeFileTotals;
            m_controller->setRuntimeFiles(runtimeFileTotals);
        }
        m_controller->makeBar(totalSentences, m_threadsNum);

        toml::value orgNameTable = toml::table{};
        try {
            if (fs::exists(nameTablePath)) {
                orgNameTable = toml::uparse(nameTablePath);
            }
        }
        catch (...) {
            m_logger->error("解析原人名表失败");
        }

        std::vector<std::pair<std::string, int>> jsonNameTablePairs = jsonNameTable
            | std::views::transform([](const auto& pair) { return std::pair{ pair.first, pair.second }; })
            | std::ranges::to<std::vector>();
        std::ranges::sort(jsonNameTablePairs, [&](const auto& a, const auto& b)
        {
            if (a.second == b.second) {
                return a.first.length() > b.first.length();
            }
            return a.second > b.second;
        });

        toml::ordered_value newNameTable = toml::ordered_table{};
        newNameTable.comments().push_back("'原名' = [ '译名', 出现次数 ]");
        for (const std::string& key : jsonNameTablePairs | std::views::keys) {
            try {
                newNameTable[key] = toml::array{ toml::find_or(orgNameTable, key, 0, ""), jsonNameTable[key] };
            }
            catch (...) {
                newNameTable[key] = toml::array{ "", jsonNameTable[key] };
            }
        }
        ofs.open(nameTablePath, std::ios::binary);
        ofs << newNameTable;
        ofs.close();
        m_logger->info("已更新 人名替换表.toml 文件");
        if (m_transEngine == TransEngine::DumpName) {
            for (const auto& [filename, count] : runtimeFileTotals) {
                for (int i = 0; i < count; ++i) {
                    m_controller->recordFileSentenceDone(filename, false);
                }
            }
            m_controller->updateBar(totalSentences);
            return std::nullopt;
        }
    }

    // 2. 独立的人名翻译模式直接在这里结束。
    if (m_transEngine == TransEngine::NameTrans) {
        NameTranslator nameTranslator(
            m_controller, m_logger, m_apiPool, m_gptDictionary, m_onPerformApi,
            m_systemPrompt, m_userPrompt, m_apiStrategy, m_targetLang, m_maxRetries, m_apiTimeOutMs,
            m_threadsNum, m_nameTransBatchSize, m_checkQuota
        );
        nameTranslator.run(nameTablePath);
        return std::nullopt;
    }

    // 3. GPT 字典生成模式直接交给 DictionaryGenerator。
    if (m_transEngine == TransEngine::GenDict) {
        auto preProcessFunc = [this](Sentence* se)
            {
                this->preProcess(se);
            };
        DictionaryGeneratorReviewOptions reviewOptions;
        if (m_agentEnabled) {
            reviewOptions.enabled = true;
            reviewOptions.projectDir = m_projectDir;
            reviewOptions.inputDir = m_inputDir;
            reviewOptions.relInputFiles = relJsonPaths;
            reviewOptions.projectNotePath = m_agentProjectInfoPath;
            reviewOptions.systemPrompt = m_genDictReviewSystemPrompt;
            reviewOptions.userPrompt = m_genDictReviewUserPrompt;
            reviewOptions.maxTurnsPerTerm = m_agentMaxTurnsPerChunk;
            reviewOptions.searchResultLimit = m_agentSearchResultLimit;
        }
        DictionaryGenerator generator(
            m_controller, m_logger, m_apiPool, m_tokenizeSourceLangFunc, m_otherCacheDir,
            std::move(preProcessFunc), m_onPerformApi, m_onDictProcessed,
            m_systemPrompt, m_userPrompt, m_apiStrategy, m_targetLang,
            m_maxRetries, m_threadsNum, m_apiTimeOutMs, m_checkQuota, std::move(reviewOptions)
        );
        const fs::path outputFilePath = m_projectDir / L"项目GPT字典-生成.toml";
        const std::vector<fs::path> inputPaths = relJsonPaths
            | std::views::transform([&](const auto& p) { return m_inputDir / p; })
            | std::ranges::to<std::vector>();
        generator.generate(inputPaths, outputFilePath);
        return std::nullopt;
    }

    // 4. 重新载入最终人名映射，供后处理替换使用。
    try {
        const auto nameTable = toml::uparse(m_projectDir / L"人名替换表.toml");

        for (const auto& [key, value] : nameTable.as_table()) {
            if (!value.is_array() || value.size() == 0) {
                continue;
            }
            const std::string transName = toml::find_or(value, 0, "");
            if (!transName.empty()) {
                m_logger->trace("发现原名 '{}' 的译名 '{}'", key, transName);
                m_nameMap.insert({ key, transName });
            }
        }
    }
    catch (const toml::exception& e) {
        m_logger->critical("解析 人名替换表.toml 时出错");
        throw std::runtime_error(e.what());
    }

    // 5. 如启用了 splitFile，则先预切分输入，后续按 part 参与并行调度。
    {
        auto splitFunc = [&](const std::function<std::vector<ordered_json>(const ordered_json&, int)>& splitImplFunc)
        {
            if (m_splitFileNum <= 0) {
                throw std::invalid_argument("文件分割数必须大于 0");
            }
            m_needsCombining = true;
            if (reportRuntimeWorkbench) {
                runtimeFileTotals.clear();
            }
            m_logger->info("检测到文件分割模式 ({})，开始预处理输入文件...", m_splitFile);
            for (const auto& relJsonPath : relJsonPaths) {
                try {
                    ifs.open(m_inputDir / relJsonPath, std::ios::binary);
                    const ordered_json data = ordered_json::parse(ifs);
                    ifs.close();
                    const std::vector<ordered_json> parts = splitImplFunc(data, m_splitFileNum);
                    const std::wstring relStem = relJsonPath.parent_path() / relJsonPath.stem();
                    for (const auto& [index, part] : parts | std::views::enumerate) {
                        const fs::path relPartPath = std::format(L"{}_part_{}{}", relStem, index, relJsonPath.extension().wstring());
                        m_splitFilePartsToJson[relPartPath] = relJsonPath;
                        if (reportRuntimeWorkbench) {
                            runtimeFileTotals[wide2Ascii(relPartPath)] = (int)part.size();
                        }
                        m_jsonToSplitFileParts[relJsonPath].insert({ relPartPath, false });
                        const fs::path partPath = m_inputCacheDir / relPartPath;
                        createParent(partPath);
                        ofs.open(partPath, std::ios::binary);
                        ofs << part.dump(2);
                        ofs.close();
                    }
                    m_logger->debug("文件 {} 已被分割成 {} 份，存入输入缓存。", wide2Ascii(relJsonPath), parts.size());
                }
                catch (const json::exception& e) {
                    m_logger->critical("分割文件 {} 时出错", wide2Ascii(relJsonPath));
                    throw std::runtime_error(e.what());
                }
            }
        };
        if (m_splitFile == "Equal") {
            splitFunc(splitJsonArrayEqual);
        }
        else if (m_splitFile == "Num") {
            splitFunc(splitJsonArrayNum);
        }
        else if (m_splitFile != "No") {
            throw std::invalid_argument(std::format("未知的文件分割模式: {}, 请使用 'No', 'Equal', 'Num'", m_splitFile));
        }
    }

    std::vector<fs::path> relFilePaths = m_needsCombining
        ? (m_splitFilePartsToJson | std::views::keys | std::ranges::to<std::vector>())
        : std::move(relJsonPaths);

    if (m_needsCombining && reportRuntimeWorkbench) {
        m_controller->setRuntimeFiles(runtimeFileTotals);
    }

    if (m_sortMethod == "size") {
        std::ranges::sort(relFilePaths, [&](const fs::path& a, const fs::path& b)
            {
                return m_needsCombining ? (fs::file_size(m_inputCacheDir / a) > fs::file_size(m_inputCacheDir / b))
                                        : (fs::file_size(m_inputDir / a) > fs::file_size(m_inputDir / b));
            });
    }
    else if (m_sortMethod == "name") {
#ifdef _WIN32
        std::ranges::sort(relFilePaths, [](const fs::path& a, const fs::path& b)
            {
                return StrCmpLogicalW(a.c_str(), b.c_str()) < 0;
            });
#else
        std::ranges::sort(relFilePaths);
#endif
    }
    else {
        throw std::invalid_argument(std::format("未知的排序模式: {}", m_sortMethod));
    }

    // 6. 可选：分析跨文件连续重复块，并将带引用信息的输入统一写入 inputCacheDir。
    if (m_reuseRepeatedBlocks && (isApiTranslationEngine(m_transEngine) || m_transEngine == TransEngine::ShowNormal)) {
        const fs::path& sourceRootDir = m_needsCombining ? m_inputCacheDir : m_inputDir;
        std::vector<std::pair<fs::path, ordered_json>> filesWithData;
        filesWithData.reserve(relFilePaths.size());

        for (const fs::path& relFilePath : relFilePaths) {
            try {
                ifs.open(sourceRootDir / relFilePath, std::ios::binary);
                ordered_json data = ordered_json::parse(ifs);
                ifs.close();
                filesWithData.emplace_back(relFilePath, std::move(data));
            }
            catch (const json::exception& e) {
                m_logger->critical("分析连续重复块时读取文件 {} 失败", wide2Ascii(relFilePath));
                throw std::runtime_error(e.what());
            }
        }

        RepeatedBlockPlan repeatedBlockPlan = analyzeRepeatedBlocks(filesWithData, m_repeatedBlockMinSize);
        if (!repeatedBlockPlan.refToByTarget.empty()) {
            m_useRepeatedBlockInputCache = true;
            for (auto& [relFilePath, data] : filesWithData) {
                applyRepeatedBlockPlanToJson(relFilePath, data, repeatedBlockPlan);
                const fs::path cachedInputPath = m_inputCacheDir / relFilePath;
                createParent(cachedInputPath);
                ofs.open(cachedInputPath, std::ios::binary);
                ofs << data.dump(2);
                ofs.close();
            }
            m_logger->info(
                "连续重复块引用分析完成，阈值 {}，引用 {} 句。",
                m_repeatedBlockMinSize,
                repeatedBlockPlan.refToByTarget.size()
            );
        }
        else {
            m_logger->info("连续重复块引用分析完成，未发现长度不小于 {} 的重复块。", m_repeatedBlockMinSize);
        }
    }

    // 7. Agent 模式启动前初始化共享状态文件。
    if (m_agentEnabled) {
        m_agentKnownRelFiles = relFilePaths;
        fs::create_directories(m_agentFileNotesDir);
        m_agentTermLedgerCache = fs::exists(m_agentTermLedgerPath) ? loadJsonFileOrFromDisk(m_agentTermLedgerPath, json::object()) : json::object();
        const std::vector<std::string> currentFileStrings = collectRelFileStrings(relFilePaths);
        const fs::path fingerprintRootDir = m_needsCombining ? m_inputCacheDir : m_inputDir;

        saveJsonFile(m_agentSearchCatalogPath, json{
            {"updated_at", nowTimestampString()},
            {"files", currentFileStrings}
        });
        
        for (const fs::path& relFilePath : relFilePaths) {
            const fs::path absPath = fingerprintRootDir / relFilePath;
            if (!fs::exists(absPath)) {
                continue;
            }
            std::ifstream sourceIfs(absPath, std::ios::binary);
            const ordered_json data = ordered_json::parse(sourceIfs);
            AgentSourceFileView fileView = buildAgentSourceFileViewFromJson(
                data,
                [this](Sentence* se)
                {
                    this->preProcess(se);
                },
                relFilePath
            );
            m_agentSourceFileViews.insert_or_assign(relFilePath, fileView);
        }
        if (!fs::exists(m_agentTermLedgerPath)) {
            m_agentTermLedgerCache = json::object();
            saveJsonFile(m_agentTermLedgerPath, m_agentTermLedgerCache);
        }
    }

    return relFilePaths;
}

void NormalJsonTranslator::normalJsonAfterRun()
{
    // 1. 汇总所有问题概览。
    if (m_problemOverview.as_array().empty()) {
        m_logger->info("\n\n```\n无问题概览\n```\n");
    }
    else {
        std::ofstream ofs;
        ofs.open(m_projectDir / L"翻译问题概览.toml", std::ios::binary);
        ofs << toml::format("problemOverview", m_problemOverview);
        ofs.close();
        ofs.open(m_projectDir / L"翻译问题概览.json", std::ios::binary);
        ofs << toml2Json(m_problemOverview).dump(2);
        ofs.close();
        m_logger->debug("已生成 翻译问题概览.json 和 翻译问题概览.toml 文件");

        absl::btree_map<std::string, absl::flat_hash_set<std::string>> problemMap;
        for (const auto& [problem, filename] : m_problemOverview.as_array()
            | std::views::transform([](const auto& tbl)
                {
                    const auto& problemsArr = tbl.at("problems").as_array();
                    const auto problemsWithFileNameView = problemsArr | std::views::transform([&](const auto& prob)
                        {
                            return std::make_pair(prob.as_string(), tbl.at("filename").as_string());
                        });
                    return problemsWithFileNameView;
                })
            | std::views::join)
        {
            problemMap[problem].insert(filename);
        }

        std::string problemOverviewStr = "\n\n```\n问题概览:\n";
        size_t problemCount = 0;
        for (const auto& [problem, files] : problemMap) {
            std::string fileStr = "(";
            size_t fileCount = 0;
            for (const auto& file : files) {
                if (fileCount == 3) {
                    break;
                }
                fileStr += file + ", ";
                ++fileCount;
            }
            if (fileCount == files.size()) {
                fileStr.pop_back();
                fileStr.pop_back();
                fileStr += ")";
            }
            else {
                fileStr += "...)";
            }
            problemOverviewStr += std::format("{}. {}  |  {}\n", ++problemCount, problem, fileStr);
        }
        m_logger->error("{}问题概览结束\n```\n", problemOverviewStr);
    }

    // 2. 保存背景文本缓存，供下次运行恢复上下文。
    {
        try {
            createParent(m_backgroundTextCachePath);
            json j = m_backgroundTextCacheMap;
            std::ofstream ofs(m_backgroundTextCachePath, std::ios::binary);
            ofs << j.dump(2);
            ofs.close();
            m_logger->debug("背景文本缓存已保存至 {}", wide2Ascii(m_backgroundTextCachePath));
        }
        catch (...) {
            m_logger->error("背景文本缓存 {} 保存失败", wide2Ascii(m_backgroundTextCachePath));
        }
    }

    if (m_needsCombining) {
        fs::remove_all(m_inputCacheDir);
        fs::remove_all(m_outputCacheDir);
    }
    else if (m_useRepeatedBlockInputCache) {
        fs::remove_all(m_inputCacheDir);
    }
    if (!m_controller->shouldStop() && m_transEngine == TransEngine::Rebuild && m_controller->m_completedSentences != m_controller->m_totalSentences) {
        m_logger->critical("重建过程中有句子未命中缓存 ({}/{} lines)，请检查日志以定位问题。",
            m_controller->m_completedSentences.load(), m_controller->m_totalSentences.load());
    }
}

void NormalJsonTranslator::normalJsonProcess(std::vector<fs::path> relFilePaths)
{
    std::vector<std::future<void>> results;
    m_threadPool.resize(std::min(m_threadsNum, (int)relFilePaths.size()));
    for (const auto& filePath : relFilePaths) {
        results.emplace_back(m_threadPool.push([=](const int id)
            {
                m_controller->addThreadNum();
                try {
                    this->processFile(filePath, id);
                }
                catch (const std::exception& e) {
                    this->recordRuntimeError("file", e.what(), filePath);
                    m_controller->reduceThreadNum();
                    throw;
                }
                m_controller->reduceThreadNum();
            }));
    }
    m_logger->info("已将 {} 个文件任务分配到线程池，等待处理完成...", results.size());
    waitForThreads(m_threadPool, results);

    if (m_useRepeatedBlockInputCache && isApiTranslationEngine(m_transEngine)) {
        resolveRepeatedBlockReferences(relFilePaths);
    }
    if (m_agentEnabled) {
        applyAgentRetranslateSuggestions();
    }
}

void NormalJsonTranslator::resolveRepeatedBlockReferences(const std::vector<fs::path>& relFilePaths)
{
    struct FileBundle {
        ordered_json input = ordered_json::array();
        json cache = json::array();
    };

    absl::flat_hash_map<fs::path, FileBundle> fileBundles;
    std::ifstream ifs;
    std::ofstream ofs;

    for (const fs::path& relFilePath : relFilePaths) {
        FileBundle bundle;
        try {
            ifs.open(m_inputCacheDir / relFilePath, std::ios::binary);
            bundle.input = ordered_json::parse(ifs);
            ifs.close();

            const fs::path cachePath = m_transCacheDir / relFilePath;
            if (fs::exists(cachePath)) {
                ifs.open(cachePath, std::ios::binary);
                bundle.cache = json::parse(ifs);
                ifs.close();
            }
        }
        catch (const json::exception& e) {
            m_logger->critical("连续重复块引用回填读取 {} 失败", wide2Ascii(relFilePath));
            throw std::runtime_error(e.what());
        }
        fileBundles.emplace(relFilePath, std::move(bundle));
    }

    absl::flat_hash_map<SentencePosition, json*> cacheByPosition;
    for (auto& [relFilePath, bundle] : fileBundles) {
        for (json& item : bundle.cache) {
            const int index = item.value("index", -1);
            if (index >= 0) {
                cacheByPosition[{ wide2Ascii(relFilePath), index }] = &item;
            }
        }
    }

    int resolvedCount = 0;
    int missingCount = 0;

    for (auto& [relFilePath, bundle] : fileBundles) {
        for (json& cacheItem : bundle.cache) {
            const auto refTo = getRepeatedBlockRefTo(cacheItem);
            if (!refTo.has_value()) {
                continue;
            }
            const auto sourceIt = cacheByPosition.find({ refTo->file, refTo->index });
            if (sourceIt == cacheByPosition.end()) {
                ++missingCount;
                continue;
            }
            const json& sourceItem = *sourceIt->second;
            cacheItem["pre_translated_text"] = sourceItem.value("pre_translated_text", "");
            cacheItem["translated_by"] = sourceItem.value("translated_by", "");
            cacheItem["translated_preview"] = sourceItem.value("translated_preview", "");
            if (sourceItem.contains("problems")) {
                cacheItem["problems"] = sourceItem["problems"];
            }
            else {
                cacheItem.erase("problems");
            }
            if (sourceItem.contains("name_preview")) {
                cacheItem["name_preview"] = sourceItem["name_preview"];
            }
            if (sourceItem.contains("names_preview")) {
                cacheItem["names_preview"] = sourceItem["names_preview"];
            }
            cacheItem.erase(std::string(repeatedBlockRefPendingKey));
            ++resolvedCount;
        }

        absl::flat_hash_map<int, const json*> cacheByIndex;
        for (const json& cacheItem : bundle.cache) {
            const int index = cacheItem.value("index", -1);
            if (index >= 0) {
                cacheByIndex[index] = &cacheItem;
            }
        }

        for (auto [index, item] : bundle.input | std::views::enumerate) {
            eraseRepeatedBlockReferenceInfo(item);
            const auto cacheIt = cacheByIndex.find((int)index);
            if (cacheIt == cacheByIndex.end()) {
                continue;
            }
            const json& cacheItem = *cacheIt->second;
            if (cacheItem.contains("name_preview")) {
                item["name"] = cacheItem["name_preview"];
            }
            else if (cacheItem.contains("names_preview")) {
                item["names"] = cacheItem["names_preview"];
            }
            item["message"] = cacheItem.value("translated_preview", "");
            if (m_outputWithSrc && cacheItem.contains("original_text")) {
                item["src_msg"] = cacheItem["original_text"];
            }
        }

        const fs::path cachePath = m_transCacheDir / relFilePath;
        createParent(cachePath);
        ofs.open(cachePath, std::ios::binary);
        ofs << bundle.cache.dump(2);
        ofs.close();

        const fs::path outputPath = m_needsCombining ? (m_outputCacheDir / relFilePath) : (m_outputDir / relFilePath);
        createParent(outputPath);
        ofs.open(outputPath, std::ios::binary);
        ofs << bundle.input.dump(2);
        ofs.close();
    }

    if (missingCount > 0) {
        m_logger->warn("连续重复块引用回填完成，共复制 {} 句。但有 {} 句未找到被引用缓存，已保留占位结果。",
            resolvedCount, missingCount);
    }
    else {
        m_logger->info("连续重复块引用回填完成，共复制 {} 句。", resolvedCount);
    }

    if (m_needsCombining) {
        for (const fs::path& originalRelFilePath : m_jsonToSplitFileParts | std::views::keys) {
            combineOutputFiles(originalRelFilePath, m_jsonToSplitFileParts[originalRelFilePath],
                m_outputCacheDir, m_outputDir, m_logger);
            if (m_onFileProcessed) {
                std::unique_lock<std::mutex> lock(m_outputMutex, std::defer_lock);
                if (!m_pythonTranslator) {
                    lock.lock();
                }
                m_onFileProcessed(originalRelFilePath);
            }
        }
    }
    else if (m_onFileProcessed) {
        for (const fs::path& relFilePath : relFilePaths) {
            std::unique_lock<std::mutex> lock(m_outputMutex, std::defer_lock);
            if (!m_pythonTranslator) {
                lock.lock();
            }
            m_onFileProcessed(relFilePath);
        }
    }
}

void NormalJsonTranslator::run()
{
    NormalJsonTranslator::normalJsonInit();
    std::optional<std::vector<fs::path>> relFilePathsOpt = NormalJsonTranslator::normalJsonBeforeRun();
    if (!relFilePathsOpt.has_value()) {
        return;
    }
    NormalJsonTranslator::normalJsonProcess(std::move(relFilePathsOpt.value()));
    NormalJsonTranslator::normalJsonAfterRun();
}
