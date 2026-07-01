module;

#include "GPPMacros.hpp"
#pragma  warning( push ) 
#pragma  warning( disable: 4244 )
#pragma  warning( disable: 4251 )
#pragma  warning( disable: 4267 )
#include <cld3/nnet_language_identifier.h>
#pragma  warning(  pop  ) 
#include <opencc/opencc.h>

module ProblemAnalyzer;

import CodePageChecker;
import Tool;

namespace fs = std::filesystem;

static thread_local std::unique_ptr<chrome_lang_id::NNetLanguageIdentifier> langIdentifier;
static thread_local std::unique_ptr<CodePageChecker> codePageChecker;
static thread_local std::unique_ptr<opencc::SimpleConverter> simpleConverter;
static absl::btree_set<std::string_view> excludeTraditionalCharList = { "乾", "阪", "篠", "塚" };


ProblemAnalyzer::~ProblemAnalyzer() {
    langIdentifier.reset();
    codePageChecker.reset();
    simpleConverter.reset();
}

ProblemAnalyzer::ProblemAnalyzer(const std::unique_ptr<GptDictionary>& gptDictionary, const std::string& targetLang, const std::shared_ptr<spdlog::logger>& logger)
    : m_gptDictionary(gptDictionary), m_targetLang(targetLang), m_logger(logger) 
{

}

