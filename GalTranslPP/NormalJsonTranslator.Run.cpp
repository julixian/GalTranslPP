module;

#define PYBIND11_HEADERS
#define PCRE2_HEADERS
#include "GPPMacros.hpp"
#ifdef _WIN32
#include <Windows.h>
#include <Shlwapi.h>
#endif
#include <toml.hpp>
#include <ctpl_stl.h>
#include <sol/sol.hpp>
#include <proxy/proxy.h>

module NormalJsonTranslator;

import ConditionTool;
import DictionaryGenerator;
import NameTranslator;
import NormalJsonTranslatorHelperTool;
import NLPTool;
import Tool;

namespace fs = std::filesystem;
namespace py = pybind11;

namespace {
    std::string nowTimestampString()
    {
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        return std::to_string(now);
    }

    json loadJsonFileOrDefault(const fs::path& path, const json& fallback = json::object())
    {
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

    void saveJsonFilePretty(const fs::path& path, const json& value)
    {
        createParent(path);
        std::ofstream ofs(path, std::ios::binary);
        ofs << value.dump(2);
    }

    bool hasAgentWorkUnitArtifacts(
        const fs::path& relFilePath,
        const fs::path& outputDir,
        const fs::path& outputCacheDir,
        const fs::path& transCacheDir,
        bool needsCombining
    ) {
        const fs::path outputPath = needsCombining ? (outputCacheDir / relFilePath) : (outputDir / relFilePath);
        const fs::path cachePath = transCacheDir / relFilePath;
        return fs::exists(outputPath) && fs::exists(cachePath);
    }
}

std::optional<std::vector<fs::path>> NormalJsonTranslator::beforeRun()
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
                    ++m_totalSentences;
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

