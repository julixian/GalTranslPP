module;

#include "GPPMacros.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <objbase.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <ImageIO/ImageIO.h>
#endif

module TextImageRenderer;

import std;

NAMESPACE_BEGIN(gpp)

namespace {

struct VisualRow {
    std::size_t logicalLineIndex = 0;
    bool continued = false;
    std::string text;
};

struct SanitizedOptions {
    int pageWidth = 1024;
    int maxPageHeight = 1536;
    int padding = 28;
    int fontSize = 22;
    int lineHeight = 34;
    int continuationIndent = 44;
    int textWidth = 968;
    int continuationTextWidth = 924;
    int linesPerPage = 43;
    std::string preferredFont;
};

std::expected<SanitizedOptions, std::string> sanitizeOptions(const TextImageRenderOptions& options) {
    if (options.pageWidth <= 0) {
        return std::unexpected("TextImageRenderOptions.pageWidth must be positive");
    }
    if (options.maxPageHeight <= 0) {
        return std::unexpected("TextImageRenderOptions.maxPageHeight must be positive");
    }
    if (options.padding < 0) {
        return std::unexpected("TextImageRenderOptions.padding must be non-negative");
    }
    if (options.fontSize <= 0) {
        return std::unexpected("TextImageRenderOptions.fontSize must be positive");
    }
    if (options.lineHeight <= 0) {
        return std::unexpected("TextImageRenderOptions.lineHeight must be positive");
    }
    if (options.continuationIndent < 0) {
        return std::unexpected("TextImageRenderOptions.continuationIndent must be non-negative");
    }

    const int textWidth = options.pageWidth - 2 * options.padding;
    if (textWidth <= 0) {
        return std::unexpected("TextImageRenderOptions.pageWidth must exceed 2 * padding");
    }

    SanitizedOptions sanitized;
    sanitized.pageWidth = options.pageWidth;
    sanitized.maxPageHeight = options.maxPageHeight;
    sanitized.padding = options.padding;
    sanitized.fontSize = options.fontSize;
    sanitized.lineHeight = options.lineHeight;
    sanitized.textWidth = textWidth;
    sanitized.continuationIndent = std::min(
        options.continuationIndent,
        std::max(0, textWidth - std::min(textWidth, options.fontSize))
    );
    sanitized.continuationTextWidth = std::max(1, textWidth - sanitized.continuationIndent);
    sanitized.linesPerPage = std::max(1, (options.maxPageHeight - 2 * options.padding) / options.lineHeight);
    sanitized.preferredFont = options.preferredFont;
    return sanitized;
}

// Decode one UTF-8 codepoint starting at `pos`; advances `pos` by at least 1 byte.
std::uint32_t decodeUtf8Codepoint(std::string_view text, std::size_t& pos) noexcept {
    const auto b0 = static_cast<unsigned char>(text[pos]);
    if (b0 < 0x80) {
        ++pos;
        return b0;
    }
    if ((b0 & 0xE0) == 0xC0 && pos + 1 < text.size()) {
        const auto b1 = static_cast<unsigned char>(text[pos + 1]);
        if ((b1 & 0xC0) == 0x80) {
            const std::uint32_t cp = ((b0 & 0x1Fu) << 6) | (b1 & 0x3Fu);
            if (cp >= 0x80u) {
                pos += 2;
                return cp;
            }
        }
    } else if ((b0 & 0xF0) == 0xE0 && pos + 2 < text.size()) {
        const auto b1 = static_cast<unsigned char>(text[pos + 1]);
        const auto b2 = static_cast<unsigned char>(text[pos + 2]);
        if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80) {
            const std::uint32_t cp = ((b0 & 0x0Fu) << 12) | ((b1 & 0x3Fu) << 6) | (b2 & 0x3Fu);
            if (cp >= 0x800u && (cp < 0xD800u || cp > 0xDFFFu)) {
                pos += 3;
                return cp;
            }
        }
    } else if ((b0 & 0xF8) == 0xF0 && pos + 3 < text.size()) {
        const auto b1 = static_cast<unsigned char>(text[pos + 1]);
        const auto b2 = static_cast<unsigned char>(text[pos + 2]);
        const auto b3 = static_cast<unsigned char>(text[pos + 3]);
        if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80 && (b3 & 0xC0) == 0x80) {
            const std::uint32_t cp = ((b0 & 0x07u) << 18) | ((b1 & 0x3Fu) << 12)
                | ((b2 & 0x3Fu) << 6) | (b3 & 0x3Fu);
            if (cp >= 0x10000u && cp <= 0x10FFFFu) {
                pos += 4;
                return cp;
            }
        }
    }
    ++pos;
    return 0xFFFDu;
}

