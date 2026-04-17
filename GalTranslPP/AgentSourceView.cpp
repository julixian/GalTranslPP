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
        fileView.lines.push_back({
            .id = se.index,
            .speaker = speaker,
            .originalText = se.original_text,
            .preProcessedText = se.pre_processed_text,
            .toolText = se.pre_processed_text,
            .toolTextLower = str2Lower(se.pre_processed_text)
        });
    }
    return fileView;
}

json agentSourceLineToJson(const AgentSourceLineView& line, bool isMatch, bool includeOriginalText) {
    json result = {
        {"id", line.id},
        {"speaker", line.speaker},
        {"message", line.toolText},
        {"joined_text", line.speaker.empty() ? line.toolText : std::format("{}: {}", line.speaker, line.toolText)},
        {"is_match", isMatch}
    };
    if (includeOriginalText) {
        result["original_text"] = line.originalText;
    }
    return result;
}

json buildAgentSourceNearbyLines(const std::vector<AgentSourceLineView>& lines, int matchIndex, int contextLines, bool includeOriginalText) {
    json nearbyLines = json::array();
    if (lines.empty()) {
        return nearbyLines;
    }
    const int start = std::max(0, matchIndex - contextLines);
    const int end = std::min((int)lines.size() - 1, matchIndex + contextLines);
    for (int i = start; i <= end; ++i) {
        nearbyLines.push_back(agentSourceLineToJson(lines[i], i == matchIndex, includeOriginalText));
    }
    return nearbyLines;
}
