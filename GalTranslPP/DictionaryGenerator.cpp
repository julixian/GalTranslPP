module;

#include "GPPMacros.hpp"
#include <toml.hpp>
#include <ctpl_stl.h>

module DictionaryGenerator;

import AgentSourceView;
import DictionaryReviewAgent;
import DictionaryReviewIndex;
import Tool;

namespace fs = std::filesystem;

DictionaryGenerator::~DictionaryGenerator() {
    saveTokenizeCache(m_tokenizeCacheMap, m_tokenizeCachePath, m_logger);
}

DictionaryGenerator::DictionaryGenerator(const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger, const std::unique_ptr<APIPool>& apiPool,
    const std::function<NLPResult(const std::string&)>& tokenizeFunc, const fs::path& otherCacheDir,
    const std::function<void(Sentence*)>& preProcessFunc, const std::function<std::string(std::string)>& onPerformApi, const std::function<DictList(DictList)>& onDictProcessed,
    const std::string& systemPrompt, const std::string& userPrompt, const std::string& apiStrategy, const std::string& targetLang,
    int maxRetries, int threadsNum, int apiTimeoutMs, bool checkQuota, const DictionaryGeneratorReviewOptions& reviewOptions)
    : m_controller(controller), m_logger(logger), m_apiPool(apiPool),
    m_tokenizeSourceLangFunc(tokenizeFunc), m_tokenizeCachePath(otherCacheDir / L"tokenizeCache_dictgen.json"),
    m_preProcessFunc(preProcessFunc), m_onPerformApi(onPerformApi), m_onDictProcessed(onDictProcessed),
    m_systemPrompt(systemPrompt), m_userPrompt(userPrompt), m_apiStrategy(apiStrategy), m_targetLang(targetLang),
    m_maxRetries(maxRetries), m_threadsNum(threadsNum), m_apiTimeoutMs(apiTimeoutMs), m_checkQuota(checkQuota),
    m_reviewOptions(reviewOptions)
{
	loadTokenizeCache(m_tokenizeCacheMap, m_tokenizeCachePath, m_logger);
}

