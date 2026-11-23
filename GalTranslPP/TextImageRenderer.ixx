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
    std::string preferredFont;   // optional system font family name or local font file path (.ttf/.otf/.ttc/.otc)
};

// Renders UTF-8 text into 1+ grayscale PNG image byte buffers.
// Returns std::unexpected(errorMessage) on failure.
// If warningOut is non-null, writes any non-fatal warning (e.g., preferredFont fallback).
std::expected<std::vector<std::vector<std::uint8_t>>, std::string>
renderTextToPngPages(
    std::string_view text,
    const TextImageRenderOptions& options = {},
    std::string* warningOut = nullptr);

NAMESPACE_END(gpp)
