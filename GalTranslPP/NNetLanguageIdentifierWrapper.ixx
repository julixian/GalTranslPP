export module NNetLanguageIdentifierWrapper;

export import std.compat;

export
{
    // 仅暴露问题分析实际需要的结果，避免 CLD3、Protobuf 和 Abseil 类型进入其它模块。
    struct NNetLanguageResult {
        std::string language;
        float probability{};
        bool isUnknown{};
    };

    class NNetLanguageIdentifierWrapper {
    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;

    public:
        NNetLanguageIdentifierWrapper(int minNumBytes, int maxNumBytes);
        ~NNetLanguageIdentifierWrapper();

        std::vector<NNetLanguageResult> findTopNMostFreqLangs(const std::string& text, int numLangs);
    };
}
