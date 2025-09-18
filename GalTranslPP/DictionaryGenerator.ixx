module;

#include <mecab/mecab.h>
#include <spdlog/spdlog.h>
#include <boost/regex.hpp>
#include <cpr/cpr.h>

export module DictionaryGenerator;

import <nlohmann/json.hpp>;
import <ctpl_stl.h>;
import <toml++/toml.hpp>;
import Tool;
import APIPool;
import Dictionary;
import ITranslator;

namespace fs = std::filesystem;
using json = nlohmann::json;

export {
    class DictionaryGenerator {
    private:
        APIPool& m_apiPool;
        std::shared_ptr<IController> m_controller;
        std::string m_systemPrompt;
        std::string m_userPrompt;
        int m_threadsNum;
        int m_apiTimeoutMs;
        std::shared_ptr<spdlog::logger> m_logger;
        std::string m_apiStrategy;
        int m_maxRetries;
        bool m_checkQuota;

        // 阶段一和二的结果
        std::vector<std::string> m_segments;
        std::vector<std::set<std::string>> m_segmentWords;
        std::map<std::string, int> m_wordCounter;
        std::set<std::string> m_nameSet;

        // 阶段四的结果 (线程安全)
        std::vector<std::tuple<std::string, std::string, std::string>> m_finalDict;
        std::map<std::string, int> m_finalCounter;
        std::mutex m_resultMutex;

        // MeCab 解析器
        std::unique_ptr<MeCab::Tagger> m_tagger;

        void preprocessAndTokenize(const std::vector<fs::path>& jsonFiles, NormalDictionary& preDict, bool usePreDictInName);
        std::vector<int> solveSentenceSelection();
        void callLLMToGenerate(int segmentIndex, int threadId);

    public:
        DictionaryGenerator(std::shared_ptr<IController> controller, std::shared_ptr<spdlog::logger> logger, APIPool& apiPool, const fs::path& dictDir,
            const std::string& systemPrompt, const std::string& userPrompt, const std::string& apiStrategy,
            int maxRetries, int threadsNum, int apiTimeoutMs, bool checkQuota);
        void generate(const fs::path& inputDir, const fs::path& outputFilePath, NormalDictionary& preDict, bool usePreDictInName);
    };
}


module :private;

DictionaryGenerator::DictionaryGenerator(std::shared_ptr<IController> controller, std::shared_ptr<spdlog::logger> logger, APIPool& apiPool,
    const fs::path& dictDir, const std::string& systemPrompt, const std::string& userPrompt, const std::string& apiStrategy,
    int maxRetries, int threadsNum, int apiTimeoutMs, bool checkQuota)
    : m_controller(controller), m_logger(logger), m_apiPool(apiPool), m_systemPrompt(systemPrompt), m_userPrompt(userPrompt),
    m_apiStrategy(apiStrategy), m_maxRetries(maxRetries), m_checkQuota(checkQuota),
    m_threadsNum(threadsNum), m_apiTimeoutMs(apiTimeoutMs) 
{
    m_tagger.reset(
        MeCab::createTagger(("-r BaseConfig/DictGenerator/mecabrc -d " + wide2Ascii(dictDir, 0)).c_str())
    );
    if (!m_tagger) {
        throw std::runtime_error("无法初始化 MeCab Tagger。请确保 BaseConfig/DictGenerator/mecabrc 和 " + wide2Ascii(dictDir) + " 存在且无特殊字符");
    }
}

