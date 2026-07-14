module;

#include "GPPMacros.hpp"
#include <toml.hpp>
#include <ctpl_stl.h>

module DictionaryGenerator;

import :ReviewAgent;
import AgentCommonSourceView;
import Tool;

namespace fs = std::filesystem;

DictionaryGenerator::~DictionaryGenerator() {
    saveTokenizeCache(m_tokenizeCacheMap, m_tokenizeCachePath, m_logger);
}

DictionaryGenerator::DictionaryGenerator(const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, const std::unique_ptr<ApiPool>& apiPool,
    const NLPTokenizeFunc& tokenizeSourceLangFunc, const fs::path& otherCacheDir,
    const std::function<void(Sentence*)>& preProcessFunc, const std::function<std::string(std::string)>& onPerformApi, const std::function<DictList(DictList)>& onDictProcessed,
    const std::string& systemPrompt, const std::string& userPrompt, const std::string& apiStrategy, const std::string& targetLang,
    int threadsNum, int maxRequestCount, int apiTimeOutMs, bool checkQuota,
    bool agentEnabled, const fs::path& projectDir, const fs::path& inputDir,
    const std::vector<fs::path>& relJsonPaths, const std::optional<fs::path>& agentProjectNotePath,
    const std::string& genDictReviewSystemPrompt, const std::string& genDictReviewUserPrompt,
    int agentMaxTurnsPerChunk, int agentSearchResultLimit, int agentContextLinesLimit)
    : m_controller(controller), m_logger(logger), m_apiPool(apiPool),
    m_preProcessFunc(preProcessFunc), m_onPerformApi(onPerformApi), m_onDictProcessed(onDictProcessed),
    m_tokenizeSourceLangFunc(tokenizeSourceLangFunc),
    m_systemPrompt(systemPrompt), m_userPrompt(userPrompt), m_apiStrategy(apiStrategy), m_targetLang(targetLang),
    m_threadsNum(threadsNum), m_maxRequestCount(maxRequestCount), m_apiTimeOutMs(apiTimeOutMs), m_checkQuota(checkQuota),
    m_agentEnabled(agentEnabled),
    m_projectDir(projectDir),
    m_inputDir(inputDir),
    m_relJsonPaths(relJsonPaths),
    m_agentProjectNotePath(agentProjectNotePath),
    m_genDictReviewSystemPrompt(genDictReviewSystemPrompt),
    m_genDictReviewUserPrompt(genDictReviewUserPrompt),
    m_agentMaxTurnsPerChunk(agentMaxTurnsPerChunk),
    m_agentSearchResultLimit(agentSearchResultLimit),
    m_agentContextLinesLimit(agentContextLinesLimit),
    m_tokenizeCachePath(otherCacheDir / L"tokenizeCache_dictgen.json")
{
	loadTokenizeCache(m_tokenizeCacheMap, m_tokenizeCachePath, m_logger);
}

