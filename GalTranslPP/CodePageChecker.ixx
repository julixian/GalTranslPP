module;

#include "GPPMacros.hpp"
#include <unicode/unistr.h>
#include <unicode/uchar.h>
#include <unicode/ucnv.h>
#include <unicode/ucnv_cb.h>
#include <toml.hpp>

export module CodePageChecker;

export import Tool;

namespace fs = std::filesystem;

export
{
    void U_CALLCONV codePageFromUCallback(
        const void* context,
        UConverterFromUnicodeArgs* fromUArgs,
        const UChar* codeUnits,
        int32_t length,
        UChar32 codePoint,
        UConverterCallbackReason reason,
        UErrorCode* pErrorCode);

    struct UConverterDeleter {
        void operator()(UConverter* converter) const {
            ucnv_close(converter);
        }
    };
    using UConverterPtr = std::unique_ptr<UConverter, UConverterDeleter>;

    class CodePageChecker {
    private:
        std::string m_codePage;
        std::string m_unmappableCharsResult;
        UConverterPtr m_u8Converter;
        UConverterPtr m_codePageConverter;
        std::weak_ptr<spdlog::logger> m_logger;

    public:
        CodePageChecker(const std::string& codePage, const std::shared_ptr<spdlog::logger>& logger);

        const std::string& getCodePage() const { return m_codePage; }

        const std::string& findUnmappableChars(const std::string& transViewToCheck);
    };
}

module :private;

CodePageChecker::CodePageChecker(const std::string& codePage, const std::shared_ptr<spdlog::logger>& logger)
    : m_codePage(codePage), m_logger(logger)
{
    UErrorCode status = U_ZERO_ERROR;
    m_u8Converter.reset(ucnv_open("utf-8", &status));
    if (U_FAILURE(status)) {
        throw std::runtime_error(gppTr("CodePageChecker.CodePageChecker", "无法创建 ICU u8 转换器: %1")
            .arg(u_errorName(status))
            .toStdString());
    }
    m_codePageConverter.reset(ucnv_open(m_codePage.c_str(), &status));
    if (U_FAILURE(status)) {
        throw std::runtime_error(gppTr("CodePageChecker.CodePageChecker", "无法创建 ICU %1 转换器: %2")
            .arg(m_codePage)
            .arg(u_errorName(status))
            .toStdString());
    }
    ucnv_setFromUCallBack(m_codePageConverter.get(), codePageFromUCallback, &m_unmappableCharsResult, nullptr, nullptr, &status);
    if (U_FAILURE(status)) {
        throw std::runtime_error(gppTr("CodePageChecker.CodePageChecker", "无法设置 ICU 回调函数: %1")
            .arg(u_errorName(status))
            .toStdString());
    }
}

/**
 * @brief ICU 回调函数，用于记录不能被转换为codePage的字符。
 *
 * @param context 指向 std::string 的指针，用于记录不能被转换的字符。
 * @param fromUArgs 转换参数。
 * @param codeUnits 输入的码点序列。
 * @param length 码点序列长度。
 * @param codePoint 输入的码点。
 * @param reason 回调原因。
 * @param pErrorCode 错误码。
 */
void U_CALLCONV codePageFromUCallback(
    const void* context,
    UConverterFromUnicodeArgs* fromUArgs,
    const UChar* codeUnits,
    int32_t length,
    UChar32 codePoint,
    UConverterCallbackReason reason,
    UErrorCode* pErrorCode)
{
    if (reason == UCNV_UNASSIGNED) {
        std::string* unmappableCharsStr = static_cast<std::string*>(const_cast<void*>(context));

        // 使用 U8_APPEND 宏直接将码点追加到 std::string 中
        const size_t currentLength = unmappableCharsStr->length();
        unmappableCharsStr->resize(currentLength + U8_MAX_LENGTH); // 预留足够空间
        char* bufferPtr = &(*unmappableCharsStr)[currentLength];
        int32_t i = 0; // U8_APPEND 需要一个索引变量
        // U8_APPEND_UNSAFE 是不进行边界检查的版本，速度最快
        // 在这里我们已经确保了空间足够，所以可以使用它
        U8_APPEND_UNSAFE(bufferPtr, i, codePoint);
        // 根据实际写入的字节数调整string的大小
        unmappableCharsStr->resize(currentLength + i);
    }
    ucnv_cbFromUWriteSub(fromUArgs, 0, pErrorCode);
}

/**
 * @brief 查找一个UTF-8字符串中所有不能被转换为codePage的字符。
 *
 * @param transViewToCheck 输入的UTF-8编码的std::string。
 */
const std::string& CodePageChecker::findUnmappableChars(const std::string& transViewToCheck) {
    m_unmappableCharsResult.clear();
    UErrorCode status = U_ZERO_ERROR;
    // 目标缓冲区可以很小，因为我们不关心成功转换的结果
    std::vector<char> targetBuffer(transViewToCheck.length() + 1);

    // 正确设置ucnv_convertEx的参数
    const char* sourcePtr = transViewToCheck.c_str();
    const char* sourceLimit = sourcePtr + transViewToCheck.length();
    char* targetPtr = targetBuffer.data();
    const char* targetLimit = targetPtr + targetBuffer.size();

    ucnv_convertEx(m_codePageConverter.get(), m_u8Converter.get(),
        &targetPtr, targetLimit,
        &sourcePtr, sourceLimit,
        nullptr, nullptr, nullptr, nullptr,
        1, 1,
        &status);

    // U_INVALID_CHAR_FOUND 错误是预期的，因为它会触发回调。
    // U_BUFFER_OVERFLOW_ERROR 也可能发生，但我们不关心，因为我们不使用目标缓冲区的结果。
    // 我们只关心其他意外的严重错误。
    if (U_FAILURE(status) && status != U_INVALID_CHAR_FOUND && status != U_BUFFER_OVERFLOW_ERROR) {
        throw std::runtime_error(gppTr("CodePageChecker.findUnmappableChars", "ICU 转换发生意外错误: %1")
            .arg(u_errorName(status))
            .toStdString());
    }
    return m_unmappableCharsResult;
}
