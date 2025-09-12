module;

#include <mecab/mecab.h>
#include <spdlog/spdlog.h>
#include <toml++/toml.hpp>
#include <unicode/regex.h>
#include <unicode/unistr.h>

export module Dictionary;
import Tool;
namespace fs = std::filesystem;

export {

    enum class CachePart { Name, OrigText, PreproText, PretransText, TransPreview };

    struct GptTabEntry {
        int priority = 0;
        std::string searchStr;
        std::string replaceStr;
        std::string note;
    };

    class GptDictionary {
    private:
        std::vector<GptTabEntry> m_entries;

        std::unique_ptr<MeCab::Model> m_model;
        std::unique_ptr<MeCab::Tagger> m_tagger;
        std::shared_ptr<spdlog::logger> m_logger;

    public:
        void setLogger(std::shared_ptr<spdlog::logger> logger) {
            m_logger = logger;
        }

        void sort();

        void createTagger(const fs::path& dictDir);

        void loadFromFile(const fs::path& filePath);

        std::string generatePrompt(const std::vector<Sentence*>& batch, TransEngine transEngine);

        std::string doReplace(const Sentence* se, CachePart targetToModify);

        std::string checkDicUse(const Sentence* sentence);
    };


    struct DictEntry {
        int priority = 0;

        std::string searchStr;
        std::string replaceStr;
        bool isReg = false;
        std::shared_ptr<icu::RegexPattern> searchReg;

        // 条件字典相关
        bool isConditional = false;
        std::shared_ptr<icu::RegexPattern> conditionReg;
        CachePart conditionTarget;
    };

    class NormalDictionary {
    private:
        std::vector<DictEntry> m_entries;
        std::shared_ptr<spdlog::logger> m_logger;

    public:
        void setLogger(std::shared_ptr<spdlog::logger> logger) {
            m_logger = logger;
        }

        void loadFromFile(const fs::path& filePath);

        void sort();

        std::string doReplace(const Sentence* sentence, CachePart targetToModify);
    };
}


module :private;

std::string chooseString(const Sentence* sentence, CachePart tar) {
    switch (tar) {
    case CachePart::Name:
        return sentence->name;
        break;
    case CachePart::OrigText:
        return sentence->original_text;
        break;
    case CachePart::PreproText:
        return sentence->pre_processed_text;
        break;
    case CachePart::PretransText:
        return sentence->pre_translated_text;
        break;
    case CachePart::TransPreview:
        return sentence->translated_preview;
        break;
    default:
        throw std::runtime_error("Invalid condition target");
    }
    return {};
}

void GptDictionary::sort() {
    std::ranges::sort(m_entries, [](const GptTabEntry& a, const GptTabEntry& b)
        {
            if (a.priority != b.priority) {
                return a.priority > b.priority;
            }
            return a.searchStr.length() > b.searchStr.length();
        });
}

void GptDictionary::createTagger(const fs::path& dictDir) {
    m_model.reset(
        MeCab::Model::create(("-r BaseConfig/DictGenerator/mecabrc -d " + wide2Ascii(dictDir, 0)).c_str())
    );
    if (!m_model) {
        throw std::runtime_error("无法初始化 MeCab Model。请确保 BaseConfig/DictGenerator/mecabrc 和 " + wide2Ascii(dictDir) + " 存在\n"
            "错误信息: " + MeCab::getLastError());
    }
    m_tagger.reset(m_model->createTagger());
    if (!m_tagger) {
        throw std::runtime_error("无法初始化 MeCab Tagger。请确保 BaseConfig/DictGenerator/mecabrc 和 " + wide2Ascii(dictDir) + " 存在\n"
            "错误信息: " + MeCab::getLastError());
    }
}

