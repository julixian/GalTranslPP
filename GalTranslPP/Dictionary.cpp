module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"
#include <toml.hpp>

module Dictionary;

import Tool;
import ConditionTool;

namespace fs = std::filesystem;

// GPT
GptDictionary::GptDictionary(const fs::path& projectDir, const fs::path& otherCacheDir, const NLPTokenizeFunc& tokenizeSourceLangFunc,
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
            return a.org.length() > b.org.length();
        });
    for (auto& entry : m_entries) {
        for (const auto& [index, otherEntry] : m_entries | std::views::enumerate) {
            if (otherEntry.org.length() > entry.org.length() &&
                otherEntry.org.contains(entry.org))
            {
                if (!entry.supersetEntryIndices) {
                    entry.supersetEntryIndices = std::make_unique<std::vector<size_t>>();
                }
                entry.supersetEntryIndices->push_back(index);
            }
        }
    }
}

std::string GptDictionary::generatePrompt(std::span<Sentence*> batch, TransEngine transEngine) const {

	std::string batchText = batch | std::views::transform([](const auto& se) { return se->name + ":::::" + se->preproc; })
        | std::views::join_with('\n') | std::ranges::to<std::string>();
    replaceStrInplace(batchText, "<tab>", "");
    replaceStrInplace(batchText, "<br>", "");

    std::string promptContent;
    for (const auto& entry : m_entries) {
        if (batchText.contains(entry.org)) {
            // *** 根据 transEngine 选择格式 ***
            switch (transEngine)
            {
            case TransEngine::ForGalJson:
                promptContent += "| " + entry.org + " | " + entry.rep + " |";
                if (!entry.note.empty()) {
                    promptContent += " " + entry.note;
                }
                promptContent += " |\n";
                break;

            case TransEngine::ForGalTsv:
            case TransEngine::ForNovelTsv:
            case TransEngine::NameTrans:
                promptContent += entry.org + "\t" + entry.rep;
                if (!entry.note.empty()) {
                    promptContent += "\t" + entry.note;
                }
                promptContent += "\n";
                break;

            case TransEngine::Sakura:
                promptContent += entry.org + "->" + entry.rep;
                if (!entry.note.empty()) {
                    promptContent += " #" + entry.note;
                }
                promptContent += "\n";
                break;

            default:
                throw std::runtime_error(gppTr("GptDictionary.getPrompt", "内部错误: 无效的提示词类型")
                    .toStdString());
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
        return "| Src | Dst(||Dst2||..) | Note |\n| --- | --- | --- |\n" + promptContent;

    case TransEngine::ForGalTsv:
    case TransEngine::ForNovelTsv:
    case TransEngine::NameTrans:
        return "SRC\tDST(||Dst2||..)\tNOTE\n" + promptContent;

    case TransEngine::Sakura:
        return promptContent; // Sakura 模式没有标题

    default:
        throw std::runtime_error(gppTr("GptDictionary.getPrompt", "内部错误: 无效的提示词类型").toStdString());
    }

    return {};
}

void GptDictionary::loadFromFile(const fs::path& filePath) {
    if (!fs::exists(filePath)) {
        m_logger->error(gppTr("GptDictionary.loadFromFile", "GPT 字典文件 [%1] 不存在")
            .arg(wide2Ascii(filePath))
            .toStdString());
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
            if (!el.contains("org")) {
                continue;
            }
            if (!el.contains("rep")) {
                continue;
            }

            entry.org = el.at("org").as_string();
            if (entry.org.empty()) {
                continue;
            }
            entry.rep = el.at("rep").as_string();
            entry.note = toml::find_or(el, "note", "");
            entry.priority = toml::find_or(el, "priority", 0);
            m_entries.push_back(std::move(entry));
            ++count;
        }
    }
    catch (const toml::exception& e) {
        throw std::runtime_error(gppTr("GptDictionary.loadFromFile", "GPT 字典文件 [%1] 解析错误: %2")
            .arg(wide2Ascii(filePath))
            .arg(e.what())
            .toStdString());
    }

    m_logger->info(gppTr("GptDictionary.loadFromFile", "已加载 GPT 字典文件 [%1], 共 %2 个词条")
        .arg(wide2Ascii(filePath.filename()))
        .arg(count)
        .toStdString());
}

std::string GptDictionary::doReplace(Sentence* se, CachePart targetToModify) const {
    std::string textToModify = chooseString(se, targetToModify);
    for (const auto& entry : m_entries) {
        if (textToModify.contains(entry.org)) {
            replaceStrInplace(textToModify, entry.org, entry.rep);
        }
    }
    return textToModify;
}

uint8_t checkTransIncludeReplace(const std::string& trans, const std::string& replace) {
    return std::ranges::any_of(replace | std::views::split(std::string_view("||"))
        | std::views::transform([](const auto& subStrView)
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
        if (!origText.contains(entry.org)) {
            continue;
        }
        checkResult = checkTransIncludeReplace(transView, entry.rep);
    }

    for (auto [checkResult, entry] : std::views::zip(checkResults, m_entries | std::views::as_const)) {
        // 如果原文中不包含这个词或译文中使用了对应的词，就跳过检查
        if (checkResult != 2) {
            continue;
        }

        if (entry.supersetEntryIndices) {
            const auto it = std::ranges::find_if(*entry.supersetEntryIndices,
                [&](const size_t otherEntryIndex)
                {
                    return checkResults[otherEntryIndex] == 1;
                });
            if (it != entry.supersetEntryIndices->end()) {
                const GptDictEntry& otherEntryRef = m_entries[*it];
                sentence->problems.push_back(gppTr(
                    "GptDictionary.checkDictUse",
                    "GPT字典 `%1`->`%2` 未使用，但使用了 `%3`->`%4` 这一包含性字典")
                    .arg(entry.org)
                    .arg(entry.rep)
                    .arg(otherEntryRef.org)
                    .arg(otherEntryRef.rep)
                    .toStdString());
                continue;
            }
        }
        if (entry.org.length() > 15) {
            // 如果字典单独出现且长度大于 15 字节，则默认认为是字典未正确使用的情况
            sentence->problems.push_back(gppTr("GptDictionary.checkDictUse", "GPT字典 `%1`->`%2` 未使用")
                .arg(entry.org)
                .arg(entry.rep)
                .toStdString());
            continue;
        }

        // 未出现则分词检查原文中是否有完整的 org 词组
        bool found = false;
        auto checkTokenFunc = [&](const WordPosVec& wordPosVec)
            {
                for (const auto& wordPos : wordPosVec) {
                    if (wordPos[0] == entry.org) {
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
            // 如果原文有完整的 org 词组且译文中没有使用对应的词，几乎可以肯定是字典未正确使用的情况
            sentence->problems.push_back(gppTr("GptDictionary.checkDictUse", "GPT字典 `%1`->`%2` 未使用")
                .arg(entry.org)
                .arg(entry.rep)
                .toStdString());
        }

    }
}



// Normal
void NormalDictionary::loadFromFile(const fs::path& filePath) {
    if (!fs::exists(filePath)) {
        m_logger->warn(gppTr("NormalDictionary.loadFromFile", "字典文件 [%1] 不存在")
            .arg(wide2Ascii(filePath))
            .toStdString());
        return;
    }

    int count = 0;
    try {
        const auto dictData = toml::uparse(filePath);
        if (!dictData.contains("normalDict")) {
            return;
        }
        const auto& dicts = dictData.at("normalDict").as_array();
        for (const auto& elem : dicts) {
            NormalDictEntry entry;
            if (!elem.contains("org")) {
                continue;
            }
            if (!elem.contains("rep")) {
                continue;
            }

            const std::string str = elem.at("org").as_string();
            if (str.empty()) {
                continue;
            }

            entry.isReg = toml::find_or(elem, "isReg", false);
            if (entry.isReg) {
                const std::string compileModifier = toml::find_or(elem, "compileModifier", defaultRegCompileModifier);
                entry.searchReg = std::make_unique<jpc::Regex>(str, compileModifier);
                if (!*entry.searchReg) {
                    throw std::runtime_error(gppTr(
                        "NormalDictionary.loadFromFile",
                        "Normal 字典文件 [%1] 正则表达式 `%2` 编译失败")
                        .arg(wide2Ascii(filePath))
                        .arg(str)
                        .toStdString());
                }
                entry.replaceModifier = std::make_unique<std::string>(toml::find_or(elem, "replaceModifier", defaultRegReplaceModifier));
            }
            else {
                entry.org = str;
            }

            entry.rep = elem.at("rep").as_string();
            entry.priority = toml::find_or(elem, "priority", 0);

            if (elem.contains("conditions") && elem.at("conditions").is_array()) {
                const auto& conditions = elem.at("conditions");
                // 出于 常用性/GUI支持 等方面的考量
                // 条件字典 只支持 gppCondition 而不像 retranslKeys/skipProblems 那样可以外接脚本
                GPPCondition gppCondition = createGppCondition(conditions);
                if (!gppCondition.empty()) {
                    entry.dictCondition = std::make_unique<CheckSeCondNormalFunc>(
                        [gppConditionR = std::move(gppCondition)](Sentence* se)
                        {
                            return checkGppCondition(gppConditionR, se);
                        });
                }
            }
            m_entries.push_back(std::move(entry));
            ++count;
        }
    }
    catch (const toml::exception& e) {
        throw std::runtime_error(gppTr("NormalDictionary.loadFromFile", "Normal 字典文件 [%1] 解析错误: %2")
            .arg(wide2Ascii(filePath))
            .arg(e.what())
            .toStdString());
    }

    m_logger->info(gppTr("NormalDictionary.loadFromFile", "已加载 Normal 字典文件 [%1], 共 %2 个词条")
        .arg(wide2Ascii(filePath.filename()))
        .arg(count)
        .toStdString());
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
            return a.org.length() > b.org.length();
        });
}

std::string NormalDictionary::doReplace(Sentence* sentence, CachePart targetToModify) {
    std::string textToModify = chooseString(sentence, targetToModify);

    for (const NormalDictEntry& entry : m_entries
        | std::views::filter([&](const NormalDictEntry& entry_)
            {
                return !entry_.dictCondition.operator bool() || entry_.dictCondition->operator()(sentence);
            }))
    {
        if (entry.isReg) {
            // 避免竞态
            jpc::RegexReplace rr(entry.searchReg.get());
            textToModify = rr.setModifier(*entry.replaceModifier).setSubject(&textToModify).setReplaceWith(&entry.rep).replace();
        }
        else {
            replaceStrInplace(textToModify, entry.org, entry.rep);
        }
    }

    return textToModify;
}
