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
        std::function<NLPResult(const std::string&)> tokenizeFunc;
    };

    template <typename InitFunc>
    NLPResult runLazyTokenizer(const std::shared_ptr<LazyTokenizeState>& state, const std::string& text, InitFunc&& initFunc)
    {
        std::call_once(state->initOnce, [&]()
            {
                try {
                    state->tokenizeFunc = initFunc();
                }
                catch (...) {
                    std::exception_ptr exception = std::current_exception();
                    state->tokenizeFunc = [=](const std::string&) -> NLPResult
                        {
                            std::rethrow_exception(exception);
                        };
                }
            });
        return state->tokenizeFunc(text);
    }
}

std::function<NLPResult(const std::string&)> getMeCabTokenizeFunc(const std::string& mecabDictDir, const std::shared_ptr<spdlog::logger>& logger)
{
    const auto state = std::make_shared<LazyTokenizeState>();
    return [state, mecabDictDir, logger](const std::string& text) -> NLPResult
        {
            return runLazyTokenizer(state, text, [&]() -> std::function<NLPResult(const std::string&)>
                {
                    logger->info("正在检查 MeCab 环境...");
                    char* argv[] = {
                        (char*)"mecab",
                        (char*)"-r",
						(char*)"BaseConfig/mecabDict/mecabrc",
						(char*)"-d",
						(char*)mecabDictDir.c_str()
                    };
                    const auto mecabModel = std::shared_ptr<MeCab::Model>(
                        MeCab::Model::create(std::size(argv), argv)
                    );
                    if (!mecabModel) {
                        throw std::runtime_error(std::format("无法初始化 MeCab Model。请确保 BaseConfig/mecabDict/mecabrc 和 {} 存在\n错误信息: {}",
                            mecabDictDir, MeCab::getLastError()));
                    }
                    const auto mecabTagger = std::shared_ptr<MeCab::Tagger>(mecabModel->createTagger());
                    if (!mecabTagger) {
                        throw std::runtime_error(std::format("无法初始化 MeCab Tagger。请确保 BaseConfig/mecabDict/mecabrc 和 {} 存在\n错误信息: {}",
                            mecabDictDir, MeCab::getLastError()));
                    }
                    logger->info("MeCab 环境检查完毕。");

                    return [mecabModel, mecabTagger](const std::string& inputText) -> NLPResult
                        {
                            WordPosVec wordPosList;
                            EntityVec entityList;
                            const std::unique_ptr<MeCab::Lattice> lattice(mecabModel->createLattice());
                            lattice->set_sentence(inputText.c_str());
                            if (!mecabTagger->parse(lattice.get())) {
                                throw std::runtime_error(std::format("分词器解析失败，错误信息: {}", MeCab::getLastError()));
                            }
                            for (const MeCab::Node* node = lattice->bos_node(); node; node = node->next) {
                                if (node->stat == MECAB_BOS_NODE || node->stat == MECAB_EOS_NODE) continue;

                                std::string surface(node->surface, node->length);
                                std::string feature = node->feature;
                                //logger->trace("分词结果：{} ({})", surface, feature);
                                if (feature.contains("固有名詞") || !extractKatakana(surface).empty()) {
                                    entityList.emplace_back(std::initializer_list<std::string>{ surface, feature });
                                }
                                wordPosList.emplace_back(std::initializer_list<std::string>{ std::move(surface), std::move(feature) });
                            }
                            return NLPResult{ std::move(wordPosList), std::move(entityList) };
                        };
                });
        };
}

std::function<NLPResult(const std::string&)> getNLPTokenizeFunc(const std::vector<std::string>& dependencies, const std::string& moduleName,
    const std::string& modelName, const std::shared_ptr<spdlog::logger>& logger)
{
    const auto state = std::make_shared<LazyTokenizeState>();
    return [state, dependencies, moduleName, modelName, logger](const std::string& text) -> NLPResult
        {
            return runLazyTokenizer(state, text, [&]() -> std::function<NLPResult(const std::string&)>
                {
                    checkPythonDependencies(dependencies, logger);

                    bool initNeedReboot = false;
                    std::shared_ptr<py::object> pythonNLPFunc = PythonMainInterpreterManager::getInstance()
                        .registerNLPFunction(moduleName, modelName, logger, initNeedReboot);
                    if (initNeedReboot) {
                        throw std::runtime_error("需要重启程序以应用新安装的 NLP 模型");
                    }

                    std::function<NLPResult(const std::string&)> ret;
                    if (pythonNLPFunc) {
                        ret = [pythonNLPFunc](const std::string& inputText) -> NLPResult
                            {
                                NLPResult result;
                                auto nlpTaskFunc = [&]()
                                    {
                                        try {
                                            result = (*pythonNLPFunc)(inputText).cast<NLPResult>();
                                        }
                                        catch (const py::error_already_set& e) {
                                            throw std::runtime_error(std::format("Python NLP 函数调用失败，错误信息: {}", e.what()));
                                        }
                                    };
                                PythonMainInterpreterManager::getInstance().submitTask(std::move(nlpTaskFunc)).get();
                                return result;
                            };
                    }
                    else {
                        ret = [](const std::string& inputText) -> NLPResult
                            {
                                return NLPResult{ { {inputText, "ORIG"} }, EntityVec{} };
                            };
                    }
                    return ret;
                });
        };
}
