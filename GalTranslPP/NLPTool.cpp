module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"
#include <mecab/mecab.h>

module NLPTool;

import Tool;
import PythonManager;

namespace fs = std::filesystem;
namespace py = pybind11;

namespace
{
    struct LazyTokenizeState {
        std::once_flag initOnce;
        NLPTokenizeFunc tokenizeFunc;
    };

    template <typename InitFunc>
    NLPResult runLazyTokenizer(const std::shared_ptr<LazyTokenizeState>& state, std::string_view text, InitFunc&& initFunc)
    {
        std::call_once(state->initOnce, [&]()
            {
                try {
                    state->tokenizeFunc = initFunc();
                }
                catch (...) {
                    std::exception_ptr exception = std::current_exception();
                    state->tokenizeFunc = [=](std::string_view) -> NLPResult
                        {
                            std::rethrow_exception(exception);
                        };
                }
            });
        return state->tokenizeFunc(text);
    }
}

NLPTokenizeFunc getMeCabTokenizeFunc(const std::string& mecabDictDir, const std::shared_ptr<spdlog::logger>& logger)
{
	auto state = std::make_shared<LazyTokenizeState>();
    return [stateR = std::move(state), mecabDictDir, logger](std::string_view str) -> NLPResult
        {
            return runLazyTokenizer(stateR, str, [&]() -> NLPTokenizeFunc
                {
                    logger->info(gppTr("NLPTool.getMeCabTokenizeFunc", "正在检查 MeCab 环境...")
                        .toStdString());
                    char* argv[] = {
                        (char*)"mecab",
                        (char*)"-r",
						(char*)"BaseConfig/mecab/mecabrc",
						(char*)"-d",
						(char*)mecabDictDir.c_str()
                    };
            		auto mecabModel = std::shared_ptr<MeCab::Model>(
                        MeCab::Model::create(std::size(argv), argv)
                    );
                    if (!mecabModel) {
                        throw std::runtime_error(gppTr(
                            "NLPTool.getMeCabTokenizeFunc",
                            "无法初始化 MeCab Model。请确保 BaseConfig/mecab/mecabrc 和 %1 存在\n错误信息: %2")
                            .arg(mecabDictDir)
                            .arg(MeCab::getLastError())
                            .toStdString());
                    }
            		auto mecabTagger = std::shared_ptr<MeCab::Tagger>(mecabModel->createTagger());
                    if (!mecabTagger) {
                        throw std::runtime_error(gppTr(
                            "NLPTool.getMeCabTokenizeFunc",
                            "无法初始化 MeCab Tagger。请确保 BaseConfig/mecab/mecabrc 和 %1 存在\n错误信息: %2")
                            .arg(mecabDictDir)
                            .arg(MeCab::getLastError())
                            .toStdString());
                    }
                    logger->info(gppTr("NLPTool.getMeCabTokenizeFunc", "MeCab 环境检查完毕")
                        .toStdString());

                    return [mecabModelR = std::move(mecabModel), mecabTaggerR = std::move(mecabTagger)]
            		            (std::string_view str_) -> NLPResult
                        {
                            WordPosVec wordPosList;
                            EntityVec entityList;
                            const std::unique_ptr<MeCab::Lattice> lattice(mecabModelR->createLattice());
                            lattice->set_sentence(str_.data(), str_.size());
                            if (!mecabTaggerR->parse(lattice.get())) {
                                throw std::runtime_error(gppTr(
                                    "NLPTool.getMeCabTokenizeFunc",
                                    "分词器解析失败，错误信息: %1")
                                    .arg(MeCab::getLastError())
                                    .toStdString());
                            }
                            for (const MeCab::Node* node = lattice->bos_node(); node; node = node->next) {
                                if (node->stat == MECAB_BOS_NODE || node->stat == MECAB_EOS_NODE) {
                                    continue;
                                }
                                const std::string_view surface(node->surface, node->length);
                                const std::string_view feature = node->feature;
                                if (feature.contains("固有名詞") || hasKatakana(surface)) {
                                    entityList.emplace_back(std::array{ std::string(surface), std::string(feature) });
                                }
                                wordPosList.emplace_back(std::array{ std::string(surface), std::string(feature) });
                            }
                            return NLPResult{ std::move(wordPosList), std::move(entityList) };
                        };
                });
        };
}

NLPTokenizeFunc getPythonNLPTokenizeFunc(const std::vector<std::string>& dependencies, const std::string& moduleName,
    const std::string& modelName, const std::shared_ptr<spdlog::logger>& logger)
{
    const auto state = std::make_shared<LazyTokenizeState>();
    return [state, dependencies, moduleName, modelName, logger](std::string_view str) -> NLPResult
        {
            return runLazyTokenizer(state, str, [&]() -> NLPTokenizeFunc
                {
                    checkPythonDependencies(dependencies, logger);

                    std::shared_ptr<PythonNLPFunction> pythonNLPFunc = PythonMainInterpreterManager::getInstance()
                        .registerNLPFunction(moduleName, modelName, logger);

                    return [pythonNLPFuncR = std::move(pythonNLPFunc), moduleName, modelName](std::string_view str_) -> NLPResult
                        {
                            NLPResult result;
                            auto nlpTaskFunc = [&]()
                                {
                                    try {
                                        result = pythonNLPFuncR->proc(str_).cast<NLPResult>();
                                    }
                                    catch (const py::error_already_set& e) {
                                        throw std::runtime_error(gppTr(
                                            "NLPTool.getPythonNLPTokenizeFunc",
                                            "Python 模块 [%1] 的模型 %2 的 NLP 函数调用失败，错误信息: %3")
                                            .arg(moduleName)
                                            .arg(modelName)
                                            .arg(e.what())
                                            .toStdString());
                                    }
                                };
                            PythonMainInterpreterManager::getInstance().submitTask(std::move(nlpTaskFunc)).get();
                            return result;
                        };
                });
        };
}

std::vector<std::string_view> splitIntoTokenViews(const WordPosVec& wordPosVec, std::string_view text) {
    std::vector<std::string_view> tokens;

    size_t searchPos = 0; // 在原始句子中搜索的起始位置
    for (const auto& wordPos : wordPosVec) {
        const auto& token = wordPos.front();
        // 从 searchPos 开始查找当前 token
        const size_t tokenPos = text.find(token, searchPos);
        // 错误处理：如果在预期位置找不到 token，说明输入有问题
        if (tokenPos == std::string::npos) {
            throw std::runtime_error(gppTr("splitIntoTokens", "在原句剩余部分中找不到 token '%1'。")
                .arg(token)
                .toStdString());
        }
        // 1. 提取并添加 token 前面的空白部分
        if (tokenPos > searchPos) {
            tokens.push_back(text.substr(searchPos, tokenPos - searchPos));
        }
        // 2. 更新下一次搜索的起始位置
        searchPos = tokenPos + token.length();
        // 3. 添加 token 本身
        tokens.push_back(std::move(token));
    }
    // 4. 处理最后一个 token 后面的尾随空白
    if (searchPos < text.length()) {
        tokens.push_back(text.substr(searchPos));
    }

    return tokens;
}

std::vector<std::string> splitIntoTokens(const WordPosVec& wordPosVec, std::string_view text) {
    const std::vector<std::string_view> tokensView = splitIntoTokenViews(wordPosVec, text);
    std::vector<std::string> tokens;
    tokens.reserve(tokensView.size());
    for (const auto& tokenView : tokensView) {
        tokens.emplace_back(tokenView);
    }
    return tokens;
}