bool isExtendingCodepoint(std::uint32_t cp) noexcept {
    // Combining marks, Japanese combining dakuten/handakuten, variation selectors, ZWJ
    return (cp >= 0x0300u && cp <= 0x036Fu)
        || (cp >= 0x1AB0u && cp <= 0x1AFFu)
        || (cp >= 0x1DC0u && cp <= 0x1DFFu)
        || (cp >= 0x20D0u && cp <= 0x20FFu)
        || cp == 0x200Du
        || (cp >= 0x3099u && cp <= 0x309Au)
        || (cp >= 0xFE00u && cp <= 0xFE0Fu)
        || (cp >= 0xFE20u && cp <= 0xFE2Fu)
        || (cp >= 0xE0100u && cp <= 0xE01EFu);
}

// Tokenize a single logical line into atomic wrap units:
// `<br>`, `\r\n`, `\n`, `\r`, `\t` literals stay atomic; otherwise one Unicode cluster per token.
std::vector<std::string_view> tokenizeForWrap(std::string_view line) {
    std::vector<std::string_view> tokens;
    std::size_t pos = 0;
    while (pos < line.size()) {
        const std::string_view remaining = line.substr(pos);
        if (remaining.starts_with("<br>")) {
            tokens.push_back(line.substr(pos, 4));
            pos += 4;
            continue;
        }
        if (remaining.starts_with("\\r\\n")) {
            tokens.push_back(line.substr(pos, 4));
            pos += 4;
            continue;
        }
        if (remaining.starts_with("\\n") || remaining.starts_with("\\r") || remaining.starts_with("\\t")) {
            tokens.push_back(line.substr(pos, 2));
            pos += 2;
            continue;
        }

        const std::size_t start = pos;
        (void)decodeUtf8Codepoint(line, pos);
        while (pos < line.size()) {
            std::size_t peek = pos;
            const std::uint32_t nextCp = decodeUtf8Codepoint(line, peek);
            if (!isExtendingCodepoint(nextCp)) {
                break;
            }
            pos = peek;
            // If ZWJ, also attach the following codepoint if present.
            if (nextCp == 0x200Du && pos < line.size()) {
                (void)decodeUtf8Codepoint(line, pos);
            }
        }
        tokens.push_back(line.substr(start, pos - start));
    }
    return tokens;
}

std::vector<std::string> splitNormalizedLines(std::string_view text) {
    std::vector<std::string> lines;
    std::string current;
    current.reserve(std::min<std::size_t>(text.size(), 256));

    for (std::size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                ++i;
            }
            lines.push_back(std::move(current));
            current.clear();
        } else if (ch == '\n') {
            lines.push_back(std::move(current));
            current.clear();
        } else if (ch == '\t') {
            current.append("    ");
        } else {
            current.push_back(ch);
        }
    }
    lines.push_back(std::move(current));
    return lines;
}

