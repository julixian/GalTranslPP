module;

// CLD3 会经由 Protobuf 引入 Abseil，因此只能在这个不导入项目模块的实现单元中出现。
#pragma warning(push)
#pragma warning(disable: 4244)
#pragma warning(disable: 4251)
#pragma warning(disable: 4267)
#include <cld3/nnet_language_identifier.h>
#pragma warning(pop)

module NNetLanguageIdentifierWrapper;

class NNetLanguageIdentifierWrapper::Impl {
public:
    Impl(int minNumBytes, int maxNumBytes)
        : identifier(minNumBytes, maxNumBytes)
    {

    }

    chrome_lang_id::NNetLanguageIdentifier identifier;
};

NNetLanguageIdentifierWrapper::NNetLanguageIdentifierWrapper(int minNumBytes, int maxNumBytes)
    : m_impl(std::make_unique<Impl>(minNumBytes, maxNumBytes))
{

}

NNetLanguageIdentifierWrapper::~NNetLanguageIdentifierWrapper() = default;

std::vector<NNetLanguageResult> NNetLanguageIdentifierWrapper::findTopNMostFreqLangs(
    const std::string& text, int numLangs)
{
	std::vector<chrome_lang_id::NNetLanguageIdentifier::Result> nativeResults =
        m_impl->identifier.FindTopNMostFreqLangs(text, numLangs);

    std::vector<NNetLanguageResult> results;
    results.reserve(nativeResults.size());
    for (auto& result : nativeResults) {
        const bool isUnknown = result.language == chrome_lang_id::NNetLanguageIdentifier::kUnknown;
        results.push_back({
            std::move(result.language),
            result.probability,
            isUnknown
        });
    }
    return results;
}
