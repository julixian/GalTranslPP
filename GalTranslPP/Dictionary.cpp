module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"
#include <toml.hpp>
#include <sol/sol.hpp>

module Dictionary;

import Tool;
import ConditionTool;

namespace fs = std::filesystem;

// GPT
GptDictionary::GptDictionary(const fs::path& projectDir, const fs::path& otherCacheDir, const std::function<NLPResult(const std::string&)>& tokenizeSourceLangFunc,
    const std::unique_ptr<LuaManager>& luaManager, const std::unique_ptr<PythonManager>& pythonManager, const std::shared_ptr<spdlog::logger>& logger)
    : m_projectDir(projectDir), m_tokenizeCachePath(otherCacheDir / L"tokenizeCache_gptdict.json"),
    m_tokenizeSourceLangFunc(tokenizeSourceLangFunc),
    m_luaManager(luaManager), m_pythonManager(pythonManager), m_logger(logger)
{
	loadTokenizeCache(m_tokenizeCacheMap, m_tokenizeCachePath, m_logger);
}

GptDictionary::~GptDictionary() {
    saveTokenizeCache(m_tokenizeCacheMap, m_tokenizeCachePath, m_logger);
}

void GptDictionary::sort() {
    std::ranges::sort(m_entries, [](const GptDictEntry& a, const GptDictEntry& b)
        {
            if (a.priority != b.priority) {
                return a.priority > b.priority;
            }
            return a.searchStr.length() > b.searchStr.length();
        });
    for (auto& entry : m_entries) {
        for (const auto& [index, otherEntry] : m_entries | std::views::enumerate) {
            if (otherEntry.searchStr.length() > entry.searchStr.length() &&
                otherEntry.searchStr.contains(entry.searchStr))
            {
                if (!entry.otherEntriesWhoseSearchStrContainsThatInThisEntry) {
                    entry.otherEntriesWhoseSearchStrContainsThatInThisEntry = std::make_unique<std::vector<size_t>>();
                }
                entry.otherEntriesWhoseSearchStrContainsThatInThisEntry->push_back(index);
            }
        }
    }
}

std::string GptDictionary::generatePrompt(std::span<Sentence*> batch, TransEngine transEngine) const {

	std::string batchText = batch | std::views::transform([](const auto& se) { return se->name + ":::::" + se->pre_processed_text; })
        | std::views::join_with('\n') | std::ranges::to<std::string>();
    replaceStrInplace(batchText, "<tab>", "");
    replaceStrInplace(batchText, "<br>", "");

    std::string promptContent;
    for (const auto& entry : m_entries) {
        if (batchText.contains(entry.searchStr)) {
            // *** 根据 transEngine 选择格式 ***
            switch (transEngine)
            {
            case TransEngine::ForGalJson:
            case TransEngine::DeepseekJson:
                promptContent += "| " + entry.searchStr + " | " + entry.replaceStr + " |";
                if (!entry.note.empty()) {
                    promptContent += " " + entry.note;
                }
                promptContent += " |\n";
                break;

            case TransEngine::ForGalTsv:
            case TransEngine::ForNovelTsv:
            case TransEngine::NameTrans:
                promptContent += entry.searchStr + "\t" + entry.replaceStr;
                if (!entry.note.empty()) {
                    promptContent += "\t" + entry.note;
                }
                promptContent += "\n";
                break;

            case TransEngine::Sakura:
                promptContent += entry.searchStr + "->" + entry.replaceStr;
                if (!entry.note.empty()) {
                    promptContent += " #" + entry.note;
                }
                promptContent += "\n";
                break;

            default:
                throw std::runtime_error(gppTr("GptDictionary.getPrompt", "无效的提示词类型"));
            }
        }
    }

    if (promptContent.empty()) {
        return {};
    }

    // *** 根据 transEngine 添加标题 ***
    switch (transEngine)
    {
    case TransEngine::ForGalJson:
    case TransEngine::DeepseekJson:
        return "# Glossary\n| Src | Dst(/Dst2/..) | Note |\n| --- | --- | --- |\n" + promptContent;

    case TransEngine::ForGalTsv:
    case TransEngine::ForNovelTsv:
    case TransEngine::NameTrans:
        return "SRC\tDST\tNOTE\n" + promptContent;

    case TransEngine::Sakura:
        return promptContent; // Sakura 模式没有标题

    default:
        throw std::runtime_error(gppTr("GptDictionary.getPrompt", "无效的提示词类型"));
    }

    return {};
}

