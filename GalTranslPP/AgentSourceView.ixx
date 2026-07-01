module;

#include "GPPMacros.hpp"

export module AgentSourceView;

export import GPPDefines;

namespace fs = std::filesystem;

export
{
    struct AgentSourceLineView {
        int id = -1;
        std::string speaker;
        std::string toolText;
        std::string toolTextLower;
        std::string speakerToolTextLower;
    };

    struct AgentSourceFileView {
        fs::path relPath;
        std::vector<AgentSourceLineView> lines;
    };

    AgentSourceFileView buildAgentSourceFileViewFromSentences(const std::vector<Sentence>& sentences, const fs::path& relPath = {});
    json buildAgentSourceNearbyLines(const std::vector<AgentSourceLineView>& lines, int matchIndex, int contextLines);
}