std::string GptDictionary::checkDicUse(const Sentence* sentence) {
    if (!m_tagger) {
        throw std::runtime_error("MeCab Tagger 未初始化，无法进行 GPT 字典检查");
    }
    std::vector<std::string> problems;
    for (const auto& entry : m_entries) {
        // 如果原文中不包含这个词，就跳过检查
        if (sentence->pre_processed_text.find(entry.searchStr) == std::string::npos) {
            continue;
        }
        // 检查译文中是否使用了对应的词
        auto replaceWords = splitString(entry.replaceStr, '/');
        bool found = false;
        for (const auto& word : replaceWords) {
            if (sentence->translated_preview.find(word) != std::string::npos || sentence->pre_translated_text.find(word)!= std::string::npos) {
                found = true;
                break;
            }
        }
        if (found) {
            // 出现了则默认使用了字典
            continue;
        }
        else if (entry.searchStr.length() > 15) {
            // 如果字典长度大于 5 个汉字字符，则默认认为是字典未正确使用的情况
            problems.push_back("GPT字典 " + entry.searchStr + "->" + entry.replaceStr + " 未使用");
            continue;
        }
        // 未出现则分词检查原文中是否有完整的 searchStr 词组
        std::unique_ptr<MeCab::Lattice> lattice(m_model->createLattice());
        lattice->set_sentence(sentence->pre_processed_text.c_str());
        if (!m_tagger->parse(lattice.get())) {
            throw std::runtime_error(std::format("分词器解析失败，错误信息: {}", MeCab::getLastError()));
        }
        
        for (const MeCab::Node* node = lattice->bos_node(); node; node = node->next) {
            if (node->stat == MECAB_BOS_NODE || node->stat == MECAB_EOS_NODE) continue;

            std::string surface(node->surface, node->length);
            std::string feature = node->feature;

            m_logger->trace("分词结果：{} ({})", surface, feature);

            if (surface == entry.searchStr) {
                found = true;
                break;
            }
        }

        if (found) {
            // 如果原文有完整的 searchStr 词组且译文中没有使用对应的词，几乎可以肯定是字典未正确使用的情况
            problems.push_back("GPT字典 " + entry.searchStr + "->" + entry.replaceStr + " 未使用");
        }
        
    }
    // 将所有问题拼接成一个字符串
    std::string result;
    for (size_t i = 0; i < problems.size(); ++i) {
        result += problems[i];
        if (i < problems.size() - 1) {
            result += ", ";
        }
    }
    return result;
}

void GptDictionary::loadFromFile(const fs::path& filePath) {
    if (!fs::exists(filePath)) {
        m_logger->warn("GPT 字典文件不存在: {}", wide2Ascii(filePath));
        return;
    }

    std::ifstream ifs(filePath);
    int count = 0;

    try {
        auto dictData = toml::parse(ifs);
        auto dicts = dictData["gptDict"].as_array();
        if (!dicts) {
            m_logger->info("已加载 GPT 字典: {}, 共 {} 个词条", wide2Ascii(filePath.filename()), count);
            return;
        }
        dicts->for_each([&](auto&& el)
            {
                GptTabEntry entry;
                if constexpr (toml::is_table<decltype(el)>) {
                    if (!el.contains("searchStr") && !el.contains("org")) {
                        throw std::invalid_argument(std::format("GPT 字典文件格式错误(未找到searchStr|org): {}", wide2Ascii(filePath)));
                    }
                    if (!el.contains("replaceStr") && !el.contains("rep")) {
                        throw std::invalid_argument(std::format("GPT 字典文件格式错误(未找到replaceStr|rep): {}", wide2Ascii(filePath)));
                    }

                    entry.searchStr = el.contains("org") ? el["org"].value_or("") : el["searchStr"].value_or("");
                    if (entry.searchStr.empty()) {
                        return;
                    }
                    entry.replaceStr = el.contains("rep") ? el["rep"].value_or("") : el["replaceStr"].value_or("");
                    if (el.contains("note")) {
                        entry.note = el["note"].value_or("");
                    }
                    entry.priority = el["priority"].value_or(0);
                    m_entries.push_back(entry);
                    count++;
                }
                else {
                    throw std::invalid_argument(std::format("GPT 字典文件格式错误(not a table): {}", wide2Ascii(filePath)));
                }
            });
    }
    catch (const toml::parse_error& e) {
        m_logger->error("GPT 字典文件解析错误: {}, 错误位置: {}, 错误信息: {}", wide2Ascii(filePath), stream2String(e.source().begin), e.description());
        throw std::runtime_error(e.what());
    }
    
    m_logger->info("已加载 GPT 字典: {}, 共 {} 个词条", wide2Ascii(filePath.filename()), count);
}

