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

import ConditionTool;
import DictionaryGenerator;
import NameTranslator;
import NormalJsonTranslatorHelperTool;
import NLPTool;
import Tool;

namespace fs = std::filesystem;
namespace py = pybind11;

void NormalJsonTranslator::processFile(const fs::path& relInputPath, int threadId)
{
    if (m_controller->shouldStop()) {
        return;
    }
    if (shouldReportRuntimeWorkbench()) {
        m_controller->setRuntimeStage("处理文件", wide2Ascii(relInputPath));
    }
    m_logger->debug("[线程 {}] 开始处理文件: {}", threadId, wide2Ascii(relInputPath));

    std::ifstream ifs;
    const fs::path inputPath = m_needsCombining ? (m_inputCacheDir / relInputPath) : (m_inputDir / relInputPath);
    const fs::path outputPath = m_needsCombining ? (m_outputCacheDir / relInputPath) : (m_outputDir / relInputPath);
    const fs::path cachePath = m_transCacheDir / relInputPath;
    const fs::path showNormalPath = m_projectDir / L"gt_show_normal" / relInputPath;

    createParent(outputPath);
    createParent(cachePath);
    ordered_json jSentences;
    std::vector<Sentence> sentences;

    // 1. 读取输入文件并构造句子链表。
    try {
        ifs.open(inputPath, std::ios::binary);
        jSentences = ordered_json::parse(ifs);
        ifs.close();
        for (const auto& [index, item] : jSentences | std::views::enumerate) {
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
            item["message"].get_to(se.original_text);
            sentences.push_back(std::move(se));
        }
        for (auto [se1, se2] : std::views::adjacent<2>(sentences)) {
            se1.next = &se2;
            se2.prev = &se1;
        }
        for (Sentence& se : sentences) {
            preProcess(&se);
        }
    }
    catch (const json::exception& e) {
        throw std::runtime_error(std::format("[线程 {}] [文件 {}] 解析失败: {}", threadId, wide2Ascii(relInputPath), e.what()));
    }

    // 2. ShowNormal 模式只输出预处理后的结构，不进入后续翻译链。
    if (m_transEngine == TransEngine::ShowNormal) {
        json showNormalJson = json::array();
        for (const auto& se : sentences) {
            json showNormalObj;
            if (se.nameType == NameType::Single) {
                showNormalObj["name"] = se.name;
            }
            else if (se.nameType == NameType::Multiple) {
                showNormalObj["names"] = se.names;
            }
            showNormalObj["original_text"] = se.original_text;
            if (!se.other_info.empty()) {
                showNormalObj["other_info"] = se.other_info;
            }
            showNormalObj["pre_processed_text"] = se.pre_processed_text;
            showNormalJson.push_back(std::move(showNormalObj));
            recordSentenceDone(relInputPath, se);
        }
        createParent(showNormalPath);
        std::ofstream ofs(showNormalPath, std::ios::binary);
        ofs << showNormalJson.dump(2);
        ofs.close();
        return;
    }

    // 3. 保存问题概览，供 afterRun 汇总。
    auto saveProblemOverviewFunc = [&]()
    {
        const std::string relInputPathStr = wide2Ascii(relInputPath);
        for (const auto& se : sentences) {
            if (se.problems.empty() || !se.complete) {
                continue;
            }
            toml::ordered_table tbl;
            tbl["filename"] = relInputPathStr;
            tbl["index"] = se.index;
            if (se.nameType == NameType::Single) {
                tbl["name"] = se.name;
                tbl["name_preview"] = se.name_preview;
            }
            else if (se.nameType == NameType::Multiple) {
                tbl["names"] = se.names;
                tbl["names_preview"] = se.names_preview;
            }
            tbl["original_text"] = se.original_text;
            if (!se.other_info.empty()) {
                tbl["other_info"] = se.other_info;
            }
            tbl["pre_processed_text"] = se.pre_processed_text;
            tbl["pre_translated_text"] = se.pre_translated_text;
            tbl["problems"] = se.problems;
            tbl["translated_by"] = se.translated_by;
            tbl["translated_preview"] = se.translated_preview;
            m_problemOverview.push_back(std::move(tbl));
        }
    };

    std::vector<Sentence*> toTranslate;

    // 4. 读取 trans_cache，并根据缓存命中情况决定本轮实际需要翻译哪些句子。
    {
        absl::flat_hash_map<std::string, json> cacheMap;

        auto insertJsonArrToCacheMap = [&](const json& jsonArr)
            {
                for (const auto& [index, item] : jsonArr | std::views::enumerate) {
                    std::string cacheKey = generateCacheKey(jsonArr, index);
                    cacheMap.insert({ std::move(cacheKey), item });
                }
            };

        auto usePotentialPartFileCacheToInsertCacheMap = [&](const fs::path& potentialCachePath)
            {
                try {
                    json jsonArr;
                    {
                        std::shared_lock<std::shared_mutex> lock(m_transCacheMutex);
                        ifs.open(potentialCachePath, std::ios::binary);
                        jsonArr = json::parse(ifs);
                        ifs.close();
                    }
                    insertJsonArrToCacheMap(jsonArr);
                }
                catch (const json::exception& e) {
                    throw std::runtime_error(std::format(
                        "[线程 {}] 缓存文件 {} 解析失败: {}",
                        threadId, wide2Ascii(fs::relative(potentialCachePath, m_transCacheDir)), e.what()
                    ));
                }
            };

        std::vector<fs::path> cachePaths;

        auto readAllPotentialPartFileCache = [&](const std::wstring& cacheSpec, const fs::path& specParentDir, const std::optional<fs::path>& additionalCachePath = std::nullopt)
            {
                for (const auto& entry : fs::directory_iterator(specParentDir)) {
                    if (!entry.is_regular_file()) {
                        continue;
                    }
    #ifdef _WIN32
                    if (PathMatchSpecW(entry.path().filename().wstring().c_str(), cacheSpec.c_str())) {
                        if (m_needsCombining) {
                            const int diff = calculateCachePartIndexDiff(relInputPath.wstring(), entry.path().wstring());
                            if (std::abs(diff) > m_cacheSearchDistance) {
                                continue;
                            }
                        }
                        if (!std::ranges::contains(cachePaths, entry.path())) {
                            cachePaths.push_back(entry.path());
                        }
                    }
    #endif
                }
                if (additionalCachePath.has_value()) {
                    cachePaths.push_back(additionalCachePath.value());
                }
                for (const auto& cp : cachePaths) {
                    usePotentialPartFileCacheToInsertCacheMap(cp);
                }
            };

        // 同名缓存优先级最高。
        if (fs::exists(cachePath)) {
            cachePaths.push_back(cachePath);
        }
        if (m_transEngine != TransEngine::Rebuild && !m_agentReconciling) { // agentReconciling 时不应该读取其它 part 的缓存
            if (m_needsCombining) {
                const std::optional<fs::path> additionalCachePath = [&]() -> std::optional<fs::path>
                    {
                        if (const auto it = m_splitFilePartsToJson.find(relInputPath);
                            it != m_splitFilePartsToJson.end() && fs::exists(m_transCacheDir / it->second))
                        {
                            return m_transCacheDir / it->second;
                        }
                        return std::nullopt;
                    }();
                const size_t pos = relInputPath.filename().wstring().rfind(L"_part_");
                const std::wstring orgStem = relInputPath.filename().wstring().substr(0, pos);
                const std::wstring cacheSpec = orgStem + L"_part_*.json";
                readAllPotentialPartFileCache(cacheSpec, m_transCacheDir / relInputPath.parent_path(), additionalCachePath);
            }
            else {
                const std::wstring cacheSpec = relInputPath.stem().wstring() + L"_part_*.json";
                readAllPotentialPartFileCache(cacheSpec, m_transCacheDir / relInputPath.parent_path());
            }
        }

        // 再合并一次所有候选缓存，尽量覆盖边缘命中场景。
        {
            json totalCacheJsonList = json::array();
            for (const auto& cp : cachePaths) {
                try {
                    json cacheJsonList;
                    {
                        std::shared_lock<std::shared_mutex> lock(m_transCacheMutex);
                        ifs.open(cp, std::ios::binary);
                        cacheJsonList = json::parse(ifs);
                        ifs.close();
                    }
                    totalCacheJsonList.insert(totalCacheJsonList.end(), cacheJsonList.begin(), cacheJsonList.end());
                }
                catch (const json::exception& e) {
                    throw std::runtime_error(std::format("[线程 {}] 缓存文件 {} 解析失败: {}", threadId, wide2Ascii(cp), e.what()));
                }
            }
            insertJsonArrToCacheMap(totalCacheJsonList);
        }

        if (!m_agentReconciling) {
            for (Sentence& se : sentences) {
                if (se.complete) {
                    postProcess(&se);
                    recordSentenceDone(relInputPath, se);
                    continue;
                }
                const std::string key = generateCacheKey(&se);
                const auto it = cacheMap.find(key);
                if (it == cacheMap.end()) {
                    toTranslate.push_back(&se);
                    continue;
                }
                const auto& item = it->second;
                if (auto jit = item.find("problems"); jit != item.end()) {
                    jit->get_to(se.problems);
                }
                if (m_transEngine != TransEngine::Rebuild && hasRetranslKey(m_retranslKeys, item, &se)) {
                    toTranslate.push_back(&se);
                    continue;
                }

                se.pre_translated_text = item.value("pre_translated_text", "");
                se.translated_by = item.value("translated_by", "");
                se.complete = true;
                postProcess(&se);
                recordSentenceDone(relInputPath, se);
            }
        }
        else {
	        if (const auto it = m_agentReconcileTargetsByFile.find(relInputPath); it != m_agentReconcileTargetsByFile.end()) {
	            const absl::flat_hash_set<int>& reconcileTargetIds = it->second;
	            for (Sentence& se : sentences) {
		            if (reconcileTargetIds.contains(se.index)) {
	                    se.complete = false;
	                    toTranslate.push_back(&se);
	                }
	                else {
	                    const std::string key = generateCacheKey(&se);
	                    const auto cacheIt = cacheMap.find(key);
	                    if (cacheIt == cacheMap.end()) {
	                        throw std::runtime_error(std::format("Reconcile processFile 未在 cacheMap 中找到缓存: {}", se.original_text));
	                    }
	                    const auto& item = cacheIt->second;
	                    if (auto jit = item.find("problems"); jit != item.end()) {
	                        jit->get_to(se.problems);
	                    }
	                    se.pre_translated_text = item.value("pre_translated_text", "");
	                    se.translated_by = item.value("translated_by", "");
	                    se.complete = true;
	                    postProcess(&se);
	                }
	            }
	        }
            else {
                throw std::runtime_error(std::format("Reconcile processFile 未在表中找到文件: {}", wide2Ascii(relInputPath)));
            }
        }

        if (!toTranslate.empty()) {
            m_logger->info(
                "[线程 {}] [文件 {}] 共 {} 句，命中缓存/跳过 {} 句，需翻译 {} 句。",
                threadId, wide2Ascii(relInputPath), sentences.size(), sentences.size() - toTranslate.size(), toTranslate.size()
            );
        }

        if (m_transEngine == TransEngine::Rebuild && !toTranslate.empty()) {
            const std::string notFoundSentences = toTranslate
                | std::views::transform([](const auto& se) { return se->original_text; })
                | std::views::join_with('\n')
                | std::ranges::to<std::string>();
            m_logger->critical(
                "[线程 {}] [文件 {}] 有 {} 句未命中缓存，这些句子是: {}",
                threadId, wide2Ascii(relInputPath), toTranslate.size(), notFoundSentences
            );
            saveCache(sentences, cachePath);
            std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
            saveProblemOverviewFunc();
            return;
        }
    }

    // 5. 进入批处理调度。普通模式每个 batch 一次问答；Agent 模式则在内部跑多轮循环。
    if (m_transEngine != TransEngine::Rebuild && !toTranslate.empty()) {
        std::unique_ptr<py::gil_scoped_release> release = m_pythonTranslator ? std::make_unique<py::gil_scoped_release>() : nullptr;

        int batchCount = 0;
        const std::string filePathWithHash = std::format("{}{:08X}", wide2Ascii(relInputPath), calculateFileCRC64(inputPath));
        std::string backgroundText = [&]()
            {
                std::shared_lock<std::shared_mutex> lock(m_backgroundTextCacheMapMutex);
                if (const auto it = m_backgroundTextCacheMap.find(filePathWithHash); it != m_backgroundTextCacheMap.end()) {
                    return it->second;
                }
                return std::string{};
            }();

        const Sentence* pLastSentence = nullptr;
        for (auto batchView : toTranslate | std::views::chunk(m_batchSize)) {
            if (!backgroundText.empty() && pLastSentence) {
                if (batchView.front()->index - pLastSentence->index > m_batchSize) {
                    backgroundText.clear();
                }
            }
            pLastSentence = batchView.back();

            if (m_controller->shouldStop()) {
                if (!backgroundText.empty()) {
                    std::lock_guard<std::shared_mutex> lock(m_backgroundTextCacheMapMutex);
                    m_backgroundTextCacheMap[filePathWithHash] = backgroundText;
                }
                m_logger->debug("[线程 {}] [文件 {}] 已停止翻译", threadId, wide2Ascii(relInputPath));
                std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
                saveCache(sentences, cachePath);
                saveProblemOverviewFunc();
                return;
            }

            if (m_agentEnabled) {
                translateBatchAgent(relInputPath, batchView, backgroundText, threadId);
            }
            else {
                translateBatch(relInputPath, batchView, backgroundText, threadId);
            }
            for (Sentence* se : batchView) {
                postProcess(se);
                if (se->complete) {
                    recordSentenceDone(relInputPath, *se, true);
                }
            }

            if (++batchCount % m_saveCacheInterval == 0) {
                m_logger->debug("[线程 {}] [文件 {}] 达到保存间隔，正在更新缓存文件...", threadId, wide2Ascii(relInputPath));
                std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
                saveCache(sentences, cachePath);
            }
        }

        std::lock_guard<std::shared_mutex> lock(m_backgroundTextCacheMapMutex);
        m_backgroundTextCacheMap.erase(filePathWithHash);
    }

    // 6. 最终保存缓存与问题概览。
    {
        std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
        m_logger->debug("[线程 {}] [文件 {}] 翻译完成，正在进行最终保存...", threadId, wide2Ascii(relInputPath));
        saveCache(sentences, cachePath);
        saveProblemOverviewFunc();
    }

    // 7. 组装最终输出文件。
    for (auto [se, item] : std::views::zip(sentences, jSentences)) {
        if (se.nameType == NameType::Single) {
            item["name"] = se.name_preview;
        }
        else if (se.nameType == NameType::Multiple) {
            item["names"] = se.names_preview;
        }
        item["message"] = se.translated_preview;
        if (m_outputWithSrc) {
            item["src_msg"] = se.original_text;
        }
    }

    std::ofstream ofs(outputPath, std::ios::binary);
    ofs << jSentences.dump(2);
    ofs.close();

    m_logger->info("[线程 {}] [文件 {}] 处理完成。", threadId, wide2Ascii(relInputPath));

    // 8. 分割文件模式下，最后一个 part 负责触发合并。
    if (m_needsCombining) {
        const fs::path& originalRelFilePath = m_splitFilePartsToJson[relInputPath];
        absl::flat_hash_map<fs::path, bool>& splitFileParts = m_jsonToSplitFileParts[originalRelFilePath];
        {
            std::lock_guard<std::mutex> lock(m_outputMutex);
            splitFileParts[relInputPath] = true;
            if (std::ranges::any_of(splitFileParts, [](const auto& p) { return !p.second; })) {
                m_logger->debug("文件 {} 尚未全部处理完成，跳过合并。", wide2Ascii(originalRelFilePath));
                return;
            }
            m_logger->debug("开始合并 {} 的缓存文件...", wide2Ascii(originalRelFilePath));
        }
        combineOutputFiles(originalRelFilePath, splitFileParts, m_outputCacheDir, m_outputDir, m_logger);
        if (m_onFileProcessed) {
            std::unique_lock<std::mutex> lock(m_outputMutex, std::defer_lock);
            if (!m_pythonTranslator) {
                lock.lock();
            }
            m_onFileProcessed(originalRelFilePath);
        }
        m_logger->debug("[线程 {}] [文件 {}] 合并处理完成。", threadId, wide2Ascii(originalRelFilePath));
    }
    else if (m_onFileProcessed) {
        std::unique_lock<std::mutex> lock(m_outputMutex, std::defer_lock);
        if (!m_pythonTranslator) {
            lock.lock();
        }
        m_onFileProcessed(relInputPath);
    }
}
