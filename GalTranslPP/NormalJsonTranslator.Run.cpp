module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"
#ifdef _WIN32
#include <Shlwapi.h>
#endif
#include <toml.hpp>
#include <ctpl_stl.h>
#include <proxy/proxy.h>

module NormalJsonTranslator;

import NormalJsonTranslatorHelperTool;
import Tool;

namespace fs = std::filesystem;
namespace py = pybind11;

ordered_json buildProblemOverviewFromCache(
    const fs::path& transCacheDir,
    const std::vector<fs::path>& relFilePaths,
    const std::shared_ptr<spdlog::logger>& logger
)
{
    ordered_json overview = ordered_json::array();
    std::ifstream ifs;
    for (const fs::path& relFilePath : relFilePaths) {
        const fs::path cachePath = transCacheDir / relFilePath;
        if (!fs::exists(cachePath)) {
            continue;
        }
        try {
            const ordered_json cacheJson = parseOrderedJson(cachePath, ifs);
            for (const ordered_json& item : cacheJson) {
                const auto problemsIt = item.find("problems");
                if (problemsIt == item.end() || !problemsIt->is_array() || problemsIt->empty()) {
                    continue;
                }
                ordered_json newItem = ordered_json::object();
                newItem["filename"] = wide2Ascii(relFilePath);
                for (const auto& kv : item.items()) {
                    newItem[kv.key()] = kv.value();
                }
                overview.push_back(std::move(newItem));
            }
        }
        catch (const std::exception& e) {
            logger->error(gppTr("buildProblemOverviewFromCache",
                "构建问题概览时读取缓存文件 [%1] 失败: %2")
                .arg(wide2Ascii(cachePath))
                .arg(e.what())
                .toStdString());
        }
    }
    return overview;
}

