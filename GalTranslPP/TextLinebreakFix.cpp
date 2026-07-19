module;

#include "GPPMacros.hpp"
#include <toml.hpp>

module TextLinebreakFix;

import NLPTool;
import Tool;

namespace fs = std::filesystem;

static absl::btree_set<std::string_view> excludePuncts = { "『", "「", "“", "‘", "'", "《", "〈", "（", "【", "〔", "〖", "≪" };;

TextLinebreakFix::~TextLinebreakFix() {
	if (m_useTokenizer) {
		saveTokenizeCache(m_tokenizeCacheMap, m_tokenizeCachePath, m_logger);
	}
}

TextLinebreakFix::TextLinebreakFix(const fs::path& otherCacheDir, const toml::value& projectConfig, const std::shared_ptr<spdlog::logger>& logger, PluginRunTime runTime)
	: m_tokenizeCachePath(otherCacheDir / L"tokenizeCache_tlf.json"), m_logger(logger), m_runTime(runTime)
{
	try {
		if (m_runTime != PluginRunTime::DPost) {
			m_logger->error(gppTr("TextLinebreakFix.TextLinebreakFix", "TextLinebreakFix 不支持 %1 阶段运行")
			    .arg(pluginRunTime2Names[m_runTime])
			    .toStdString());
			return;
		}

		bool reversePriority = false;
		const fs::path pluginConfigPath = [&]()
			{
				fs::path ret = textPluginConfigPath / std::format(L"TextLinebreakFix-{}.toml", ascii2Wide(pluginRunTime2Names[m_runTime]));
				if (!fs::exists(ret)) {
					ret = textPluginConfigPath / L"TextLinebreakFix.toml";
				}
				else {
					reversePriority = true;
				}
				return ret;
			}();
		const auto pluginConfig = toml::uparse(pluginConfigPath);

		const std::string linebreakMode = parseToml<std::string>(projectConfig, pluginConfig,
			"plugins.TextLinebreakFix.linebreakMode", reversePriority);
		if (linebreakMode == "average") {
			m_mode = LinebreakFixMode::Average;
		}
		else if (linebreakMode == "fixedChars") {
			m_mode = LinebreakFixMode::FixCharCount;
		}
		else if (linebreakMode == "keepPosition") {
			m_mode = LinebreakFixMode::KeepPositions;
		}
		else if (linebreakMode == "preferPunctuation") {
			m_mode = LinebreakFixMode::PreferPunctuations;
		}
		else if (linebreakMode == "checkOnly") {
			m_mode = LinebreakFixMode::CheckOnly;
		}
		else {
			throw std::invalid_argument(gppTr(
			    "TextLinebreakFix.TextLinebreakFix",
			    "TextLinebreakFix-%1 无效的换行模式: %2")
			    .arg(pluginRunTime2Names[m_runTime])
			    .arg(linebreakMode)
			    .toStdString());
		}
		m_priorityThreshold = parseToml<double>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.priorityThreshold", reversePriority);
		m_segmentThreshold = parseToml<int>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.segmentThreshold", reversePriority);
		m_forceFix = parseToml<bool>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.forceFix", reversePriority);
		m_errorThreshold = parseToml<int>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.errorThreshold", reversePriority);
		m_useTokenizer = parseToml<bool>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.useTokenizer", reversePriority);


		if (m_useTokenizer) {
			loadTokenizeCache(m_tokenizeCacheMap, m_tokenizeCachePath, m_logger);
			const std::string tokenizerBackend = parseToml<std::string>(projectConfig, pluginConfig,
				"plugins.TextLinebreakFix.tokenizerBackend", reversePriority);
			if (tokenizerBackend == "MeCab") {
				const std::string mecabDictDir = parseToml<std::string>(projectConfig, pluginConfig,
					"plugins.TextLinebreakFix.mecabDictDir", reversePriority);
				m_logger->info(gppTr(
				    "TextLinebreakFix.TextLinebreakFix",
				    "TextLinebreakFix-%1 已配置 MeCab 分词器，首次使用时加载")
				    .arg(pluginRunTime2Names[m_runTime])
				    .toStdString());
				m_tokenizeTargetLangFunc = getMeCabTokenizeFunc(mecabDictDir, m_logger);
			}
			else if (tokenizerBackend == "spaCy") {
				const std::string spaCyModelName = parseToml<std::string>(projectConfig, pluginConfig,
					"plugins.TextLinebreakFix.spaCyModelName", reversePriority);
				m_logger->info(gppTr(
				    "TextLinebreakFix.TextLinebreakFix",
				    "TextLinebreakFix-%1 已配置 spaCy 分词器，首次使用时加载")
				    .arg(pluginRunTime2Names[m_runTime])
				    .toStdString());
				m_tokenizeTargetLangFunc = getPythonNLPTokenizeFunc({ "click", "spacy" }, "tokenizer_spacy",
					spaCyModelName, m_logger);
			}
			else if (tokenizerBackend == "Stanza") {
				const std::string stanzaLang = parseToml<std::string>(projectConfig, pluginConfig,
					"plugins.TextLinebreakFix.stanzaLang", reversePriority);
				m_logger->info(gppTr(
				    "TextLinebreakFix.TextLinebreakFix",
				    "TextLinebreakFix-%1 已配置 Stanza 分词器，首次使用时加载")
				    .arg(pluginRunTime2Names[m_runTime])
				    .toStdString());
				m_tokenizeTargetLangFunc = getPythonNLPTokenizeFunc({ "stanza" }, "tokenizer_stanza",
					stanzaLang, m_logger);
			}
			else if (tokenizerBackend == "pkuseg") {
				m_logger->info(gppTr(
				    "TextLinebreakFix.TextLinebreakFix",
				    "TextLinebreakFix-%1 已配置 pkuseg 分词器，首次使用时加载")
				    .arg(pluginRunTime2Names[m_runTime])
				    .toStdString());
				m_tokenizeTargetLangFunc = getPythonNLPTokenizeFunc({ "setuptools", "nes-py", "cython", "pkuseg" },
					"tokenizer_pkuseg", "default", m_logger);
			}
			else {
				throw std::invalid_argument(gppTr(
				    "TextLinebreakFix.TextLinebreakFix",
				    "TextLinebreakFix-%1 无效的 tokenizerBackend: %2")
				    .arg(pluginRunTime2Names[m_runTime])
				    .arg(tokenizerBackend)
				    .toStdString());
			}
		}

		if (m_segmentThreshold <= 0) {
			throw std::runtime_error(gppTr(
			    "TextLinebreakFix.TextLinebreakFix",
			    "TextLinebreakFix-%1 分段字数阈值必须大于0")
			    .arg(pluginRunTime2Names[m_runTime])
			    .toStdString());
		}
		if (m_errorThreshold <= 0) {
			throw std::runtime_error(gppTr(
			    "TextLinebreakFix.TextLinebreakFix",
			    "TextLinebreakFix-%1 报错阈值必须大于0")
			    .arg(pluginRunTime2Names[m_runTime])
			    .toStdString());
		}

		m_logger->info(gppTr(
		    "TextLinebreakFix.TextLinebreakFix",
		    "已加载插件 TextLinebreakFix-%1, 换行模式: %2, 优先阈值 %3, 分段字数阈值: %4, 强制修复: %5, 报错阈值: %6")
		    .arg(pluginRunTime2Names[m_runTime])
		    .arg(linebreakMode)
		    .arg(m_priorityThreshold, 0, 'f', 3)
		    .arg(m_segmentThreshold)
		    .arg(m_forceFix ? "true" : "false")
		    .arg(m_errorThreshold)
		    .toStdString());
		if (m_useTokenizer) {
			m_logger->info(gppTr("TextLinebreakFix.TextLinebreakFix", "插件 TextLinebreakFix-%1 分词器已启用")
			    .arg(pluginRunTime2Names[m_runTime])
			    .toStdString());
		}
    }
    catch (const toml::exception& e) {
        throw std::runtime_error(gppTr(
            "TextLinebreakFix.TextLinebreakFix",
            "TextLinebreakFix-%1 插件配置文件解析错误: %2")
            .arg(pluginRunTime2Names[m_runTime])
            .arg(e.what())
            .toStdString());
    }
}

