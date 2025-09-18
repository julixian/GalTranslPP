module;

#include <Windows.h>
#include <spdlog/spdlog.h>
#include <boost/regex.hpp>
#include <cpr/cpr.h>
#include <zip.h>
#include <unicode/unistr.h>
#include <unicode/uchar.h>
#include <unicode/brkiter.h>
#include <unicode/schriter.h>
#include <unicode/uscript.h>

export module Tool;

import <nlohmann/json.hpp>;
import <toml++/toml.hpp>;
export import std;
import ITranslator;

using json = nlohmann::json;
namespace fs = std::filesystem;

export {

    std::string GPPVERSION = "1.0.0";

    fs::path pluginConfigsPath = L"BaseConfig/pluginConfigs";

    std::string wide2Ascii(const std::wstring& wide, UINT CodePage = 65001, LPBOOL usedDefaultChar = nullptr) {
        int len = WideCharToMultiByte
        (CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, usedDefaultChar);
        if (len == 0) return {};
        std::string ascii(len, '\0');
        WideCharToMultiByte
        (CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
        ascii.pop_back();
        return ascii;
    }

    std::wstring ascii2Wide(const std::string& ascii, UINT CodePage = 65001) {
        int len = MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, nullptr, 0);
        if (len == 0) return {};
        std::wstring wide(len, L'\0');
        MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, &wide[0], len);
        wide.pop_back();
        return wide;
    }

    std::string ascii2Ascii(const std::string& ascii, UINT src, UINT dst, LPBOOL usedDefaultChar = nullptr) {
        return wide2Ascii(ascii2Wide(ascii, src), dst, usedDefaultChar);
    }

    void createParent(const fs::path& path) {
        if (path.has_parent_path()) {
            fs::create_directories(path.parent_path());
        }
    }

    std::wstring wstr2Lower(const std::wstring& wstr) {
        std::wstring result = wstr;
        std::transform(result.begin(), result.end(), result.begin(), [](wchar_t wc) { return std::tolower(wc); });
        return result;
    }

    std::vector<std::string> splitString(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    std::vector<std::string> splitString(const std::string& s, const std::string& delimiter) {
        std::vector<std::string> parts;
        for (const auto& part_view : std::views::split(s, delimiter)) {
            parts.emplace_back(part_view.begin(), part_view.end());
        }
        return parts;
    }

    int getConsoleWidth() {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h == INVALID_HANDLE_VALUE) {
            return 80;
        }
        // 获取控制台屏幕缓冲区信息
        if (GetConsoleScreenBufferInfo(h, &csbi)) {
            // 宽度 = 右坐标 - 左坐标 + 1
            return csbi.srWindow.Right - csbi.srWindow.Left + 1;
        }
        return 80; // 获取失败，返回默认值
    }

    struct Sentence {
        int index;
        std::string name;
        std::string name_preview;
        std::string original_text;
        std::string pre_processed_text;
        std::string pre_translated_text;
        std::string problem;
        std::string translated_by;
        std::string translated_preview;
        std::map<std::string, std::string> other_info;

        bool complete = false;
        bool hasName = false;
        Sentence* prev = nullptr;
        Sentence* next = nullptr;
        std::string originalLinebreak;
    };

    struct TranslationAPI {
        std::string apikey;
        std::string apiurl;
        std::string modelName;
        std::chrono::steady_clock::time_point lastReportTime;
        int reportCount = 0;
        bool stream = false;
    };

    enum  class TransEngine
    {
        ForGalJson, ForGalTsv, ForNovelTsv, DeepseekJson, Sakura, DumpName, GenDict, Rebuild, ShowNormal
    };

    struct ApiResponse {
        bool success = false;
        std::string content; // 成功时的内容 或 失败时的错误信息
        long statusCode = 0;   // HTTP 状态码
    };

    /**
    * @brief 根据句子的上下文生成唯一的缓存键，复刻 GalTransl 逻辑
    */
    std::string generateCacheKey(const Sentence* s) {
        std::string prev_text = "None";
        const Sentence* temp = s->prev;
        if (temp) {
            prev_text = temp->name + temp->original_text + temp->pre_processed_text;
        }

        std::string current_text = s->name + s->original_text + s->pre_processed_text;

        std::string next_text = "None";
        temp = s->next;
        if (temp) {
            next_text = temp->name + temp->original_text + temp->pre_processed_text;
        }

        return prev_text + current_text + next_text;
    }

    /**
    * @brief 构建用于 Prompt 的上下文历史
    */
    std::string buildContextHistory(const std::vector<Sentence*>& batch, TransEngine transEngine, int contextHistorySize) {
        if (batch.empty() || !batch[0]->prev) {
            return {};
        }

        std::string history;

        switch (transEngine) {
        case TransEngine::ForGalTsv: {
            history = "NAME\tDST\tID\n"; // Or DST\tID for novel
            std::vector<std::string> contextLines;
            const Sentence* current = batch[0]->prev;
            int count = 0;
            while (current && count < contextHistorySize) {
                if (current->complete) {
                    std::string name = current->name.empty() ? "null" : current->name;
                    contextLines.push_back(name + "\t" + current->pre_translated_text + "\t" + std::to_string(current->index));
                    count++;
                }
                current = current->prev;
            }
            std::reverse(contextLines.begin(), contextLines.end());
            if (contextLines.empty()) return {};
            for (const auto& line : contextLines) {
                history += line + "\n";
            }
            break;
        }
        case TransEngine::ForNovelTsv: {
            history = "DST\tID\n"; // Or DST\tID for novel
            std::vector<std::string> contextLines;
            const Sentence* current = batch[0]->prev;
            int count = 0;
            while (current && count < contextHistorySize) {
                if (current->complete) {
                    contextLines.push_back(current->pre_translated_text + "\t" + std::to_string(current->index));
                    count++;
                }
                current = current->prev;
            }
            std::reverse(contextLines.begin(), contextLines.end());
            if (contextLines.empty()) return {};
            for (const auto& line : contextLines) {
                history += line + "\n";
            }
            break;
        }

        case TransEngine::ForGalJson:
        case TransEngine::DeepseekJson:
        {
            json historyJson = json::array();
            const Sentence* current = batch[0]->prev;
            int count = 0;
            while (current && count < contextHistorySize) {
                if (current->complete) { // current->complete
                    json item;
                    item["id"] = current->index;
                    if (!current->name.empty()) item["name"] = current->name;
                    item["dst"] = current->pre_translated_text;
                    historyJson.push_back(item);
                    count++;
                }
                current = current->prev;
            }
            std::reverse(historyJson.begin(), historyJson.end());
            for (const auto& item : historyJson) {
                history += item.dump() + "\n";
            }
            history = "```jsonline\n" + history + "```";
        }
        break;

        case TransEngine::Sakura:
        {
            const Sentence* current = batch[0]->prev;
            int count = 0;
            std::vector<std::string> contextLines;
            while (current && count < contextHistorySize) {
                if (current->complete) {
                    if (!current->name.empty()) {
                        contextLines.push_back(current->name + ":::::" + current->pre_translated_text); // :::::
                    }
                    else {
                        contextLines.push_back(current->pre_translated_text);
                    }
                    count++;
                }
                current = current->prev;
            }
            std::reverse(contextLines.begin(), contextLines.end());
            for (const auto& line : contextLines) {
                history += line + "\n";
            }
        }
        break;
        default:
            throw std::runtime_error("未知的 PromptType");
        }

        return history;
    }

    void combineOutputFiles(const fs::path& originalRelFilePath, const std::map<fs::path, bool>& splitFileParts,
        std::shared_ptr<spdlog::logger> logger, const fs::path& outputCacheDir, const fs::path& outputDir) {

        json combinedJson = json::array();

        std::ifstream ifs;
        logger->debug("开始合并文件: {}", wide2Ascii(originalRelFilePath));

        std::vector<fs::path> partPaths;
        for (const auto& [relPartPath, ready] : splitFileParts) {
            partPaths.push_back(relPartPath);
        }

        std::ranges::sort(partPaths, [&](const fs::path& a, const fs::path& b)
            {
                size_t posA = a.filename().wstring().rfind(L"_part_");
                size_t posB = b.filename().wstring().rfind(L"_part_");
                if (posA == std::wstring::npos || posB == std::wstring::npos) {
                    return false;
                }
                std::wstring numA = a.filename().wstring().substr(posA + 6, a.filename().wstring().length() - posA - 11);
                std::wstring numB = b.filename().wstring().substr(posB + 6, b.filename().wstring().length() - posB - 11);
                return std::stoi(numA) < std::stoi(numB);
            });

        for (const auto& relPartPath : partPaths) {
            fs::path partPath = outputCacheDir / relPartPath;
            if (fs::exists(partPath)) {
                try {
                    ifs.open(partPath);
                    json partData = json::parse(ifs);
                    ifs.close();
                    combinedJson.insert(combinedJson.end(), partData.begin(), partData.end());
                }
                catch (const json::exception& e) {
                    ifs.close();
                    logger->critical("合并文件 {} 时出错", wide2Ascii(partPath));
                    throw std::runtime_error(e.what());
                }
            }
            else {
                throw std::runtime_error(std::format("试图合并 {} 时出错，缺少文件 {}", wide2Ascii(originalRelFilePath), wide2Ascii(partPath)));
            }
        }

        fs::path finalOutputPath = outputDir / originalRelFilePath;
        createParent(finalOutputPath);
        std::ofstream ofs(finalOutputPath);
        ofs << combinedJson.dump(2);
        logger->info("文件 {} 合并完成，已保存到 {}", wide2Ascii(originalRelFilePath), wide2Ascii(finalOutputPath));
    }

    ApiResponse performApiRequest(json& payload, const TranslationAPI& api, int threadId,
        std::shared_ptr<IController> controller, std::shared_ptr<spdlog::logger> logger, int apiTimeOutMs) {
        ApiResponse apiResponse;

        if (api.stream) {
            // =================================================
            // ===========   流式请求处理路径   ================
            // =================================================
            payload["stream"] = true;
            std::string concatenatedContent;
            std::string sseBuffer;

            // 1. 定义一个符合 cpr::WriteCallback 构造函数要求的 lambda
            auto callbackLambda = [&](const std::string_view& data, intptr_t userdata) -> bool
                {
                    // 将接收到的数据块(string_view)追加到缓冲区(string)
                    sseBuffer.append(data);
                    size_t pos;
                    while ((pos = sseBuffer.find('\n')) != std::string::npos) {
                        std::string line = sseBuffer.substr(0, pos);
                        sseBuffer.erase(0, pos + 1);

                        if (line.rfind("data: ", 0) == 0) {
                            std::string jsonDataStr = line.substr(6);
                            if (jsonDataStr == "[DONE]") {
                                return true;
                            }
                            try {
                                json chunk = json::parse(jsonDataStr);
                                if (!chunk["choices"].empty() && chunk["choices"][0].contains("delta") && chunk["choices"][0]["delta"].contains("content")) {
                                    auto content_node = chunk["choices"][0]["delta"]["content"];
                                    if (content_node.is_string()) {
                                        concatenatedContent += content_node.get<std::string>();
                                    }
                                }
                            }
                            catch (const json::exception&) {

                            }
                        }
                    }
                    // 继续接收数据
                    return !controller->shouldStop();
                };

            // 2. 使用上面定义的 lambda 来构造一个 cpr::WriteCallback 类的实例
            cpr::WriteCallback writeCallbackInstance(callbackLambda);

            // 3. 将该实例传递给 cpr::Post
            cpr::Response response = cpr::Post(
                cpr::Url{ api.apiurl },
                cpr::Body{ payload.dump() },
                cpr::Header{ {"Content-Type", "application/json"}, {"Authorization", "Bearer " + api.apikey} },
                cpr::Timeout{ apiTimeOutMs },
                writeCallbackInstance // 传递类的实例
            );

            apiResponse.statusCode = response.status_code;
            if (response.status_code == 200) {
                apiResponse.success = true;
                apiResponse.content = concatenatedContent;
            }
            else {
                apiResponse.success = false;
                apiResponse.content = response.text;
                logger->error("[线程 {}] API 流式请求失败，状态码: {}, 错误: {}", threadId, response.status_code, response.text);
            }
        }
        else {
            // =================================================
            // =========   非流式请求处理路径   =========
            // =================================================
            cpr::Response response = cpr::Post(
                cpr::Url{ api.apiurl },
                cpr::Body{ payload.dump() },
                cpr::Header{ {"Content-Type", "application/json"}, {"Authorization", "Bearer " + api.apikey} },
                cpr::Timeout{ apiTimeOutMs }
            );

            apiResponse.statusCode = response.status_code;
            apiResponse.content = response.text; // 先记录原始响应体

            if (response.status_code == 200) {
                try {
                    // 解析完整的JSON响应
                    apiResponse.content = json::parse(response.text)["choices"][0]["message"]["content"];
                    apiResponse.success = true;
                }
                catch (const json::exception& e) {
                    logger->error("[线程 {}] 成功响应但JSON解析失败: {}, 错误: {}", threadId, response.text, e.what());
                    apiResponse.success = false;
                }
            }
            else {
                logger->error("[线程 {}] API 非流请求失败，状态码: {}, 错误: {}", threadId, response.status_code, response.text);
                apiResponse.success = false;
            }
        }

        return apiResponse;
    }

    bool hasRetranslKey(const std::vector<std::string>& retranslKeys, const Sentence* se) {
        return std::any_of(retranslKeys.begin(), retranslKeys.end(), [&](const std::string& key)
            {
                return se->original_text.find(key) != std::string::npos || se->problem.find(key) != std::string::npos;
            });
    }

    std::string cvt2StdApiUrl(const std::string& url) {
        std::string ret = url;
        if (ret.ends_with("/")) {
            ret.pop_back();
        }
        if (ret.ends_with("/chat/completions")) {
            return ret;
        }
        if (ret.ends_with("/chat")) {
            return ret + "/completions";
        }
        if (boost::regex_search(ret, boost::regex(R"(/v\d+$)"))) {
            return ret + "/chat/completions";
        }
        return ret + "/v1/chat/completions";
    }

    template<typename T>
    T parseToml(toml::v3::table& config, toml::v3::table& backup, const std::string& path) {
        if constexpr (std::is_same_v<T, toml::array>) {
            auto arr = config.at_path(path).as_array();
            if (!arr) {
                arr = backup.at_path(path).as_array();
            }
            if (!arr) {
                throw std::runtime_error(std::format("配置项 {} 未找到", path));
            }
            return *arr;
        }
        else if constexpr (std::is_same_v<T, toml::table>) {
            auto tbl = config.at_path(path).as_table();
            if (!tbl) {
                tbl = backup.at_path(path).as_table();
            }
            if (!tbl) {
                throw std::runtime_error(std::format("配置项 {} 未找到", path));
            }
            return *tbl;
        }
        else {
            auto optValue = config.at_path(path).value<T>();
            if (!optValue.has_value()) {
                optValue = backup.at_path(path).value<T>();
            }
            if (!optValue.has_value()) {
                throw std::runtime_error(std::format("配置项 {} 未找到", path));
            }
            return optValue.value();
        }
    }

    /**
     * @brief 保存缓存，写入一个包含完整句子信息的 JSON 数组
     */
    void saveCache(const std::vector<Sentence>& allSentences, const fs::path& cachePath) {
        json cacheJson = json::array();
        for (const auto& se : allSentences) {
            if (!se.complete) {
                continue;
            }
            json cacheObj;
            cacheObj["index"] = se.index;
            cacheObj["name"] = se.name;
            cacheObj["name_preview"] = se.name_preview;
            cacheObj["original_text"] = se.original_text;
            if (!se.other_info.empty()) {
                cacheObj["other_info"] = se.other_info;
            }
            cacheObj["pre_processed_text"] = se.pre_processed_text;
            cacheObj["pre_translated_text"] = se.pre_translated_text;
            if (!se.problem.empty()) {
                cacheObj["problem"] = se.problem;
            }
            cacheObj["translated_by"] = se.translated_by;
            cacheObj["translated_preview"] = se.translated_preview;
            cacheJson.push_back(cacheObj);
        }
        std::ofstream ofs(cachePath);
        ofs << cacheJson.dump(2);
    }

    /**
    * @brief 将一个大的 JSON 数组按指定的数量分割成多个小数组
    * @param originalData 原始的 JSON 数组
    * @param chunkSize 每个小数组包含的元素数量
    * @return 一个包含多个小 JSON 数组的 vector
    */
    std::vector<json> splitJsonArrayNum(const json& originalData, int chunkSize) {
        if (chunkSize <= 0 || !originalData.is_array() || originalData.empty()) {
            return { originalData };
        }

        std::vector<json> parts;
        size_t totalSize = originalData.size();

        for (size_t i = 0; i < totalSize; i += chunkSize) {
            size_t end = std::min(i + chunkSize, totalSize);
            json part = json::array();
            for (size_t j = i; j < end; ++j) {
                part.push_back(originalData[j]);
            }
            parts.push_back(part);
        }
        return parts;
    }

    /**
     * @brief 将一个大的 JSON 数组分割成多个小数组
     * @param originalData 原始的 JSON 数组
     * @param numParts 要分割成的份数
     * @return 一个包含多个小 JSON 数组的 vector
     */
    std::vector<json> splitJsonArrayEqual(const json& originalData, int numParts) {
        if (numParts <= 1 || !originalData.is_array() || originalData.empty()) {
            return { originalData };
        }

        std::vector<json> parts;
        size_t totalSize = originalData.size();
        size_t partSize = totalSize / numParts;
        size_t remainder = totalSize % numParts;
        size_t currentIndex = 0;

        for (int i = 0; i < numParts; ++i) {
            size_t currentPartSize = partSize + (i < remainder ? 1 : 0);
            if (currentPartSize == 0) continue;

            json part = json::array();
            for (size_t j = 0; j < currentPartSize; ++j) {
                part.push_back(originalData[currentIndex + j]);
            }
            parts.push_back(part);
            currentIndex += currentPartSize;
        }
        return parts;
    }

    bool isSameExtension(const fs::path& filePath, const std::wstring& ext) {
        return _wcsicmp(filePath.extension().wstring().c_str(), ext.c_str()) == 0;
    }

    std::string removePunctuation(const std::string& text) {
        UErrorCode errorCode = U_ZERO_ERROR;
        icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(text);
        icu::UnicodeString result;

        // 创建一个用于遍历字形簇的 BreakIterator
        std::unique_ptr<icu::BreakIterator> boundary(icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), errorCode));
        if (U_FAILURE(errorCode)) {
            throw std::runtime_error(std::format("Failed to create a character break iterator: {}", u_errorName(errorCode)));
        }
        boundary->setText(ustr);

        int32_t start = boundary->first();
        // 遍历每个字形簇
        for (int32_t end = boundary->next(); end != icu::BreakIterator::DONE; start = end, end = boundary->next()) {
            icu::UnicodeString grapheme;
            ustr.extract(start, end - start, grapheme); // 提取一个完整的字形簇
            // 判断这个字形簇是否是标点
            // 一个标点符号通常自身就是一个单独的码点构成的字形簇。
            // 如果一个字形簇包含多个码点（如 'e' + '´'），它几乎不可能是标点。
            bool isPunctuationCluster = (grapheme.length() == 1 && u_ispunct(grapheme.char32At(0)));
            if (!isPunctuationCluster) {
                result.append(grapheme); // 如果不是标点，则保留整个字形簇
            }
        }

        std::string resultStr;
        result.toUTF8String(resultStr);
        return resultStr;
    }

    std::pair<std::string, int> getMostCommonChar(const std::string& s) {
        if (s.empty()) {
            return { {}, 0};
        }

        UErrorCode errorCode = U_ZERO_ERROR;
        icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(s);

        std::map<icu::UnicodeString, int> counts;

        std::unique_ptr<icu::BreakIterator> boundary(icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), errorCode));
        if (U_FAILURE(errorCode)) {
            throw std::runtime_error(std::format("Failed to create a character break iterator: {}", u_errorName(errorCode)));
        }
        boundary->setText(ustr);

        int32_t start = boundary->first();
        for (int32_t end = boundary->next(); end != icu::BreakIterator::DONE; start = end, end = boundary->next()) {
            icu::UnicodeString grapheme;
            ustr.extract(start, end - start, grapheme);
            counts[grapheme]++;
        }

        if (counts.empty()) {
            return { {}, 0 };
        }

        auto maxIterator = std::max_element(counts.begin(), counts.end(),
            [](const auto& a, const auto& b) {
                return a.second < b.second;
            });

        icu::UnicodeString mostCommonGrapheme = maxIterator->first;
        int maxCount = maxIterator->second;

        std::string resultStr;
        mostCommonGrapheme.toUTF8String(resultStr);

        return { resultStr, maxCount };
    }

    std::vector<std::string> splitIntoGraphemes(const std::string& sourceString) {
        std::vector<std::string> resultVector;

        if (sourceString.empty()) {
            return resultVector;
        }

        UErrorCode errorCode = U_ZERO_ERROR;
        icu::UnicodeString uString = icu::UnicodeString::fromUTF8(sourceString);

        std::unique_ptr<icu::BreakIterator> breakIterator(
            icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), errorCode)
        );

        if (U_FAILURE(errorCode)) {
            throw std::runtime_error(std::format("Failed to create a character break iterator: {}", u_errorName(errorCode)));
        }

        breakIterator->setText(uString);

        int32_t start = breakIterator->first();
        for (int32_t end = breakIterator->next(); end != icu::BreakIterator::DONE; start = end, end = breakIterator->next()) {
            icu::UnicodeString graphemeUString;
            uString.extract(start, end - start, graphemeUString);
            std::string graphemeStdString;
            graphemeUString.toUTF8String(graphemeStdString);
            resultVector.push_back(graphemeStdString);
        }

        return resultVector;
    }

    // 计算子串出现次数
    int countSubstring(const std::string& text, const std::string& sub) {
        if (sub.empty()) return 0;
        int count = 0;
        for (size_t offset = text.find(sub); offset != std::string::npos; offset = text.find(sub, offset + sub.length())) {
            ++count;
        }
        return count;
    }

    // 计算的是子串在删去子串后的主串中出现的位置
    std::vector<double> getSubstringPositions(const std::string& text, const std::string& sub) {
        if (text.empty() || sub.empty()) return {};
        std::vector<size_t> positions;
        std::vector<double> relpositions;
        
        for (size_t offset = text.find(sub); offset != std::string::npos; offset = text.find(sub, offset + sub.length())) {
            positions.push_back(offset);
        }
        size_t newTotalLength = text.length() - positions.size() * sub.length();
        if (newTotalLength == 0) {
            return {};
        }
        for (size_t i = 0; i < positions.size(); i++) {
            size_t newPos = positions[i] - i * sub.length();
            relpositions.push_back((double)newPos / newTotalLength);
        }
        return relpositions;
    }

    void replaceStrInplace(std::string& str, const std::string& org, const std::string& rep) {
        size_t pos = 0;
        while ((pos = str.find(org, pos)) != std::string::npos) {
            str = str.replace(pos, org.length(), rep);
            pos += rep.length();
        }
    }

    template<typename T>
    std::string stream2String(const T& output) {
        std::stringstream ss;
        ss << output;
        return ss.str();
    }
    template std::string stream2String(const toml::table& output);

    template<typename T>
    T calculateAbs(T a, T b) {
        return a > b ? a - b : b - a;
    }

    bool containsKatakana(const std::string& sourceString) {
        icu::UnicodeString uString = icu::UnicodeString::fromUTF8(sourceString);

        icu::StringCharacterIterator iter(uString);
        UChar32 codePoint;

        // 遍历字符串中的每一个码点
        while (iter.hasNext()) {
            codePoint = iter.next32PostInc(); // 获取当前码点并前进

            UErrorCode errorCode = U_ZERO_ERROR;
            // 使用 uscript_getScript 获取码点的脚本属性
            UScriptCode script = uscript_getScript(codePoint, &errorCode);

            // 检查是否获取脚本失败，或者脚本是片假名
            if (U_SUCCESS(errorCode) && script == USCRIPT_KATAKANA) {
                return true;
            }
        }

        return false;
    }

    bool containsKana(const std::string& sourceString) {
        icu::UnicodeString uString = icu::UnicodeString::fromUTF8(sourceString);
        icu::StringCharacterIterator iter(uString);
        UChar32 codePoint;
        while (iter.hasNext()) {
            codePoint = iter.next32PostInc();
            UErrorCode errorCode = U_ZERO_ERROR;
            UScriptCode script = uscript_getScript(codePoint, &errorCode);
            if (U_SUCCESS(errorCode) && (script == USCRIPT_KATAKANA || script == USCRIPT_HIRAGANA)) {
                return true;
            }
        }
        return false;
    }

    bool containsLatin(const std::string& sourceString) {
        icu::UnicodeString uString = icu::UnicodeString::fromUTF8(sourceString);
        icu::StringCharacterIterator iter(uString);
        UChar32 codePoint;
        while (iter.hasNext()) {
            codePoint = iter.next32PostInc();
            UErrorCode errorCode = U_ZERO_ERROR;
            UScriptCode script = uscript_getScript(codePoint, &errorCode);
            if (U_SUCCESS(errorCode) && script == USCRIPT_LATIN) {
                return true;
            }
        }
        return false;
    }

    bool containsHangul(const std::string& sourceString) {
        icu::UnicodeString uString = icu::UnicodeString::fromUTF8(sourceString);
        icu::StringCharacterIterator iter(uString);
        UChar32 codePoint;
        while (iter.hasNext()) {
            codePoint = iter.next32PostInc();
            UErrorCode errorCode = U_ZERO_ERROR;
            UScriptCode script = uscript_getScript(codePoint, &errorCode);
            if (U_SUCCESS(errorCode) && script == USCRIPT_HANGUL) {
                return true;
            }
        }
        return false;
    }

    template<typename T>
    void insertToml(toml::table& table, const std::string& path, const T& value)
    {
        std::vector<std::string> keys = splitString(path, '.');
        toml::table* currentTable = &table;
        for (size_t i = 0; i < keys.size() - 1; i++) {
            auto it = currentTable->find(keys[i]);
            if (it == currentTable->end()) {
                currentTable->insert(keys[i], toml::table{});
            }
            currentTable = (*currentTable)[keys[i]].as_table();
            if (!currentTable) {
                return;
            }
        }
        currentTable->insert_or_assign(keys.back(), value);
    }

    template void insertToml(toml::table& table, const std::string& path, const std::string& value);
    template void insertToml(toml::table& table, const std::string& path, const int& value);
    template void insertToml(toml::table& table, const std::string& path, const double& value);
    template void insertToml(toml::table& table, const std::string& path, const bool& value);
    template void insertToml(toml::table& table, const std::string& path, const toml::array& value);
    template void insertToml(toml::table& table, const std::string& path, const toml::table& value);

    bool extractZip(const fs::path& zipPath, const fs::path& outputDir) {
        int error = 0;
        zip* za = zip_open(wide2Ascii(zipPath).c_str(), 0, &error);
        if (!za) {
            return false;
        }
        zip_int64_t num_entries = zip_get_num_entries(za, 0);
        for (zip_int64_t i = 0; i < num_entries; i++) {
            zip_stat_t zs;
            zip_stat_index(za, i, 0, &zs);
            fs::path outpath = outputDir / ascii2Wide(zs.name);
            if (zs.name[strlen(zs.name) - 1] == '/') {
                fs::create_directories(outpath);
            }
            else {
                zip_file* zf = zip_fopen_index(za, i, 0);
                if (!zf) {
                    throw std::runtime_error(std::format("Failed to open file in zip archive: {}, file: {}", wide2Ascii(zipPath), zs.name));
                }
                std::vector<char> buffer(zs.size);
                zip_fread(zf, buffer.data(), zs.size);
                zip_fclose(zf);
                fs::create_directories(outpath.parent_path());
                std::ofstream ofs(outpath, std::ios::binary);
                ofs.write(buffer.data(), buffer.size());
            }
        }
        zip_close(za);
        return true;
    }

}