void DictionaryGenerator::preprocessAndTokenize(const std::vector<fs::path>& jsonFiles, NormalDictionary& preDict, bool usePreDictInName) {
    m_logger->info("阶段一：预处理和分词...");
    std::string currentSegment;
    const size_t MAX_SEGMENT_LEN = 512;

    Sentence se;
    for (const auto& filePath : jsonFiles) {
        std::ifstream f(filePath);
        json data = json::parse(f);
        for (const auto& item : data) {
            se.name = item.value("name", "");
            se.original_text = item.value("message", "");
            if (usePreDictInName) {
                se.name = preDict.doReplace(&se, CachePart::Name);
            }
            se.original_text = preDict.doReplace(&se, CachePart::OrigText);
            if (!se.name.empty()) {
                m_nameSet.insert(se.name);
                m_wordCounter[se.name] += 2;
            }
            currentSegment += se.name + se.original_text + "\n";

            if (currentSegment.length() > MAX_SEGMENT_LEN) {
                m_segments.push_back(currentSegment);
                currentSegment.clear();
            }
        }
    }
    if (!currentSegment.empty()) {
        m_segments.push_back(currentSegment);
    }

    m_logger->info("共分割成 {} 个文本块，开始使用 MeCab 分词...", m_segments.size());
    m_segmentWords.reserve(m_segments.size());
    for (const auto& segment : m_segments) {
        std::set<std::string> wordsInSegment;
        const MeCab::Node* node = m_tagger->parseToNode(segment.c_str());
        for (; node; node = node->next) {
            if (node->stat == MECAB_BOS_NODE || node->stat == MECAB_EOS_NODE) continue;

            std::string surface(node->surface, node->length);
            std::string feature = node->feature;

            m_logger->trace("分词结果：{} ({})", surface, feature);

            if (surface.length() <= 1) continue;

            if (feature.find("固有名詞") != std::string::npos || containsKatakana(surface)) {
                wordsInSegment.insert(surface);
                m_wordCounter[surface]++;
            }
        }
        m_segmentWords.push_back(wordsInSegment);
    }
}

std::vector<int> DictionaryGenerator::solveSentenceSelection() {
    m_logger->info("阶段二：搜索并选择信息量最大的文本块...");

    // 剔除出现次数小于2的词语，人名除外
    std::set<std::string> allWords;
    for (const auto& [word, count] : m_wordCounter) {
        if (count >= 2 || m_nameSet.count(word)) {
            allWords.insert(word);
        }
    }

    // 过滤每个 segment 中的词，只保留在 allWords 中的
    std::vector<std::set<std::string>> filteredSegmentWords;
    for (const auto& segment : m_segmentWords) {
        std::set<std::string> filteredSet;
        std::set_intersection(segment.begin(), segment.end(),
            allWords.begin(), allWords.end(),
            std::inserter(filteredSet, filteredSet.begin()));
        filteredSegmentWords.push_back(filteredSet);
    }

    std::set<std::string> coveredWords;
    std::vector<int> selectedIndices;
    std::vector<int> remainingIndices(filteredSegmentWords.size());
    std::iota(remainingIndices.begin(), remainingIndices.end(), 0);

    while (coveredWords.size() < allWords.size() && !remainingIndices.empty()) {
        int bestIndex = -1;
        size_t maxNewCoverage = 0;

        for (int index : remainingIndices) {
            std::set<std::string> tempSet;
            std::set_difference(
                filteredSegmentWords[index].begin(), filteredSegmentWords[index].end(),
                coveredWords.begin(), coveredWords.end(),
                std::inserter(tempSet, tempSet.begin())
            );
            size_t newCoverage = tempSet.size();

            if (newCoverage > maxNewCoverage) {
                maxNewCoverage = newCoverage;
                bestIndex = index;
            }
        }

        if (bestIndex != -1) {
            coveredWords.insert(filteredSegmentWords[bestIndex].begin(), filteredSegmentWords[bestIndex].end());
            selectedIndices.push_back(bestIndex);
            remainingIndices.erase(std::remove(remainingIndices.begin(), remainingIndices.end(), bestIndex), remainingIndices.end());
        }
        else {
            break;
        }
    }
    return selectedIndices;
}

