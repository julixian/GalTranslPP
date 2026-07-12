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

ProblemAnalyzer::ProblemAnalyzer(
    const std::unique_ptr<GptDictionary>& gptDictionary,
    const std::string& targetLang,
    const std::string& punctSet,
    const std::string& codePage,
    double langProbability,
    const std::shared_ptr<spdlog::logger>& logger
)
    : m_gptDictionary(gptDictionary),
    m_punctsToCheck(splitIntoGraphemes(punctSet)),
    m_probabilityThreshold(langProbability),
    m_codePage(codePage),
    m_targetLang(targetLang),
    m_logger(logger)
{

}

void ProblemAnalyzer::analyze(Sentence* sentence) {
    if (sentence->transview.empty()) {
        if (!sentence->preproc.empty()) {
            sentence->problems.push_back(gppTr("ProblemAnalyzer.analyze", "翻译为空").toStdString());
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
                sentence->problems.push_back(gppTr("ProblemAnalyzer.analyze", "词频过高-'%1'%2次")
                    .arg(mostWord)
                    .arg(wordCount)
                    .toStdString());
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

            if (orgHas && !transHas) {
                sentence->problems.push_back(gppTr(
                    "ProblemAnalyzer.analyze",
                    "本有 %1 符号")
                    .arg(punctToCheck)
                    .toStdString());
            }
            if (!orgHas && transHas) {
                sentence->problems.push_back(gppTr(
                    "ProblemAnalyzer.analyze",
                    "本无 %1 符号")
                    .arg(punctToCheck)
                    .toStdString());
            }
        }
    }

    // 残留日文
    if (m_problems.remainJp.use) {
        const std::string& transView = chooseStringRef(sentence, m_problems.remainJp.check);
        if (std::string kanas = extractKana(transView); !kanas.empty()) {
            sentence->problems.push_back(gppTr("ProblemAnalyzer.analyze", "残留日文: %1")
                .arg(kanas)
                .toStdString());
        }
    }

    // 引入拉丁字母
    if (m_problems.introLatin.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.introLatin.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.introLatin.check);
        if (std::string latins = extractLatin(transView); !latins.empty() && !hasLatin(origText)) {
            sentence->problems.push_back(gppTr("ProblemAnalyzer.analyze", "引入拉丁字母: %1")
                .arg(latins)
                .toStdString());
        }
    }

    // 引入韩文
    if (m_problems.introHangul.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.introHangul.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.introHangul.check);
        if (std::string hanguls = extractHangul(transView); !hanguls.empty() && !hasHangul(origText)) {
            sentence->problems.push_back(gppTr("ProblemAnalyzer.analyze", "引入韩文: %1")
                .arg(hanguls)
                .toStdString());
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
            for (const std::string_view origGrapheme : splitIntoGraphemeViews(transView)
                | std::views::filter([&](std::string_view g) { return !excludeTraditionalCharList.contains(g); }))
            {
                // 经过初筛之后的实际检查还是分单个文字进行的，我测下来这样效果会好一点，大概是因为 opencc/icu 这些库本来搞这个的目的
                // 是真的用来繁简转换而不是用来测有没有『繁体字』的，让 opencc 联系上下文反而会出现一些误报
                // 这实际上是放宽了对繁体字的检测，有待观察吧
                // 但加了不少速是真的（）
                if (const std::string simplifiedGrapheme = simpleConverter->Convert(origGrapheme.data(), origGrapheme.size()); simplifiedGrapheme != origGrapheme)
                {
                    traditionalGraphemes += std::format("({} -> {})", origGrapheme, simplifiedGrapheme);
                }
            }
            if (!traditionalGraphemes.empty()) {
                sentence->problems.push_back(gppTr("ProblemAnalyzer.analyze", "引入繁体字: %1")
                    .arg(traditionalGraphemes)
                    .toStdString());
            }
        }
    }

    // 换行符不匹配
    if (m_problems.linebreakLost.use) {
        if (!sentence->linebreak.empty()) {
            const std::string& origText = chooseStringRef(sentence, m_problems.linebreakLost.base);
            const std::string& transView = chooseStringRef(sentence, m_problems.linebreakLost.check);
            int origLinebreaks = countSubstring(origText, m_problems.linebreakAdded.base == CachePart::Orig ? sentence->linebreak : "<br>");
            int transLinebreaks = countSubstring(transView, m_problems.linebreakAdded.check == CachePart::Transview ? sentence->linebreak : "<br>");
            if (origLinebreaks > transLinebreaks) {
                sentence->problems.push_back(gppTr("ProblemAnalyzer.analyze", "丢失换行(%1/%2)")
                    .arg(transLinebreaks)
                    .arg(origLinebreaks)
                    .toStdString());
            }
        }
    }
    if (m_problems.linebreakAdded.use) {
        if (!sentence->linebreak.empty()) {
            const std::string& origText = chooseStringRef(sentence, m_problems.linebreakAdded.base);
            const std::string& transView = chooseStringRef(sentence, m_problems.linebreakAdded.check);
            int origLinebreaks = countSubstring(origText, m_problems.linebreakLost.base == CachePart::Orig ? sentence->linebreak : "<br>");
            int transLinebreaks = countSubstring(transView, m_problems.linebreakLost.check == CachePart::Transview ? sentence->linebreak : "<br>");
            if (origLinebreaks < transLinebreaks) {
                sentence->problems.push_back(gppTr("ProblemAnalyzer.analyze", "多加换行(%1/%2)")
                    .arg(transLinebreaks)
                    .arg(origLinebreaks)
                    .toStdString());
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
                gppTr("ProblemAnalyzer.analyze", "比原文严格长 %1 倍(%2/%3字符)")
                    .arg(transViewCharCount / (double)origTextCharCount, 0, 'f', 2)
                    .arg(transViewCharCount)
                    .arg(origTextCharCount)
                    .toStdString()
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
                gppTr("ProblemAnalyzer.analyze", "比原文长 %1 倍(%2/%3字符)")
                    .arg(transViewCharCount / (double)origTextCharCount, 0, 'f', 2)
                    .arg(transViewCharCount)
                    .arg(origTextCharCount)
                    .toStdString()
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
                    if (result.probability < m_probabilityThreshold) {
                        continue;
                    }
                    langSet.insert(result.language);
                }
            }
            if (transTextLen > 6) {
                auto results = langIdentifier->FindTopNMostFreqLangs(transTextToCheck, 3);
                if (results[0].language == chrome_lang_id::NNetLanguageIdentifier::kUnknown && !langSet.empty()) {
                    sentence->problems.push_back(gppTr("ProblemAnalyzer.analyze", "无法识别的语言")
                        .toStdString());
                }
                for (const auto& result : results) {
                    if (result.language == chrome_lang_id::NNetLanguageIdentifier::kUnknown) {
                        break;
                    }
                    if (result.probability < m_probabilityThreshold) {
                        continue;
                    }
                    if (result.language != simplifiedTargetLang && !langSet.contains(result.language)) {
                        sentence->problems.push_back(gppTr("ProblemAnalyzer.analyze", "引入(%1, %2)")
                            .arg(result.language)
                            .arg(result.probability, 0, 'f', 3)
                            .toStdString());
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
            sentence->problems.push_back(gppTr("ProblemAnalyzer.analyze", "非 %1 字符: %2")
                .arg(m_codePage)
                .arg(unmappedChars)
                .toStdString());
        }
    }

}

void ProblemAnalyzer::setProblemRule(const std::string& problemKey, bool enabled, const std::string& base, const std::string& check)
{
    ProblemCompareObj* obj = nullptr;
    if (problemKey == "HighFrequency") {
        obj = &m_problems.highFrequency;
    }
    else if (problemKey == "PunctuationMismatch") {
        obj = &m_problems.punctsMiss;
    }
    else if (problemKey == "JapaneseRemains") {
        obj = &m_problems.remainJp;
    }
    else if (problemKey == "LatinIntroduced") {
        obj = &m_problems.introLatin;
    }
    else if (problemKey == "HangulIntroduced") {
        obj = &m_problems.introHangul;
    }
    else if (problemKey == "TraditionalChineseIntroduced") {
        obj = &m_problems.introTraditionalChinese;
    }
    else if (problemKey == "LinebreakLost") {
        obj = &m_problems.linebreakLost;
    }
    else if (problemKey == "LinebreakAdded") {
        obj = &m_problems.linebreakAdded;
    }
    else if (problemKey == "LongerThanSource") {
        obj = &m_problems.longer;
    }
    else if (problemKey == "StrictlyLongerThanSource") {
        obj = &m_problems.strictlyLonger;
    }
    else if (problemKey == "DictionaryUnused") {
        obj = &m_problems.dictUnused;
    }
    else if (problemKey == "NotTargetLanguage") {
        obj = &m_problems.notTargetLang;
    }
    else if (problemKey == "InvalidCharacter") {
        obj = &m_problems.invalidChar;
    }
    else {
        throw std::invalid_argument(gppTr("ProblemAnalyzer.setProblemRule", "未知问题: %1")
            .arg(problemKey)
            .toStdString());
    }

    obj->use = enabled;
    obj->base = chooseCachePart(base);
    obj->check = chooseCachePart(check);
}
