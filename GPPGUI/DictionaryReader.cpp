#include "DictionaryReader.h"
#include "ElaMessageBar.h"

import Tool;

using namespace gpp;

QString DictionaryReader::readDictStr(const fs::path& dictPath)
{
	if (!fs::exists(dictPath)) {
		return {};
	}
	std::ifstream ifs(dictPath, std::ios::binary);
	std::string result((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	return QString::fromStdString(result);
}

QList<GuiGptDictEntry> DictionaryReader::readGptDict(const fs::path& dictPath)
{
	QList<GuiGptDictEntry> result;
	if (!fs::exists(dictPath)) {
		return result;
	}

	if (isSameExtension(dictPath, L".toml")) {
		try {
			const toml tbl = parseToml(dictPath);
			if (!tbl.contains("gptDict") || !tbl.at("gptDict").isArray()) {
				return result;
			}
			const auto& dictArr = tbl.at("gptDict");
			for (const auto& dict : dictArr) {
				if (!dict.isTable()) {
					continue;
				}
				GuiGptDictEntry entry;
				entry.original = QString::fromStdString(tomlkit::findOr(dict, "org", ""));
				entry.translation = QString::fromStdString(tomlkit::findOr(dict, "rep", ""));
				entry.description = dict.contains("note") ? QString::fromStdString(tomlkit::findOr(dict, "note", "")) : QString{};
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
				GuiGptDictEntry entry;
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
			GuiGptDictEntry entry;
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

QList<GuiGptDictEntry> DictionaryReader::readGptDicts(const std::vector<fs::path>& dictPaths)
{
	QList<GuiGptDictEntry> result;
	for (const auto& dictPath : dictPaths) {
		result.append(readGptDict(dictPath));
	}
	return result;
}

QString DictionaryReader::readGptDictsStr(const std::vector<fs::path>& dictPaths)
{
	tomlkit::Item newDictArr = tomlkit::Item::array();
	for (const auto& dictPath : dictPaths) {
		if (!isSameExtension(dictPath, L".toml") || !fs::exists(dictPath)) {
			continue;
		}
		try {
			toml tbl = parseToml(dictPath);
			if (!tbl.contains("gptDict") || !tbl.at("gptDict").isArray()) {
				continue;
			}
			const auto& dictArr = tbl.at("gptDict");
			for (const auto& dict : dictArr) {
				if (!dict.isTable()) {
					continue;
				}
				if (dict.isInlineTable()) {
					newDictArr.push_back(dict);
				}
				else {
					tomlkit::Item inlineDict = tomlkit::Item::inlineTable();
					for (const auto [k, v] : dict.tableItems()) {
						inlineDict[k.name()] = v;
					}
					newDictArr.push_back(std::move(inlineDict));
				}
			}
		}
		catch (...) {

		}
	}
	newDictArr.setMultilineArray(true);
	toml doc = toml::document();
	doc["gptDict"] = std::move(newDictArr);
	return QString::fromStdString(tomlkit::format(doc));
}

QList<GuiNormalDictEntry> DictionaryReader::readNormalDict(const fs::path& dictPath)
{
	QList<GuiNormalDictEntry> result;
	if (!fs::exists(dictPath)) {
		return result;
	}

	if (isSameExtension(dictPath, L".toml")) {
		try {
			const toml tbl = parseToml(dictPath);
			if (!tbl.contains("normalDict") || !tbl.at("normalDict").isArray()) {
				return result;
			}
			const auto& dictArr = tbl.at("normalDict");
			for (const auto& dict : dictArr) {
				if (!dict.isTable()) {
					continue;
				}
				GuiNormalDictEntry entry;
				entry.original = QString::fromStdString(tomlkit::findOr(dict, "org", ""));
				entry.translation = QString::fromStdString(tomlkit::findOr(dict, "rep", ""));
				entry.isReg = tomlkit::findOr(dict, "isReg", false);
				entry.priority = tomlkit::findOr(dict, "priority", 0);

				if (dict.contains("conditions") && dict.at("conditions").isArray()) {
					const auto& conditions = dict.at("conditions");
					for (const auto& conditionValue : conditions) {
						if (!conditionValue.isTable() || !conditionValue.contains("conditionTarget")
							|| !conditionValue.contains("conditionReg")
							|| !conditionValue.at("conditionTarget").isString()
							|| !conditionValue.at("conditionReg").isString()) {
							continue;
						}
						NormalCondition condition;
						QString target = QString::fromStdString(conditionValue.at("conditionTarget").asString());
						while (target.startsWith("prev_")) {
							--condition.sentenceOffset;
							target = target.mid(5);
						}
						while (target.startsWith("next_")) {
							++condition.sentenceOffset;
							target = target.mid(5);
						}
						if (target != "index" && target != "filename"
							&& target != "name" && target != "names" && target != "nametrans" && target != "namestrans"
							&& target != "orig" && target != "preproc" && target != "problems" && target != "otherinfo"
							&& target != "transby" && target != "transraw" && target != "transview") {
							continue;
						}
						const QString pattern = QString::fromStdString(conditionValue.at("conditionReg").asString());
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
				GuiNormalDictEntry entry;
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
