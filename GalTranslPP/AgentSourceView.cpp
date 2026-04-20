module;

#include "GPPMacros.hpp"

module AgentSourceView;

import Tool;

namespace fs = std::filesystem;

AgentSourceFileView buildAgentSourceFileViewFromSentences(const std::vector<Sentence>& sentences, const fs::path& relPath) {
    AgentSourceFileView fileView;
    fileView.relPath = relPath;
    fileView.lines.reserve(sentences.size());
    for (const Sentence& se : sentences) {
        const std::string speaker = getNameString(&se);
        const std::string toolTextLower = str2Lower(se.pre_processed_text);
        fileView.lines.push_back({
            .id = se.index,
            .speaker = speaker,
            .toolText = se.pre_processed_text,
            .toolTextLower = toolTextLower,
            .speakerToolTextLower = speaker.empty()
                ? toolTextLower
                : str2Lower(speaker + "\n" + se.pre_processed_text)
        });
    }
    return fileView;
}

json agentSourceLineToJson(const AgentSourceLineView& line, bool isMatch) {
    json result = {
        {"id", line.id},
        {"speaker", line.speaker},
        {"message", line.toolText},
        {"is_match", isMatch}
    };
    return result;
}

json buildAgentSourceNearbyLines(const std::vector<AgentSourceLineView>& lines, int matchIndex, int contextLines) {
    json nearbyLines = json::array();
    if (lines.empty()) {
        return nearbyLines;
    }
    const int start = std::max(0, matchIndex - contextLines);
    const int end = std::min((int)lines.size() - 1, matchIndex + contextLines);
    for (int i = start; i <= end; ++i) {
        nearbyLines.push_back(agentSourceLineToJson(lines[i], i == matchIndex));
    }
    return nearbyLines;
}
