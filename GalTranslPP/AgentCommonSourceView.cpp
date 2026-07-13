module;

#include "GPPMacros.hpp"

module AgentCommonSourceView;

import Tool;

AgentCommonSourceFileView buildAgentCommonSourceFileViewFromSentences(const std::vector<Sentence>& sentences) {
    AgentCommonSourceFileView fileView;
    fileView.lines.reserve(sentences.size());
    for (const Sentence& se : sentences) {
        const std::string speaker = getNameString(se);
        const std::string sourceTextLower = str2Lower(se.preproc);
        fileView.lines.push_back({
            .id = se.index,
            .speaker = speaker,
            .sourceText = se.preproc,
            .sourceTextLower = sourceTextLower,
            .sourceTextWithSpeakerLower = speaker.empty()
                ? sourceTextLower
                : str2Lower(speaker + "\n" + se.preproc)
        });
    }
    return fileView;
}

AgentCommonSourceFileView buildAgentCommonSourceFileViewFromJson(
    const ordered_json& data,
    const std::function<void(Sentence*)>& preProcessFunc
) {
    std::vector<Sentence> sentences;
    sentences.reserve(data.size());
    for (const auto& [index, item] : data | std::views::enumerate) {
        Sentence se;
        se.index = (int)index;
        if (auto jit = item.find("name"); jit != item.end()) {
            se.nameType = NameType::Single;
            jit->get_to(se.name);
        }
        else if (jit = item.find("names"); jit != item.end()) {
            se.nameType = NameType::Multiple;
            jit->get_to(se.names);
        }
        item["message"].get_to(se.orig);
        sentences.push_back(std::move(se));
    }
    for (auto [se1, se2] : std::views::adjacent<2>(sentences)) {
        se1.next = &se2;
        se2.prev = &se1;
    }

    for (Sentence& se : sentences) {
        preProcessFunc(&se);
    }
    return buildAgentCommonSourceFileViewFromSentences(sentences);
}

json buildAgentCommonSourceNearbyLines(const std::vector<AgentCommonSourceLineView>& lines, int matchIndex, int contextLines) {
    json nearbyLines = json::array();
    const int start = std::max(0, matchIndex - contextLines);
    const int end = std::min((int)lines.size() - 1, matchIndex + contextLines);
    for (int i = start; i <= end; ++i) {
        if (i != matchIndex) {
            const auto& line = lines[i];
            nearbyLines.push_back(json{
            {"id", line.id},
            {"speaker", line.speaker},
            {"message", line.sourceText},
            });
        }
    }
    return nearbyLines;
}