std::string GptDictionary::doReplace(const Sentence* se, CachePart targetToModify) {
    std::string textToModify = chooseString(se, targetToModify);

    if (textToModify.empty()) {
        return textToModify;
    }

    for (const auto& entry : m_entries) {
        replaceStrInplace(textToModify, entry.searchStr, entry.replaceStr);
    }

    return textToModify;
}

std::string GptDictionary::generatePrompt(const std::vector<Sentence*>& batch, TransEngine transEngine) {
    std::string batchText;
    for (const auto& s : batch) {
        batchText += s->name + ":" + s->pre_processed_text + "\n";
    }

    std::string promptContent;
    for (const auto& entry : m_entries) {
        if (batchText.find(entry.searchStr) != std::string::npos) {
            // *** 根据 transEngine 选择格式 ***
            switch (transEngine) {
            case TransEngine::ForGalJson:
            case TransEngine::DeepseekJson:
                promptContent += "| " + entry.searchStr + " | " + entry.replaceStr + " |";
                if (!entry.note.empty()) {
                    promptContent += " " + entry.note;
                }
                promptContent += " |\n";
                break;

            case TransEngine::ForGalTsv:
            case TransEngine::ForNovelTsv:
                promptContent += entry.searchStr + "\t" + entry.replaceStr;
                if (!entry.note.empty()) {
                    promptContent += "\t" + entry.note;
                }
                promptContent += "\n";
                break;

            case TransEngine::Sakura:
                promptContent += entry.searchStr + "->" + entry.replaceStr;
                if (!entry.note.empty()) {
                    promptContent += " #" + entry.note;
                }
                promptContent += "\n";
                break;

            default:
                throw std::runtime_error("Invalid prompt type");
            }
        }
    }

    if (promptContent.empty()) {
        return {};
    }

    // *** 根据 transEngine 添加标题 ***
    switch (transEngine) {
    case TransEngine::ForGalJson:
    case TransEngine::DeepseekJson:
        return "# Glossary\n| Src | Dst(/Dst2/..) | Note |\n| --- | --- | --- |\n" + promptContent;

    case TransEngine::ForGalTsv:
    case TransEngine::ForNovelTsv:
        return "SRC\tDST\tNOTE\n" + promptContent;

    case TransEngine::Sakura:
        return promptContent; // Sakura 模式没有标题

    default:
        throw std::runtime_error("Invalid prompt type");
    }

    return {};
}




