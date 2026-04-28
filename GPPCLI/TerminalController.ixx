export module TerminalController;

import Tool;
import ProgressBar;
export import ITranslator;

export {

    class TerminalController : public IController {
    public:
        
        virtual void writeLog(const std::string& log) override {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_log += log;
        }

        virtual bool shouldStop() override
        {
            return false;
        }

        virtual void flush() override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_log.empty()) {
                if (m_bar) {
                    std::cout << "\r" << std::string(getConsoleWidth(), ' ') << "\r";
                }
                std::cout << m_log;
                if (m_bar) {
                    m_bar->update(0, false);
                }
                m_log.clear();
            }
            if (m_bar) {
                m_bar->update(m_progress, true);
                m_progress = 0;
            }
        }

    protected:
        virtual void onMakeBar(int totalSentences, int totalThreads) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_bar = std::make_unique<ProgressBar>(totalSentences, totalThreads);
            m_bar->update(0, false);
        }

        virtual void onAddThreadNum(int) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_bar) {
                throw std::runtime_error("ProgressBar not created");
            }
            m_bar->add_thread_num();
        }

        virtual void onReduceThreadNum(int) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_bar) {
                throw std::runtime_error("ProgressBar not created");
            }
            m_bar->reduce_thread_num();
        }

        virtual void onUpdateBar(int ticks, int, int) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_progress += ticks;
        }

    public:
        TerminalController()
        {
            m_log.reserve(1024 * 1024);
            m_flushThread = std::thread([this]()
                {
                    while (m_controlling.load()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                        flush();
                    }
                });
        }

        virtual ~TerminalController() override
        {
            m_controlling = false;
            if (m_flushThread.joinable()) {
                m_flushThread.join();
            }
        }

    private:
        std::mutex m_mutex;
        std::unique_ptr<ProgressBar> m_bar;
        std::atomic<bool> m_controlling = true;

        std::string m_log;
        int m_progress = 0;
        std::thread m_flushThread;
    };
}