void DictionaryGenerator::callLLMToGenerate(int segmentIndex, int threadId) {
    if (m_controller->shouldStop()) {
        return;
    }
    m_controller->addThreadNum();
    std::string text = m_segments[segmentIndex];
    std::string hint = "无";
    std::string nameHit;
    for (const auto& name : m_nameSet) {
        if (text.find(name) != std::string::npos) {
            nameHit += name + "\n";
        }
    }
    if (!nameHit.empty()) {
        hint = "输入文本中的这些词语是一定要加入术语表的: \n" + nameHit;
    }

    std::string prompt = m_userPrompt;
    prompt = boost::regex_replace(prompt, boost::regex(R"(\{input\})"), text);
    prompt = boost::regex_replace(prompt, boost::regex(R"(\{hint\})"), hint);

    json messages = json::array({
        {{"role", "system"}, {"content", m_systemPrompt}},
        {{"role", "user"}, {"content", prompt}}
        });

    int retryCount = 0;
    while (retryCount < m_maxRetries) {
        if (m_controller->shouldStop()) {
            return;
        }
        auto optAPI = m_apiStrategy == "random" ? m_apiPool.getAPI() : m_apiPool.getFirstAPI();
        if (!optAPI) {
            throw std::runtime_error("没有可用的API Key了");
        }
        TranslationAPI currentAPI = optAPI.value();
        json payload = { {"model", currentAPI.modelName}, {"messages", messages}, /*{"temperature", 0.6}*/ };

        m_logger->info("[线程 {}] 开始从段落中生成术语表\ninputBlock: \n{}", threadId, text);
        ApiResponse response = performApiRequest(payload, currentAPI, threadId, m_controller, m_logger, m_apiTimeoutMs);

        if (response.success) {
            m_logger->info("[线程 {}] AI 字典生成成功:\n {}", threadId, response.content);
            auto lines = splitString(response.content, '\n');
            for (const auto& line : lines) {
                auto parts = splitString(line, '\t');
                if (parts.size() < 3 || parts[0].starts_with("日文原词") || parts[0].starts_with("NULL")) continue;

                std::lock_guard<std::mutex> lock(m_resultMutex);
                m_finalCounter[parts[0]]++;
                if (m_finalCounter[parts[0]] == 2) {
                    m_logger->trace("发现重复术语: {}\t{}\t{}", parts[0], parts[1], parts[2]);
                }
                m_finalDict.emplace_back(parts[0], parts[1], parts[2]);
            }
            break;
        }
        else {

            std::string lowerErrorMsg = response.content;
            std::transform(lowerErrorMsg.begin(), lowerErrorMsg.end(), lowerErrorMsg.begin(), ::tolower);

            // 情况一：额度用尽 (Quota)
            if (
                m_checkQuota &&
                (lowerErrorMsg.find("quota") != std::string::npos ||
                    lowerErrorMsg.find("invalid tokens") != std::string::npos)
                )
            {
                m_logger->error("[线程 {}] API Key [{}] 疑似额度用尽，短期内多次报告将从池中移除。", threadId, currentAPI.apikey);
                m_apiPool.reportProblem(currentAPI);
                // 不需要增加 retryCount
                continue;
            }
            // key 没有这个模型
            else if (lowerErrorMsg.find("no available") != std::string::npos) {
                m_logger->error("[线程 {}] API Key [{}] 没有 [{}] 模型，短期内多次报告将从池中移除。", threadId, currentAPI.apikey, currentAPI.modelName);
                m_apiPool.reportProblem(currentAPI);
                continue;
            }

            // 情况二：频率限制 (429) 或其他可重试错误
            // 状态码 429 是最明确的信号
            if (response.statusCode == 429 || lowerErrorMsg.find("rate limit") != std::string::npos || lowerErrorMsg.find("try again") != std::string::npos) {
                retryCount++;
                m_logger->warn("[线程 {}] 遇到频率限制或可重试错误，进行第 {} 次退避等待...", threadId, retryCount);

                // 实现指数退避与抖动
                int sleepSeconds;
                int maxSleepSeconds = static_cast<int>(std::pow(2, std::min(retryCount, 6)));
                if (maxSleepSeconds > 0) {
                    sleepSeconds = std::rand() % maxSleepSeconds;
                }
                else {
                    sleepSeconds = 0;
                }
                m_logger->debug("将等待 {} 秒后重试...", sleepSeconds);
                if (sleepSeconds > 0) {
                    std::this_thread::sleep_for(std::chrono::seconds(sleepSeconds));
                }
                continue;
            }

            // 其他无法识别的硬性错误
            retryCount++;
            m_logger->warn("[线程 {}] 遇到未知API错误，进行第 {} 次重试...", threadId, retryCount);
            if (m_apiStrategy == "fallback") {
                m_logger->warn("[线程 {}] 将切换到下一个 API Key(如果有多个API Key的话)", threadId);
                m_apiPool.resortTokens();
            }
            std::this_thread::sleep_for(std::chrono::seconds(2)); // 简单等待
            continue;
        }
    }
    if (retryCount >= m_maxRetries) {
        m_logger->error("[线程 {}] AI 字典生成失败，已达到最大重试次数。", threadId);
    }

    m_controller->updateBar();
    m_controller->reduceThreadNum();
}

