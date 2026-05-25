export module ITranslator;

import std;

namespace fs = std::filesystem;

export
{
    struct RuntimeSuccessEvent {
        std::string id;
        std::string timestamp;
        std::string filename;
        int index{0};
        std::vector<std::string> speakers;
        std::string sourcePreview;
        std::string translationPreview;
        std::string translatedBy;
    };

    struct RuntimeErrorEvent {
        std::string id;
        std::string timestamp;
        std::string kind;
        std::string level{"error"};
        std::string message;
        std::string filename;
        std::string indexRange;
        int retryCount{-1};
        std::string model;
        double sleepSeconds{-1.0};
    };

    struct RuntimeFileProgress {
        std::string filename;
        int total{0};
        int completed{0};
        int problems{0};
    };

	class IController {
	public:

		std::atomic<int> m_totalSentences{ 0 };
		std::atomic<int> m_completedSentences{ 0 };
		std::atomic<int> m_workersActive{ 0 };
		std::atomic<int> m_workersConfigured{ 0 };

		void makeBar(int totalSentences, int totalThreads);

		virtual void writeLog(const std::string& log) = 0;

		void addThreadNum();

		void reduceThreadNum();

		void updateBar(int ticks = 1);

		void setRuntimeFiles(const std::map<std::string, int>& fileTotals);

		void setRuntimeStage(const std::string& stage, const std::string& currentFile = {});

		void recordFileSentenceDone(const std::string& runtimeFile, bool hasProblem);

		void recordRuntimeSuccess(RuntimeSuccessEvent event);

		void recordRuntimeError(RuntimeErrorEvent event);

		virtual bool shouldStop() = 0;

		virtual void flush() = 0;

		IController();

		virtual ~IController();

	protected:
		virtual void onMakeBar(int totalSentences, int totalThreads) {}
		virtual void onAddThreadNum(int workersActive) {}
		virtual void onReduceThreadNum(int workersActive) {}
		virtual void onUpdateBar(int ticks, int completedSentences, int totalSentences) {}
		virtual void onRuntimeFilesReset(const std::vector<RuntimeFileProgress>& files) {}
		virtual void onRuntimeStageChanged(const std::string& stage, const std::string& currentFile) {}
		virtual void onRuntimeFileProgress(const RuntimeFileProgress& file) {}
		virtual void onRuntimeSuccess(const RuntimeSuccessEvent& event) {}
		virtual void onRuntimeError(const RuntimeErrorEvent& event) {}

	private:
		mutable std::mutex m_runtimeMutex;
		std::map<std::string, RuntimeFileProgress> m_runtimeFiles;
		std::string m_runtimeStage;
		std::string m_runtimeCurrentFile;
		std::atomic<unsigned long long> m_runtimeEventCounter{ 0 };
	};

	class ITranslator {

	public:

		virtual void run() = 0;

		ITranslator();

		virtual ~ITranslator();
	};

	std::unique_ptr<ITranslator> createTranslator(const fs::path& projectDir, const std::shared_ptr<IController>& controller);
}
