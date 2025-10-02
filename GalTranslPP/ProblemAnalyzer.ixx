module;

#include <spdlog/spdlog.h>
#include <cld3/nnet_language_identifier.h>
#include <cld3/language_identifier_features.h>

export module ProblemAnalyzer;

import Tool;
import Dictionary;

export {

    struct ProblemCompareObj {
        bool use = false;
        CachePart base = CachePart::OrigText;
        CachePart check = CachePart::TransPreview;
    };

	struct Problems {
        ProblemCompareObj highFrequency;
        ProblemCompareObj punctMiss;
        ProblemCompareObj remainJp;
        ProblemCompareObj introLatin;
        ProblemCompareObj introHangul;
        ProblemCompareObj introNonGbk;
        ProblemCompareObj introTraditionalChinese;
        ProblemCompareObj linebreakLost;
        ProblemCompareObj linebreakAdded;
        ProblemCompareObj longer;
        ProblemCompareObj strictlyLonger;
        ProblemCompareObj dictUnused;
        ProblemCompareObj notTargetLang;
	};

	class ProblemAnalyzer {

	private:

        std::vector<std::string> m_checks;
        std::shared_ptr<spdlog::logger> m_logger;
        double m_probabilityThreshold = 0.85;

		Problems m_problems;

	public:

        ProblemAnalyzer(std::shared_ptr<spdlog::logger> logger) : m_logger(logger) {}

		void loadProblems(const std::vector<std::string>& problemList, const std::string& punctSet, double langProbability);

        void overwriteCompareObj(const std::string& problemKey, const std::string& base, const std::string& check);

		void analyze(Sentence* sentence, GptDictionary& gptDict, const std::string& targetLang);
	};
}


module :private;

