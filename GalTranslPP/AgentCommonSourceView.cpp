module;

#include "GPPMacros.hpp"

module AgentCommonSourceView;

import Tool;

NAMESPACE_BEGIN(gpp)

AgentCommonSourceFileView buildAgentCommonSourceFileViewFromSentences(const std::vector<Sentence>& sentences) {
    AgentCommonSourceFileView fileView;
    fileView.lines.reserve(sentences.size());
    for (const Sentence& se : sentences) {
        const std::string speaker = getNameString(se);
        fileView.lines.push_back({
            .id = se.index,
            .speaker = speaker,
            .sourceText = se.preproc
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

NAMESPACE_END(gpp)
