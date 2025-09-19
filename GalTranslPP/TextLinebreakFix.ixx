module;

#include <spdlog/spdlog.h>

export module TextLinebreakFix;

import <toml++/toml.hpp>;
import Tool;
import IPlugin;

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

		m_logger->info("已加载插件 TextLinebreakFix, 换行模式: {}, 分段字数阈值: {}, 强制修复: {}", m_mode, m_segmentThreshold, m_forceFix);
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

	int origLinebreakCount = countSubstring(se->pre_processed_text, "<br>");
	int newLinebreakCount = countSubstring(se->translated_preview, "<br>");

	if (newLinebreakCount == origLinebreakCount && !m_forceFix) {
		return;
	}

	m_logger->debug("需要修复换行的句子[{}]: 原文 {} 行, 译文 {} 行", se->pre_processed_text, origLinebreakCount + 1, newLinebreakCount + 1);

	std::string orgText = se->translated_preview;
	std::string textToModify = se->translated_preview;
	replaceStrInplace(textToModify, "<br>", "");
	if (textToModify.empty()) {
		for (int i = 0; i < origLinebreakCount; i++) {
			textToModify += "<br>";
		}
		se->translated_preview = textToModify;
		m_logger->debug("译文[{}]({}行): 修正后译文[{}]({}行)", orgText, origLinebreakCount + 1, se->translated_preview, newLinebreakCount + 1);
		return;
	}
	std::vector<std::string> graphemes = splitIntoGraphemes(textToModify);

	if (m_mode == "平均") {
		int linebreakAdded = 0;

		size_t charCountLine = graphemes.size() / (origLinebreakCount + 1);
		if (charCountLine == 0) {
			charCountLine = 1;
		}

		textToModify.clear();
		for (size_t i = 0; i < graphemes.size(); i++) {
			textToModify += graphemes[i];
			if ((i + 1) % charCountLine == 0 && linebreakAdded < origLinebreakCount) {
				textToModify += "<br>";
				linebreakAdded++;
			}
		}
		se->translated_preview = textToModify;
	}
	else if (m_mode == "固定字数") {

		textToModify.clear();
		for (size_t i = 0; i < graphemes.size(); i++) {
			textToModify += graphemes[i];
			if ((i + 1) % m_segmentThreshold == 0 && i != graphemes.size() - 1) {
				textToModify += "<br>";
			}
		}
		se->translated_preview = textToModify;
	}
	else if (m_mode == "保持位置") {
		std::vector<double> relPositons = getSubstringPositions(se->pre_processed_text, "<br>");

		std::vector<size_t> linebreakPositions;
		for (double pos : relPositons) {
			linebreakPositions.push_back(static_cast<size_t>(pos * textToModify.length()));
		}

		std::reverse(graphemes.begin(), graphemes.end());

		textToModify.clear();

		size_t currentPos = 0;
		for (size_t pos : linebreakPositions) {
			while (currentPos < pos) {
				if (graphemes.empty()) {
					break;
				}
				textToModify += graphemes.back();
				currentPos += graphemes.back().length();
				graphemes.pop_back();
			}
			textToModify += "<br>";
		}

		while (!graphemes.empty()) {
			textToModify += graphemes.back();
			graphemes.pop_back();
		}

		se->translated_preview = textToModify;
	}
	else if (m_mode == "优先标点") {

		std::vector<double> relPositons = getSubstringPositions(se->pre_processed_text, "<br>");

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
				if (pos.second >= textToModify.length()) {
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
			linebreakPositions.push_back(static_cast<size_t>(pos * textToModify.length()));
		}

		std::vector<size_t> positionsToAddLinebreak; // 最终要在 textToModify 中插入换行符的位置

		if (punctPositions.size() == origLinebreakCount) {
			for (auto& pos : punctPositions) {
				positionsToAddLinebreak.push_back(pos.second);
			}
		}
		else if (punctPositions.size() < origLinebreakCount) {
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
				if (currentPos < textToModify.length() && std::find(positionsToAddLinebreak.begin(), positionsToAddLinebreak.end(), currentPos) == positionsToAddLinebreak.end()) {
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
			textToModify.insert(pos, "<br>");
		}

	}
	else {
		throw std::runtime_error("未知的换行模式: " + m_mode);
	}

	se->translated_preview = textToModify;
	m_logger->debug("句子[{}]({}行): 修正后译文[{}]({}行)", orgText, origLinebreakCount + 1, se->translated_preview, countSubstring(se->translated_preview, "<br>") + 1);
}