void ProblemAnalyzer::analyze(Sentence* sentence, GptDictionary& gptDict, const std::string& targetLang) {
    if (sentence->translated_preview.empty()) {
        if (!sentence->pre_processed_text.empty() && !sentence->pre_translated_text.empty()) {
            sentence->problems = { "翻译为空" };
        }
        else {
            sentence->problems.clear();
        }
        return;
    }
    if (sentence->translated_preview.starts_with("(Failed to translate)")) {
        sentence->problems = { "翻译失败" };
        return;
    }

    sentence->problems.clear();

    // 1. 词频过高
    if (m_problems.highFrequency.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.highFrequency.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.highFrequency.check);
        const auto [mostWord, wordCount] = getMostCommonChar(transView);
        const auto [mostWordOrg, wordCountOrg] = getMostCommonChar(origText);
        if (wordCount > 20 && wordCount > (wordCountOrg > 0 ? wordCountOrg * 2 : 20)) {
            sentence->problems.push_back("词频过高-'" + mostWord + "'" + std::to_string(wordCount) + "次");
        }
    }

    // 2. 标点错漏
    if (m_problems.punctMiss.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.punctMiss.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.punctMiss.check);
        for (const auto& check : m_checks) {
            bool orgHas = origText.find(check) != std::string::npos;
            bool transHas = transView.find(check) != std::string::npos;

            if (orgHas && !transHas) sentence->problems.push_back("本有 " + check + " 符号");
            if (!orgHas && transHas) sentence->problems.push_back("本无 " + check + " 符号");
        }
    }

    // 3. 残留日文
    if (m_problems.remainJp.use) {
        const std::string& transView = chooseStringRef(sentence, m_problems.remainJp.check);
        if (std::string kanas = extractKana(transView); !kanas.empty()) {
            sentence->problems.push_back("残留日文: " + kanas);
        }
    }

    if (m_problems.introLatin.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.introLatin.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.introLatin.check);
        if (std::string latins = extractLatin(transView); !latins.empty() && extractLatin(origText).empty()) {
            sentence->problems.push_back("引入拉丁字母: " + latins);
        }
    }

    if (m_problems.introHangul.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.introHangul.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.introHangul.check);
        if (std::string hanguls = extractHangul(transView); !hanguls.empty() && extractHangul(origText).empty()) {
            sentence->problems.push_back("引入韩文: " + hanguls);
        }
    }

    if (m_problems.introNonGbk.use) {
        const std::string& transView = chooseStringRef(sentence, m_problems.introNonGbk.check);
        if (std::string nonGbk = extractNonGbkChars(transView); !nonGbk.empty()) {
            sentence->problems.push_back("引入非GBK字符: " + nonGbk);
        }
    }

    if (m_problems.introTraditionalChinese.use) {
        const std::string& transView = chooseStringRef(sentence, m_problems.introTraditionalChinese.check);
        if (std::string traditionalChars = extractTraditionalChinese(transView); !traditionalChars.empty()) {
            sentence->problems.push_back("引入繁体字: " + traditionalChars);
        }
    }

    // 4. 换行符不匹配
    if (m_problems.linebreakLost.use) {
        if (!sentence->originalLinebreak.empty()) {
            const std::string& origText = chooseStringRef(sentence, m_problems.linebreakLost.base);
            const std::string& transView = chooseStringRef(sentence, m_problems.linebreakLost.check);
            int orgLinebreaks = countSubstring(origText, sentence->originalLinebreak);
            int transLinebreaks = countSubstring(transView, sentence->originalLinebreak);
            if (orgLinebreaks > transLinebreaks) {
                sentence->problems.push_back(std::format("丢失换行({}/{})", orgLinebreaks, transLinebreaks));
            }
        }
    }
    if (m_problems.linebreakAdded.use) {
        if (!sentence->originalLinebreak.empty()) {
            const std::string& origText = chooseStringRef(sentence, m_problems.linebreakAdded.base);
            const std::string& transView = chooseStringRef(sentence, m_problems.linebreakAdded.check);
            int orgLinebreaks = countSubstring(origText, sentence->originalLinebreak);
            int transLinebreaks = countSubstring(transView, sentence->originalLinebreak);
            if (orgLinebreaks < transLinebreaks) {
                sentence->problems.push_back(std::format("多加换行({}/{})", orgLinebreaks, transLinebreaks));
            }
        }
    }

    // 5. 译文长度异常
    if (m_problems.strictlyLonger.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.strictlyLonger.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.strictlyLonger.check);
        size_t origTextCharCount = splitIntoGraphemes(origText).size();
        size_t transViewCharCount = splitIntoGraphemes(transView).size();
        if (transViewCharCount > origTextCharCount && origTextCharCount != 0) {
            sentence->problems.push_back(
                std::format("比原文严格长 {:.2f} 倍({}/{}字符)", transViewCharCount / (double)origTextCharCount, transViewCharCount, origTextCharCount)
            );
        }
    }
    else if (m_problems.longer.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.longer.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.longer.check);
        size_t origTextCharCount = splitIntoGraphemes(origText).size();
        size_t transViewCharCount = splitIntoGraphemes(transView).size();
        if (transViewCharCount > origTextCharCount * 1.3 && origTextCharCount != 0) {
            sentence->problems.push_back(
                std::format("比原文长 {:.2f} 倍({}/{}字符)", transViewCharCount / (double)origTextCharCount, transViewCharCount, origTextCharCount)
            );
        }
    }


    // 6. 字典未使用
    if (m_problems.dictUnused.use) {
        gptDict.checkDicUse(sentence, m_problems.dictUnused.base, m_problems.dictUnused.check);
    }

    // 7. 语言不通
    if (m_problems.notTargetLang.use) {
        const std::string& origText = chooseStringRef(sentence, m_problems.notTargetLang.base);
        const std::string& transView = chooseStringRef(sentence, m_problems.notTargetLang.check);
        std::string simplifiedTargetLang = targetLang;
        if (size_t pos = targetLang.find('-'); pos != std::string::npos) {
            simplifiedTargetLang = targetLang.substr(0, pos);
        }
        std::string origTextToCheck = removePunctuation(origText);
        std::string transTextToCheck = removePunctuation(transView);

        size_t origTextLen = origTextToCheck.length();
        size_t transTextLen = transTextToCheck.length();

        if (origTextLen > 6 || transTextLen > 6) {
            std::set<std::string> langSet;
            auto m_langIdentifier = std::make_unique<chrome_lang_id::NNetLanguageIdentifier>(3, 300);
            if (origTextLen > 6) {
                auto results = m_langIdentifier->FindTopNMostFreqLangs(origTextToCheck, 3);
                for (const auto& result : results) {
                    if (result.language == chrome_lang_id::NNetLanguageIdentifier::kUnknown) {
                        break;
                    }
                    m_logger->trace("CLD3: {} -> {} ({}, {}, {})", origText, result.language, result.is_reliable, result.probability, result.proportion);
                    if (result.probability < m_probabilityThreshold) {
                        continue;
                    }
                    langSet.insert(result.language);
                }
            }
            if (transTextLen > 6) {
                auto results = m_langIdentifier->FindTopNMostFreqLangs(transTextToCheck, 3);
                if (results[0].language == chrome_lang_id::NNetLanguageIdentifier::kUnknown && !langSet.empty()) {
                    sentence->problems.push_back("无法识别的语言");
                }
                for (const auto& result : results) {
                    if (result.language == chrome_lang_id::NNetLanguageIdentifier::kUnknown) {
                        break;
                    }
                    m_logger->trace("CLD3: {} -> {} ({}, {}, {})", transView, result.language, result.is_reliable, result.probability, result.proportion);
                    if (result.probability < m_probabilityThreshold) {
                        continue;
                    }
                    if (result.language != simplifiedTargetLang && langSet.find(result.language) == langSet.end()) {
                        sentence->problems.push_back(std::format("引入({}, {:.3f})", result.language, result.probability));
                    }
                }
            }
        }
        
    }

}

void ProblemAnalyzer::loadProblems(const std::vector<std::string>& problemList, const std::string& punctSet, double langProbability)
{
    m_probabilityThreshold = langProbability;
    if (!punctSet.empty()) {
        m_checks = splitIntoGraphemes(punctSet);
    }
	for (const auto& problem : problemList)
	{
        if (problem.empty()) {
            continue;
        }
		if (problem == "词频过高") {
			m_problems.highFrequency.use = true;
		}
		else if (problem == "标点错漏") {
			m_problems.punctMiss.use = true;
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
        else if (problem == "引入非GBK字符") {
            m_problems.introNonGbk.use = true;
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
		}
		else {
			throw std::invalid_argument("未知问题: " + problem);
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
                throw std::invalid_argument("未知缓存键: " + base);
            }
            if (obj.check == CachePart::None) {
                throw std::invalid_argument("未知缓存键: " + check);
            }
        };

    if (problemKey == "词频过高") {
        saveCachePart(m_problems.highFrequency, base, check);
    }
    else if (problemKey == "标点错漏") {
        saveCachePart(m_problems.punctMiss, base, check);
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
    else if (problemKey == "引入非GBK字符") {
        saveCachePart(m_problems.introNonGbk, base, check);
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
    else {
        throw std::invalid_argument("不支持的问题比较对象覆写: " + problemKey);
    }
}