void DictionaryGenerator::preprocessAndTokenize(const std::vector<fs::path>& jsonFiles) {
    m_logger->info("阶段一：预处理和分词...");
    std::string currentSegment;
    constexpr size_t MAX_SEGMENT_LEN = 512;

    std::ifstream ifs;
    for (const auto& jsonFile : jsonFiles)
    {
        ifs.open(jsonFile, std::ios::binary);
        json data = json::parse(ifs);
        ifs.close();

        std::vector<Sentence> sourceSentences;
        sourceSentences.reserve(data.size());

        for (const auto& [index, item] : data | std::views::enumerate) {
            ++m_totalSentences;
            Sentence se;
            se.index = (int)index;
            if (item.contains("name")) {
                se.nameType = NameType::Single;
                se.name = item.value("name", "");
            }
            else if (item.contains("names")) {
                se.nameType = NameType::Multiple;
                se.names = item["names"].get<std::vector<std::string>>();
            }
            else {
                se.nameType = NameType::None;
            }
            se.original_text = item.value("message", "");

            m_preProcessFunc(&se);
            if (se.complete) {
                continue;
            }
            replaceStrInplace(se.pre_processed_text, "<br>", "");
            replaceStrInplace(se.pre_processed_text, "<tab>", "");

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

            std::string currentText = getNameString(&se);
            if (!currentText.empty()) {
                currentText += ": ";
            }
            currentText += se.pre_processed_text + "\n";
            currentSegment += currentText;
            if (currentSegment.length() > MAX_SEGMENT_LEN && countGraphemes(currentSegment) > MAX_SEGMENT_LEN) {
                m_segments.push_back(std::move(currentSegment));
                currentSegment.clear();
            }
        }

        if (!currentSegment.empty()) {
            m_segments.push_back(std::move(currentSegment));
            currentSegment.clear();
        }

        m_reviewSourceFiles.push_back(buildAgentSourceFileViewFromSentences(sourceSentences, fs::relative(jsonFile, m_reviewOptions.inputDir)));
    }

    if (!currentSegment.empty()) {
        m_segments.push_back(std::move(currentSegment));
    }

    m_logger->info("共分割成 {} 个文本块，开始进行分词(使用依赖 Python 且未进行 GPU加速 的分词器这步会非常慢)...", m_segments.size());
    m_controller->makeBar((int)m_segments.size(), 1);
    m_controller->addThreadNum();
    m_segmentWords.reserve(m_segments.size());

    absl::flat_hash_set<std::string> wordsInSegment;
    auto procEntityVecFunc = [&](const EntityVec& entityVec, const std::string& segment)
        {
            for (const auto& entity : entityVec) {
                wordsInSegment.insert(entity.front());
                ++m_wordCounter[entity.front()];
            }
            if (m_logger->should_log(spdlog::level::trace)) {
                const std::string entityStr = entityVec | std::views::transform([](const auto& entity)
	                {
		                if (entity.size() > 1) {
			                return "[" + entity[0] + ", " + entity[1] + "]";
		                }
                        return std::string{};
                    }) | std::views::join_with(' ') | std::ranges::to<std::string>();
                m_logger->trace("原文: {}\n分词实体结果: {}", segment, entityStr);
            }
        };

    const absl::btree_set<std::string_view> excludeEntities =
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
            std::erase_if(entityVec, [&excludeEntities](const std::vector<std::string>& entity)
                {
                    if (excludeEntities.contains(entity[1])) {
                        return true;
                    }
                    return removePunctuation(entity.front()).empty();
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
    m_controller->reduceThreadNum();
}

std::vector<int> DictionaryGenerator::solveSentenceSelection() {
    if (m_controller->shouldStop()) {
        return {};
    }
    m_logger->info("阶段二：搜索并选择信息量最大的文本块(单线程)...");

    // 剔除出现次数小于2的词语，人名除外
    absl::flat_hash_set<std::string> allWords;
    allWords.reserve(m_wordCounter.size()); // 预分配空间，减少哈希冲突
    for (const auto& [word, count] : m_wordCounter) {
        if (count >= 2 || m_nameSet.contains(word)) {
            allWords.insert(word);
        }
    }

    // 过滤每个 segment 中的词，只保留在 allWords 中的
    std::vector<absl::flat_hash_set<std::string>> filteredSegmentWords;
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
    m_controller->addThreadNum();

    while (coveredWords.size() < allWords.size()) {
        int bestIndex = -1;
        size_t maxNewCoverage = 0;
        // 手动计算每个未被选择的段落能覆盖的新词数量
        for (size_t i = 0; i < filteredSegmentWords.size(); ++i) {
            if (usedIndices[i]) {
                continue;
            }
            size_t currentNewCoverage = 0;
            // 遍历候选段落中的词
            for (const auto& word : filteredSegmentWords[i]) {
                if (!coveredWords.contains(word)) {
                    ++currentNewCoverage;
                }
            }
            if (currentNewCoverage > maxNewCoverage) {
                maxNewCoverage = currentNewCoverage;
                bestIndex = static_cast<int>(i);
            }
        }
        if (bestIndex != -1) {
            // 将找到的最佳段落标记为已使用
            usedIndices[bestIndex] = 1;
            selectedIndices.push_back(bestIndex);
            // 将新覆盖的词加入 coveredWords 集合
            for (const auto& word : filteredSegmentWords[bestIndex]) {
                coveredWords.insert(word);
            }
            m_controller->updateBar();
        }
        else {
            // 如果没有段落能提供新词，则提前退出
            break;
        }
    }
    m_controller->reduceThreadNum();
    return selectedIndices;
}

void DictionaryGenerator::callLLMToGenerate(int segmentIndex, int threadId) {
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

    int retryCount = 0;
    while (retryCount == 0 || retryCount < m_maxRetries) {
        if (m_controller->shouldStop()) {
            return;
        }

        const std::optional<TranslationApi> apiOpt = m_apiStrategy == "random" ? m_apiPool->getApi() : m_apiPool->getFirstApi();
        if (!apiOpt) {
            throw std::runtime_error("没有可用的 API key 了");
        }
        const TranslationApi& currentApi = apiOpt.value();

        json payload = { {"messages", messages} };

        std::string logBlock;
        if (!hint.empty()) {
            logBlock += "\nHint:\n" + hint + "\n";
        }
        logBlock += "\ninputBlock:\n" + text;
        m_logger->info("[线程 {}] 开始从段落中生成术语表:\n{}", threadId, logBlock);

        const ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, m_controller, m_logger, threadId, m_apiTimeoutMs);

        /*bool checkResponse(const ApiResponse& response, const std::unique_ptr<APIPool>& m_apiPool, const TranslationApi& currentAPI,
            const std::filesystem::path& relInputPath, const std::string& m_apiStrategy, const std::shared_ptr<spdlog::logger>& m_logger,
            int& retryCount, int threadId, bool m_checkQuota);*/
        if (!checkResponse(
            response, m_apiPool, currentApi, L"字典生成——段落输入", m_apiStrategy, m_controller, m_logger, retryCount, threadId, m_checkQuota
        )) {
            continue;
        }
        else {
            m_logger->info("[线程 {}] AI 字典生成成功:\n {}", threadId, response.content);
            const auto lines = splitStringView(response.content, '\n');
            for (const auto& line : lines) {
                const auto parts = splitStringView(line, '\t');
                if (parts.size() < 3 || parts[0].starts_with("Original_terms") || parts[0].starts_with("日文原词") || parts[0].starts_with("NULL")) {
                    continue;
                }

                std::lock_guard<std::mutex> lock(m_resultMutex);
                if (int& counter = m_finalCounter[parts[0]]; ++counter == 2) {
                    m_logger->debug("发现重复术语: {}\t{}\t{}", parts[0], parts[1], parts[2]);
                }
                m_finalDict.emplace_back(std::string(parts[0]), std::string(parts[1]), std::string(parts[2]));
            }
            break;
        }
    }
    if (retryCount >= m_maxRetries) {
        m_logger->error("[线程 {}] AI 字典生成失败，已达到最大重试次数。", threadId);
    }

    m_controller->updateBar();
}

DictList DictionaryGenerator::finalizeCoarseCandidates() const {
    DictList finalList;
    for (const auto& item : m_finalDict) {
        const auto& src = std::get<0>(item);
        const auto& note = std::get<2>(item);
        if (m_finalCounter.at(src) > 1 || note.contains("人名") || note.contains("地名") || m_wordCounter.contains(src) || m_nameSet.contains(src)) {
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
                bool boy = false;
                bool girl = false;
                if (noteInSeen.contains("男性") || note.contains("男性")) {
                    boy = true;
                }
                if (noteInSeen.contains("女性") || note.contains("女性")) {
                    girl = true;
                }
                if (boy && girl) {
                    note += "，与其它字典存在性别争议";
                    return false;
                }
                return true;
            }
            seen.insert({ orgWord, note });
            return false;
        });
    return finalList;
}