void DictionaryGenerator::preprocessAndTokenize(const std::vector<fs::path>& jsonFiles) {
    m_logger->info(gppTr("DictionaryGenerator.preprocessAndTokenize", "阶段一: 预处理和分词...")
        .toStdString());
    std::string currentSegment;
    constexpr size_t MaxSegmentLength = 512;

    std::ifstream ifs;
    for (const auto& jsonFile : jsonFiles)
    {
        const json data = parseJson(jsonFile, ifs);

        std::vector<Sentence> sourceSentences;
        sourceSentences.reserve(data.size());

        for (const auto& [index, item] : data | std::views::enumerate) {
            ++m_totalSentences;
            Sentence se;
            se.index = (int)index;
            if (auto it = item.find("name"); it != item.end()) {
                se.nameType = NameType::Single;
                it->get_to(se.name);
            }
            else if (it = item.find("names"); it != item.end()) {
                se.nameType = NameType::Multiple;
                it->get_to(se.names);
            }
            else {
                se.nameType = NameType::None;
            }
            se.orig = item.value("message", "");

            m_preProcessFunc(&se);
            if (se.transCompleted) {
                continue;
            }
            replaceStrInplace(se.preproc, "<br>", "");
            replaceStrInplace(se.preproc, "<tab>", "");

            sourceSentences.push_back(se);

            if (se.nameType == NameType::Single && !se.name.empty()) {
                m_nameSet.insert(se.name);
                m_wordCounter[se.name] += 2;
            }
            else if (se.nameType == NameType::Multiple) {
                for (const auto& name : se.names | std::views::filter([](const std::string& name) { return !name.empty(); })) {
                    m_nameSet.insert(name);
                    m_wordCounter[name] += 2;
                }
            }

            std::string currentText = getNameString(se);
            if (!currentText.empty()) {
                currentText += ": ";
            }
            currentText += se.preproc + "\n";
            currentSegment += currentText;
            if (currentSegment.length() > MaxSegmentLength && countGraphemes(currentSegment) > MaxSegmentLength) {
                m_segments.push_back(std::move(currentSegment));
                currentSegment.clear();
            }
        }

        if (!currentSegment.empty()) {
            m_segments.push_back(std::move(currentSegment));
            currentSegment.clear();
        }

        m_reviewSourceFiles.push_back(buildAgentCommonSourceFileViewFromSentences(sourceSentences));
    }

    if (!currentSegment.empty()) {
        m_segments.push_back(std::move(currentSegment));
    }

    m_logger->info(gppTr(
        "DictionaryGenerator.preprocessAndTokenize",
        "共分割成 %1 个文本块，开始进行分词 (使用依赖 Python 且未进行 GPU加速 的分词器这步会非常慢)...")
        .arg(m_segments.size())
        .toStdString());
    m_controller->makeBar((int)m_segments.size(), 1);
    ActiveWorkerGuard workerGuard(m_controller);
    m_segmentWords.reserve(m_segments.size());

    absl::flat_hash_set<std::string> wordsInSegment;
    auto procEntityVecFunc = [&](const EntityVec& entityVec, const std::string& segment)
        {
            for (const auto& entity : entityVec) {
                wordsInSegment.insert(entity.front());
                ++m_wordCounter[entity.front()];
            }
        };

    static const absl::btree_set<std::string_view> excludeEntities =
    {
        "TITLE_AFFIX", "QUANTITY", "ORDINAL", "DATE", "MONEY"
    };
    for (const auto& segment : m_segments) {
        if (auto it = m_tokenizeCacheMap.find(segment); it != m_tokenizeCacheMap.end()) {
            EntityVec& entityVec = it->second;
            procEntityVecFunc(entityVec, segment);
        }
        else {
            NLPResult result = m_tokenizeSourceLangFunc(segment);
            EntityVec& entityVec = std::get<1>(result);
            std::erase_if(entityVec, [](const NLPPair& entity)
                {
                    if (excludeEntities.contains(entity[1])) {
                        return true;
                    }
                    return hasPunctuation(entity[0]);
                });
            procEntityVecFunc(entityVec, segment);
            m_tokenizeCacheMap.insert({ segment, std::move(entityVec) });
        }
        m_segmentWords.push_back(std::move(wordsInSegment));
        wordsInSegment.clear();

        m_controller->updateBar();
        if (m_controller->shouldStop()) {
            break;
        }
    }
}