std::vector<std::string_view> TextLinebreakFix::splitIntoTokenViews(std::string_view str)
{
	{
		std::shared_lock<std::shared_mutex> lock(m_tokenizeCacheMapMutex);
		if (const auto it = m_tokenizeCacheMap.find(str); it != m_tokenizeCacheMap.end()) {
			return ::splitIntoTokenViews(it->second, str);
		}
	}

	NLPResult result = m_tokenizeTargetLangFunc(str);
	WordPosVec& wordPosVec = std::get<0>(result);

	std::vector<std::string_view> ret = ::splitIntoTokenViews(wordPosVec, str);
	{
		std::lock_guard<std::shared_mutex> lock(m_tokenizeCacheMapMutex);
		m_tokenizeCacheMap.insert({ std::string(str), std::move(wordPosVec) });
	}
	return ret;
}

void TextLinebreakFix::dPostRun(Sentence* se)
{
	if (m_runTime != PluginRunTime::DPost || se->linebreak.empty()) {
		return;
	}

	const int origLinebreakCount = countSubstring(se->orig, se->linebreak);
	const int transLinebreakCount = countSubstring(se->transview, se->linebreak);

	auto checkLineCharCountFunc = [&](const std::string& transViewToModify)
		{
			if (transViewToModify.length() > m_errorThreshold) {
				for (const auto& [index, newLineView] : transViewToModify | std::views::split(se->linebreak)
					| std::views::transform([](const auto& subStrView)
						{
							return std::string_view(subStrView.begin(), subStrView.end());
						})
					| std::views::enumerate)
				{
					if (size_t charCount = countGraphemes(newLineView); charCount > m_errorThreshold) {
						se->problems.push_back(gppTr("TextLinebreakFix.checkLineLength", "第 %1 行字数超出报错阈值[%2/%3]")
						    .arg(index + 1)
						    .arg(charCount)
						    .arg(m_errorThreshold)
						    .toStdString());
					}
				}
			}
		};

	if (
		(m_mode == LinebreakFixMode::CheckOnly) ||
		(transLinebreakCount == origLinebreakCount && !m_forceFix)
		)
	{
		checkLineCharCountFunc(se->transview);
		return;
	}

	m_logger->debug(gppTr("TextLinebreakFix.fixLinebreak", "需要修复换行的句子[%1]: 原文 %2 行, 译文 %3 行")
	    .arg(se->transview)
	    .arg(origLinebreakCount + 1)
	    .arg(transLinebreakCount + 1)
	    .toStdString());

	std::string origTransView = se->transview;
	std::string transViewToModify = se->transview;

	if (transViewToModify.empty()) {
		for (int i = 0; i < origLinebreakCount; i++) {
			transViewToModify += se->linebreak;
		}
		se->transview = std::move(transViewToModify);
		se->otherinfo.insert({ gppTr("TextLinebreakFix.fixLinebreak", "换行修复")
		    .toStdString(), gppTr("TextLinebreakFix.fixLinebreak", "原文 %1 行, 译文 %2 行, 修正后 %3 行")
		        .arg(origLinebreakCount + 1)
		        .arg(transLinebreakCount + 1)
		        .arg(transLinebreakCount + 1)
		        .toStdString() });
		m_logger->debug(gppTr("TextLinebreakFix.fixLinebreak", "译文[%1](%2行) -> 修正后译文[%3](%4行)")
		    .arg(origTransView)
		    .arg(origLinebreakCount + 1)
		    .arg(se->transview)
		    .arg(transLinebreakCount + 1)
		    .toStdString());
		return;
	}

	replaceStrInplace(transViewToModify, se->linebreak, "");
	const std::string constTransView = transViewToModify;
	const std::string_view constTransViewView = constTransView;

	auto removeRepeat = [](std::vector<size_t>& positions)
		{
			std::ranges::sort(positions);
			const auto [first, last] = std::ranges::unique(positions);
			positions.erase(first, last);
		};

	switch (m_mode)
	{
	case LinebreakFixMode::Average:
	{
		const std::vector<std::string_view> tokenViews = m_useTokenizer
			? this->splitIntoTokenViews(constTransViewView)
			: splitIntoGraphemeViews(constTransViewView);
		const size_t totalCharCount = tokenViews.size();
		int linebreakAdded = 0;

		size_t charCountLine = totalCharCount / (origLinebreakCount + 1);
		if (charCountLine == 0) {
			charCountLine = 1;
		}

		transViewToModify.clear();
		for (const auto& [index, token] : tokenViews | std::views::enumerate) {
			transViewToModify += token;
			if ((index + 1) % charCountLine == 0 && linebreakAdded < origLinebreakCount && index != totalCharCount - 1) {
				transViewToModify += se->linebreak;
				++linebreakAdded;
			}
		}
	}
	break;

	case LinebreakFixMode::FixCharCount:
	{
		const std::vector<std::string_view> graphemes = splitIntoGraphemeViews(constTransViewView);
		transViewToModify.clear();
		for (const auto& [index, grapheme] : graphemes | std::views::enumerate) {
			transViewToModify += grapheme;
			if ((index + 1) % m_segmentThreshold == 0 && index != graphemes.size() - 1) {
				transViewToModify += se->linebreak;
			}
		}
	}
	break;

	case LinebreakFixMode::KeepPositions:
	{
		const std::vector<std::string_view> tokens = m_useTokenizer
			? this->splitIntoTokenViews(constTransViewView)
			: splitIntoGraphemeViews(constTransViewView);
		const std::vector<double> relLinebreakPositions = getSubstringPositions(se->orig, se->linebreak);
		std::vector<size_t> positionsToAddLinebreak; // 最终要在 transViewToModify 中插入换行符的位置

		size_t currentPos = 0;
		size_t currentTokenIndex = 0;
		for (const auto& relLinebreakPos : relLinebreakPositions) {
			while (currentPos / (double)transViewToModify.length() < relLinebreakPos) {
				currentPos += tokens[currentTokenIndex].length();
				++currentTokenIndex;
			}
			positionsToAddLinebreak.push_back(currentPos);
		}

		removeRepeat(positionsToAddLinebreak);
		for (const auto& posToAddLinebreak : positionsToAddLinebreak | std::views::reverse) {
			transViewToModify.insert(posToAddLinebreak, se->linebreak);
		}
	}
	break;

	case LinebreakFixMode::PreferPunctuations:
	{
		const std::vector<std::string_view> graphemes = splitIntoGraphemeViews(constTransViewView);
		const std::vector<std::string_view> tokens = m_useTokenizer
			? this->splitIntoTokenViews(constTransViewView)
			: graphemes;

		// 预处理标点信息，获取所有可以添加换行的标点位置
		std::vector<double> relLinebreakPositions = getSubstringPositions(se->orig, se->linebreak);

		struct PunctInfo
		{
			size_t prePos;
			size_t postPos;
			double relPos;
		};
		std::vector<PunctInfo> punctPositions;
		size_t currentPos = 0;

		for (const auto& [index, grapheme] : graphemes | std::views::enumerate) {
			currentPos += grapheme.length();
			if (hasPunctuation(grapheme)) {
				punctPositions.push_back({ currentPos - grapheme.length(),
					currentPos, currentPos / (double)transViewToModify.length() });
			}
			// 如果当前字符的后一个字符是全角空格或如上左边界字符，则把当前字符作为标点对待
			else if ((size_t)index + 1 < graphemes.size()) {
				if (const auto& g = graphemes[index + 1]; g == "　" || excludePuncts.contains(g)) {
					punctPositions.push_back({ currentPos - grapheme.length(),
						currentPos, currentPos / (double)transViewToModify.length() });
				}
			}
		}

		std::erase_if(punctPositions, [&](const PunctInfo& pos)
			{
				if (pos.postPos >= transViewToModify.length()) {
					return true;
				}
				const std::string_view punctStrView(transViewToModify.c_str() + pos.prePos, pos.postPos - pos.prePos);
				// 不在这些标点后添加换行符
				if (excludePuncts.contains(punctStrView)) {
					return true;
				}
				// 如果标点后面还有标点，则不插入换行符
				return std::ranges::any_of(punctPositions, [&](const PunctInfo& otherPos)
					{
						return pos.postPos == otherPos.prePos;
					});
			});

		std::vector<size_t> positionsToAddLinebreak; // 最终要在 transViewToModify 中插入换行符的位置
		// 预处理信息完毕

		if (origLinebreakCount <= (int)punctPositions.size()) {
			// 换行挑标点
			std::vector<PunctInfo> filteredPunctPositions; // 被挑出来的标点位置
			for (const auto& relLinebreakPos : relLinebreakPositions) {
				const auto it = std::ranges::min_element(punctPositions, [&](const auto& a, const auto& b)
					{
						return calculateAbs(a.relPos, relLinebreakPos)
							< calculateAbs(b.relPos, relLinebreakPos);
					});
				filteredPunctPositions.push_back(*it);
				punctPositions.erase(it);
			}
			std::ranges::sort(filteredPunctPositions, [](const auto& a, const auto& b)
				{
					return a.prePos < b.prePos;
				});
			currentPos = 0;
			size_t currentTokenIndex = 0;
			for (const auto& [relLinebreakPos, punctPos] : std::views::zip(relLinebreakPositions, filteredPunctPositions)) {
				if (calculateAbs(relLinebreakPos, punctPos.relPos) > m_priorityThreshold) {
					while (currentTokenIndex < tokens.size()) {
						double relCur = currentPos / (double)transViewToModify.length();
						currentPos += tokens[currentTokenIndex].length();
						++currentTokenIndex;
						if (double newRelCur = currentPos / (double)transViewToModify.length(); newRelCur >= relLinebreakPos) {
							if (newRelCur - relLinebreakPos > relLinebreakPos - relCur) {
								--currentTokenIndex;
								currentPos -= tokens[currentTokenIndex].length();
							}
							break;
						}
					}
					positionsToAddLinebreak.push_back(currentPos);
				}
				else {
					positionsToAddLinebreak.push_back(punctPos.postPos);
				}
			}
		}
		else {
			// 标点挑换行
			std::vector<double> filteredRelLinebreakPositions; // 被挑出来的换行位置
			for (const auto& punctPos : punctPositions) {
				const auto it = std::ranges::min_element(relLinebreakPositions, [&](const auto& a, const auto& b)
					{
						return calculateAbs(a, punctPos.relPos)
							< calculateAbs(b, punctPos.relPos);
					});
				filteredRelLinebreakPositions.push_back(*it);
				relLinebreakPositions.erase(it);
			}
			std::ranges::sort(filteredRelLinebreakPositions);
			currentPos = 0;
			size_t currentTokenIndex = 0;
			for (const auto& [punctPosition, relLinebreakPos] : std::views::zip(punctPositions, filteredRelLinebreakPositions)) {
				if (double punctPos = punctPosition.relPos;  calculateAbs(relLinebreakPos, punctPos) > m_priorityThreshold) {
					while (currentTokenIndex < tokens.size()) {
						double relCur = currentPos / (double)transViewToModify.length();
						currentPos += tokens[currentTokenIndex].length();
						++currentTokenIndex;
						if (double newRelCur = currentPos / (double)transViewToModify.length(); newRelCur >= relLinebreakPos) {
							if (newRelCur - relLinebreakPos > relLinebreakPos - relCur) {
								--currentTokenIndex;
								currentPos -= tokens[currentTokenIndex].length();
							}
							break;
						}
					}
					positionsToAddLinebreak.push_back(currentPos);
				}
				else {
					positionsToAddLinebreak.push_back(punctPosition.postPos);
				}
			}
			currentPos = 0;
			currentTokenIndex = 0;
			for (const auto& relLinebreakPos : relLinebreakPositions) {
				while (currentTokenIndex < tokens.size()) {
					double relCur = currentPos / (double)transViewToModify.length();
					currentPos += tokens[currentTokenIndex].length();
					++currentTokenIndex;
					if (double newRelCur = currentPos / (double)transViewToModify.length(); newRelCur >= relLinebreakPos) {
						if (newRelCur - relLinebreakPos > relLinebreakPos - relCur) {
							--currentTokenIndex;
							currentPos -= tokens[currentTokenIndex].length();
						}
						break;
					}
				}
				positionsToAddLinebreak.push_back(currentPos);
			}
		}

		removeRepeat(positionsToAddLinebreak);
		//std::ranges::sort(positionsToAddLinebreak); // removeRepeat 时已经排序

		for (const auto& posToAddLinebreak : positionsToAddLinebreak | std::views::reverse) {
			transViewToModify.insert(posToAddLinebreak, se->linebreak);
		}

		if (m_useTokenizer && m_logger->should_log(spdlog::level::debug)) {
			const std::string tokensStr = std::ranges::fold_left(tokens, std::string{}, [](const std::string& acc, const auto& token)
				{
					return acc + "[" + token + "]";
				});
			se->otherinfo.insert({ gppTr("TextLinebreakFix.fixLinebreak", "译文分词结果")
			    .toStdString(), std::move(tokensStr) });
		}
	}
	break;

	default:
		throw std::invalid_argument(gppTr("TextLinebreakFix.fixLinebreak", "无效的 TextLinebreakFix 模式")
		    .toStdString());

	}

	se->transview = std::move(transViewToModify);
	checkLineCharCountFunc(se->transview);

	const int newLinebreakCount = countSubstring(se->transview, se->linebreak);
	se->otherinfo.insert({ gppTr("TextLinebreakFix.fixLinebreak", "换行修复")
	    .toStdString(), gppTr("TextLinebreakFix.fixLinebreak", "原文 %1 行, 译文 %2 行, 修正后 %3 行")
	        .arg(origLinebreakCount + 1)
	        .arg(transLinebreakCount + 1)
	        .arg(newLinebreakCount + 1)
	        .toStdString() });
	m_logger->debug(gppTr("TextLinebreakFix.fixLinebreak", "句子[%1](%2行) -> 修正后译文[%3](%4行)")
	    .arg(origTransView)
	    .arg(transLinebreakCount + 1)
	    .arg(se->transview)
	    .arg(newLinebreakCount + 1)
	    .toStdString());
}
