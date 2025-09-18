export module ITranslator;

import std;

namespace fs = std::filesystem;

export {

	class IController {
	public:

		virtual void makeBar(int totalSentences, int totalThreads) = 0;

		virtual void writeLog(const std::string& log) = 0;

		virtual void addThreadNum() = 0;

		virtual void reduceThreadNum() = 0;

		virtual void updateBar(int ticks = 1) = 0;

		virtual bool shouldStop() = 0;

		IController();

		virtual ~IController();
	};

	class ITranslator {

	public:

		virtual void run() = 0;

		ITranslator();

		virtual ~ITranslator();
	};

	std::unique_ptr<ITranslator> createTranslator(const fs::path& projectDir, std::shared_ptr<IController> controller);
}