void ProblemAnalyzer::analyze(Sentence* sentence) {
    if (sentence->translated_preview.empty()) {
        if (!sentence->pre_processed_text.empty()) {
            sentence->problems.push_back("翻译为空");
        }
        return;
    }

    // 词频过高
    if (m_problems.highFrequency.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.highFrequency.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.highFrequency.check);
        const auto [mostWord, wordCount] = getMostCommonChar(transView);
        if (wordCount > 20) {
            const auto [mostWordOrg, wordCountOrg] = getMostCommonChar(origText);
            if (wordCount > (wordCountOrg > 0 ? wordCountOrg * 2 : 20)) {
                sentence->problems.push_back(std::format("词频过高-'{}'{}次", mostWord, wordCount));
            }
        }
    }

    // 标点错漏
    if (m_problems.punctsMiss.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.punctsMiss.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.punctsMiss.check);
        for (const auto& punctToCheck : m_punctsToCheck) {
            bool orgHas = origText.contains(punctToCheck);
            bool transHas = transView.contains(punctToCheck);

            if (orgHas && !transHas) sentence->problems.push_back("本有 " + punctToCheck + " 符号");
            if (!orgHas && transHas) sentence->problems.push_back("本无 " + punctToCheck + " 符号");
        }
    }

    // 残留日文
    if (m_problems.remainJp.use) {
        const std::string& transView = chooseStringRef(sentence, m_problems.remainJp.check);
        if (std::string kanas = extractKana(transView); !kanas.empty()) {
            sentence->problems.push_back("残留日文: " + kanas);
        }
    }

    // 引入拉丁字母
    if (m_problems.introLatin.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.introLatin.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.introLatin.check);
        if (std::string latins = extractLatin(transView); !latins.empty() && extractLatin(origText).empty()) {
            sentence->problems.push_back("引入拉丁字母: " + latins);
        }
    }

    // 引入韩文
    if (m_problems.introHangul.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.introHangul.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.introHangul.check);
        if (std::string hanguls = extractHangul(transView); !hanguls.empty() && extractHangul(origText).empty()) {
            sentence->problems.push_back("引入韩文: " + hanguls);
        }
    }


    // 引入繁体字
    if (m_problems.introTraditionalChinese.use) {
        if (!simpleConverter) {
            simpleConverter = std::make_unique<opencc::SimpleConverter>("BaseConfig/opencc/t2s.json");
        }
        const std::string& transView = chooseStringRef(sentence, m_problems.introTraditionalChinese.check);
        if (const std::string simplifiedView = simpleConverter->Convert(transView); simplifiedView != transView) { // 这一步主要是为了初筛加速
            std::string traditionalGraphemes;
            for (const auto& origGrapheme : splitIntoGraphemes(transView)
                | std::views::filter([&](const std::string& g) { return !excludeTraditionalCharList.contains(g); }))
            {
                // 经过初筛之后的实际检查还是分单个文字进行的，我测下来这样效果会好一点，大概是因为 opencc/icu 这些库本来搞这个的目的
                // 是真的用来繁简转换而不是用来测有没有『繁体字』的，让 opencc 联系上下文反而会出现一些误报
                // 这实际上是放宽了对繁体字的检测，有待观察吧
                // 但加了不少速是真的（）
                if (const std::string simplifiedGrapheme = simpleConverter->Convert(origGrapheme); simplifiedGrapheme != origGrapheme)
                {
                    traditionalGraphemes += std::format("({} -> {})", origGrapheme, simplifiedGrapheme);
                }
            }
            if (!traditionalGraphemes.empty()) {
                sentence->problems.push_back("引入繁体字: " + traditionalGraphemes);
            }
        }
    }

    // 换行符不匹配
    if (m_problems.linebreakLost.use) {
        if (!sentence->originalLinebreak.empty()) {
            const std::string& origText = chooseStringRef(sentence, m_problems.linebreakLost.base);
            const std::string& transView = chooseStringRef(sentence, m_problems.linebreakLost.check);
            int origLinebreaks = countSubstring(origText, m_problems.linebreakLost.base == CachePart::OrigText ? sentence->originalLinebreak : "<br>");
            int transLinebreaks = countSubstring(transView, m_problems.linebreakLost.check == CachePart::TransPreview ? sentence->originalLinebreak : "<br>");
            if (origLinebreaks > transLinebreaks) {
                sentence->problems.push_back(std::format("丢失换行({}/{})", transLinebreaks, origLinebreaks));
            }
        }
    }
    if (m_problems.linebreakAdded.use) {
        if (!sentence->originalLinebreak.empty()) {
            const std::string& origText = chooseStringRef(sentence, m_problems.linebreakAdded.base);
            const std::string& transView = chooseStringRef(sentence, m_problems.linebreakAdded.check);
            int origLinebreaks = countSubstring(origText, m_problems.linebreakLost.base == CachePart::OrigText ? sentence->originalLinebreak : "<br>");
            int transLinebreaks = countSubstring(transView, m_problems.linebreakLost.check == CachePart::TransPreview ? sentence->originalLinebreak : "<br>");
            if (origLinebreaks < transLinebreaks) {
                sentence->problems.push_back(std::format("多加换行({}/{})", transLinebreaks, origLinebreaks));
            }
        }
    }

    // 译文长度异常
    if (m_problems.strictlyLonger.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.strictlyLonger.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.strictlyLonger.check);
        size_t origTextCharCount = countGraphemes(origText);
        size_t transViewCharCount = countGraphemes(transView);
        if (transViewCharCount > origTextCharCount && origTextCharCount != 0) {
            sentence->problems.push_back(
                std::format("比原文严格长 {:.2f} 倍({}/{}字符)", transViewCharCount / (double)origTextCharCount, transViewCharCount, origTextCharCount)
            );
        }
    }
    else if (m_problems.longer.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.longer.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.longer.check);
        size_t origTextCharCount = countGraphemes(origText);
        size_t transViewCharCount = countGraphemes(transView);
        if (transViewCharCount > (double)origTextCharCount * 1.3 && origTextCharCount != 0) {
            sentence->problems.push_back(
                std::format("比原文长 {:.2f} 倍({}/{}字符)", transViewCharCount / (double)origTextCharCount, transViewCharCount, origTextCharCount)
            );
        }
    }

    // 字典未使用
    if (m_problems.dictUnused.use) {
        m_gptDictionary->checkDictUse(sentence, m_problems.dictUnused.base, m_problems.dictUnused.check);
    }

    // 语言不通
    if (m_problems.notTargetLang.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.notTargetLang.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.notTargetLang.check);
        std::string_view simplifiedTargetLang;
        if (size_t pos = m_targetLang.find('-'); pos != std::string::npos) {
            simplifiedTargetLang = std::string_view(m_targetLang).substr(0, pos);
        }
        else {
            simplifiedTargetLang = m_targetLang;
        }
        std::string origTextToCheck = removePunctuation(origText);
        std::string transTextToCheck = removePunctuation(transView);

        size_t origTextLen = origTextToCheck.length();
        size_t transTextLen = transTextToCheck.length();

        if (origTextLen > 6 || transTextLen > 6) {
            std::set<std::string> langSet;
            if (!langIdentifier) {
                langIdentifier = std::make_unique<chrome_lang_id::NNetLanguageIdentifier>(3, 300);
            }
            if (origTextLen > 6) {
                auto results = langIdentifier->FindTopNMostFreqLangs(origTextToCheck, 3);
                for (const auto& result : results) {
                    if (result.language == chrome_lang_id::NNetLanguageIdentifier::kUnknown) {
                        break;
                    }
                    //m_logger->trace("CLD3: {} -> {} ({}, {}, {})", origText, result.language, result.is_reliable, result.probability, result.proportion);
                    if (result.probability < m_probabilityThreshold) {
                        continue;
                    }
                    langSet.insert(result.language);
                }
            }
            if (transTextLen > 6) {
                auto results = langIdentifier->FindTopNMostFreqLangs(transTextToCheck, 3);
                if (results[0].language == chrome_lang_id::NNetLanguageIdentifier::kUnknown && !langSet.empty()) {
                    sentence->problems.push_back("无法识别的语言");
                }
                for (const auto& result : results) {
                    if (result.language == chrome_lang_id::NNetLanguageIdentifier::kUnknown) {
                        break;
                    }
                    //m_logger->trace("CLD3: {} -> {} ({}, {}, {})", transView, result.language, result.is_reliable, result.probability, result.proportion);
                    if (result.probability < m_probabilityThreshold) {
                        continue;
                    }
                    if (result.language != simplifiedTargetLang && !langSet.contains(result.language)) {
                        sentence->problems.push_back(std::format("引入({}, {:.3f})", result.language, result.probability));
                    }
                }
            }
        }

    }

    // 非法字符
    if (m_problems.invalidChar.use) {
        if (!codePageChecker) {
            codePageChecker = std::make_unique<CodePageChecker>(m_codePage, m_logger);
        }
        const std::string& transView = chooseStringRef(sentence, m_problems.invalidChar.check);
        const std::string& unmappedChars = codePageChecker->findUnmappableChars(transView);
        if (!unmappedChars.empty()) {
            sentence->problems.push_back("非 " + m_codePage + " 字符: " + unmappedChars);
        }
    }

}