void GptDictionary::loadFromFile(const fs::path& filePath) {
    if (!fs::exists(filePath)) {
        m_logger->error(gppTr("GptDictionary.loadFromFile", "GPT 字典文件不存在: %1", wide2Ascii(filePath)));
        return;
    }

    int count = 0;

    try {
        const auto dictData = toml::uparse(filePath);
        if (!dictData.contains("gptDict")) {
            return;
        }
        const auto& dictTbls = dictData.at("gptDict").as_array();
        for (const auto& el : dictTbls) {
            GptDictEntry entry;
            if (!el.contains("org") && !el.contains("searchStr")) {
                continue;
            }
            if (!el.contains("rep") && !el.contains("replaceStr")) {
                continue;
            }

            entry.searchStr = el.contains("org") ? el.at("org").as_string() : el.at("searchStr").as_string();
            if (entry.searchStr.empty()) {
                continue;
            }
            entry.replaceStr = el.contains("rep") ? el.at("rep").as_string() : el.at("replaceStr").as_string();
            entry.note = toml::find_or(el, "note", "");
            entry.priority = toml::find_or(el, "priority", 0);
            m_entries.push_back(std::move(entry));
            ++count;
        }
    }
    catch (const toml::exception& e) {
        throw std::runtime_error(gppTr("GptDictionary.loadFromFile", "GPT 字典文件解析错误: %1: %2", wide2Ascii(filePath), e.what()));
    }

    m_logger->info(gppTr("GptDictionary.loadFromFile", "已加载 GPT 字典: %1, 共 %2 个词条", wide2Ascii(filePath.filename()), count));
}

std::string GptDictionary::doReplace(const Sentence* se, CachePart targetToModify) const {
    std::string textToModify = chooseString(se, targetToModify);
    for (const auto& entry : m_entries) {
        if (textToModify.contains(entry.searchStr)) {
            replaceStrInplace(textToModify, entry.searchStr, entry.replaceStr);
        }
    }
    return textToModify;
}

uint8_t checkTransIncludeReplace(const std::string& trans, const std::string& replace) {
    return std::ranges::any_of(replace | std::views::split('/') | std::views::transform([](const auto& subStrView)
        {
            return std::string_view(subStrView.begin(), subStrView.end());
        }),
        [&](const std::string_view subStr)
        {
            return trans.contains(subStr);
        }) ? 1 : 2;
}

void GptDictionary::checkDictUse(Sentence* sentence, CachePart base, CachePart check) {
    const std::string& origText = chooseStringRef(sentence, base);
    const std::string& transView = chooseStringRef(sentence, check);

    std::vector<uint8_t> checkResults(m_entries.size(), 0); // 0: 原文不包含, 1: 原文包含且译文使用字典, 2: 原文包含但译文没有使用字典
    for (auto [checkResult, entry] : std::views::zip(checkResults, m_entries | std::views::as_const)) {
        if (!origText.contains(entry.searchStr)) {
            continue;
        }
        checkResult = checkTransIncludeReplace(transView, entry.replaceStr);
    }

    for (auto [checkResult, entry] : std::views::zip(checkResults, m_entries | std::views::as_const)) {
        // 如果原文中不包含这个词或译文中使用了对应的词，就跳过检查
        if (checkResult != 2) {
            continue;
        }

        if (entry.otherEntriesWhoseSearchStrContainsThatInThisEntry) {
            const auto it = std::ranges::find_if(*entry.otherEntriesWhoseSearchStrContainsThatInThisEntry, [&](const size_t otherEntryIndex)
                {
                    return checkResults[otherEntryIndex] == 1;
                });
            if (it != entry.otherEntriesWhoseSearchStrContainsThatInThisEntry->end()) {
                const GptDictEntry& otherEntryRef = m_entries[*it];
                sentence->problems.push_back(gppTr("GptDictionary.checkDictUse", "GPT字典 %1->%2 未使用，但使用了 %3->%4 这一包含性字典",
                    entry.searchStr, entry.replaceStr, otherEntryRef.searchStr, otherEntryRef.replaceStr));
                continue;
            }
        }
        if (entry.searchStr.length() > 15) {
            // 如果字典单独出现且长度大于 15 字节，则默认认为是字典未正确使用的情况
            sentence->problems.push_back(gppTr("GptDictionary.checkDictUse", "GPT字典 %1->%2 未使用", entry.searchStr, entry.replaceStr));
            continue;
        }

        // 未出现则分词检查原文中是否有完整的 searchStr 词组
        bool found = false;
        auto checkTokenFunc = [&](const WordPosVec& wordPosVec)
            {
                for (const auto& wordPos : wordPosVec) {
                    if (wordPos[0] == entry.searchStr) {
                        found = true;
                        break;
                    }
                }
            };

        const bool foundInCache = [&]()
	        {
                std::shared_lock<std::shared_mutex> lock(m_tokenizeCacheMapMutex);
                if (const auto it = m_tokenizeCacheMap.find(origText); it != m_tokenizeCacheMap.end()) {
                    checkTokenFunc(it->second);
                    return true;
                }
                return false;
            }();
        if (!foundInCache) {
            NLPResult tokens = m_tokenizeSourceLangFunc(origText);
            WordPosVec& wordPosVec = std::get<0>(tokens);
            checkTokenFunc(wordPosVec);
            std::lock_guard<std::shared_mutex> lock(m_tokenizeCacheMapMutex);
            m_tokenizeCacheMap.insert({ origText, std::move(wordPosVec) });
        }

        if (found) {
            // 如果原文有完整的 searchStr 词组且译文中没有使用对应的词，几乎可以肯定是字典未正确使用的情况
            sentence->problems.push_back(gppTr("GptDictionary.checkDictUse", "GPT字典 %1->%2 未使用", entry.searchStr, entry.replaceStr));
        }

    }
}