void DictionaryGenerator::generate(const std::vector<fs::path>& jsonFiles, const fs::path& outputFilePath) {
    if (jsonFiles.empty()) {
        throw std::runtime_error("没有输入文件，无法生成字典。");
    }

    preprocessAndTokenize(jsonFiles);

    std::vector<int> selectedIndices = solveSentenceSelection();

    if (int maxSelectedIndicesCount = std::max(m_totalSentences / 250, 128); selectedIndices.size() > maxSelectedIndicesCount) {
        selectedIndices.resize(maxSelectedIndicesCount);
    }
    if (m_controller->shouldStop()) {
        m_logger->info("任务终止，将不会生成字典文件。");
        return;
    }

    m_logger->info("阶段三：启动 {} 个线程，向 AI 发送 {} 个任务...", m_threadsNum, selectedIndices.size());
    m_controller->makeBar((int)selectedIndices.size(), m_threadsNum);

    ctpl::thread_pool pool(m_threadsNum);
    std::vector<std::future<void>> results;

    for (int segmentIdx : selectedIndices) {
        results.emplace_back(pool.push([=](int threadId)
            {
                m_controller->addThreadNum();
                this->callLLMToGenerate(segmentIdx, threadId);
                m_controller->reduceThreadNum();
            }));
    }
    waitForThreads(pool, results);

    if (m_controller->shouldStop()) {
        m_logger->info("任务终止，将保存已经生成的字典结果。");
    }
    m_logger->info("阶段四：整理并保存结果...");

    // 按出现次数排序
    std::ranges::sort(m_finalDict, [&](const auto& a, const auto& b)
        {
            return m_finalCounter.at(std::get<0>(a)) > m_finalCounter.at(std::get<0>(b));
        });

	DictList coarseDefaultList = finalizeCoarseCandidates();
    DictList finalList;
    if (m_reviewOptions.enabled) {
        const std::vector<DictionaryReviewTermGroup> termGroups = DictionaryReviewIndex::build(
            m_finalDict, m_finalCounter, m_segments, selectedIndices, m_nameSet, m_wordCounter
        );
        m_controller->makeBar((int)termGroups.size(), 1);
        m_controller->addThreadNum();
        try {
            DictionaryReviewAgentConfig reviewConfig{
                .projectDir = m_reviewOptions.projectDir,
                .relInputFiles = m_reviewOptions.relInputFiles,
                .projectNotePath = m_reviewOptions.projectNotePath,
                .systemPrompt = m_reviewOptions.systemPrompt,
                .userPrompt = m_reviewOptions.userPrompt,
                .apiStrategy = m_apiStrategy,
                .targetLang = m_targetLang,
                .maxRetries = m_maxRetries,
                .maxTurnsPerTerm = m_reviewOptions.maxTurnsPerTerm,
                .searchResultLimit = m_reviewOptions.searchResultLimit,
                .apiTimeoutMs = m_apiTimeoutMs,
                .checkQuota = m_checkQuota,
                .allowCrossFileSearch = m_reviewOptions.allowCrossFileSearch
            };
            DictionaryReviewAgent reviewAgent(m_controller, m_logger, m_apiPool, m_onPerformApi, std::move(reviewConfig));
            DictList reviewedList = reviewAgent.review(termGroups, m_reviewSourceFiles);
            finalList = m_onDictProcessed ? m_onDictProcessed(std::move(reviewedList)) : std::move(reviewedList);
            m_logger->info("阶段四：Review Agent 审校完成，使用审校后的字典结果。");
        }
        catch (const std::exception& e) {
            m_logger->error("阶段四：Review Agent 失败，将回退到粗候选整理结果。错误: {}", e.what());
            finalList = m_onDictProcessed ? m_onDictProcessed(m_finalDict) : std::move(coarseDefaultList);
        }
        m_controller->reduceThreadNum();
    }
    else {
        finalList = m_onDictProcessed ? m_onDictProcessed(m_finalDict) : std::move(coarseDefaultList);
    }


    toml::ordered_value arr = toml::array{};
    for (const auto& item : finalList) {
        arr.push_back(toml::ordered_table{ { "org", std::get<0>(item) }, { "rep", std::get<1>(item) }, { "note", std::get<2>(item) } });
    }

    arr.as_array_fmt().fmt = toml::array_format::multiline;
    createParent(outputFilePath);
    std::ofstream ofs(outputFilePath, std::ios::binary);
    ofs << toml::ordered_value{ toml::ordered_table{{ "gptDict", arr }} };
    ofs.close();

    m_logger->info("字典生成完成，共 {} 个词语，已保存到 {}", finalList.size(), wide2Ascii(outputFilePath));
}
