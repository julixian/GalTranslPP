module;

#include "GPPMacros.hpp"
#include <toml.hpp>

module TextLinebreakFix;

import NLPTool;
import Tool;

namespace fs = std::filesystem;

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
			m_logger->error(gppTr("TextLinebreakFix.TextLinebreakFix", "TextLinebreakFix 不支持 %1 阶段运行", pluginRunTimeNames[m_runTime]));
			return;
		}

		const fs::path pluginConfigPath = [&]()
			{
				fs::path ret = textPluginConfigPath / std::format(L"TextLinebreakFix-{}.toml", ascii2Wide(pluginRunTimeNames[m_runTime]));
				if (!fs::exists(ret)) {
					ret = textPluginConfigPath / L"TextLinebreakFix.toml";
				}
				return ret;
			}();
		const auto pluginConfig = toml::uparse(pluginConfigPath);

		const std::string linebreakMode = parseToml<std::string>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.换行模式");
		if (linebreakMode == "平均") {
			m_mode = LinebreakFixMode::Average;
		}
		else if (linebreakMode == "固定字数") {
			m_mode = LinebreakFixMode::FixCharCount;
		}
		else if (linebreakMode == "保留位置") {
			m_mode = LinebreakFixMode::KeepPositions;
		}
		else if (linebreakMode == "优先标点") {
			m_mode = LinebreakFixMode::PreferPunctuations;
		}
		else if (linebreakMode == "不修改") {
			m_mode = LinebreakFixMode::NotFix;
		}
		else {
			throw std::invalid_argument(gppTr("TextLinebreakFix.TextLinebreakFix", "TextLinebreakFix-%1 无效的换行模式: %2", pluginRunTimeNames[m_runTime], linebreakMode));
		}
		m_priorityThreshold = parseToml<double>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.优先阈值");
		m_segmentThreshold = parseToml<int>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.分段字数阈值");
		m_forceFix = parseToml<bool>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.强制修复");
		m_errorThreshold = parseToml<int>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.报错阈值");
		m_useTokenizer = parseToml<bool>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.使用分词器");


		if (m_useTokenizer) {
			loadTokenizeCache(m_tokenizeCacheMap, m_tokenizeCachePath, m_logger);
			const std::string tokenizerBackend = parseToml<std::string>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.tokenizerBackend");
			if (tokenizerBackend == "MeCab") {
				const std::string mecabDictDir = parseToml<std::string>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.mecabDictDir");
				m_logger->info(gppTr("TextLinebreakFix.TextLinebreakFix", "TextLinebreakFix-%1 已配置 MeCab 分词器，首次使用时加载。", pluginRunTimeNames[m_runTime]));
				m_tokenizeTargetLangFunc = getMeCabTokenizeFunc(mecabDictDir, m_logger);
			}
			else if (tokenizerBackend == "spaCy") {
				const std::string spaCyModelName = parseToml<std::string>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.spaCyModelName");
				m_logger->info(gppTr("TextLinebreakFix.TextLinebreakFix", "TextLinebreakFix-%1 已配置 spaCy 分词器，首次使用时加载。", pluginRunTimeNames[m_runTime]));
				m_tokenizeTargetLangFunc = getNLPTokenizeFunc({ "spacy" }, "tokenizer_spacy", spaCyModelName, m_logger);
			}
			else if (tokenizerBackend == "Stanza") {
				const std::string stanzaLang = parseToml<std::string>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.stanzaLang");
				m_logger->info(gppTr("TextLinebreakFix.TextLinebreakFix", "TextLinebreakFix-%1 已配置 Stanza 分词器，首次使用时加载。", pluginRunTimeNames[m_runTime]));
				m_tokenizeTargetLangFunc = getNLPTokenizeFunc({ "stanza" }, "tokenizer_stanza", stanzaLang, m_logger);
			}
			else if (tokenizerBackend == "pkuseg") {
				m_logger->info(gppTr("TextLinebreakFix.TextLinebreakFix", "TextLinebreakFix-%1 已配置 pkuseg 分词器，首次使用时加载。", pluginRunTimeNames[m_runTime]));
				m_tokenizeTargetLangFunc = getNLPTokenizeFunc({ "setuptools", "nes-py", "cython", "pkuseg" }, "tokenizer_pkuseg", "default", m_logger);
			}
			else {
				throw std::invalid_argument(gppTr("TextLinebreakFix.TextLinebreakFix", "TextLinebreakFix-%1 无效的 tokenizerBackend: %2", pluginRunTimeNames[m_runTime], tokenizerBackend));
			}
		}

		if (m_segmentThreshold <= 0) {
			throw std::runtime_error(gppTr("TextLinebreakFix.TextLinebreakFix", "TextLinebreakFix-%1 分段字数阈值必须大于0", pluginRunTimeNames[m_runTime]));
		}
		if (m_errorThreshold <= 0) {
			throw std::runtime_error(gppTr("TextLinebreakFix.TextLinebreakFix", "TextLinebreakFix-%1 报错阈值必须大于0", pluginRunTimeNames[m_runTime]));
		}

		m_excludePuncts = { "『", "「", "“", "‘", "'", "《", "〈", "（", "【", "〔", "〖" };

		m_logger->info(gppTr("TextLinebreakFix.TextLinebreakFix", "已加载插件 TextLinebreakFix-%1, 换行模式: %2, 优先阈值 %3, 分段字数阈值: %4, 强制修复: %5, 报错阈值: %6",
			pluginRunTimeNames[m_runTime], linebreakMode, std::format("{:.3f}", m_priorityThreshold),
			m_segmentThreshold, m_forceFix ? "true" : "false", m_errorThreshold));
		if (m_useTokenizer) {
			m_logger->info(gppTr("TextLinebreakFix.TextLinebreakFix", "插件 TextLinebreakFix-%1 分词器已启用", pluginRunTimeNames[m_runTime]));
		}
    }
    catch (const toml::exception& e) {
        throw std::runtime_error(gppTr("TextLinebreakFix.TextLinebreakFix", "TextLinebreakFix-%1 插件配置文件解析错误: %2", pluginRunTimeNames[m_runTime], e.what()));
    }
}