// Normal
void NormalDictionary::loadFromFile(const fs::path& filePath) {
    if (!fs::exists(filePath)) {
        m_logger->warn(gppTr("NormalDictionary.loadFromFile", "字典文件不存在: %1", wide2Ascii(filePath)));
        return;
    }

    int count = 0;
    try {
        const auto dictData = toml::uparse(filePath);
        if (!dictData.contains("normalDict")) {
            return;
        }
        const auto& dicts = dictData.at("normalDict").as_array();
        for (const auto& el : dicts) {
            NormalDictEntry entry;
            if (!el.contains("org") && !el.contains("searchStr")) {
                continue;
            }
            if (!el.contains("rep") && !el.contains("replaceStr")) {
                continue;
            }
            entry.isReg = toml::find_or(el, "isReg", false);

            const std::string str = el.contains("org") ? el.at("org").as_string() : el.at("searchStr").as_string();
            if (str.empty()) {
                continue;
            }

            if (entry.isReg) {
                const std::string compileModifier = toml::find_or(el, "compile_modifier", defaultRegCompileModifier);
                entry.searchReg = std::make_unique<jpc::Regex>(str, compileModifier);
                if (!*entry.searchReg) {
                    throw std::runtime_error(gppTr("NormalDictionary.loadFromFile", "Normal 字典文件格式错误(正则表达式错误): %1  ——  %2", wide2Ascii(filePath), str));
                }
                entry.replaceModifier = std::make_unique<std::string>(toml::find_or(el, "replace_modifier", defaultRegReplaceModifier));
            }
            else {
                entry.searchStr = str;
            }

            entry.replaceStr = el.contains("rep") ? el.at("rep").as_string() : el.at("replaceStr").as_string();
            entry.priority = toml::find_or(el, "priority", 0);

            if (getConditionType(el) != ConditionType::None) {
                entry.dictCondition = std::make_unique<CheckSeCondFunc>(getCheckSeCondFunc(el, m_projectDir, m_pythonManager, m_luaManager, m_logger));
            }
            m_entries.push_back(std::move(entry));
            ++count;
        }
    }
    catch (const toml::exception& e) {
        throw std::runtime_error(gppTr("NormalDictionary.loadFromFile", "Normal 字典文件解析错误: %1: %2", wide2Ascii(filePath), e.what()));
    }

    m_logger->info(gppTr("NormalDictionary.loadFromFile", "已加载 Normal 字典: %1, 共 %2 个词条", wide2Ascii(filePath.filename()), count));
}

void NormalDictionary::sort() {
    std::ranges::sort(m_entries, [](const NormalDictEntry& a, const NormalDictEntry& b)
        {
            if (a.priority != b.priority) {
                return a.priority > b.priority;
            }
            if (a.isReg && !b.isReg) {
                return true;
            }
            else if (!a.isReg && b.isReg) {
                return false;
            }
            return a.searchStr.length() > b.searchStr.length();
        });
}

std::string NormalDictionary::doReplace(const Sentence* sentence, CachePart targetToModify) {
    std::string textToModify = chooseString(sentence, targetToModify);

    for (const NormalDictEntry& entry : m_entries
        | std::views::filter([&](const NormalDictEntry& e)
            {
                return !e.dictCondition.operator bool() || e.dictCondition->operator ()(sentence);
            }))
    {
        if (entry.isReg) {
            // 避免竞态
            jpc::RegexReplace rr(entry.searchReg.get());
            textToModify = rr.setModifier(*entry.replaceModifier).setSubject(&textToModify).setReplaceWith(&entry.replaceStr).replace();
        }
        else {
            replaceStrInplace(textToModify, entry.searchStr, entry.replaceStr);
        }
    }

    return textToModify;
}
