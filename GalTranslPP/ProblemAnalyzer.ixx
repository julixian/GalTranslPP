module;

#include <spdlog/spdlog.h>
#include <cld3/nnet_language_identifier.h>
#include <cld3/language_identifier_features.h>

export module ProblemAnalyzer;

import Tool;
import Dictionary;

export {

	struct Problems {
		bool highFrequency = false;
		bool punctMiss = false;
		bool remainJp = false;
        bool introLatin = false;
        bool introHangul = false;
		bool linebreakLost = false;
		bool linebreakAdded = false;
		bool longer = false;
		bool strictlyLonger = false;
		bool dictUnused = false;
		bool notTargetLang = false;
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

		void analyze(Sentence* sentence, GptDictionary& gptDict, const std::string& targetLang);
	};
}


module :private;

void ProblemAnalyzer::analyze(Sentence* sentence, GptDictionary& gptDict, const std::string& targetLang) {
    if (sentence->translated_preview.empty()) {
        if (!sentence->pre_processed_text.empty() && !sentence->pre_translated_text.empty()) {
            sentence->problem = "翻译为空";
        }
        else {
            sentence->problem.clear();
        }
        return;
    }
    if (sentence->translated_preview.starts_with("(Failed to translate)")) {
        sentence->problem = "翻译失败";
        return;
    }

    sentence->problem.clear();
    std::vector<std::string> problemList;
    const std::string& origText = sentence->original_text;
    const std::string& transView = sentence->translated_preview;

    // 1. 词频过高
    if (m_problems.highFrequency) {
        auto [mostWord, wordCount] = getMostCommonChar(transView);
        auto [mostWordOrg, wordCountOrg] = getMostCommonChar(origText);
        if (wordCount > 20 && wordCount > (wordCountOrg > 0 ? wordCountOrg * 2 : 20)) {
            problemList.push_back("词频过高-'" + mostWord + "'" + std::to_string(wordCount) + "次");
        }
    }

    // 2. 标点错漏
    if (m_problems.punctMiss) {
        for (const auto& check : m_checks) {
            bool orgHas = origText.find(check) != std::string::npos;
            bool transHas = transView.find(check) != std::string::npos;

            if (orgHas && !transHas) problemList.push_back("本有 " + check + " 符号");
            if (!orgHas && transHas) problemList.push_back("本无 " + check + " 符号");
        }
    }

    // 3. 残留日文
    if (m_problems.remainJp) {
        if (containsKana(transView)) {
            problemList.push_back("残留日文");
        }
    }

    if (m_problems.introLatin) {
        if (containsLatin(transView) && !containsLatin(origText)) {
            problemList.push_back("引入拉丁字母");
        }
    }

    if (m_problems.introHangul) {
        if (containsHangul(transView) && !containsHangul(origText)) {
            problemList.push_back("引入韩文");
        }
    }

    // 4. 换行符不匹配
    if ((m_problems.linebreakLost || m_problems.linebreakAdded) && !sentence->originalLinebreak.empty()) {
        int orgLinebreaks = countSubstring(origText, sentence->originalLinebreak);
        int transLinebreaks = countSubstring(transView, sentence->originalLinebreak);
        if (m_problems.linebreakLost && orgLinebreaks > transLinebreaks) {
            problemList.push_back("丢失换行");
        }
        if (m_problems.linebreakAdded && orgLinebreaks < transLinebreaks) {
            problemList.push_back("多加换行");
        }
    }

    // 5. 译文长度异常
    if (m_problems.longer || m_problems.strictlyLonger) {
        double lenBeta = m_problems.strictlyLonger ? 1.0 : 1.3;
        if (transView.length() > origText.length() * lenBeta && !origText.empty()) {
            double ratio = transView.length() / (double)origText.length();
            problemList.push_back(std::format("比日文长 {:.2f} 倍", ratio));
        }
    }

    // 6. 字典未使用
    if (m_problems.dictUnused) {
        std::string dictProblem = gptDict.checkDicUse(sentence);
        if (!dictProblem.empty()) {
            problemList.push_back(dictProblem);
        }
    }

    // 7. 语言不通
    if (m_problems.notTargetLang) {
        std::string simplifiedTargetLang = targetLang;
        if (targetLang.find('-') != std::string::npos) {
            simplifiedTargetLang = targetLang.substr(0, targetLang.find('-'));
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
                    problemList.push_back("无法识别的语言");
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
                        problemList.push_back(std::format("引入({}, {:.3f})", result.language, result.probability));
                    }
                }
            }
        }
        
    }

    // 组合所有问题
    if (!problemList.empty()) {
        for (size_t i = 0; i < problemList.size(); ++i) {
            sentence->problem += problemList[i];
            if (i < problemList.size() - 1) {
                sentence->problem += ", ";
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
	for (auto& problem : problemList)
	{
		if (problem == "词频过高") {
			m_problems.highFrequency = true;
		}
		else if (problem == "标点错漏") {
			m_problems.punctMiss = true;
		}
		else if (problem == "残留日文") {
			m_problems.remainJp = true;
		}
        else if (problem == "引入拉丁字母") {
            m_problems.introLatin = true;
        }
        else if (problem == "引入韩文") {
            m_problems.introHangul = true;
		}
		else if (problem == "丢失换行") {
			m_problems.linebreakLost = true;
		}
		else if (problem == "多加换行") {
			m_problems.linebreakAdded = true;
		}
		else if (problem == "比原文长") {
			m_problems.longer = true;
		}
		else if (problem == "比原文长严格") {
			m_problems.strictlyLonger = true;
		}
		else if (problem == "字典未使用") {
			m_problems.dictUnused = true;
		}
		else if (problem == "语言不通") {
			m_problems.notTargetLang = true;
		}
		else {
			throw std::invalid_argument("未知问题: " + problem);
		}
	}
}