void NormalDictionary::loadFromFile(const fs::path& filePath) {
    if (!fs::exists(filePath)) {
        m_logger->warn("字典文件不存在: {}", wide2Ascii(filePath));
        return;
    }

    int count = 0;
    std::ifstream ifs(filePath);
    try {
        auto dictData = toml::parse(ifs);
        auto dicts = dictData["normalDict"].as_array();
        if (!dicts) {
            m_logger->info("已加载 Normal 字典: {}, 共 {} 个词条", wide2Ascii(filePath.filename()), count);
            return;
        }
        dicts->for_each([&](auto&& el)
            {
                if constexpr (toml::is_table<decltype(el)>) {
                    DictEntry entry;
                    if (!el.contains("searchStr") && !el.contains("org")) {
                        throw std::invalid_argument(std::format("Normal 字典文件格式错误(未找到searchStr|org): {}", wide2Ascii(filePath)));
                    }
                    if (!el.contains("replaceStr") && !el.contains("rep")) {
                        throw std::invalid_argument(std::format("Normal 字典文件格式错误(未找到replaceStr|rep): {}", wide2Ascii(filePath)));
                    }
                    entry.isReg = el["isReg"].value_or(false);

                    std::string str = el.contains("org") ? el["org"].value_or("") : el["searchStr"].value_or("");
                    if (str.empty()) {
                        return;
                    }

                    UErrorCode status = U_ZERO_ERROR;
                    if (entry.isReg) {
                        icu::UnicodeString ustr(icu::UnicodeString::fromUTF8(str));
                        entry.searchReg.reset(icu::RegexPattern::compile(ustr, 0, status));
                        if (U_FAILURE(status)) {
                            throw std::runtime_error(std::format("Normal 字典文件格式错误(正则表达式错误): {}  ——  {}", wide2Ascii(filePath), str));
                        }
                    }
                    else {
                        entry.searchStr = str;
                    }
                    
                    entry.replaceStr = el.contains("rep") ? el["rep"].value_or("") : el["replaceStr"].value_or("");
                    entry.priority = el["priority"].value_or(0);
                    entry.isConditional = !(el["conditionTarget"].value_or(std::string{}).empty()) && !(el["conditionReg"].value_or(std::string{}).empty());

                    if (!entry.isConditional) {
                        m_entries.push_back(entry);
                        count++;
                        return;
                    }

                    std::string conditionTarget = el["conditionTarget"].value_or("");
                    if (conditionTarget == "name") entry.conditionTarget = CachePart::Name;
                    else if (conditionTarget == "orig_text") entry.conditionTarget = CachePart::OrigText;
                    else if (conditionTarget == "preproc_text") entry.conditionTarget = CachePart::PreproText;
                    else if (conditionTarget == "pretrans_text") entry.conditionTarget = CachePart::PretransText;
                    else if (conditionTarget == "trans_preview") entry.conditionTarget = CachePart::TransPreview;
                    else {
                        throw std::invalid_argument(std::format("Normal 字典文件格式错误(conditionTarget 无效): {}  ——  {}",
                            wide2Ascii(filePath), conditionTarget));
                    }
                    
                    str = el["conditionReg"].value_or("");
                    icu::UnicodeString ustr(icu::UnicodeString::fromUTF8(str));
                    entry.conditionReg.reset(icu::RegexPattern::compile(ustr, 0, status));
                    if (U_FAILURE(status)) {
                        throw std::runtime_error(std::format("Normal 字典文件格式错误(conditionReg 正则表达式错误): {}  ——  {}",
                            wide2Ascii(filePath), str));
                    }

                    m_entries.push_back(entry);
                    count++;
                }
            });
    }
    catch (const toml::parse_error& e) {
        m_logger->error("Normal 字典文件解析错误: {}, 错误位置: {}, 错误信息: {}", wide2Ascii(filePath), stream2String(e.source().begin), e.description());
        throw std::runtime_error(e.what());
    }
    
    m_logger->info("已加载 Normal 字典: {}, 共 {} 个词条", wide2Ascii(filePath.filename()), count);
}

void NormalDictionary::sort() {
    std::ranges::sort(m_entries, [](const DictEntry& a, const DictEntry& b) 
        {
            if (a.priority != b.priority) {
                return a.priority > b.priority;
            }
            if (a.isReg && !b.isReg) {
                return true;
            }
            else if (!a.isReg && b.isReg) {
                return false;
            }
            return a.searchStr.length() > b.searchStr.length();
        });
}

std::string NormalDictionary::doReplace(const Sentence* sentence, CachePart targetToModify) {
    std::string textToModify = chooseString(sentence, targetToModify);

    if (textToModify.empty()) {
        return textToModify;
    }

    for (const auto& entry : m_entries) {
        bool canReplace = false;
        if (!entry.isConditional) {
            canReplace = true;
        }
        else {
            icu::UnicodeString textToInspect = icu::UnicodeString::fromUTF8(chooseString(sentence, entry.conditionTarget));
            UErrorCode status = U_ZERO_ERROR;
            std::unique_ptr<icu::RegexMatcher> matcher(entry.conditionReg->matcher(textToInspect, status));
            if (!U_FAILURE(status)) {
                canReplace = matcher->find();
            }
            else {
                m_logger->error("正则表达式创建matcher失败: {}, 句子: [{}]", u_errorName(status), textToModify);
            }
        }

        if (canReplace) {
            if (entry.isReg) {
                icu::UnicodeString ustr(icu::UnicodeString::fromUTF8(textToModify));
                UErrorCode status = U_ZERO_ERROR;
                std::unique_ptr<icu::RegexMatcher> matcher(entry.searchReg->matcher(ustr, status));
                if (U_FAILURE(status)) {
                    m_logger->error("正则表达式创建matcher失败: {}, 句子: [{}]", u_errorName(status), textToModify);
                    return textToModify;
                }
                icu::UnicodeString result = matcher->replaceAll(icu::UnicodeString::fromUTF8(entry.replaceStr), status);
                std::string str;
                textToModify = result.toUTF8String(str);
            }
            else {
                replaceStrInplace(textToModify, entry.searchStr, entry.replaceStr);
            }
        }
    }

    return textToModify;
}
