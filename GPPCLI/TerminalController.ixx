module;

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

import std;
import Tool;
import ProgressBar;
export import ITranslator;
export module TerminalController;

export {

    class TerminalController : public IController {
    public:
        
        virtual void makeBar(int totalSentences, int totalThreads) override {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_bar = std::make_unique<progressbar>(totalSentences, totalThreads);
            m_bar->update(0, false);
        }

        virtual void writeLog(const std::string& log) override {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_bar) {
                std::cout << "\r" << std::string(getConsoleWidth(), ' ') << "\r";
            }
            std::cout << log;
            if (m_bar) {
                m_bar->update(0, false);
            }
        }

        virtual void addThreadNum() override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_bar) {
                throw std::runtime_error("ProgressBar not created");
            }
            m_bar->add_thread_num();
        }

        virtual void reduceThreadNum() override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_bar) {
                throw std::runtime_error("ProgressBar not created");
            }
            m_bar->reduce_thread_num();
        }

        virtual void updateBar(int ticks) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_bar->update(ticks, true);
        }

        virtual bool shouldStop() override
        {
            return false;
        }

    private:
        std::mutex m_mutex;
        std::unique_ptr<progressbar> m_bar;
    };
}