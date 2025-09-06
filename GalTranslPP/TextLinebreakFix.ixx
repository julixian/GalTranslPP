module;

#include <spdlog/spdlog.h>
#include <toml++/toml.hpp>

import std;
import Tool;
import IPlugin;
export module TextLinebreakFix;
namespace fs = std::filesystem;

export {
	class TextLinebreakFix : public IPlugin {

	private:

		std::string m_mode;
		int m_segmentThreshold;
		bool m_forceFix;

	public:

		TextLinebreakFix(const fs::path& projectDir, std::shared_ptr<spdlog::logger> logger);

		virtual void run(Sentence* se) override;

		virtual ~TextLinebreakFix() = default;
	};
}


module :private;

TextLinebreakFix::TextLinebreakFix(const fs::path& projectDir, std::shared_ptr<spdlog::logger> logger)
	: IPlugin(projectDir, logger)
{
	try {
		std::ifstream ifs;
		ifs.open(m_projectDir / L"config.toml");
		auto projectConfig = toml::parse(ifs);
		ifs.close();

		ifs.open(pluginConfigsPath / L"textPostPlugins/TextLinebreakFix.toml");
		auto pluginConfig = toml::parse(ifs);
		ifs.close();

		m_mode = parseToml<std::string>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.换行模式");
		m_segmentThreshold = parseToml<int>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.分段字数阈值");
		m_forceFix = parseToml<bool>(projectConfig, pluginConfig, "plugins.TextLinebreakFix.强制修复");

		if (m_segmentThreshold <= 0) {
			throw std::runtime_error("分段字数阈值必须大于0");
		}
	}
	catch (const toml::parse_error& e) {
		m_logger->critical("换行修复配置文件解析错误: {}", e.description());
		throw std::runtime_error(e.what());
	}
}

