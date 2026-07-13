#include "DictionaryReader.h"
#include "ElaMessageBar.h"
#include <toml.hpp>

import Tool;


QString DictionaryReader::readDictStr(const fs::path& dictPath)
{
	if (!fs::exists(dictPath)) {
		return {};
	}
	std::ifstream ifs(dictPath, std::ios::binary);
	std::string result((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	return QString::fromStdString(result);
}

QList<GptDictEntry> DictionaryReader::readGptDict(const fs::path& dictPath)
{
	QList<GptDictEntry> result;
	if (!fs::exists(dictPath)) {
		return result;
	}

	if (isSameExtension(dictPath, L".toml")) {
		try {
			const toml::ordered_value tbl = toml::uoparse(dictPath);
			if (!tbl.contains("gptDict") || !tbl.at("gptDict").is_array()) {
				return result;
			}
			const auto& dictArr = tbl.at("gptDict").as_array();
			for (const auto& dict : dictArr) {
				if (!dict.is_table()) {
					continue;
				}
				GptDictEntry entry;
				entry.original = QString::fromStdString(toml::find_or(dict, "org", ""));
				entry.translation = QString::fromStdString(toml::find_or(dict, "rep", ""));
				entry.description = dict.contains("note") ? QString::fromStdString(toml::find_or(dict, "note", "")) : QString{};
				result.push_back(entry);
			}
			return result;
		}
		catch (...) {
			ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"),
				tr("%1 不符合 toml 规范").arg(QString::fromStdWString(dictPath.filename().wstring())), 3000);
			return result;
		}
	}
	else if (isSameExtension(dictPath, L".json")) {
		try {
			const json j = parseJson(dictPath);
			if (!j.is_array()) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"),
					tr("%1 不是预期的 json 格式").arg(QString::fromStdWString(dictPath.filename().wstring())), 3000);
				return result;
			}
			for (const auto& elem : j) {
				if (!elem.is_object()) {
					continue;
				}
				GptDictEntry entry;
				entry.original = QString::fromStdString(elem.value("src", ""));
				entry.translation = QString::fromStdString(elem.value("dst", ""));
				entry.description = QString::fromStdString(elem.value("info", ""));
				result.push_back(entry);
			}
			return result;
		}
		catch (...) {
			ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"),
				tr("%1 不符合 json 规范").arg(QString::fromStdWString(dictPath.filename().wstring())), 3000);
			return result;
		}
	}
	else if (isSameExtension(dictPath, L".txt") || isSameExtension(dictPath, L".tsv")) {
		std::ifstream ifs(dictPath, std::ios::binary);
		std::string line;
		while (std::getline(ifs, line)) {
			if (line.starts_with("//")) {
				continue;
			}
			static constexpr std::array<std::string_view, 2> delimiters = { "\t", "    " };
			const std::vector<std::string_view> tokens = splitTsvLineView(line, delimiters); // GalTransl支持4空格分割
			if (tokens.size() < 2) {
				continue;
			}
			GptDictEntry entry;
			entry.original = QString::fromUtf8(tokens[0]);
			entry.translation = QString::fromUtf8(tokens[1]);
			if (tokens.size() > 2) {
				entry.description = QString::fromUtf8(tokens[2]);
			}
			result.push_back(entry);
		}
		ifs.close();
		return result;
	}
	else {
		ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"),
			tr("%1 不是支持的格式").arg(QString::fromStdWString(dictPath.filename().wstring())), 3000);
		return result;
	}

	return result;
}

QList<GptDictEntry> DictionaryReader::readGptDicts(const std::vector<fs::path>& dictPaths)
{
	QList<GptDictEntry> result;
	for (const auto& dictPath : dictPaths) {
		result.append(readGptDict(dictPath));
	}
	return result;
}

