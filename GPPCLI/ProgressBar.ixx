export module ProgressBar;

import Tool;

export {
    class ProgressBar {

    public:
        // default destructor
        ~ProgressBar() = default;

        // delete everything else
        ProgressBar(ProgressBar const&) = delete;
        ProgressBar& operator=(ProgressBar const&) = delete;
        ProgressBar(ProgressBar&&) = delete;
        ProgressBar& operator=(ProgressBar&&) = delete;

        // 句子数量 并发线程数量
        inline ProgressBar(int n, int t);

        // chose your style
        inline void set_done_char(const std::string& sym) { done_char = sym; }
        inline void set_todo_char(const std::string& sym) { todo_char = sym; }
        inline void set_opening_bracket_char(const std::string& sym) { opening_bracket_char = sym; }
        inline void set_closing_bracket_char(const std::string& sym) { closing_bracket_char = sym; }

        // main function
        inline void update(int ticks, bool removeCurrentLine);

        void add_thread_num() {
            current_thread_num++;
            update(0, true);
        }

        void reduce_thread_num() {
            current_thread_num--;
            update(0, true);
        }

    private:
        int progress;
        int n_cycles;


        std::string done_char;
        std::string todo_char;
        std::string opening_bracket_char;
        std::string closing_bracket_char;

        int bar_width = 40;
        int total_thread_num;
        int current_thread_num;
        std::chrono::high_resolution_clock::time_point start_time;
    };
}

inline ProgressBar::ProgressBar(int n, int t) :
    progress(0),
    n_cycles(n),
    total_thread_num(t),
    current_thread_num(0),
    done_char("█"),
    todo_char(" "),
    opening_bracket_char("|"),
    closing_bracket_char("|") 
{
    start_time = std::chrono::steady_clock::now();
}

inline void ProgressBar::update(int ticks, bool removeCurrentLine) {

    if (n_cycles == 0) throw std::runtime_error(
        "ProgressBar::update: number of cycles not set");

    int consoleWidth = getConsoleWidth();

    if (removeCurrentLine) {
        std::string spaceFill(consoleWidth, ' ');
        std::cout << "\r" << spaceFill << "\r";
    }

    progress += ticks;
    double perc = progress * 100. / (n_cycles);

    int length_to_fill = (int)(bar_width * perc / 100);
    std::string fill;
    fill.reserve(length_to_fill * done_char.size() + (bar_width - length_to_fill) * todo_char.size());
    for (int i = 0; i < length_to_fill; ++i) {
        fill += done_char;
    }
    for (int i = 0; i < bar_width - length_to_fill; ++i) {
        fill += todo_char;
    }

    double duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count() / 1000.0;

    std::string current_bar = std::format("翻译进度 [{}] {}{}{} {}/{} lines [{:.2f}%] in {:.1f}s ({:.2f} lines/s)",
        progress == n_cycles ? "处理完成" : std::format("{}/{}线程并发", current_thread_num, total_thread_num),
        opening_bracket_char,
        fill,
        closing_bracket_char,
        progress,
        n_cycles,
        perc,
        duration,
        progress / duration);

    std::vector<std::string> graphemes = splitIntoGraphemes(current_bar);
    current_bar.clear();
    for (size_t i = 0, width = 0; i < graphemes.size(); i++) {
        if (graphemes[i] == "█") {
            width += 1;
        }
        else if (graphemes[i].length() > 1) {
            width += 2;
        }
        else {
            width++;
        }
        if (width > consoleWidth) {
            break;
        }
        current_bar += graphemes[i];
    }

    std::cout << current_bar << std::flush;

    return;
}