void DictionaryGenerator::callLLMToGenerate(int segmentIndex, int batchIndex, int threadId) {
    if (m_controller->shouldStop()) {
        return;
    }

    const std::string& text = m_segments[segmentIndex];
    std::string hint = m_nameSet | std::views::filter([&](const auto& name) { return text.contains(name); }) 
	    | std::views::join_with('\n') | std::ranges::to<std::string>();
    if (!hint.empty()) {
        hint = "The real names in this list should always be added into glossary:\n" + hint;
    }

    std::string prompt = m_userPrompt;
    replaceStrInplace(prompt, "[TargetLang]", m_targetLang);
    replaceStrInplace(prompt, "[Input]", text);
    replaceStrInplace(prompt, "[Hint]", hint.empty() ? "None" : hint);

    json messages = json::array({
        {{"role", "system"}, {"content", m_systemPrompt}},
        {{"role", "user"}, {"content", prompt}}
        });

    int requestCount = 0;
    while (requestCount < m_maxRequestCount) {
        if (m_controller->shouldStop()) {
            return;
        }

        const std::optional<TranslationApi> apiOpt = m_apiPool->getApi(m_apiStrategy);
        if (!apiOpt) {
            throw std::runtime_error(gppTr(
                "DictionaryGenerator.callLLMToGenerate",
                "没有可用的 Api key 了")
                .toStdString());
        }
        const TranslationApi& currentApi = apiOpt.value();

        json payload = { {"messages", messages} };

        std::string logBlock;
        if (!hint.empty()) {
            logBlock += "\nHint:\n" + hint + "\n";
        }
        logBlock += "\ninputBlock:\n" + text;
        m_logger->info(gppTr("DictionaryGenerator.callLLMToGenerate",
            "[线程 %1] [批次 %2] [请求 %3] 开始生成术语表:\n%4")
            .arg(threadId)
            .arg(batchIndex)
            .arg(requestCount + 1)
            .arg(logBlock)
            .toStdString());

        ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, m_controller, m_logger,
            threadId, m_apiTimeOutMs);

        const std::string checkResponseLogPrefix = gppTr(
            "DictionaryGenerator.callLLMToGenerate",
            "[线程 %1] [批次 %2] [请求 %3]")
            .arg(threadId)
            .arg(batchIndex)
            .arg(requestCount + 1)
            .toStdString();
        if (
            !checkResponse(
            response, m_apiPool, currentApi, checkResponseLogPrefix, fs::path{},
            m_apiStrategy, m_controller, m_logger, requestCount, m_checkQuota
            ))
        {
            continue;
        }

        m_logger->info(gppTr("DictionaryGenerator.callLLMToGenerate",
            "[线程 %1] [批次 %2] [请求 %3] AI 字典生成成功:\n%4")
            .arg(threadId)
            .arg(batchIndex)
            .arg(requestCount + 1)
            .arg(response.content)
            .toStdString());
        const auto lines = splitStringView(response.content, '\n');
        for (const auto& line : lines) {
            const auto parts = splitStringView(line, '\t');
            if (parts.size() < 3 || parts[0].starts_with("Source") || parts[0].starts_with("NULL")) {
                continue;
            }

            std::lock_guard<std::mutex> lock(m_resultMutex);
            if (int& counter = m_finalCounter[parts[0]]; ++counter == 2) {
                m_logger->debug(gppTr(
                    "DictionaryGenerator.callLLMToGenerate",
                    "发现重复术语: %1\t%2\t%3")
                    .arg(parts[0])
                    .arg(parts[1])
                    .arg(parts[2])
                    .toStdString());
            }
            m_finalDict.emplace_back(std::string(parts[0]), std::string(parts[1]), std::string(parts[2]));
        }
        return;
    }

    m_logger->error(gppTr(
        "DictionaryGenerator.callLLMToGenerate",
        "[线程 %1] [批次 %2] 在 %3 次请求后彻底失败，没有生成字典")
        .arg(threadId)
        .arg(batchIndex)
        .arg(requestCount)
        .toStdString());

}