void ProblemAnalyzer::loadProblems(const std::vector<std::string>& problemList, const std::string& punctSet, const std::string& codePage, double langProbability)
{
    for (const auto& problem : problemList)
    {
        if (problem.empty()) {
            continue;
        }
        if (problem == "词频过高") {
            m_problems.highFrequency.use = true;
        }
        else if (problem == "标点错漏") {
            m_problems.punctsMiss.use = true;
            m_punctsToCheck = splitIntoGraphemes(punctSet);
        }
        else if (problem == "残留日文") {
            m_problems.remainJp.use = true;
        }
        else if (problem == "引入拉丁字母") {
            m_problems.introLatin.use = true;
        }
        else if (problem == "引入韩文") {
            m_problems.introHangul.use = true;
        }
        else if (problem == "引入繁体字") {
            m_problems.introTraditionalChinese.use = true;
        }
        else if (problem == "丢失换行") {
            m_problems.linebreakLost.use = true;
        }
        else if (problem == "多加换行") {
            m_problems.linebreakAdded.use = true;
        }
        else if (problem == "比原文长") {
            m_problems.longer.use = true;
        }
        else if (problem == "比原文长严格") {
            m_problems.strictlyLonger.use = true;
        }
        else if (problem == "字典未使用") {
            m_problems.dictUnused.use = true;
        }
        else if (problem == "语言不通") {
            m_problems.notTargetLang.use = true;
            m_probabilityThreshold = langProbability;
        }
        else if (problem == "非法字符") {
            m_problems.invalidChar.use = true;
            m_codePage = codePage;
        }
        else {
            throw std::invalid_argument(std::format("未知问题: {}", problem));
        }
    }
}

void ProblemAnalyzer::overwriteCompareObj(const std::string& problemKey, const std::string& base, const std::string& check)
{
    auto saveCachePart = [](ProblemCompareObj& obj, const std::string& base, const std::string& check)
        {
            obj.base = chooseCachePart(base);
            obj.check = chooseCachePart(check);
            if (obj.base == CachePart::None) {
                throw std::invalid_argument(std::format("未知缓存键: {}", base));
            }
            if (obj.check == CachePart::None) {
                throw std::invalid_argument(std::format("未知缓存键: {}", check));
            }
        };

    if (problemKey == "词频过高") {
        saveCachePart(m_problems.highFrequency, base, check);
    }
    else if (problemKey == "标点错漏") {
        saveCachePart(m_problems.punctsMiss, base, check);
    }
    else if (problemKey == "残留日文") {
        saveCachePart(m_problems.remainJp, base, check);
    }
    else if (problemKey == "引入拉丁字母") {
        saveCachePart(m_problems.introLatin, base, check);
    }
    else if (problemKey == "引入韩文") {
        saveCachePart(m_problems.introHangul, base, check);
    }
    else if (problemKey == "引入繁体字") {
        saveCachePart(m_problems.introTraditionalChinese, base, check);
    }
    else if (problemKey == "丢失换行") {
        saveCachePart(m_problems.linebreakLost, base, check);
    }
    else if (problemKey == "多加换行") {
        saveCachePart(m_problems.linebreakAdded, base, check);
    }
    else if (problemKey == "比原文长") {
        saveCachePart(m_problems.longer, base, check);
    }
    else if (problemKey == "比原文长严格") {
        saveCachePart(m_problems.strictlyLonger, base, check);
    }
    else if (problemKey == "字典未使用") {
        saveCachePart(m_problems.dictUnused, base, check);
    }
    else if (problemKey == "语言不通") {
        saveCachePart(m_problems.notTargetLang, base, check);
    }
    else if (problemKey == "非法字符") {
        saveCachePart(m_problems.invalidChar, base, check);
    }
    else {
        throw std::invalid_argument(std::format("不支持的问题比较对象覆写: {}", problemKey));
    }
}