void TextLinebreakFix::run(Sentence* se)
{
	if (se->originalLinebreak.empty()) {
		return;
	}

	int orgLinebreakCount = countSubstring(se->original_text, se->originalLinebreak);
	int newLinebreakCount = countSubstring(se->pre_translated_text, se->originalLinebreak);

	if (newLinebreakCount == orgLinebreakCount && !m_forceFix) {
		return;
	}

	m_logger->debug("需要修复换行的句子[{}]: 原文{}行, 译文{}行", se->original_text, orgLinebreakCount + 1, newLinebreakCount + 1);

	std::string orgText = se->pre_translated_text;

	if (m_mode == "平均") {
		int linebreakAdded = 0;
		std::string text = se->pre_translated_text;
		replaceStrInplace(text, se->originalLinebreak, "");
		std::vector<std::string> graphemes = splitIntoGraphemes(text);
		size_t totalCharCount = graphemes.size();
		if (totalCharCount == 0) {
			return;
		}
		size_t charsPerLine = totalCharCount / (orgLinebreakCount + 1);
		if (charsPerLine == 0) {
			charsPerLine = 1;
		}

		text.clear();
		for (size_t i = 0; i < graphemes.size(); i++) {
			text += graphemes[i];
			if ((i + 1) % charsPerLine == 0 && linebreakAdded < orgLinebreakCount && i != graphemes.size() - 1) {
				text += se->originalLinebreak;
				linebreakAdded++;
			}
		}
		se->pre_translated_text = text;
	}
	else if (m_mode == "固定字数") {
		std::string text = se->pre_translated_text;
		replaceStrInplace(text, se->originalLinebreak, "");
		std::vector<std::string> graphemes = splitIntoGraphemes(text);
		if (graphemes.size() == 0) {
			return;
		}

		text.clear();
		for (size_t i = 0; i < graphemes.size(); i++) {
			text += graphemes[i];
			if ((i + 1) % m_segmentThreshold == 0 && i != graphemes.size() - 1) {
				text += se->originalLinebreak;
			}
		}
		se->pre_translated_text = text;
	}
	else if (m_mode == "保持位置") {
		std::string text = se->pre_translated_text;
		std::vector<double> relPositons = getSubstringPositions(se->original_text, se->originalLinebreak);
		replaceStrInplace(text, se->originalLinebreak, "");
		std::vector<size_t> linebreakPositions;
		for (double pos : relPositons) {
			linebreakPositions.push_back(static_cast<size_t>(pos * text.length()));
		}

		std::vector<std::string> graphemes = splitIntoGraphemes(text);
		std::reverse(graphemes.begin(), graphemes.end());

		text.clear();

		size_t currentPos = 0;
		for (size_t pos : linebreakPositions) {
			while (currentPos < pos) {
				if (graphemes.empty()) {
					break;
				}
				text += graphemes.back();
				currentPos += graphemes.back().length();
				graphemes.pop_back();
			}
			if (!graphemes.empty() && !text.ends_with(se->originalLinebreak)) {
				text += se->originalLinebreak;
			}
		}

		while (!graphemes.empty()) {
			text += graphemes.back();
			graphemes.pop_back();
		}

		se->pre_translated_text = text;
	}
	else if (m_mode == "优先标点") {

		std::string text = se->pre_translated_text;
		std::vector<double> relPositons = getSubstringPositions(se->original_text, se->originalLinebreak);
		replaceStrInplace(text, se->originalLinebreak, "");
		std::vector<std::string> graphemes = splitIntoGraphemes(text);

		std::vector<std::pair<size_t, size_t>> punctPositions;
		size_t currentPos = 0;
		for (size_t i = 0; i < graphemes.size(); i++) {
			currentPos += graphemes[i].length();
			if (removePunctuation(graphemes[i]).empty()) {
				punctPositions.push_back(std::make_pair(currentPos - graphemes[i].length(), currentPos));
			}
		}
		std::erase_if(punctPositions, [&](auto& pos)
			{
				if (pos.second >= text.length()) {
					return true;
				}
				// 如果标点后面还有标点，则不插入换行符
				return std::any_of(punctPositions.begin(), punctPositions.end(), [&](auto& otherPos)
					{
						return pos.second == otherPos.first;
					});
			});

		std::vector<size_t> linebreakPositions; // 根据相对位置大致估算新换行在译文中的位置
		for (double pos : relPositons) {
			linebreakPositions.push_back(static_cast<size_t>(pos * text.length()));
		}

		std::vector<size_t> positionsToAddLinebreak; // 最终要在 text 中插入换行符的位置

		if (punctPositions.size() == orgLinebreakCount) {
			for (auto& pos : punctPositions) {
				positionsToAddLinebreak.push_back(pos.second);
			}
		}
		else if (punctPositions.size() < orgLinebreakCount) {
			for (auto& pos : punctPositions) {
				positionsToAddLinebreak.push_back(pos.second);
			}
			for (size_t pos : positionsToAddLinebreak) {
				auto nearestIterator = std::min_element(linebreakPositions.begin(), linebreakPositions.end(), [&](size_t a, size_t b)
					{
						return calculateAbs(a, pos) < calculateAbs(b, pos);
					});
				linebreakPositions.erase(nearestIterator);
			}
			for (size_t pos : linebreakPositions) {
				size_t currentPos = 0;
				for (size_t i = 0; i < graphemes.size(); i++) {
					currentPos += graphemes[i].length();
					if (currentPos >= pos) {
						break;
					}
				}
				if (currentPos < text.length() && std::find(positionsToAddLinebreak.begin(), positionsToAddLinebreak.end(), currentPos) == positionsToAddLinebreak.end()) {
					positionsToAddLinebreak.push_back(currentPos);
				}
			}
		}
		else {
			for (size_t pos : linebreakPositions) {
				auto nearestIterator = std::min_element(punctPositions.begin(), punctPositions.end(), [&](auto& a, auto& b)
					{
						return calculateAbs(a.second, pos) < calculateAbs(b.second, pos);
					});
				positionsToAddLinebreak.push_back(nearestIterator->second);
				punctPositions.erase(nearestIterator);
			}
		}

		std::ranges::sort(positionsToAddLinebreak, [&](size_t a, size_t b)
			{
				return a > b;
			});

		for (size_t pos : positionsToAddLinebreak) {
			text.insert(pos, se->originalLinebreak);
		}

		se->pre_translated_text = text;
	}
	else {
		throw std::runtime_error("未知的换行模式: " + m_mode);
	}

	m_logger->debug("句子[{}]({}行): 修正后译文[{}]({}行)", orgText, orgLinebreakCount + 1, se->pre_translated_text, countSubstring(se->pre_translated_text, se->originalLinebreak) + 1);
}