std::vector<std::string> TextLinebreakFix::splitIntoTokens(const std::string& text)
{
	{
		std::shared_lock<std::shared_mutex> lock(m_tokenizeCacheMapMutex);
		if (const auto it = m_tokenizeCacheMap.find(text); it != m_tokenizeCacheMap.end()) {
			return ::splitIntoTokens(it->second, text);
		}
	}

	NLPResult result = m_tokenizeTargetLangFunc(text);
	WordPosVec& wordPosVec = std::get<0>(result);

	std::vector<std::string> ret = ::splitIntoTokens(wordPosVec, text);
	{
		std::lock_guard<std::shared_mutex> lock(m_tokenizeCacheMapMutex);
		m_tokenizeCacheMap.insert({ text, std::move(wordPosVec) });
	}
	return ret;
}

void TextLinebreakFix::dPostRun(Sentence* se)
{
	if (m_runTime != PluginRunTime::DPost || se->originalLinebreak.empty()) {
		return;
	}

	const int origLinebreakCount = countSubstring(se->original_text, se->originalLinebreak);
	const int transLinebreakCount = countSubstring(se->translated_preview, se->originalLinebreak);

	auto checkLineCharCountFunc = [&](const std::string& transViewToModify)
		{
			if (transViewToModify.length() > m_errorThreshold) {
				for (const auto& [index, newLineView] : transViewToModify | std::views::split(se->originalLinebreak)
					| std::views::transform([](const auto& subStrView)
						{
							return std::string_view(subStrView.begin(), subStrView.end());
						})
					| std::views::enumerate)
				{
					if (size_t charCount = countGraphemes(newLineView); charCount > m_errorThreshold) {
						se->problems.push_back(gppTr("TextLinebreakFix.checkLineLength", "第 %1 行字数超出报错阈值[%2/%3]", index + 1, charCount, m_errorThreshold));
					}
				}
			}
		};

	if (
		(m_mode == LinebreakFixMode::NotFix) ||
		(transLinebreakCount == origLinebreakCount && !m_forceFix)
		) {
		checkLineCharCountFunc(se->translated_preview);
		return;
	}

	m_logger->debug(gppTr("TextLinebreakFix.fixLinebreak", "需要修复换行的句子[%1]: 原文 %2 行, 译文 %3 行", se->translated_preview, origLinebreakCount + 1, transLinebreakCount + 1));

	std::string origTransPreview = se->translated_preview;
	std::string transViewToModify = se->translated_preview;
	if (transViewToModify.empty()) {
		for (int i = 0; i < origLinebreakCount; i++) {
			transViewToModify += se->originalLinebreak;
		}
		se->translated_preview = std::move(transViewToModify);
		se->other_info.insert({ gppTr("TextLinebreakFix.fixLinebreak", "换行修复"), gppTr("TextLinebreakFix.fixLinebreak", "原文 %1 行, 译文 %2 行, 修正后 %3 行", origLinebreakCount + 1, transLinebreakCount + 1, transLinebreakCount + 1) });
		m_logger->debug(gppTr("TextLinebreakFix.fixLinebreak", "译文[%1](%2行) -> 修正后译文[%3](%4行)", origTransPreview, origLinebreakCount + 1, se->translated_preview, transLinebreakCount + 1));
		return;
	}
	replaceStrInplace(transViewToModify, se->originalLinebreak, "");

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
		const std::vector<std::string> tokens = m_useTokenizer ? splitIntoTokens(transViewToModify) : splitIntoGraphemes(transViewToModify);
		const size_t totalCharCount = tokens.size();
		int linebreakAdded = 0;

		size_t charCountLine = totalCharCount / (origLinebreakCount + 1);
		if (charCountLine == 0) {
			charCountLine = 1;
		}

		transViewToModify.clear();
		for (const auto& [index, token] : tokens | std::views::enumerate) {
			transViewToModify += token;
			if ((index + 1) % charCountLine == 0 && linebreakAdded < origLinebreakCount && index != totalCharCount - 1) {
				transViewToModify += se->originalLinebreak;
				++linebreakAdded;
			}
		}
	}
	break;

	case LinebreakFixMode::FixCharCount:
	{
		const std::vector<std::string> graphemes = splitIntoGraphemes(transViewToModify);
		transViewToModify.clear();
		for (const auto& [index, grapheme] : graphemes | std::views::enumerate) {
			transViewToModify += grapheme;
			if ((index + 1) % m_segmentThreshold == 0 && index != graphemes.size() - 1) {
				transViewToModify += se->originalLinebreak;
			}
		}
	}
	break;

	case LinebreakFixMode::KeepPositions:
	{
		const std::vector<std::string> tokens = m_useTokenizer ? splitIntoTokens(transViewToModify) : splitIntoGraphemes(transViewToModify);
		const std::vector<double> relLinebreakPositions = getSubstringPositions(se->original_text, se->originalLinebreak);
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
			transViewToModify.insert(posToAddLinebreak, se->originalLinebreak);
		}
	}
	break;

	case LinebreakFixMode::PreferPunctuations:
	{
		std::vector<std::string> graphemes = splitIntoGraphemes(transViewToModify);
		std::vector<std::string> tokens = m_useTokenizer ? splitIntoTokens(transViewToModify) : graphemes;

		// 预处理标点信息，获取所有可以添加换行的标点位置
		std::vector<double> relLinebreakPositions = getSubstringPositions(se->original_text, se->originalLinebreak);

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
			if (removePunctuation(grapheme).empty()) {
				punctPositions.push_back({ currentPos - grapheme.length(), currentPos, currentPos / (double)transViewToModify.length() });
			}
			// 如果当前字符的后一个字符是全角空格或如上左边界字符，则把当前字符作为标点对待
			else if ((size_t)index + 1 < graphemes.size()) {
				if (const auto& g = graphemes[index + 1]; g == "　" || m_excludePuncts.contains(g)) {
					punctPositions.push_back({ currentPos - grapheme.length(), currentPos, currentPos / (double)transViewToModify.length() });
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
				if (m_excludePuncts.contains(punctStrView)) {
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
			transViewToModify.insert(posToAddLinebreak, se->originalLinebreak);
		}

		if (m_useTokenizer && m_logger->should_log(spdlog::level::debug)) {
			const std::string tokensStr = std::ranges::fold_left(tokens, std::string{}, [](const std::string& acc, const auto& token)
				{
					return acc + "[" + token + "]";
				});
			se->other_info.insert({ gppTr("TextLinebreakFix.fixLinebreak", "译文分词结果"), std::move(tokensStr) });
		}
	}
	break;

	default:
		throw std::invalid_argument(gppTr("TextLinebreakFix.fixLinebreak", "无效的 TextLinebreakFix 模式"));

	}

	se->translated_preview = std::move(transViewToModify);
	checkLineCharCountFunc(se->translated_preview);

	const int newLinebreakCount = countSubstring(se->translated_preview, se->originalLinebreak);
	se->other_info.insert({ gppTr("TextLinebreakFix.fixLinebreak", "换行修复"), gppTr("TextLinebreakFix.fixLinebreak", "原文 %1 行, 译文 %2 行, 修正后 %3 行", origLinebreakCount + 1, transLinebreakCount + 1, newLinebreakCount + 1) });
	m_logger->debug(gppTr("TextLinebreakFix.fixLinebreak", "句子[%1](%2行) -> 修正后译文[%3](%4行)", origTransPreview, transLinebreakCount + 1, se->translated_preview, newLinebreakCount + 1));
}