        if (m_totalSentences == 0) {
            throw std::runtime_error("未找到有效的 Sentence");
        }
        m_controller->makeBar(m_totalSentences, m_threadsNum);

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
            m_completedSentences += m_totalSentences;
            m_controller->updateBar(m_totalSentences);
            return std::nullopt;
        }
    }

    // 2. 独立的人名翻译模式直接在这里结束。
    if (m_transEngine == TransEngine::NameTrans) {
        NameTranslator nameTranslator(
            m_controller, m_logger, m_apiPool, m_gptDictionary, m_onPerformApi,
            m_systemPrompt, m_userPrompt, m_apiStrategy, m_targetLang, m_maxRetries, m_apiTimeOutMs, m_checkQuota
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
        DictionaryGenerator generator(
            m_controller, m_logger, m_apiPool, m_tokenizeSourceLangFunc, m_otherCacheDir,
            std::move(preProcessFunc), m_onPerformApi, m_onDictProcessed,
            m_systemPrompt, m_userPrompt, m_apiStrategy, m_targetLang,
            m_maxRetries, m_threadsNum, m_apiTimeOutMs, m_checkQuota
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
            return str2Lower(a) < str2Lower(b);
        });
#else
        std::ranges::sort(relFilePaths);
#endif
    }
    else {
        throw std::invalid_argument(std::format("未知的排序模式: {}", m_sortMethod));
    }

    // 6. Agent 模式启动前构建 run_state、search_catalog 等共享状态文件。
    if (m_agentEnabled) {
        m_agentKnownRelFiles = relFilePaths;
        createParent(m_agentRunStatePath);
        fs::create_directories(m_agentFileNotesDir);
        const json currentAgentConfig = {
            {"threads_num", m_threadsNum},
            {"split_file", m_splitFile},
            {"split_file_num", m_splitFileNum},
            {"chunk_size", m_agentChunkSize},
            {"max_turns_per_chunk", m_agentMaxTurnsPerChunk},
            {"soft_context_chars", m_agentSoftContextChars},
            {"hard_context_chars", m_agentHardContextChars},
            {"lookahead_lines", m_agentLookaheadLines},
            {"search_result_limit", m_agentSearchResultLimit},
            {"allow_cross_file_search", m_agentAllowCrossFileSearch},
            {"native_function_calling", m_agentNativeFunctionCalling},
            {"final_reconcile_single_thread", m_agentFinalReconcileSingleThread},
            {"rewrite_mode", m_agentRewriteMode}
        };
        const std::string configSignature = currentAgentConfig.dump();

        json runState = loadJsonFileOrDefault(m_agentRunStatePath, json::object());
        const json previousAgentConfig = runState.contains("config") && runState["config"].is_object() ? runState["config"] : json::object();
        const bool hasPreviousConfig = !previousAgentConfig.empty();
        const bool threadsChanged = hasPreviousConfig && previousAgentConfig.value("threads_num", m_threadsNum) != m_threadsNum;
        const bool workLayoutChanged = !hasPreviousConfig
            || previousAgentConfig.value("split_file", m_splitFile) != m_splitFile
            || previousAgentConfig.value("split_file_num", m_splitFileNum) != m_splitFileNum
            || previousAgentConfig.value("chunk_size", m_agentChunkSize) != m_agentChunkSize;

        // `threads_num` 变化只影响调度并发度，不改变工作单元本身的边界，因此可以保留旧的
        // `last_committed_index`，只需清掉旧 lease 重新排队未完成任务。
        //
        // `split_file / split_file_num / chunk_size` 变化则属于“工作单元边界变化”：
        // 重新切分后，同一个 part 的内容和覆盖范围都可能变，旧的 `last_committed_index`
        // 已经不能安全映射到新 part，所以这里会：
        // 1. 重建新的工作单元列表
        // 2. 丢弃旧工作单元级进度（把 `last_committed_index` 置回 -1）
        // 3. 保留更稳定的持久化产物：trans_cache / term_ledger / file_note / 已完成输出
        // 后续恢复时会依靠缓存重新命中已翻好的句子，而不是试图沿用旧 part 的游标。

        absl::flat_hash_map<std::string, json> oldEntries;
        if (runState.contains("files") && runState["files"].is_array()) {
            for (const auto& item : runState["files"]) {
                if (!item.is_object()) {
                    continue;
                }
                const std::string relFile = item.value("file", "");
                if (!relFile.empty()) {
                    oldEntries.insert_or_assign(relFile, item);
                }
            }
        }

        int preservedDoneCount = 0;
        int requeuedCount = 0;
        json normalizedFiles = json::array();
        const std::string updatedAt = nowTimestampString();
        for (const auto& relFilePath : relFilePaths) {
            const std::string relFileStr = wide2Ascii(relFilePath);
            json entry = {
                {"file", relFileStr},
                {"status", "pending"},
                {"lease_owner", ""},
                {"last_committed_index", -1},
                {"updated_at", updatedAt}
            };

            const bool artifactDone = hasAgentWorkUnitArtifacts(relFilePath, m_outputDir, m_outputCacheDir, m_transCacheDir, m_needsCombining);
            if (const auto it = oldEntries.find(relFileStr); it != oldEntries.end()) {
                const json& oldEntry = it->second;
                const std::string oldStatus = oldEntry.value("status", "pending");
                if (!workLayoutChanged) {
                    entry["last_committed_index"] = oldEntry.value("last_committed_index", -1);
                }
                if (artifactDone && oldStatus == "done") {
                    // 只有“新工作单元名仍然存在，且输出/缓存成品也都还在”时，才直接继承 done。
                    entry["status"] = "done";
                    ++preservedDoneCount;
                }
                else {
                    // 工作单元边界变了时，不尝试把旧 part 进度硬映射到新 part；
                    // 统一回到 pending，由缓存系统在 processFile 阶段重新消化已完成内容。
                    if (oldStatus == "in_progress" || oldStatus == "done") {
                        ++requeuedCount;
                    }
                    if (workLayoutChanged) {
                        entry["last_committed_index"] = -1;
                    }
                }
            }
            else if (artifactDone) {
                entry["status"] = "done";
                ++preservedDoneCount;
            }

            normalizedFiles.push_back(std::move(entry));
        }

        runState = {
            {"config_signature", configSignature},
            {"config", currentAgentConfig},
            {"updated_at", updatedAt},
            {"files", normalizedFiles}
        };
        saveJsonFilePretty(m_agentRunStatePath, runState);

        if (hasPreviousConfig) {
            if (workLayoutChanged) {
                m_logger->info("Agent 运行配置发生工作单元变化，已重建调度列表并保留 {} 个已完成工作单元。", preservedDoneCount);
            }
            else if (threadsChanged) {
                m_logger->info("Agent 线程数发生变化，已清理旧 lease，并保留 {} 个已完成工作单元。", preservedDoneCount);
            }
            else if (requeuedCount > 0) {
                m_logger->info("Agent 已恢复上次未完成的进度，重新排队 {} 个工作单元。", requeuedCount);
            }
        }

        if (m_needsCombining) {
            for (const auto& item : runState["files"]) {
                const fs::path relFilePath = ascii2Wide(item.value("file", ""));
                const bool isDone = item.value("status", "pending") == "done"
                    && hasAgentWorkUnitArtifacts(relFilePath, m_outputDir, m_outputCacheDir, m_transCacheDir, m_needsCombining);
                if (const auto it = m_splitFilePartsToJson.find(relFilePath); it != m_splitFilePartsToJson.end()) {
                    m_jsonToSplitFileParts[it->second][relFilePath] = isDone;
                }
            }
        }

        saveJsonFilePretty(m_agentSearchCatalogPath, json{
            {"updated_at", nowTimestampString()},
            {"files", relFilePaths | std::views::transform([](const fs::path& p) { return wide2Ascii(p); }) | std::ranges::to<std::vector>()}
        });
        if (!fs::exists(m_agentTermLedgerPath)) {
            saveJsonFilePretty(m_agentTermLedgerPath, json::object());
        }
        if (!fs::exists(m_agentRewriteQueuePath)) {
            saveJsonFilePretty(m_agentRewriteQueuePath, json::array());
        }
    }

    return relFilePaths;
}

