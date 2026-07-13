#ifndef DICTIONARYREADER_H
#define DICTIONARYREADER_H

#include "NormalDictModel.h"
#include "GptDictModel.h"
#include <QList>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

class DictionaryReader : public QObject
{
	Q_OBJECT

public:
	explicit DictionaryReader(QObject* parent = nullptr);

	static QList<GptDictEntry> readGptDict(const fs::path& dictPath);
	static QList<GptDictEntry> readGptDicts(const std::vector<fs::path>& dictPaths);

	static QList<NormalDictEntry> readNormalDict(const fs::path& dictPath);

	static QString readGptDictsStr(const std::vector<fs::path>& dictPaths);
	static QString readDictStr(const fs::path& dictPath);


};

#endif