void DictionaryGenerator::generate(const fs::path& inputDir, const fs::path& outputFilePath, NormalDictionary& preDict, bool usePreDictInName) {
    std::vector<fs::path> jsonFiles;
    for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
        if (entry.is_regular_file() && isSameExtension(entry.path(), L".json")) {
            jsonFiles.push_back(entry.path());
        }
    }
    if (jsonFiles.empty()) {
        m_logger->warn("输入目录 {} 为空，无法生成字典。", inputDir.string());
        return;
    }

    preprocessAndTokenize(jsonFiles, preDict, usePreDictInName);

    auto selectedIndices = solveSentenceSelection();
    if (selectedIndices.size() > 128) {
        selectedIndices.resize(128);
    }

    int threadsNum = std::min(m_threadsNum, (int)selectedIndices.size());
    m_logger->info("阶段三：启动 {} 个线程，向 AI 发送 {} 个任务...", threadsNum, selectedIndices.size());
    m_controller->makeBar((int)selectedIndices.size(), threadsNum);
    ctpl::thread_pool pool(threadsNum);
    std::vector<std::future<void>> results;
    for (int segmentIdx : selectedIndices) {
        results.emplace_back(pool.push([=](int threadId) 
            {
                this->callLLMToGenerate(segmentIdx, threadId);
            }));
    }
    for (auto& res : results) {
        res.get();
    }

    m_logger->info("阶段四：整理并保存结果...");
    std::vector<std::tuple<std::string, std::string, std::string>> finalList;

    // 按出现次数排序
    std::sort(m_finalDict.begin(), m_finalDict.end(), [&](const auto& a, const auto& b) {
        return m_finalCounter[std::get<0>(a)] > m_finalCounter[std::get<0>(b)];
        });

    // 过滤
    for (const auto& item : m_finalDict) {
        const auto& src = std::get<0>(item);
        const auto& note = std::get<2>(item);
        if (m_finalCounter[src] > 1 || note.find("人名") != std::string::npos || note.find("地名") != std::string::npos || m_wordCounter.count(src) || m_nameSet.count(src)) {
            finalList.push_back(item);
        }
    }

    // 去重
    std::set<std::string> seen;
    finalList.erase(std::remove_if(finalList.begin(), finalList.end(), [&](const auto& item) {
        if (seen.count(std::get<0>(item))) {
            return true;
        }
        seen.insert(std::get<0>(item));
        return false;
        }), finalList.end());


    std::ofstream ofs(outputFilePath);
    ofs << "gptDict = [" << std::endl;
    
    for (const auto& item : finalList) {
        auto tbl = toml::table{ { "org", std::get<0>(item) }, { "rep", std::get<1>(item) }, { "note", std::get<2>(item) } };
        ofs << std::format("    {{ org = {}, rep = {}, note = {} }},",
            stream2String(tbl["org"]), stream2String(tbl["rep"]), stream2String(tbl["note"])) << std::endl;
    }
    
    ofs << "]" << std::endl;
    m_logger->info("字典生成完成，共 {} 个词语，已保存到 {}", finalList.size(), wide2Ascii(outputFilePath));
}