template <typename MeasureFn>
std::expected<std::vector<std::vector<VisualRow>>, std::string>
layoutAndPaginate(std::string_view text, const SanitizedOptions& options, MeasureFn&& measureText) {
    const std::vector<std::string> logicalLines = splitNormalizedLines(text);

    std::unordered_map<std::string, float> tokenWidthCache;
    tokenWidthCache.reserve(512);

    auto measureToken = [&](std::string_view token) -> std::expected<float, std::string> {
        const std::string key(token);
        if (const auto it = tokenWidthCache.find(key); it != tokenWidthCache.end()) {
            return it->second;
        }
        auto measured = measureText(token);
        if (!measured) {
            return std::unexpected(measured.error());
        }
        tokenWidthCache.emplace(key, *measured);
        return *measured;
    };

    std::vector<std::vector<VisualRow>> pages;
    std::vector<VisualRow> currentPage;

    const float firstLineLimit = static_cast<float>(options.textWidth);
    const float continuationLimit = static_cast<float>(options.continuationTextWidth);

    for (std::size_t lineIndex = 0; lineIndex < logicalLines.size(); ++lineIndex) {
        const std::string& logicalLine = logicalLines[lineIndex];
        std::vector<VisualRow> group;

        if (logicalLine.empty()) {
            group.push_back(VisualRow{lineIndex, false, ""});
        } else {
            const std::vector<std::string_view> tokens = tokenizeForWrap(logicalLine);
            std::string currentText;
            float estimatedWidth = 0.0f;
            float limit = firstLineLimit;
            bool continued = false;

            for (const std::string_view token : tokens) {
                auto tokenWidthResult = measureToken(token);
                if (!tokenWidthResult) {
                    return std::unexpected(tokenWidthResult.error());
                }
                const float tokenWidth = *tokenWidthResult;

                if (!currentText.empty()) {
                    bool exceeds = (estimatedWidth + tokenWidth > limit);
                    // Near the wrap boundary, verify with full-line measurement so kerning/fallback never clips.
                    if (exceeds || (estimatedWidth + tokenWidth > limit - 6.0f)) {
                        std::string candidate;
                        candidate.reserve(currentText.size() + token.size());
                        candidate.append(currentText);
                        candidate.append(token);
                        auto candidateWidth = measureText(candidate);
                        if (!candidateWidth) {
                            return std::unexpected(candidateWidth.error());
                        }
                        exceeds = (*candidateWidth > limit);
                        if (!exceeds) {
                            currentText = std::move(candidate);
                            estimatedWidth = *candidateWidth;
                            continue;
                        }
                    }

                    if (exceeds) {
                        group.push_back(VisualRow{lineIndex, continued, std::move(currentText)});
                        currentText.clear();
                        estimatedWidth = 0.0f;
                        continued = true;
                        limit = continuationLimit;
                    }
                }

                currentText.append(token);
                estimatedWidth += tokenWidth;
            }
            group.push_back(VisualRow{lineIndex, continued, std::move(currentText)});
        }

        const auto maxRows = static_cast<std::size_t>(options.linesPerPage);
        if (!currentPage.empty() && currentPage.size() + group.size() > maxRows) {
            pages.push_back(std::move(currentPage));
            currentPage.clear();
        }

        currentPage.insert(
            currentPage.end(),
            std::make_move_iterator(group.begin()),
            std::make_move_iterator(group.end())
        );

        while (currentPage.size() > maxRows) {
            std::vector<VisualRow> chunk;
            chunk.reserve(maxRows);
            chunk.insert(
                chunk.end(),
                std::make_move_iterator(currentPage.begin()),
                std::make_move_iterator(currentPage.begin() + static_cast<std::ptrdiff_t>(maxRows))
            );
            pages.push_back(std::move(chunk));
            currentPage.erase(
                currentPage.begin(),
                currentPage.begin() + static_cast<std::ptrdiff_t>(maxRows)
            );
        }
    }

    if (!currentPage.empty()) {
        pages.push_back(std::move(currentPage));
    }

    if (pages.empty()) {
        pages.push_back({VisualRow{0, false, ""}});
    }

    return pages;
}

#ifdef _WIN32

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() { reset(); }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ComPtr(ComPtr&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            reset();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    void reset(T* ptr = nullptr) noexcept {
        if (m_ptr) {
            m_ptr->Release();
        }
        m_ptr = ptr;
    }

    T* get() const noexcept { return m_ptr; }
    T* operator->() const noexcept { return m_ptr; }
    explicit operator bool() const noexcept { return m_ptr != nullptr; }

    T** put() noexcept {
        reset();
        return &m_ptr;
    }

    void** putVoid() noexcept {
        reset();
        return reinterpret_cast<void**>(&m_ptr);
    }

private:
    T* m_ptr = nullptr;
};

struct ComInitGuard {
    bool shouldUninit = false;
    HRESULT result = S_OK;

    ComInitGuard() noexcept {
        result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (result == S_OK || result == S_FALSE) {
            shouldUninit = true;
            result = S_OK;
        } else if (result == RPC_E_CHANGED_MODE) {
            // Already initialized on this thread in another apartment (e.g., STA).
            shouldUninit = false;
            result = S_OK;
        }
    }

    ~ComInitGuard() {
        if (shouldUninit) {
            CoUninitialize();
        }
    }

    ComInitGuard(const ComInitGuard&) = delete;
    ComInitGuard& operator=(const ComInitGuard&) = delete;
};

std::string formatHResult(std::string_view context, HRESULT hr) {
    return std::format("{} (HRESULT=0x{:08X})", context, static_cast<std::uint32_t>(hr));
}

std::wstring utf8ToUtf16(std::string_view utf8) {
    std::wstring wide;
    wide.reserve(utf8.size());
    std::size_t pos = 0;
    while (pos < utf8.size()) {
        const std::uint32_t cp = decodeUtf8Codepoint(utf8, pos);
        if (cp <= 0xFFFFu) {
            wide.push_back(static_cast<wchar_t>(cp));
        } else {
            const std::uint32_t adjusted = cp - 0x10000u;
            wide.push_back(static_cast<wchar_t>(0xD800u + (adjusted >> 10)));
            wide.push_back(static_cast<wchar_t>(0xDC00u + (adjusted & 0x3FFu)));
        }
    }
    return wide;
}

