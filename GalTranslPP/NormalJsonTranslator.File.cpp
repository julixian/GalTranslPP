module;

#define PYBIND11_HEADERS
#define SOL2_HEADERS
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

void NormalJsonTranslator::processFile(const fs::path& relInputPath, int threadId)
{
    if (m_controller->shouldStop()) {
        return;
    }
    if (m_transEngine != TransEngine::Rebuild && m_transEngine != TransEngine::ShowNormal) {
        m_controller->setRuntimeStage(gppTr("NormalJsonTranslator.processFile", "处理文件")
            .toStdString(), wide2Ascii(relInputPath));
    }
    m_logger->debug(gppTr("NormalJsonTranslator.processFile", "[线程 %1] 开始处理文件: %2")
        .arg(threadId)
        .arg(wide2Ascii(relInputPath))
        .toStdString());

    std::ifstream ifs;
    const fs::path inputPath = (m_splitFileEnabled || m_reuseRepeatedBlocks)
	                            ? (m_inputCacheDir / relInputPath)
	                            : (m_inputDir / relInputPath);
    const fs::path outputPath = (m_splitFileEnabled || m_reuseRepeatedBlocks)
        ? (m_outputCacheDir / relInputPath)
        : (m_outputDir / relInputPath);
    const fs::path cachePath = m_transCacheDir / relInputPath;
    const fs::path showNormalPath = m_projectDir / L"gt_show_normal" / relInputPath;

    createParent(outputPath);
    createParent(cachePath);
    ordered_json jSentences;
    std::vector<Sentence> sentences;

    // 读取输入文件并构造句子链表。
    try {
        jSentences = parseOrderedJson(inputPath, ifs);
        sentences.reserve(jSentences.size());
        const std::string relInputFileName = wide2Ascii(relInputPath);
        for (const auto& [index, item] : jSentences | std::views::enumerate) {
            Sentence se;
            se.index = (int)index;
            se.fileName = relInputFileName;
            if (auto jit = item.find("name"); jit != item.end()) {
                se.nameType = NameType::Single;
                jit->get_to(se.name);
            }
            else if (jit = item.find("names"); jit != item.end()) {
                se.nameType = NameType::Multiple;
                jit->get_to(se.names);
            }
            item["message"].get_to(se.orig);
            itemReferenceInfoToSentence(item, se);
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
    catch (const std::exception& e) {
        throw std::runtime_error(gppTr(
            "NormalJsonTranslator.processFile",
            "[线程 %1] [文件 %2] 解析失败: %3")
            .arg(threadId)
            .arg(wide2Ascii(relInputPath))
            .arg(e.what())
            .toStdString());
    }

    // ShowNormal 模式只输出预处理后的结构，不进入后续翻译链。
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
            showNormalObj["original_text"] = se.orig;
            if (!se.otherinfo.empty()) {
                showNormalObj["other_info"] = se.otherinfo;
            }
            sentenceReferenceInfoToItem(showNormalObj, se, false);
            showNormalObj["pre_processed_text"] = se.preproc;
            showNormalJson.push_back(std::move(showNormalObj));
            recordSentenceDoneHelper(relInputPath, se);
        }
        atomicOutputFile(showNormalPath, showNormalJson.dump(2));
        return;
    }

    // Rebuild 模式预收集问题概览，避免重建输出时再读一遍缓存。
    auto saveRebuildProblemOverviewFunc = [&]()
        {
            const std::string relInputPathStr = wide2Ascii(relInputPath);
            for (const auto& se : sentences) {
                if (se.problems.empty() || !se.transCompleted) {
                    continue;
                }
                ordered_json tbl = ordered_json::object();
                tbl["file_name"] = relInputPathStr;
                tbl["index"] = se.index;
                if (se.nameType == NameType::Single) {
                    tbl["name"] = se.name;
                    tbl["name_translated"] = se.nametrans;
                }
                else if (se.nameType == NameType::Multiple) {
                    tbl["names"] = se.names;
                    tbl["names_translated"] = se.namestrans;
                }
                tbl["original_text"] = se.orig;
                if (!se.otherinfo.empty()) {
                    tbl["other_info"] = se.otherinfo;
                }
                tbl["pre_processed_text"] = se.preproc;
                tbl["problems"] = se.problems;
                tbl["translated_by"] = se.transby;
                tbl["translated_raw_text"] = se.transraw;
                tbl["translated_view_text"] = se.transview;
                m_problemOverview.push_back(std::move(tbl));
            }
        };

    std::vector<Sentence*> toTranslate;

    // 读取 trans_cache，并根据缓存命中情况决定本轮实际需要翻译哪些句子。
    {
        absl::flat_hash_map<std::string, json> cacheMap;

        auto insertJsonArrToCacheMap = [&](const json& jsonArr)
            {
                for (const auto& [index, item] : jsonArr | std::views::enumerate) {
                    if (isRefPendingFromItem(item)) {
                        continue;
                    }
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
                        jsonArr = parseJson(potentialCachePath, ifs);
                    }
                    insertJsonArrToCacheMap(jsonArr);
                }
                catch (const std::exception& e) {
                    throw std::runtime_error(gppTr(
                        "NormalJsonTranslator.processFile",
                        "[线程 %1] 缓存文件 [%2] 解析失败: %3")
                        .arg(threadId)
                        .arg(wide2Ascii(fs::relative(potentialCachePath, m_transCacheDir)))
                        .arg(e.what())
                        .toStdString());
                }
            };

        std::vector<fs::path> cachePaths;

        auto readAllPotentialPartFileCache = [&](const std::wstring& cacheSpec, const fs::path& specParentDir,
            const std::optional<fs::path>& additionalCachePath = std::nullopt)
            {
                for (const auto& entry : fs::directory_iterator(specParentDir)) {
                    if (!entry.is_regular_file()) {
                        continue;
                    }
                    if (
#ifdef _WIN32
                        PathMatchSpecW(entry.path().filename().wstring().c_str(), cacheSpec.c_str())
#endif
                        )
                    {
                        if (m_splitFileEnabled) {
                            const int diff = calculateCachePartIndexDiff(relInputPath.wstring(), entry.path().wstring());
                            if (std::abs(diff) > m_cacheSearchDistance) {
                                continue;
                            }
                        }
                        if (!std::ranges::contains(cachePaths, entry.path())) {
                            cachePaths.push_back(entry.path());
                        }
                    }
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
        if (m_transEngine != TransEngine::Rebuild) {
            if (m_splitFileEnabled) {
                const std::optional<fs::path> additionalCachePath = [&]() -> std::optional<fs::path>
                    {
                        if (const auto it = m_splitFilePartsToJson.find(relInputPath);
                            it != m_splitFilePartsToJson.end())
                        {
                        	fs::path origFilePath = m_transCacheDir / it->second;
                            if (fs::exists(origFilePath)) {
                                return origFilePath;
                            }
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
                        cacheJsonList = parseJson(cp, ifs);
                    }
                    totalCacheJsonList.insert(totalCacheJsonList.end(), cacheJsonList.begin(), cacheJsonList.end());
                }
                catch (const std::exception& e) {
                    throw std::runtime_error(gppTr(
                        "NormalJsonTranslator.processFile",
                        "[线程 %1] 缓存文件 [%2] 解析失败: %3")
                        .arg(threadId)
                        .arg(wide2Ascii(cp))
                        .arg(e.what())
                        .toStdString());
                }
            }
            insertJsonArrToCacheMap(totalCacheJsonList);
        }

        for (Sentence& se : sentences) {
            if (se.ref.has_value()) {
                se.nametrans = se.name;
                se.namestrans = se.names;
                se.transby = "GPP-Reference";
                se.transraw = se.preproc;
                se.transview = se.preproc;
                se.transCompleted = true;
                se.problemAnalyzeDisabled = true;
                se.isRefPending = true;
                recordSentenceDoneHelper(relInputPath, se);
                continue;
            }
            if (se.transCompleted) {
                postProcess(&se);
                recordSentenceDoneHelper(relInputPath, se);
                continue;
            }
            const std::string key = generateCacheKey(&se);
            const auto it = cacheMap.find(key);
            if (it == cacheMap.end()) {
                toTranslate.push_back(&se);
                continue;
            }
            const auto& item = it->second;
            if (m_transEngine == TransEngine::Rebuild ||
                !hasRetranslKey(m_retranslKeys, item, se))
            {
                se.transby = item.value("translated_by", "");
                se.transraw = item.value("translated_raw_text", "");
                se.transCompleted = true;
                postProcess(&se);
                recordSentenceDoneHelper(relInputPath, se);
                continue;
            }

            toTranslate.push_back(&se);
        }

        if (!toTranslate.empty()) {
            m_logger->info(gppTr(
                "NormalJsonTranslator.processFile",
            "[线程 %1] [文件 %2] 共 %3 句，命中缓存/跳过 %4 句，需翻译 %5 句")
                .arg(threadId)
                .arg(wide2Ascii(relInputPath))
                .arg(sentences.size())
                .arg(sentences.size() - toTranslate.size())
                .arg(toTranslate.size())
                .toStdString()
            );
        }

        if (m_transEngine == TransEngine::Rebuild && !toTranslate.empty()) {
            const std::string notFoundSentences = toTranslate
                | std::views::transform([](const auto& se) { return se->preproc; })
                | std::views::join_with('\n')
                | std::ranges::to<std::string>();
            m_logger->critical(gppTr(
                "NormalJsonTranslator.processFile",
                "[线程 %1] [文件 %2] 有 %3 句未命中缓存，这些句子是: %4")
                .arg(threadId)
                .arg(wide2Ascii(relInputPath))
                .arg(toTranslate.size())
                .arg(notFoundSentences)
                .toStdString()
            );
            saveCache(sentences, cachePath);
            std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
            saveRebuildProblemOverviewFunc();
            return;
        }
    }

    // 进入批处理调度。普通模式每个 batch 一次问答；Agent 模式则在内部跑多轮循环。
    if (m_transEngine != TransEngine::Rebuild && !toTranslate.empty()) {
        int batchCount = 0;
        const std::string filePathWithHash = std::format("{}{:08X}", wide2Ascii(relInputPath), calculateFileCRC64(inputPath));
        std::string rollingContext = [&]()
            {
                std::shared_lock<std::shared_mutex> lock(m_rollingContextCacheMapMutex);
                if (const auto it = m_rollingContextCacheMap.find(filePathWithHash);
                    it != m_rollingContextCacheMap.end())
                {
                    return it->second;
                }
                return std::string{};
            }();

        const Sentence* pLastSentence = nullptr;
        for (auto [batchIndex, batchView] : toTranslate
            | std::views::chunk(m_batchSize) | std::views::enumerate)
        {
            if (!rollingContext.empty() && pLastSentence) {
                if (batchView.front()->index - pLastSentence->index > m_batchSize) {
                    rollingContext.clear();
                }
            }
            pLastSentence = batchView.back();

            if (m_controller->shouldStop()) {
                if (!rollingContext.empty()) {
                    std::lock_guard<std::shared_mutex> lock(m_rollingContextCacheMapMutex);
                    m_rollingContextCacheMap[filePathWithHash] = rollingContext;
                }
                m_logger->debug(gppTr("NormalJsonTranslator.processFile", "[线程 %1] [文件 %2] 已停止翻译")
                    .arg(threadId)
                    .arg(wide2Ascii(relInputPath))
                    .toStdString());
                std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
                saveCache(sentences, cachePath);
                return;
            }

            if (m_agentEnabled) {
                m_transAgent->translateBatch(relInputPath, batchView, rollingContext, threadId, batchIndex);
            }
            else {
                int recursionIndex = 0;
                int recursionConut = 0;
                translateBatch(relInputPath, batchView, rollingContext,
                    threadId, batchIndex, recursionIndex, recursionConut);
            }
            for (Sentence* se : batchView) {
                postProcess(se);
                recordSentenceDoneHelper(relInputPath, *se, true);
            }

            if (++batchCount % m_saveCacheInterval == 0) {
                m_logger->debug(gppTr(
                    "NormalJsonTranslator.processFile",
                    "[线程 %1] [文件 %2] 达到保存间隔，正在更新缓存文件...")
                    .arg(threadId)
                    .arg(wide2Ascii(relInputPath))
                    .toStdString());
                std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
                saveCache(sentences, cachePath);
            }
        }

        std::lock_guard<std::shared_mutex> lock(m_rollingContextCacheMapMutex);
        m_rollingContextCacheMap.erase(filePathWithHash);
    }

    // 最终保存缓存与问题概览。
    {
        if (m_transEngine == TransEngine::Rebuild) {
            saveCache(sentences, cachePath);
            m_logger->debug(gppTr(
                "NormalJsonTranslator.processFile",
                "[线程 %1] [文件 %2] 重建完成，正在进行最终保存...")
                .arg(threadId)
                .arg(wide2Ascii(relInputPath))
                .toStdString());
            std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
            saveRebuildProblemOverviewFunc();
        }
        else {
            m_logger->debug(gppTr(
                "NormalJsonTranslator.processFile",
                "[线程 %1] [文件 %2] 翻译完成，正在进行最终保存...")
                .arg(threadId)
                .arg(wide2Ascii(relInputPath))
                .toStdString());
            std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
            saveCache(sentences, cachePath);
            saveRebuildProblemOverviewFunc();
        }
    }

    // 组装最终输出文件。
    for (auto [se, item] : std::views::zip(sentences, jSentences)) {
        if (m_reuseRepeatedBlocks) {
            item.erase("_gpp_ref_by");
            item.erase("_gpp_ref_pending");
        }
        if (se.nameType == NameType::Single) {
            item["name"] = se.nametrans;
        }
        else if (se.nameType == NameType::Multiple) {
            item["names"] = se.namestrans;
        }
        item["message"] = se.transview;
        if (m_outputWithSrc) {
            item["src_msg"] = se.orig;
        }
    }

    atomicOutputFile(outputPath, jSentences.dump(2));

    m_logger->info(gppTr("NormalJsonTranslator.processFile", "[线程 %1] [文件 %2] 处理完成")
        .arg(threadId)
        .arg(wide2Ascii(relInputPath))
        .toStdString());

    if (m_reuseRepeatedBlocks) {
        m_logger->debug(gppTr(
            "NormalJsonTranslator.processFile",
            "[线程 %1] [文件 %2] 连续重复块引用模式启用，延后最终输出回填与文件回调")
            .arg(threadId)
            .arg(wide2Ascii(relInputPath))
            .toStdString());
        std::lock_guard<std::mutex> lock(m_outputMutex);
        m_repeatedBlockCompletedRelFilePaths.insert(relInputPath);
        return;
    }

    // 分割文件模式下，最后一个 part 负责触发合并。
    if (m_splitFileEnabled) {
        const fs::path& originalRelFilePath = m_splitFilePartsToJson[relInputPath];
        absl::flat_hash_map<fs::path, bool>& splitFileParts = m_jsonToSplitFileParts[originalRelFilePath];
        {
            std::lock_guard<std::mutex> lock(m_outputMutex);
            splitFileParts[relInputPath] = true;
            if (std::ranges::any_of(splitFileParts, [](const auto& p) { return !p.second; })) {
                m_logger->debug(gppTr("NormalJsonTranslator.processFile", "文件 %1 尚未全部处理完成，跳过合并")
                    .arg(wide2Ascii(originalRelFilePath))
                    .toStdString());
                return;
            }
        }
        m_logger->debug(gppTr("NormalJsonTranslator.processFile", "开始合并 %1 的缓存文件...")
            .arg(wide2Ascii(originalRelFilePath))
            .toStdString());
        combineOutputFiles(originalRelFilePath, splitFileParts, m_outputCacheDir, m_outputDir, m_logger);
        if (m_onFileProcessed) {
            m_onFileProcessed(originalRelFilePath);
        }
        m_logger->debug(gppTr("NormalJsonTranslator.processFile", "[线程 %1] [文件 %2] 合并处理完成")
            .arg(threadId)
            .arg(wide2Ascii(originalRelFilePath))
            .toStdString());
    }
    else if (m_onFileProcessed) {
        m_onFileProcessed(relInputPath);
    }
}
