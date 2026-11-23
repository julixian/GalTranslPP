module;

#include "GPPMacros.hpp"

export module TextImageRenderer;

import std;

export NAMESPACE_BEGIN(gpp)

struct TextImageRenderOptions {
    int pageWidth = 1024;
    int maxPageHeight = 1536;
    int padding = 28;
    int fontSize = 22;
    int lineHeight = 34;
    int continuationIndent = 44; // 2 * fontSize
    std::string preferredFont;   // optional font family name
};

// Renders UTF-8 text into 1+ grayscale PNG image byte buffers.
// Returns std::unexpected(errorMessage) on failure.
std::expected<std::vector<std::vector<std::uint8_t>>, std::string>
renderTextToPngPages(std::string_view text, const TextImageRenderOptions& options = {});

NAMESPACE_END(gpp)