std::wstring selectWindowsFontFamily(IDWriteFactory* dwriteFactory, std::string_view preferredFont) {
    ComPtr<IDWriteFontCollection> collection;
    if (FAILED(dwriteFactory->GetSystemFontCollection(collection.put(), FALSE)) || !collection) {
        return preferredFont.empty() ? L"Microsoft YaHei" : utf8ToUtf16(preferredFont);
    }

    auto familyExists = [&](const std::wstring& name) -> bool {
        if (name.empty()) {
            return false;
        }
        UINT32 index = 0;
        BOOL exists = FALSE;
        const HRESULT hr = collection->FindFamilyName(name.c_str(), &index, &exists);
        return SUCCEEDED(hr) && exists == TRUE;
    };

    if (!preferredFont.empty()) {
        std::wstring preferredWide = utf8ToUtf16(preferredFont);
        if (familyExists(preferredWide)) {
            return preferredWide;
        }
    }

    static constexpr std::array<const wchar_t*, 9> kFallbackFonts = {
        L"Microsoft YaHei",
        L"Microsoft YaHei UI",
        L"Yu Gothic UI",
        L"Yu Gothic",
        L"Meiryo",
        L"Noto Sans SC",
        L"Noto Sans JP",
        L"SimSun",
        L"Segoe UI",
    };

    for (const wchar_t* candidate : kFallbackFonts) {
        std::wstring name(candidate);
        if (familyExists(name)) {
            return name;
        }
    }

    return L"";
}

std::expected<std::vector<std::uint8_t>, std::string> encodeWicBitmapToGrayPng(
    IWICImagingFactory* wicFactory,
    IWICBitmap* wicBitmap,
    UINT width,
    UINT height)
{
    ComPtr<IWICFormatConverter> converter;
    HRESULT hr = wicFactory->CreateFormatConverter(converter.put());
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("IWICImagingFactory::CreateFormatConverter failed", hr));
    }

    hr = converter->Initialize(
        wicBitmap,
        GUID_WICPixelFormat8bppGray,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("IWICFormatConverter::Initialize(8bppGray) failed", hr));
    }

    ComPtr<IStream> stream;
    hr = CreateStreamOnHGlobal(nullptr, TRUE, stream.put());
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("CreateStreamOnHGlobal failed", hr));
    }

    ComPtr<IWICBitmapEncoder> encoder;
    hr = wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put());
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("IWICImagingFactory::CreateEncoder(PNG) failed", hr));
    }

    hr = encoder->Initialize(stream.get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("IWICBitmapEncoder::Initialize failed", hr));
    }

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> propertyBag;
    hr = encoder->CreateNewFrame(frame.put(), propertyBag.put());
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("IWICBitmapEncoder::CreateNewFrame failed", hr));
    }

    hr = frame->Initialize(propertyBag.get());
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("IWICBitmapFrameEncode::Initialize failed", hr));
    }

    hr = frame->SetSize(width, height);
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("IWICBitmapFrameEncode::SetSize failed", hr));
    }

    WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat8bppGray;
    hr = frame->SetPixelFormat(&pixelFormat);
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("IWICBitmapFrameEncode::SetPixelFormat failed", hr));
    }

    hr = frame->WriteSource(converter.get(), nullptr);
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("IWICBitmapFrameEncode::WriteSource failed", hr));
    }

    hr = frame->Commit();
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("IWICBitmapFrameEncode::Commit failed", hr));
    }

    hr = encoder->Commit();
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("IWICBitmapEncoder::Commit failed", hr));
    }

    STATSTG stat{};
    hr = stream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("IStream::Stat failed", hr));
    }

    const std::size_t byteCount = static_cast<std::size_t>(stat.cbSize.QuadPart);
    std::vector<std::uint8_t> pngBytes(byteCount);
    if (byteCount > 0) {
        LARGE_INTEGER zero{};
        hr = stream->Seek(zero, STREAM_SEEK_SET, nullptr);
        if (FAILED(hr)) {
            return std::unexpected(formatHResult("IStream::Seek failed", hr));
        }

        std::size_t totalRead = 0;
        while (totalRead < byteCount) {
            const ULONG chunkSize = static_cast<ULONG>(
                std::min<std::size_t>(byteCount - totalRead, 0x7FFFFFFFu)
            );
            ULONG bytesRead = 0;
            hr = stream->Read(pngBytes.data() + totalRead, chunkSize, &bytesRead);
            if (FAILED(hr)) {
                return std::unexpected(formatHResult("IStream::Read failed", hr));
            }
            if (bytesRead == 0) {
                break;
            }
            totalRead += bytesRead;
        }
        pngBytes.resize(totalRead);
    }

    return pngBytes;
}

