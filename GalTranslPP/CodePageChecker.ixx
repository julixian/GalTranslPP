module;

#include <unicode/uchar.h>
#include <unicode/ucnv.h>

export module CodePageChecker;

export import GPPDefines;

export
{
    class CodePageChecker {
    private:
        struct UConverterDeleter {
            void operator()(UConverter* converter) const;
        };
        using UConverterPtr = std::unique_ptr<UConverter, UConverterDeleter>;

        std::vector<uint8_t> m_targetBuffer;
        std::string m_codePage;
        absl::btree_set<UChar32> m_unmappableCharsSet;
        std::string m_unmappableCharsResult;
        UConverterPtr m_u8Converter;
        UConverterPtr m_codePageConverter;
        size_t m_codePageConverterMaxCharSize;

    public:
        explicit CodePageChecker(const std::string& codePage);

        const std::string& findUnmappableChars(const std::string& transViewToCheck);
    };
}