void DictionaryGenerator::generate(const fs::path& outputFilePath) {

    if (m_relJsonPaths.empty()) {
        throw std::runtime_error(gppTr("DictionaryGenerator.generate", "没有输入文件，无法生成字典。")
            .toStdString());
    }

    const std::vector<fs::path> jsonFiles = m_relJsonPaths
        | std::views::transform([&](const auto& p) { return m_inputDir / p; })
        | std::ranges::to<std::vector>();

    preprocessAndTokenize(jsonFiles);

    if (m_controller->shouldStop()) {
        m_logger->info(gppTr("DictionaryGenerator.generate", "任务终止，将不会生成字典文件").toStdString());
        return;
    }

    m_logger->info(gppTr("DictionaryGenerator.generate", "阶段二: 搜索并选择信息量最大的文本块(单线程)...")
        .toStdString());

    absl::flat_hash_set<std::string> allWords;
    allWords.reserve(m_wordCounter.size());
    for (const auto& [word, count] : m_wordCounter) {
        if (count >= 2 || m_nameSet.contains(word)) {
            allWords.insert(word);
        }
    }

    std::vector<absl::flat_hash_set<std::string>> filteredSegmentWords;
    filteredSegmentWords.reserve(m_segmentWords.size());
    for (const auto& segment : m_segmentWords) {
        absl::flat_hash_set<std::string> filteredSet;
        for (const auto& word : segment) {
            if (allWords.contains(word)) {
                filteredSet.insert(word);
            }
        }
        filteredSegmentWords.push_back(std::move(filteredSet));
    }

    absl::flat_hash_set<std::string> coveredWords;
    coveredWords.reserve(allWords.size());
    std::vector<int> selectedIndices;
    std::vector<uint8_t> usedIndices(filteredSegmentWords.size(), 0);

    m_controller->makeBar((int)allWords.size(), 1);
    {
        ActiveWorkerGuard workerGuard(m_controller);
        while (coveredWords.size() < allWords.size()) {
            int bestIndex = -1;
            size_t maxNewCoverage = 0;
            for (size_t i = 0; i < filteredSegmentWords.size(); ++i) {
                if (usedIndices[i]) {
                    continue;
                }
                size_t currentNewCoverage = 0;
                for (const auto& word : filteredSegmentWords[i]) {
                    if (!coveredWords.contains(word)) {
                        ++currentNewCoverage;
                    }
                }
                if (currentNewCoverage > maxNewCoverage) {
                    maxNewCoverage = currentNewCoverage;
                    bestIndex = (int)i;
                }
            }
            if (bestIndex < 0) {
                break;
            }
            usedIndices[bestIndex] = 1;
            selectedIndices.push_back(bestIndex);
            for (const auto& word : filteredSegmentWords[bestIndex]) {
                coveredWords.insert(word);
            }
            m_controller->updateBar();
        }
    }

    if (int maxSelectedIndicesCount = std::max(m_totalSentences / 250, 128); selectedIndices.size() > maxSelectedIndicesCount) {
        selectedIndices.resize(maxSelectedIndicesCount);
    }
    if (m_controller->shouldStop()) {
        m_logger->info(gppTr("DictionaryGenerator.generate", "任务终止，将不会生成字典文件").toStdString());
        return;
    }

    m_logger->info(gppTr("DictionaryGenerator.generate", "阶段三: 启动 %1 个线程，向 AI 发送 %2 个任务...")
        .arg(m_threadsNum)
        .arg(selectedIndices.size())
        .toStdString());
    m_controller->makeBar((int)selectedIndices.size(), m_threadsNum);

    ctpl::thread_pool pool(m_threadsNum);
    std::vector<std::future<void>> results;

    for (const auto& [taskIndex, segmentIdx] : selectedIndices | std::views::enumerate) {
        results.emplace_back(pool.push([=](int threadId)
            {
                ActiveWorkerGuard workerGuard(m_controller);
                this->callLLMToGenerate(segmentIdx, (int)taskIndex + 1, threadId + 1);
                m_controller->updateBar();
            }));
    }
    waitForThreads(pool, results);

    if (m_controller->shouldStop()) {
        m_logger->info(gppTr("DictionaryGenerator.generate", "任务终止，将保存已经生成的字典结果").toStdString());
    }
    m_logger->info(gppTr("DictionaryGenerator.generate", "阶段四: 整理并保存结果...").toStdString());

    // 按出现次数排序
    std::ranges::sort(m_finalDict, [&](const auto& a, const auto& b)
        {
            return m_finalCounter.at(std::get<0>(a)) > m_finalCounter.at(std::get<0>(b));
        });

    DictList finalList;
    if (m_agentEnabled && !m_controller->shouldStop() && !m_finalDict.empty()) {
        auto reviewAgent = std::make_unique<DictionaryGeneratorReviewAgent>(
            m_controller,
            m_logger,
            m_apiPool,
            m_onPerformApi,
            m_projectDir,
            m_relJsonPaths,
            m_agentProjectNotePath,
            m_genDictReviewSystemPrompt,
            m_genDictReviewUserPrompt,
            m_apiStrategy,
            m_targetLang,
            m_maxRequestCount,
            m_threadsNum,
            m_agentMaxTurnsPerChunk,
            m_agentSearchResultLimit,
            m_agentContextLinesLimit,
            m_apiTimeOutMs,
            m_checkQuota
        );
        DictList reviewedList = reviewAgent->review(
            m_finalDict,
            m_finalCounter,
            m_segments,
            selectedIndices,
            m_nameSet,
            m_wordCounter,
            m_reviewSourceFiles
        );
        finalList = m_onDictProcessed ? m_onDictProcessed(std::move(reviewedList)) : std::move(reviewedList);
        if (m_controller->shouldStop()) {
            m_logger->info(gppTr(
                "DictionaryGenerator.generate",
                "任务终止，已保留完成审校的词条")
                .toStdString());
        }
        else {
            m_logger->info(gppTr(
                "DictionaryGenerator.generate",
                "阶段四: 字典审校 Agent 完成，使用审校后的字典结果")
                .toStdString());
        }
    }
    else if (m_onDictProcessed) {
        finalList = m_onDictProcessed(m_finalDict);
    }
    else {
        for (const auto& item : m_finalDict) {
            const auto& src = std::get<0>(item);
            const auto& note = std::get<2>(item);
            if (m_finalCounter.at(src) > 1 ||
                note.contains(gppTr("DictionaryGenerator.generate", "人名").toStdString()) ||
                note.contains(gppTr("DictionaryGenerator.generate", "地名").toStdString()) ||
                m_wordCounter.contains(src) ||
                m_nameSet.contains(src))
            {
                finalList.push_back(item);
            }
        }

        absl::flat_hash_map<std::string, std::string> seen;
        seen.reserve(finalList.size());
        std::erase_if(finalList, [&](std::tuple<std::string, std::string, std::string>& item)
            {
                const auto& orgWord = std::get<0>(item);
                auto& note = std::get<2>(item);
                if (const auto it = seen.find(orgWord); it != seen.end()) {
                    const auto& noteInSeen = it->second;
                    static const std::string boyStr = gppTr("DictionaryGenerator.generate", "男性").toStdString();
                    static const std::string girlStr = gppTr("DictionaryGenerator.generate", "女性").toStdString();
                    const bool boy = noteInSeen.contains(boyStr) || note.contains(boyStr);
                    const bool girl = noteInSeen.contains(girlStr) || note.contains(girlStr);
                    if (boy && girl) {
                        note += gppTr("DictionaryGenerator.generate", "，与其它字典存在性别争议")
                            .toStdString();
                        return false;
                    }
                    return true;
                }
                seen.insert({ orgWord, note });
                return false;
            });
    }


    toml::ordered_value arr = toml::array{};
    for (const auto& item : finalList) {
        arr.push_back(toml::ordered_table{ { "org", std::get<0>(item) }, { "rep", std::get<1>(item) }, { "note", std::get<2>(item) } });
    }

    arr.as_array_fmt().fmt = toml::array_format::multiline;
    atomicOutputFile(outputFilePath, toml::format(toml::ordered_value{ toml::ordered_table{{ "gptDict", arr }} }));

    m_logger->info(gppTr("DictionaryGenerator.generate", "字典生成完成，共 %1 个词语，已保存到 [%2]")
        .arg(finalList.size())
        .arg(wide2Ascii(outputFilePath))
        .toStdString());
}
