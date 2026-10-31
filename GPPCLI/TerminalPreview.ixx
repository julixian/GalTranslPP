module;

#include "../GalTranslPP/GPPMacros.hpp"

export module TerminalPreview;

export import ProjectConfig;
export import ProblemOverview;

export NAMESPACE_BEGIN(gpp)

bool hasInteractiveTerminal();
// 返回 true 表示用户确认开始；预览关闭后恢复终端，再运行翻译器。
bool showProjectPreview(const std::filesystem::path& projectDir);
void printProjectPreview(const std::filesystem::path& projectDir);

NAMESPACE_END(gpp)