void NormalJsonTranslator::afterRun()
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
    if (!m_controller->shouldStop() && m_transEngine == TransEngine::Rebuild && m_completedSentences != m_totalSentences) {
        m_logger->critical("重建过程中有句子未命中缓存 ({}/{} lines)，请检查日志以定位问题。", m_completedSentences.load(), m_totalSentences);
    }
}

void NormalJsonTranslator::process(std::vector<fs::path> relFilePaths)
{
    // Agent 恢复模式下，会在调度前把已完成工作单元过滤掉。
    if (m_agentEnabled && !m_agentReconciling) {
        json runState = loadJsonFileOrDefault(m_agentRunStatePath, json::object());
        absl::flat_hash_map<std::string, std::string> statusByFile;
        if (runState.contains("files") && runState["files"].is_array()) {
            for (const auto& item : runState["files"]) {
                if (!item.is_object()) {
                    continue;
                }
                const std::string relFile = item.value("file", "");
                if (!relFile.empty()) {
                    statusByFile.insert_or_assign(relFile, item.value("status", "pending"));
                }
            }
        }

        if (m_needsCombining) {
            for (const auto& [originalRelFilePath, splitFileParts] : m_jsonToSplitFileParts) {
                const bool allDone = std::ranges::all_of(splitFileParts, [&](const auto& partState)
                {
                    return partState.second
                        && hasAgentWorkUnitArtifacts(partState.first, m_outputDir, m_outputCacheDir, m_transCacheDir, m_needsCombining);
                });
                if (!allDone || fs::exists(m_outputDir / originalRelFilePath)) {
                    continue;
                }
                m_logger->info("Agent 恢复已完成的分割输出合并: {}", wide2Ascii(originalRelFilePath));
                combineOutputFiles(originalRelFilePath, splitFileParts, m_outputCacheDir, m_outputDir, m_logger);
            }
        }

        std::vector<fs::path> pendingFilePaths;
        int skippedDoneCount = 0;
        for (const auto& relFilePath : relFilePaths) {
            const std::string relFileStr = wide2Ascii(relFilePath);
            const auto statusIt = statusByFile.find(relFileStr);
            const bool isDone = statusIt != statusByFile.end()
                && statusIt->second == "done"
                && hasAgentWorkUnitArtifacts(relFilePath, m_outputDir, m_outputCacheDir, m_transCacheDir, m_needsCombining);
            if (isDone) {
                ++skippedDoneCount;
                continue;
            }
            pendingFilePaths.push_back(relFilePath);
        }

        if (skippedDoneCount > 0) {
            m_logger->info("Agent 恢复模式跳过 {} 个已完成工作单元。", skippedDoneCount);
        }
        relFilePaths = std::move(pendingFilePaths);
    }

    if (relFilePaths.empty()) {
        m_logger->info("没有需要重新调度的文件任务。");
        runAgentFinalReconcile();
        return;
    }

    std::vector<std::future<void>> results;
    m_threadPool.resize(std::min(m_threadsNum, (int)relFilePaths.size()));
    for (const auto& filePath : relFilePaths) {
        results.emplace_back(m_threadPool.push([=](const int id)
        {
            m_controller->addThreadNum();
            this->processFile(filePath, id);
            m_controller->reduceThreadNum();
        }));
    }
    m_logger->info("已将 {} 个文件任务分配到线程池，等待处理完成...", results.size());
    waitForThreads(m_threadPool, results);
    runAgentFinalReconcile();
}

void NormalJsonTranslator::run()
{
    NormalJsonTranslator::init();
    std::optional<std::vector<fs::path>> relFilePathsOpt = NormalJsonTranslator::beforeRun();
    if (!relFilePathsOpt.has_value()) {
        return;
    }
    NormalJsonTranslator::process(std::move(relFilePathsOpt.value()));
    NormalJsonTranslator::afterRun();
}
