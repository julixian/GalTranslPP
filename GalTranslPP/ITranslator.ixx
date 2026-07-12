export module ITranslator;

export import GPPDefines;

namespace fs = std::filesystem;

export
{
    struct RuntimeTransSuccessEvent {
        std::string timestamp;
        std::string filename;
        int index{0};
        std::vector<std::string> speakers;
        std::string sourcePreview;
        std::string translationPreview;
        std::string translatedBy;
    };

    struct RuntimeTransErrorEvent {
        std::string timestamp;
        std::string kind;
        std::string level{"error"};
        std::string message;
        std::string filename;
        std::string indexRange;
        int requestCount{-1};
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
		std::atomic<int> m_activeThreads{ 0 };
		std::atomic<int> m_totalThreads{ 0 };

		void makeBar(int totalSentences, int totalThreads);

		virtual void writeLog(const std::string& log) = 0;

		void addThreadNum();

		void reduceThreadNum();

		void updateBar(int ticks = 1);

		void setRuntimeFiles(const std::map<std::string, int>& fileTotals);

		void setRuntimeStage(const std::string& stage, const std::string& currentFile = {});

		// 语义解释：SentenceDone 不一定是 TransSuccess，更不一定 Runtime
		void recordFileSentenceDone(const std::string& runtimeFile, bool hasProblem);

		void recordRuntimeTransSuccess(RuntimeTransSuccessEvent event);

		void recordRuntimeTransError(RuntimeTransErrorEvent event);

		virtual bool shouldStop() = 0;

		virtual void flush() = 0;

		IController();

		virtual ~IController();

	protected:
		virtual void onMakeBar(int totalSentences, int totalThreads) {}
		virtual void onAddThreadNum(int activeThreads) {}
		virtual void onReduceThreadNum(int activeThreads) {}
		virtual void onUpdateBar(int ticks, int completedSentences, int totalSentences) {}
		virtual void onRuntimeFilesReset(const std::vector<RuntimeFileProgress>& files) {}
		virtual void onRuntimeStageChanged(const std::string& stage, const std::string& currentFile) {}
		virtual void onRuntimeFileProgress(const RuntimeFileProgress& file) {}
		virtual void onRuntimeTransSuccess(const RuntimeTransSuccessEvent& event) {}
		virtual void onRuntimeTransError(const RuntimeTransErrorEvent& event) {}

	private:
		mutable std::mutex m_runtimeMutex;
		std::map<std::string, RuntimeFileProgress> m_runtimeFiles;
	};

	class ITranslator {

	public:

		virtual void run() = 0;

		ITranslator();

		virtual ~ITranslator();
	};

	std::unique_ptr<ITranslator> createTranslator(const fs::path& projectDir, const std::shared_ptr<IController>& controller);
}
