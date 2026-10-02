module;

#include "GPPMacros.hpp"

export module AgentCommonSourceView;

export import GPPDefines;

export NAMESPACE_BEGIN(gpp)

struct AgentCommonSourceLineView {
    int id = -1;
    std::string speaker;
    std::string sourceText;
};

struct AgentCommonSourceFileView {
    std::vector<AgentCommonSourceLineView> lines;
};

// 从已预处理的句子构造 Agent 工具读取的只读源文件视图。
AgentCommonSourceFileView buildAgentCommonSourceFileViewFromSentences(const std::vector<Sentence>& sentences);

// 从 Normal JSON 输入构造 Agent 工具读取的只读源文件视图。
AgentCommonSourceFileView buildAgentCommonSourceFileViewFromJson(
    const ordered_json& data,
    const std::function<void(Sentence*)>& preProcessFunc
);

NAMESPACE_END(gpp)