std::expected<std::vector<std::vector<std::uint8_t>>, std::string>
renderWindows(std::string_view text, const SanitizedOptions& options) {
    ComInitGuard comGuard;
    if (FAILED(comGuard.result)) {
        return std::unexpected(formatHResult("CoInitializeEx failed", comGuard.result));
    }

    ComPtr<IWICImagingFactory> wicFactory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(wicFactory.put())
    );
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("CoCreateInstance(CLSID_WICImagingFactory) failed", hr));
    }

    ComPtr<ID2D1Factory> d2dFactory;
    hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory),
        nullptr,
        d2dFactory.putVoid()
    );
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("D2D1CreateFactory failed", hr));
    }

    ComPtr<IDWriteFactory> dwriteFactory;
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwriteFactory.put())
    );
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("DWriteCreateFactory failed", hr));
    }

    const std::wstring fontFamily = selectWindowsFontFamily(dwriteFactory.get(), options.preferredFont);

    ComPtr<IDWriteTextFormat> textFormat;
    hr = dwriteFactory->CreateTextFormat(
        fontFamily.c_str(),
        nullptr,
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        static_cast<FLOAT>(options.fontSize),
        L"",
        textFormat.put()
    );
    if (FAILED(hr)) {
        return std::unexpected(formatHResult("IDWriteFactory::CreateTextFormat failed", hr));
    }

    textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    auto measureText = [&](std::string_view sample) -> std::expected<float, std::string> {
        if (sample.empty()) {
            return 0.0f;
        }
        const std::wstring wide = utf8ToUtf16(sample);
        ComPtr<IDWriteTextLayout> layout;
        const HRESULT layoutHr = dwriteFactory->CreateTextLayout(
            wide.c_str(),
            static_cast<UINT32>(wide.size()),
            textFormat.get(),
            1.0e6f,
            static_cast<FLOAT>(options.lineHeight),
            layout.put()
        );
        if (FAILED(layoutHr)) {
            return std::unexpected(formatHResult("IDWriteFactory::CreateTextLayout(measure) failed", layoutHr));
        }
        DWRITE_TEXT_METRICS metrics{};
        const HRESULT metricsHr = layout->GetMetrics(&metrics);
        if (FAILED(metricsHr)) {
            return std::unexpected(formatHResult("IDWriteTextLayout::GetMetrics failed", metricsHr));
        }
        return metrics.widthIncludingTrailingWhitespace;
    };

    auto pagesResult = layoutAndPaginate(text, options, measureText);
    if (!pagesResult) {
        return std::unexpected(pagesResult.error());
    }

    std::vector<std::vector<std::uint8_t>> renderedPages;
    renderedPages.reserve(pagesResult->size());

    const D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f,
        D2D1_RENDER_TARGET_USAGE_NONE,
        D2D1_FEATURE_LEVEL_DEFAULT
    );

    const D2D1_COLOR_F whiteColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    const D2D1_COLOR_F bandColor = D2D1::ColorF(238.0f / 255.0f, 238.0f / 255.0f, 238.0f / 255.0f, 1.0f);
    const D2D1_COLOR_F blackColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);

    for (const auto& pageRows : *pagesResult) {
        const UINT width = static_cast<UINT>(options.pageWidth);
        const UINT height = static_cast<UINT>(
            2 * options.padding + static_cast<int>(pageRows.size()) * options.lineHeight
        );

        ComPtr<IWICBitmap> wicBitmap;
        hr = wicFactory->CreateBitmap(
            width,
            height,
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapCacheOnDemand,
            wicBitmap.put()
        );
        if (FAILED(hr)) {
            return std::unexpected(formatHResult("IWICImagingFactory::CreateBitmap failed", hr));
        }

        ComPtr<ID2D1RenderTarget> renderTarget;
        hr = d2dFactory->CreateWicBitmapRenderTarget(
            wicBitmap.get(),
            rtProps,
            renderTarget.put()
        );
        if (FAILED(hr)) {
            return std::unexpected(formatHResult("ID2D1Factory::CreateWicBitmapRenderTarget failed", hr));
        }

        renderTarget->SetDpi(96.0f, 96.0f);
        renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

        ComPtr<ID2D1SolidColorBrush> bandBrush;
        hr = renderTarget->CreateSolidColorBrush(bandColor, bandBrush.put());
        if (FAILED(hr)) {
            return std::unexpected(formatHResult("CreateSolidColorBrush(band) failed", hr));
        }

        ComPtr<ID2D1SolidColorBrush> textBrush;
        hr = renderTarget->CreateSolidColorBrush(blackColor, textBrush.put());
        if (FAILED(hr)) {
            return std::unexpected(formatHResult("CreateSolidColorBrush(text) failed", hr));
        }

        renderTarget->BeginDraw();
        renderTarget->Clear(whiteColor);

        for (std::size_t rowIdx = 0; rowIdx < pageRows.size(); ++rowIdx) {
            const VisualRow& row = pageRows[rowIdx];
            const FLOAT top = static_cast<FLOAT>(options.padding + static_cast<int>(rowIdx) * options.lineHeight);
            const FLOAT bottom = top + static_cast<FLOAT>(options.lineHeight);

            if (row.logicalLineIndex % 2 == 1) {
                renderTarget->FillRectangle(
                    D2D1::RectF(0.0f, top, static_cast<FLOAT>(options.pageWidth), bottom),
                    bandBrush.get()
                );
            }

            if (!row.text.empty()) {
                const FLOAT left = static_cast<FLOAT>(
                    options.padding + (row.continued ? options.continuationIndent : 0)
                );
                const std::wstring wide = utf8ToUtf16(row.text);
                ComPtr<IDWriteTextLayout> layout;
                const HRESULT layoutHr = dwriteFactory->CreateTextLayout(
                    wide.c_str(),
                    static_cast<UINT32>(wide.size()),
                    textFormat.get(),
                    static_cast<FLOAT>(std::max(1, options.pageWidth - static_cast<int>(left))),
                    static_cast<FLOAT>(options.lineHeight),
                    layout.put()
                );
                if (FAILED(layoutHr)) {
                    (void)renderTarget->EndDraw();
                    return std::unexpected(formatHResult("IDWriteFactory::CreateTextLayout(draw) failed", layoutHr));
                }
                layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

                renderTarget->DrawTextLayout(
                    D2D1::Point2F(left, top),
                    layout.get(),
                    textBrush.get(),
                    D2D1_DRAW_TEXT_OPTIONS_NONE
                );
            }
        }

        hr = renderTarget->EndDraw();
        if (FAILED(hr)) {
            return std::unexpected(formatHResult("ID2D1RenderTarget::EndDraw failed", hr));
        }

        // Release render target before WIC reads/converts the underlying bitmap.
        textBrush.reset();
        bandBrush.reset();
        renderTarget.reset();

        auto pngResult = encodeWicBitmapToGrayPng(wicFactory.get(), wicBitmap.get(), width, height);
        if (!pngResult) {
            return std::unexpected(pngResult.error());
        }
        renderedPages.push_back(std::move(*pngResult));
    }

    return renderedPages;
}