void NormalJsonTranslator::normalJsonBeforeRun()
{
    if (fs::exists(m_transCacheDir)) {
        try {
            fs::copy(
                m_transCacheDir,
                m_transCacheDir.parent_path() / (m_transCacheDir.filename().wstring() + L"_bak"),
                fs::copy_options::recursive | fs::copy_options::overwrite_existing
            );
        }
        catch (const fs::filesystem_error& e) {
            m_logger->error(gppTr("NormalJsonTranslator.normalJsonBeforeRun", "复制缓存文件夹时出现异常: %1")
                .arg(e.what())
                .toStdString());
        }
    }
    for (const auto& dir : { m_inputDir, m_outputDir, m_transCacheDir }) {
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
            m_logger->debug(gppTr("NormalJsonTranslator.normalJsonBeforeRun", "已创建目录: [%1]")
                .arg(wide2Ascii(dir))
                .toStdString());
        }
    }

    std::vector<fs::path> relJsonPaths;

    std::map<std::string, int> runtimeFileTotals; // 路径可能会变为 split 切片，value 是句子数

    std::ifstream ifs;
    std::ofstream ofs;


    // 扫描输入、校验 message 字段，并更新项目人名表。
    {
        int totalSentences = 0;
        absl::flat_hash_map<std::string, int> nameTableFromInputJson;
        Sentence se;

        auto insertHelperFunc = [&](const std::string& name)
            {
                if (!name.empty()) {
                    ++nameTableFromInputJson[name];
                }
            };

        for (const auto& entry : fs::recursive_directory_iterator(m_inputDir)) {
            if (!entry.is_regular_file() || !isSameExtension(entry.path(), L".json")) {
                continue;
            }
            const fs::path relInputPath = fs::relative(entry.path(), m_inputDir);
            try {
                json data = parseJson(entry.path(), ifs);

                auto& fileSentenceNum = runtimeFileTotals[wide2Ascii(relInputPath)];
                for (const auto& [index, item] : data | std::views::enumerate) {
                    if (!item.contains("message")) {
                        throw std::runtime_error(gppTr(
                            "NormalJsonTranslator.normalJsonBeforeRun",
                            "第 %1 个对象缺少 message 字段。")
                            .arg(index)
                            .toStdString());
                    }
                    ++totalSentences;
                    ++fileSentenceNum;
                    if (auto jit = item.find("name"); jit != item.end()) {
                        jit->get_to(se.name);
                        if (m_usePreDictInName) {
                            se.name = m_preDictionary->doReplace(&se, CachePart::Name);
                        }
                        insertHelperFunc(se.name);
                    }
                    else if (jit = item.find("names"); jit != item.end()) {
                        for (const auto& name : jit.value()) {
                            name.get_to(se.name);
                            if (m_usePreDictInName) {
                                se.name = m_preDictionary->doReplace(&se, CachePart::Name);
                            }
                            insertHelperFunc(se.name);
                        }
                    }
                }

                relJsonPaths.push_back(relInputPath);
            }
            catch (const std::exception& e) {
                throw std::runtime_error(gppTr(
                    "NormalJsonTranslator.normalJsonBeforeRun",
                    "读取文件 [%1] 时出错: %2")
                    .arg(wide2Ascii(relInputPath))
                    .arg(e.what())
                    .toStdString());
            }
        }

        if (totalSentences == 0) {
            throw std::runtime_error(gppTr(
                "NormalJsonTranslator.normalJsonBeforeRun",
                "未找到有效的 Sentence")
                .toStdString());
        }
        if (m_transEngine != TransEngine::Rebuild && m_transEngine != TransEngine::ShowNormal) {
            m_controller->setRuntimeFiles(runtimeFileTotals);
        }
        m_controller->makeBar(totalSentences, m_threadsNum);

        toml::value orgNameTable = toml::table{};
        try {
            if (fs::exists(m_nameTablePath)) {
                orgNameTable = toml::uparse(m_nameTablePath);
            }
        }
        catch (...) {
            m_logger->error(gppTr("NormalJsonTranslator.normalJsonBeforeRun", "解析原人名表失败")
                .toStdString());
        }

        std::vector<std::pair<std::string, int>> nameTablePairsFromInputJson = nameTableFromInputJson
            | std::views::transform([](const auto& pair) { return std::pair{ pair.first, pair.second }; })
            | std::ranges::to<std::vector>();
        std::ranges::sort(nameTablePairsFromInputJson, [&](const auto& a, const auto& b)
            {
                if (a.second == b.second) {
                    return a.first.length() > b.first.length();
                }
                return a.second > b.second;
            });

        toml::ordered_value newNameTable = toml::ordered_table{};
        newNameTable.comments().push_back("'原名' = [ '译名', 出现次数 ]");
        for (const std::string& key : nameTablePairsFromInputJson | std::views::keys) {
            try {
                newNameTable[key] = toml::array{ toml::find_or(orgNameTable, key, 0, ""), nameTableFromInputJson[key] };
            }
            catch (...) {
                newNameTable[key] = toml::array{ "", nameTableFromInputJson[key] };
            }
        }
        atomicOutputFile(ofs, m_nameTablePath, toml::format(newNameTable));
        m_logger->info(gppTr("NormalJsonTranslator.normalJsonBeforeRun", "已更新 NameTable.toml 文件")
            .toStdString());
        if (m_transEngine == TransEngine::DumpName) {
            return;
        }
    }

    // 人名翻译模式。
    if (m_transEngine == TransEngine::NameTrans) {
        m_nameTranslator = std::make_unique<NameTranslator>(
            m_controller, m_logger, m_apiPool, m_gptDictionary, m_onPerformApi,
            m_systemPrompt, m_userPrompt, m_apiStrategy, m_targetLang,
            m_threadsNum, m_nameTransBatchSize, m_inputBlockMaxLines, m_maxRequestCount, m_apiTimeOutMs, m_checkQuota
        );
        return;
    }

    // 字典生成模式。
    if (m_transEngine == TransEngine::GenDict) {
        auto preProcessFunc = [this](Sentence* se)
            {
                this->preProcess(se);
            };
        m_dictionaryGenerator = std::make_unique<DictionaryGenerator>(
            m_controller, m_logger, m_apiPool, m_tokenizeSourceLangFunc, m_otherCacheDir,
            preProcessFunc, m_onPerformApi, m_onDictProcessed,
            m_systemPrompt, m_userPrompt, m_apiStrategy, m_targetLang,
            m_threadsNum, m_inputBlockMaxLines, m_maxRequestCount, m_apiTimeOutMs, m_checkQuota,
            m_agentEnabled,
            m_projectDir,
            m_inputDir,
            relJsonPaths,
            m_agentProjectNotePath,
            m_genDictReviewSystemPrompt,
            m_genDictReviewUserPrompt,
            m_agentMaxTurnsPerChunk,
            m_agentSearchResultLimit,
            m_agentContextLinesLimit
        );
        return;
    }

    // 重新载入最终人名映射，供后处理替换使用。
    try {
        const auto nameTable = toml::uparse(m_nameTablePath);

        for (const auto& [key, value] : nameTable.as_table()) {
            if (!value.is_array() || value.size() == 0) {
                continue;
            }
            const std::string transName = toml::find_or(value, 0, "");
            if (!transName.empty()) {
                m_nameMap.insert({ key, transName });
            }
        }
    }
    catch (const toml::exception& e) {
        throw std::runtime_error(gppTr(
            "NormalJsonTranslator.normalJsonBeforeRun",
            "解析 NameTable.toml 时出错: %1")
            .arg(e.what())
            .toStdString());
    }

    // 如启用了 splitFileMethod，则先预切分输入，后续按 part 参与并行调度。
    {
        auto splitFunc = [&](const std::function<std::vector<ordered_json>(const ordered_json&, int)>& splitImplFunc)
            {
                m_splitFileEnabled = true;
                runtimeFileTotals.clear();
                m_logger->info(gppTr(
                    "NormalJsonTranslator.normalJsonBeforeRun",
                    "检测到文件分割模式 (%1)，开始预处理输入文件...")
                    .arg(m_splitFileMethod)
                    .toStdString());
                for (const auto& relJsonPath : relJsonPaths) {
                    try {
                        const ordered_json data = parseOrderedJson(m_inputDir / relJsonPath, ifs);
                        const std::vector<ordered_json> parts = splitImplFunc(data, m_splitFileNum);
                        const std::wstring relStem = relJsonPath.parent_path() / relJsonPath.stem();
                        for (const auto& [index, part] : parts | std::views::enumerate) {
                            const fs::path relPartPath = std::format(L"{}_part_{}{}",
                                relStem, index, relJsonPath.extension().wstring());
                            m_splitFilePartsToJson[relPartPath] = relJsonPath;
                            if (m_transEngine != TransEngine::Rebuild && m_transEngine != TransEngine::ShowNormal) {
                                runtimeFileTotals[wide2Ascii(relPartPath)] = (int)part.size();
                            }
                            m_jsonToSplitFileParts[relJsonPath].insert({ relPartPath, false });
                            const fs::path partPath = m_inputCacheDir / relPartPath;
                            atomicOutputFile(ofs, partPath, part.dump(2));
                        }
                        m_logger->debug(gppTr(
                            "NormalJsonTranslator.normalJsonBeforeRun",
                            "文件 [%1] 已被分割成 %2 份，存入输入缓存")
                            .arg(wide2Ascii(relJsonPath))
                            .arg(parts.size())
                            .toStdString());
                    }
                    catch (const std::exception& e) {
                        throw std::runtime_error(gppTr(
                            "NormalJsonTranslator.normalJsonBeforeRun",
                            "分割文件 [%1] 时出错: %2")
                            .arg(wide2Ascii(relJsonPath))
                            .arg(e.what())
                            .toStdString());
                    }
                }
                if (m_transEngine != TransEngine::Rebuild && m_transEngine != TransEngine::ShowNormal) {
                    m_controller->setRuntimeFiles(runtimeFileTotals);
                }
            };
        if (m_splitFileMethod == "Equal") {
            splitFunc(splitJsonArrayEqual);
        }
        else if (m_splitFileMethod == "Num") {
            splitFunc(splitJsonArrayNum);
        }
        else if (m_splitFileMethod != "No") {
            throw std::invalid_argument(gppTr(
                "NormalJsonTranslator.normalJsonBeforeRun",
                "未知的文件分割模式: %1, 请使用 'No', 'Equal', 'Num'")
                .arg(m_splitFileMethod)
                .toStdString());
        }
    }

    std::vector<fs::path> relFilePaths = m_splitFileEnabled
        ? (m_splitFilePartsToJson | std::views::keys | std::ranges::to<std::vector>())
        : std::move(relJsonPaths);

    if (m_sortMethod == "size") {
        std::ranges::sort(relFilePaths, [&](const fs::path& a, const fs::path& b)
            {
                return m_splitFileEnabled ? (fs::file_size(m_inputCacheDir / a) > fs::file_size(m_inputCacheDir / b))
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
        throw std::invalid_argument(gppTr("NormalJsonTranslator.normalJsonBeforeRun", "未知的排序模式: %1")
            .arg(m_sortMethod)
            .toStdString());
    }

    // 分析跨文件连续重复块，并将带引用信息的输入统一写入 inputCacheDir。
    if (m_reuseRepeatedBlocks) {
        if (isApiTranslationEngine(m_transEngine) || m_transEngine == TransEngine::ShowNormal) {
            const fs::path& sourceRootDir = m_splitFileEnabled ? m_inputCacheDir : m_inputDir;
            std::vector<std::pair<fs::path, ordered_json>> filesWithData;
            filesWithData.reserve(relFilePaths.size());

            for (const fs::path& relFilePath : relFilePaths) {
                ordered_json data = parseOrderedJson(sourceRootDir / relFilePath, ifs);
                filesWithData.emplace_back(relFilePath, std::move(data));
            }

            RepeatedBlockReferenceMap repeatedBlockReferences = buildRepeatedBlockReferenceMap(filesWithData, m_repeatedBlockMinSize);
            if (!repeatedBlockReferences.targetToSourceMap.empty()) {
                for (auto& [relFilePath, data] : filesWithData) {
                    addReferenceInfoToInputJson(relFilePath, data, repeatedBlockReferences);
                    const fs::path cachedInputPath = m_inputCacheDir / relFilePath;
                    atomicOutputFile(ofs, cachedInputPath, data.dump(2));
                }
                m_logger->info(gppTr(
                    "NormalJsonTranslator.normalJsonBeforeRun",
                    "连续重复块引用分析完成，阈值 %1，共配置引用 %2 句")
                    .arg(m_repeatedBlockMinSize)
                    .arg(repeatedBlockReferences.targetToSourceMap.size())
                    .toStdString()
                );
            }
            else {
                m_reuseRepeatedBlocks = false;
                m_logger->info(gppTr(
                    "NormalJsonTranslator.normalJsonBeforeRun",
                    "连续重复块引用分析完成，未发现长度不小于 %1 的重复块")
                    .arg(m_repeatedBlockMinSize)
                    .toStdString());
            }
        }
        else {
            m_reuseRepeatedBlocks = false;
        }
    }

    // Agent 模式初始化共享状态文件。
    if (m_agentEnabled) {
        m_transAgent = std::make_unique<NormalJsonTranslatorTransAgent>(
            m_transEngine,
            m_controller,
            m_logger,
            m_apiPool,
            m_gptDictionary,
            m_onPerformApi,
            m_projectDir,
            m_splitFileEnabled ? m_inputCacheDir : m_inputDir,
            m_transCacheDir,
            m_agentTermLedgerPath,
            m_agentFileNotesDir,
            m_agentSystemPrompt,
            m_agentUserPrompt,
            m_targetLang,
            m_apiStrategy,
            m_maxRequestCount,
            m_apiTimeOutMs,
            m_agentMaxTurnsPerChunk,
            m_agentCompactContextThresholdBytes,
            m_agentSearchResultLimit,
            m_agentContextLinesLimit,
            m_inputBlockMaxLines,
            m_problemMaxLines,
            m_glossaryMaxLines,
            m_smartRetry,
            m_checkQuota,
            m_transCacheMutex,
            relFilePaths,
            m_gptDictionaryPaths,
            m_agentProjectNotePath,
            [this](Sentence* se)
            {
                this->preProcess(se);
            }
        );
    }

    m_currentRunRelFilePaths = std::move(relFilePaths);
}

void NormalJsonTranslator::normalJsonAfterRun()
{
    if (!m_currentRunRelFilePaths.has_value() || m_transEngine == TransEngine::ShowNormal) {
        return;
    }
    if (m_transEngine != TransEngine::Rebuild) {
        m_problemOverview = buildProblemOverviewFromCache(m_transCacheDir, m_currentRunRelFilePaths.value(), m_logger);
    }
    else {
        auto& overviewArray = m_problemOverview.get_ref<ordered_json::array_t&>();
        std::ranges::sort(overviewArray, [](const ordered_json& a, const ordered_json& b)
            {
                const int result = StrCmpLogicalW(ascii2Wide(a.at("filename").get<std::string>()).c_str(),
                    ascii2Wide(b.at("filename").get<std::string>()).c_str());
                if (result == 0) {
                    return a.at("index").get<int>() < b.at("index").get<int>();
                }
                return result < 0;
            });
    }

    // 汇总所有问题概览。
    if (m_problemOverview.empty()) {
        m_logger->info(gppTr("NormalJsonTranslator.normalJsonAfterRun", "\n\n```\n无问题概览\n```\n")
            .toStdString());
    }
    else {
        std::ofstream ofs;
        if (m_problemOverviewFormat == "json") {
            atomicOutputFile(ofs, m_projectDir / "ProblemOverview.json", m_problemOverview.dump(2));
        }
        else {
            atomicOutputFile(ofs, m_projectDir / "ProblemOverview.toml",
                toml::format("problemOverview", json2Toml(m_problemOverview)));
        }
        m_logger->debug(gppTr(
            "NormalJsonTranslator.normalJsonAfterRun",
            "已生成 [ProblemOverview.%1] 文件")
            .arg(m_problemOverviewFormat)
            .toStdString());

        absl::btree_map<std::string_view, absl::btree_set<std::string_view>> problemMap;
        for (const ordered_json& item : m_problemOverview) {
            const std::string& filename = item.at("filename").get_ref<const std::string&>();
            for (const ordered_json& problemItem : item.at("problems")) {
                const std::string& problem = problemItem.get_ref<const std::string&>();
                auto& fileNames = problemMap[problem];
                if (fileNames.size() <= 3) {
                    fileNames.insert(filename);
                }
            }
        }

        std::string problemOverviewStr = gppTr(
            "NormalJsonTranslator.normalJsonAfterRun",
            "\n\n```\n问题概览:\n")
            .toStdString();
        size_t problemCount = 0;
        for (const auto& [problem, files] : problemMap) {
            std::string fileStr = "(";
            size_t fileCount = 0;
            for (const auto& file : files) {
                if (fileCount == 3) {
                    break;
                }
                fileStr.append(file).append(", ");
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
        m_logger->error(problemOverviewStr + gppTr(
            "NormalJsonTranslator.normalJsonAfterRun",
            "问题概览结束\n```\n")
            .toStdString());
    }

    // 保存 rolling context 缓存，供下次运行恢复上下文。
    {
        try {
            const json j = m_rollingContextCacheMap;
            atomicOutputFile(m_rollingContextCachePath, j.dump(2));
            m_logger->debug(gppTr("NormalJsonTranslator.normalJsonAfterRun", "rolling context 缓存已保存至 [%1]")
                .arg(wide2Ascii(m_rollingContextCachePath))
                .toStdString());
        }
        catch (...) {
            m_logger->error(gppTr("NormalJsonTranslator.normalJsonAfterRun", "rolling context 缓存 [%1] 保存失败")
                .arg(wide2Ascii(m_rollingContextCachePath))
                .toStdString());
        }
    }

    for (const auto& cacheDir : { m_inputCacheDir, m_outputCacheDir }) {
	    if (fs::exists(cacheDir)) {
            fs::remove_all(cacheDir);
	    }
    }
    if (!m_controller->shouldStop() && m_transEngine == TransEngine::Rebuild &&
        m_controller->m_completedSentences != m_controller->m_totalSentences)
    {
        m_logger->critical(gppTr(
            "NormalJsonTranslator.normalJsonAfterRun",
            "重建过程中有句子未命中缓存 (%1 / %2 lines)，请检查日志以定位问题")
            .arg(m_controller->m_completedSentences.load())
            .arg(m_controller->m_totalSentences.load())
            .toStdString());
    }
}

void NormalJsonTranslator::normalJsonProcessFiles(const std::vector<fs::path>& relFilePaths)
{
    std::vector<std::future<void>> results;
    m_threadPool.resize(std::min(m_threadsNum, (int)relFilePaths.size()));
    for (const auto& relFilePath : relFilePaths) {
        results.emplace_back(m_threadPool.push([=](const int id)
            {
                ActiveWorkerGuard workerGuard(m_controller);
                try {
                    this->processFile(relFilePath, id + 1);
                }
                catch (const std::exception& e) {
                    if (m_transEngine != TransEngine::Rebuild && m_transEngine != TransEngine::ShowNormal) {
                        m_controller->recordRuntimeTransError(RuntimeTransErrorEvent{
                            .kind = "file",
                            .message = e.what(),
                            .filename = wide2Ascii(relFilePath)
                        });
                    }
                    throw;
                }
            }));
    }
    m_logger->info(gppTr(
        "NormalJsonTranslator.normalJsonProcessFiles",
        "已将 %1 个文件任务分配到线程池，等待处理完成...")
        .arg(results.size())
        .toStdString());
    waitForThreads(m_threadPool, results);
}

void NormalJsonTranslator::normalJsonProcess()
{
    if (m_transEngine == TransEngine::DumpName) {
        m_controller->updateBar(m_controller->m_totalSentences.load());
        return;
    }
    if (m_transEngine == TransEngine::NameTrans) {
        m_nameTranslator->run(m_nameTablePath);
        m_nameTranslator.reset();
        return;
    }
    if (m_transEngine == TransEngine::GenDict) {
        const fs::path outputFilePath = m_projectDir / L"ProjGptDict-Gen.toml";
        m_dictionaryGenerator->generate(outputFilePath);
        m_dictionaryGenerator.reset();
        return;
    }
    if (!m_currentRunRelFilePaths.has_value()) {
        return;
    }
    normalJsonProcessFiles(m_currentRunRelFilePaths.value());

    if (m_reuseRepeatedBlocks && isApiTranslationEngine(m_transEngine)) {
        resolveRepeatedBlockReferences();
    }
    if (m_agentEnabled && m_transAgent) {
        m_transAgent->applyAgentSuggestions();
    }
}

void NormalJsonTranslator::resolveRepeatedBlockReferences()
{
    struct FileBundle {
        ordered_json input = ordered_json::array();
        json cache = json::array();
    };

    const auto& relFilePaths = m_currentRunRelFilePaths.value();

    absl::flat_hash_map<fs::path, FileBundle> fileBundles;
    std::ifstream ifs;
    std::ofstream ofs;
    int totalReferenceCount = 0;

    for (const fs::path& relFilePath : relFilePaths) {
        FileBundle bundle;
        bundle.input = parseOrderedJson(m_inputCacheDir / relFilePath, ifs);
        for (const ordered_json& item : bundle.input) {
            if (getRefToFromItem(item).has_value()) {
                ++totalReferenceCount;
            }
        }
        const fs::path cachePath = m_transCacheDir / relFilePath;
        if (fs::exists(cachePath)) {
            bundle.cache = parseJson(cachePath, ifs);
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

    for (auto& [relFilePath, bundle] : fileBundles) {
        for (json& cacheItem : bundle.cache) {
            const auto refTo = getRefToFromItem(cacheItem);
            if (!refTo.has_value()) {
                continue;
            }
            const auto sourceIt = cacheByPosition.find(refTo.value());
            if (sourceIt == cacheByPosition.end()) {
                continue;
            }
            const json& sourceItem = *sourceIt->second;
            cacheItem["translated_raw_text"] = sourceItem.value("translated_raw_text", "");
            cacheItem["translated_by"] = sourceItem.value("translated_by", "");
            cacheItem["translated_view_text"] = sourceItem.value("translated_view_text", "");
            if (sourceItem.contains("problems")) {
                cacheItem["problems"] = sourceItem["problems"];
            }
            else {
                cacheItem.erase("problems");
            }
            if (sourceItem.contains("name_translated")) {
                cacheItem["name_translated"] = sourceItem["name_translated"];
            }
            if (sourceItem.contains("names_translated")) {
                cacheItem["names_translated"] = sourceItem["names_translated"];
            }
            cacheItem.erase("_gpp_ref_pending");
            ++resolvedCount;
        }

        const fs::path cachePath = m_transCacheDir / relFilePath;
        atomicOutputFile(ofs, cachePath, bundle.cache.dump(2));

        if (!m_repeatedBlockCompletedRelFilePaths.contains(relFilePath)) {
            continue;
        }

        const bool hasRefPending = std::ranges::any_of(bundle.cache, [](const json& cacheItem)
            {
                return isRefPendingFromItem(cacheItem);
            });

        if (hasRefPending) {
            m_logger->debug(gppTr(
                "NormalJsonTranslator.resolveRepeatedBlockReferences",
                "文件 [%1] 仍有未回填的连续重复块引用，跳过本轮最终输出")
                .arg(wide2Ascii(relFilePath))
                .toStdString());
            m_repeatedBlockCompletedRelFilePaths.erase(relFilePath);
            continue;
        }

        absl::flat_hash_map<int, const json*> cacheByIndex;
        for (const json& cacheItem : bundle.cache) {
            const int index = cacheItem.value("index", -1);
            if (index >= 0) {
                cacheByIndex[index] = &cacheItem;
            }
        }

        for (auto [index, item] : bundle.input | std::views::enumerate) {
            eraseItemReferenceInfo(item);
            const auto cacheIt = cacheByIndex.find((int)index);
            if (cacheIt == cacheByIndex.end()) {
                continue;
            }
            const json& cacheItem = *cacheIt->second;
            if (cacheItem.contains("name_translated")) {
                item["name"] = cacheItem["name_translated"];
            }
            else if (cacheItem.contains("names_translated")) {
                item["names"] = cacheItem["names_translated"];
            }
            item["message"] = cacheItem.value("translated_view_text", "");
            if (m_outputWithSrc && cacheItem.contains("original_text")) {
                item["src_msg"] = cacheItem["original_text"];
            }
        }

        const fs::path outputPath = m_splitFileEnabled ? (m_outputCacheDir / relFilePath) : (m_outputDir / relFilePath);
        atomicOutputFile(ofs, outputPath, bundle.input.dump(2));
    }

    m_logger->info(gppTr(
        "NormalJsonTranslator.resolveRepeatedBlockReferences",
        "连续重复块引用回填完成，共复制 (%1 / %2) 句")
        .arg(resolvedCount)
        .arg(totalReferenceCount)
        .toStdString());

    if (m_splitFileEnabled) {
        for (const auto& [originalRelFilePath, splitFileParts] : m_jsonToSplitFileParts) {
            const bool allPartsResolved = std::ranges::all_of(splitFileParts, [&](const auto& part)
                {
                    return m_repeatedBlockCompletedRelFilePaths.contains(part.first);
                });
            if (!allPartsResolved) {
                m_logger->debug(gppTr(
                    "NormalJsonTranslator.resolveRepeatedBlockReferences",
                    "文件 [%1] 尚未翻译完毕或分割输出尚未全部回填完成，跳过本轮合并")
                    .arg(wide2Ascii(originalRelFilePath))
                    .toStdString());
                continue;
            }
            combineOutputFiles(originalRelFilePath, splitFileParts, m_outputCacheDir, m_outputDir, m_logger);
            if (m_onFileProcessed) {
                m_onFileProcessed(originalRelFilePath);
            }
        }
    }
    else if (m_onFileProcessed) {
        for (const fs::path& relFilePath : m_repeatedBlockCompletedRelFilePaths) {
            m_onFileProcessed(relFilePath);
        }
    }
}

void NormalJsonTranslator::run()
{
    NormalJsonTranslator::normalJsonInit();
    NormalJsonTranslator::normalJsonBeforeRun();
    NormalJsonTranslator::normalJsonProcess();
    NormalJsonTranslator::normalJsonAfterRun();
}