QString DictionaryReader::readGptDictsStr(const std::vector<fs::path>& dictPaths)
{
	toml::ordered_value newDictArr = toml::array{};
	for (const auto& dictPath : dictPaths) {
		if (!isSameExtension(dictPath, L".toml") || !fs::exists(dictPath)) {
			continue;
		}
		try {
			toml::ordered_value tbl = toml::uoparse(dictPath);
			if (!tbl.contains("gptDict") || !tbl.at("gptDict").is_array()) {
				continue;
			}
			const auto& dictArr = tbl.at("gptDict").as_array();
			for (const auto& dict : dictArr) {
				if (!dict.is_table()) {
					continue;
				}
				newDictArr.push_back(dict);
			}
		}
		catch (...) {

		}
	}
	newDictArr.as_array_fmt().fmt = toml::array_format::multiline;
	return QString::fromStdString(toml::format(toml::ordered_value{ toml::ordered_table{{ "gptDict", newDictArr }} }));
}

QList<NormalDictEntry> DictionaryReader::readNormalDict(const fs::path& dictPath)
{
	QList<NormalDictEntry> result;
	if (!fs::exists(dictPath)) {
		return result;
	}

	if (isSameExtension(dictPath, L".toml")) {
		try {
			toml::ordered_value tbl = toml::uoparse(dictPath);
			if (!tbl["normalDict"].is_array()) {
				return result;
			}
			auto dictArr = tbl["normalDict"].as_array();
			for (const auto& dict : dictArr) {
				if (!dict.is_table()) {
					continue;
				}
				NormalDictEntry entry;
				entry.original = QString::fromStdString(toml::find_or(dict, "org", ""));
				entry.translation = QString::fromStdString(toml::find_or(dict, "rep", ""));
				entry.isReg = toml::find_or(dict, "isReg", false);
				entry.priority = toml::find_or(dict, "priority", 0);

				if (dict.contains("conditions") && dict.at("conditions").is_array()) {
					const auto& conditions = dict.at("conditions");
					for (const auto& conditionValue : conditions.as_array()) {
						if (!conditionValue.is_table() || !conditionValue.contains("conditionTarget")
							|| !conditionValue.contains("conditionReg")
							|| !conditionValue.at("conditionTarget").is_string()
							|| !conditionValue.at("conditionReg").is_string()) {
							continue;
						}
						NormalCondition condition;
						QString target = QString::fromStdString(conditionValue.at("conditionTarget").as_string());
						while (target.startsWith("prev_")) {
							--condition.sentenceOffset;
							target = target.mid(5);
						}
						while (target.startsWith("next_")) {
							++condition.sentenceOffset;
							target = target.mid(5);
						}
						if (target != "name" && target != "names" && target != "nametrans" && target != "namestrans"
							&& target != "orig" && target != "preproc" && target != "problems" && target != "otherinfo"
							&& target != "transby" && target != "transraw" && target != "transview") {
							continue;
						}
						const QString pattern = QString::fromStdString(conditionValue.at("conditionReg").as_string());
						if (pattern.isEmpty()) {
							continue;
						}
						condition.target = target;
						condition.pattern = pattern;
						entry.conditions.push_back(std::move(condition));
					}
				}
				result.push_back(entry);
			}
			return result;
		}
		catch (...) {
			ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"),
				tr("%1 不符合 toml 规范").arg(QString::fromStdWString(dictPath.filename().wstring())), 3000);
			return result;
		}
	}
	else if (isSameExtension(dictPath, L".json")) {
		try {
			const json j = parseJson(dictPath);
			if (!j.is_array()) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"),
					tr("%1 不是预期的 json 格式").arg(QString::fromStdWString(dictPath.filename().wstring())), 3000);
				return result;
			}
			for (const auto& elem : j) {
				if (!elem.is_object()) {
					continue;
				}
				NormalDictEntry entry;
				entry.original = QString::fromStdString(elem.value("src", ""));
				entry.translation = QString::fromStdString(elem.value("dst", ""));
				const QString conditionPattern = QString::fromStdString(elem.value("regex", ""));
				entry.isReg = !conditionPattern.isEmpty();
				if (entry.isReg) {
					entry.conditions.push_back({ conditionPattern, "preproc" });
				}
				result.push_back(entry);
			}
			return result;
		}
		catch (...) {
			ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"),
				tr("%1 不符合 json 规范").arg(QString::fromStdWString(dictPath.filename().wstring())), 3000);
			return result;
		}
	}
	else {
		ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"),
			tr("%1 不是支持的格式").arg(QString::fromStdWString(dictPath.filename().wstring())), 3000);
	}

	return result;
}

DictionaryReader::DictionaryReader(QObject* parent) : QObject(parent)
{

}