#elif defined(__APPLE__)

template <typename T>
class CFPtr {
public:
    CFPtr() = default;
    explicit CFPtr(T ptr) noexcept : m_ptr(ptr) {}
    ~CFPtr() { reset(); }

    CFPtr(const CFPtr&) = delete;
    CFPtr& operator=(const CFPtr&) = delete;

    CFPtr(CFPtr&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    CFPtr& operator=(CFPtr&& other) noexcept {
        if (this != &other) {
            reset();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    void reset(T ptr = nullptr) noexcept {
        if (m_ptr) {
            CFRelease(m_ptr);
        }
        m_ptr = ptr;
    }

    T get() const noexcept { return m_ptr; }
    explicit operator bool() const noexcept { return m_ptr != nullptr; }

private:
    T m_ptr = nullptr;
};

CFPtr<CFStringRef> createCFString(std::string_view utf8) {
    CFPtr<CFStringRef> str(CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(utf8.data()),
        static_cast<CFIndex>(utf8.size()),
        kCFStringEncodingUTF8,
        false
    ));
    if (str) {
        return str;
    }
    // Fallback for malformed UTF-8: decode lossily to UTF-16 with U+FFFD replacement.
    std::vector<UniChar> utf16;
    utf16.reserve(utf8.size());
    std::size_t pos = 0;
    while (pos < utf8.size()) {
        const std::uint32_t cp = decodeUtf8Codepoint(utf8, pos);
        if (cp <= 0xFFFFu) {
            utf16.push_back(static_cast<UniChar>(cp));
        } else {
            const std::uint32_t adjusted = cp - 0x10000u;
            utf16.push_back(static_cast<UniChar>(0xD800u + (adjusted >> 10)));
            utf16.push_back(static_cast<UniChar>(0xDC00u + (adjusted & 0x3FFu)));
        }
    }
    return CFPtr<CFStringRef>(CFStringCreateWithCharacters(
        kCFAllocatorDefault,
        utf16.data(),
        static_cast<CFIndex>(utf16.size())
    ));
}

CFPtr<CTFontRef> tryCreateMacFont(std::string_view name, CGFloat size, CFStringRef attributeKey) {
    if (name.empty()) {
        return {};
    }
    const CFPtr<CFStringRef> cfName = createCFString(name);
    if (!cfName) {
        return {};
    }

    const void* keys[] = {attributeKey};
    const void* values[] = {cfName.get()};
    const CFPtr<CFDictionaryRef> attrs(CFDictionaryCreate(
        kCFAllocatorDefault,
        keys,
        values,
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    ));
    if (!attrs) {
        return {};
    }

    const CFPtr<CTFontDescriptorRef> descriptor(CTFontDescriptorCreateWithAttributes(attrs.get()));
    if (!descriptor) {
        return {};
    }

    const CFPtr<CTFontDescriptorRef> matched(
        CTFontDescriptorCreateMatchingFontDescriptor(descriptor.get(), nullptr)
    );
    if (!matched) {
        return {};
    }

    return CFPtr<CTFontRef>(CTFontCreateWithFontDescriptor(matched.get(), size, nullptr));
}

CFPtr<CTFontRef> selectMacFont(std::string_view preferredFont, CGFloat size) {
    if (!preferredFont.empty()) {
        if (auto font = tryCreateMacFont(preferredFont, size, kCTFontFamilyNameAttribute)) {
            return font;
        }
        if (auto font = tryCreateMacFont(preferredFont, size, kCTFontNameAttribute)) {
            return font;
        }
    }

    static constexpr std::array<std::string_view, 6> kFallbackFonts = {
        "PingFang SC",
        "Hiragino Sans",
        "Hiragino Sans GB",
        "Heiti SC",
        "Apple SD Gothic Neo",
        "Helvetica Neue",
    };

    for (const std::string_view candidate : kFallbackFonts) {
        if (auto font = tryCreateMacFont(candidate, size, kCTFontFamilyNameAttribute)) {
            return font;
        }
    }

    return CFPtr<CTFontRef>(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, size, nullptr));
}

std::expected<std::vector<std::vector<std::uint8_t>>, std::string>
renderMacOS(std::string_view text, const SanitizedOptions& options) {
    const CFPtr<CTFontRef> font = selectMacFont(options.preferredFont, static_cast<CGFloat>(options.fontSize));
    if (!font) {
        return std::unexpected("Failed to create CoreText font");
    }

    const CFPtr<CGColorSpaceRef> graySpace(CGColorSpaceCreateDeviceGray());
    if (!graySpace) {
        return std::unexpected("CGColorSpaceCreateDeviceGray failed");
    }

    const CGFloat blackComponents[2] = {0.0, 1.0};
    const CFPtr<CGColorRef> blackColor(CGColorCreate(graySpace.get(), blackComponents));
    if (!blackColor) {
        return std::unexpected("CGColorCreate(black) failed");
    }

    const void* attrKeys[] = {kCTFontAttributeName, kCTForegroundColorAttributeName};
    const void* attrValues[] = {font.get(), blackColor.get()};
    const CFPtr<CFDictionaryRef> textAttrs(CFDictionaryCreate(
        kCFAllocatorDefault,
        attrKeys,
        attrValues,
        2,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    ));
    if (!textAttrs) {
        return std::unexpected("CFDictionaryCreate(textAttrs) failed");
    }

    auto createCTLine = [&](std::string_view sample) -> CFPtr<CTLineRef> {
        const CFPtr<CFStringRef> cfStr = createCFString(sample);
        if (!cfStr) {
            return {};
        }
        const CFPtr<CFAttributedStringRef> attrStr(
            CFAttributedStringCreate(kCFAllocatorDefault, cfStr.get(), textAttrs.get())
        );
        if (!attrStr) {
            return {};
        }
        return CFPtr<CTLineRef>(CTLineCreateWithAttributedString(attrStr.get()));
    };

    auto measureText = [&](std::string_view sample) -> std::expected<float, std::string> {
        if (sample.empty()) {
            return 0.0f;
        }
        const CFPtr<CTLineRef> line = createCTLine(sample);
        if (!line) {
            return std::unexpected("CTLineCreateWithAttributedString(measure) failed");
        }
        const double width = CTLineGetTypographicBounds(line.get(), nullptr, nullptr, nullptr);
        return static_cast<float>(width);
    };

    auto pagesResult = layoutAndPaginate(text, options, measureText);
    if (!pagesResult) {
        return std::unexpected(pagesResult.error());
    }

    const CGFloat ascent = CTFontGetAscent(font.get());
    const CGFloat descent = CTFontGetDescent(font.get());
    const CGFloat lineHeight = static_cast<CGFloat>(options.lineHeight);
    const CGFloat baselineOffsetFromRowBottom = (lineHeight - (ascent + descent)) * 0.5 + descent;

    std::vector<std::vector<std::uint8_t>> renderedPages;
    renderedPages.reserve(pagesResult->size());

    for (const auto& pageRows : *pagesResult) {
        const std::size_t width = static_cast<std::size_t>(options.pageWidth);
        const std::size_t height = static_cast<std::size_t>(
            2 * options.padding + static_cast<int>(pageRows.size()) * options.lineHeight
        );

        const CFPtr<CGContextRef> ctx(CGBitmapContextCreate(
            nullptr,
            width,
            height,
            8,
            0,
            graySpace.get(),
            static_cast<CGBitmapInfo>(kCGImageAlphaNone)
        ));
        if (!ctx) {
            return std::unexpected("CGBitmapContextCreate failed");
        }

        CGContextSetShouldAntialias(ctx.get(), true);
        CGContextSetShouldSmoothFonts(ctx.get(), false);

        // White background
        CGContextSetGrayFillColor(ctx.get(), 1.0, 1.0);
        CGContextFillRect(ctx.get(), CGRectMake(0.0, 0.0, static_cast<CGFloat>(width), static_cast<CGFloat>(height)));

        for (std::size_t rowIdx = 0; rowIdx < pageRows.size(); ++rowIdx) {
            const VisualRow& row = pageRows[rowIdx];
            const CGFloat topFromTop = static_cast<CGFloat>(
                options.padding + static_cast<int>(rowIdx) * options.lineHeight
            );
            const CGFloat cgRowBottom = static_cast<CGFloat>(height) - (topFromTop + lineHeight);

            if (row.logicalLineIndex % 2 == 1) {
                CGContextSetGrayFillColor(ctx.get(), 238.0 / 255.0, 1.0);
                CGContextFillRect(ctx.get(), CGRectMake(0.0, cgRowBottom, static_cast<CGFloat>(width), lineHeight));
            }

            if (!row.text.empty()) {
                const CFPtr<CTLineRef> line = createCTLine(row.text);
                if (!line) {
                    return std::unexpected("CTLineCreateWithAttributedString(draw) failed");
                }
                const CGFloat left = static_cast<CGFloat>(
                    options.padding + (row.continued ? options.continuationIndent : 0)
                );
                const CGFloat baselineY = cgRowBottom + baselineOffsetFromRowBottom;

                CGContextSetTextMatrix(ctx.get(), CGAffineTransformIdentity);
                CGContextSetTextPosition(ctx.get(), left, baselineY);
                CTLineDraw(line.get(), ctx.get());
            }
        }

        const CFPtr<CGImageRef> image(CGBitmapContextCreateImage(ctx.get()));
        if (!image) {
            return std::unexpected("CGBitmapContextCreateImage failed");
        }

        const CFPtr<CFMutableDataRef> pngData(CFDataCreateMutable(kCFAllocatorDefault, 0));
        if (!pngData) {
            return std::unexpected("CFDataCreateMutable failed");
        }

        const CFPtr<CGImageDestinationRef> dest(CGImageDestinationCreateWithData(
            pngData.get(),
            CFSTR("public.png"),
            1,
            nullptr
        ));
        if (!dest) {
            return std::unexpected("CGImageDestinationCreateWithData(public.png) failed");
        }

        CGImageDestinationAddImage(dest.get(), image.get(), nullptr);
        if (!CGImageDestinationFinalize(dest.get())) {
            return std::unexpected("CGImageDestinationFinalize failed");
        }

        const CFIndex length = CFDataGetLength(pngData.get());
        const UInt8* ptr = CFDataGetBytePtr(pngData.get());
        std::vector<std::uint8_t> bytes;
        if (ptr && length > 0) {
            bytes.assign(ptr, ptr + length);
        }
        renderedPages.push_back(std::move(bytes));
    }

    return renderedPages;
}

#endif

} // namespace

std::expected<std::vector<std::vector<std::uint8_t>>, std::string>
renderTextToPngPages(std::string_view text, const TextImageRenderOptions& options) {
    auto sanitized = sanitizeOptions(options);
    if (!sanitized) {
        return std::unexpected(sanitized.error());
    }

#ifdef _WIN32
    return renderWindows(text, *sanitized);
#elif defined(__APPLE__)
    return renderMacOS(text, *sanitized);
#else
    return std::unexpected("renderTextToPngPages is not supported on this platform");
#endif
}

NAMESPACE_END(gpp)
