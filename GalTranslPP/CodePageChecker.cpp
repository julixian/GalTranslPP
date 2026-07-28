module;

#include "GPPMacros.hpp"
#include <unicode/ucnv_cb.h>
#include <utf8cpp/utf8.h>

module CodePageChecker;

import Tool;

namespace
{
    void U_CALLCONV codePageFromUCallback(
        const void* context,
        UConverterFromUnicodeArgs* fromUArgs,
        const UChar*,
        int32_t,
        UChar32 codePoint,
        UConverterCallbackReason reason,
        UErrorCode* pErrorCode)
    {
        if (reason != UCNV_UNASSIGNED) {
            return;
        }

        auto* unmappableCharsSet = (absl::btree_set<UChar32>*)context;
        unmappableCharsSet->insert(codePoint);

        // UCNV_UNASSIGNED 在这里是预期结果，写入替代字符后继续扫描后续输入。
        *pErrorCode = U_ZERO_ERROR;
        ucnv_cbFromUWriteSub(fromUArgs, 0, pErrorCode);
    }
}

void CodePageChecker::UConverterDeleter::operator()(UConverter* converter) const
{
    ucnv_close(converter);
}

CodePageChecker::CodePageChecker(const std::string& codePage)
    : m_codePage(codePage)
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

    const int8_t maxCharSize = ucnv_getMaxCharSize(m_codePageConverter.get());
    m_codePageConverterMaxCharSize = maxCharSize > 0 ? (size_t)maxCharSize : 2;

    ucnv_setFromUCallBack(m_codePageConverter.get(), codePageFromUCallback, &m_unmappableCharsSet,
        nullptr, nullptr, &status);
    if (U_FAILURE(status)) {
        throw std::runtime_error(gppTr("CodePageChecker.CodePageChecker", "无法设置 ICU 回调函数: %1")
            .arg(u_errorName(status))
            .toStdString());
    }
}

const std::string& CodePageChecker::findUnmappableChars(const std::string& transViewToCheck)
{
    m_unmappableCharsResult.clear();
    UErrorCode status = U_ZERO_ERROR;

    const size_t maxBufferSize = transViewToCheck.length() * m_codePageConverterMaxCharSize + 1;
    if (maxBufferSize > m_targetBuffer.size()) {
        m_targetBuffer.resize(maxBufferSize);
    }

    const char* sourcePtr = transViewToCheck.c_str();
    const char* sourceLimit = sourcePtr + transViewToCheck.length();
    char* targetPtr = (char*)m_targetBuffer.data();
    const char* targetLimit = targetPtr + m_targetBuffer.size();

    ucnv_convertEx(m_codePageConverter.get(), m_u8Converter.get(),
        &targetPtr, targetLimit,
        &sourcePtr, sourceLimit,
        nullptr, nullptr, nullptr, nullptr,
        true, true,
        &status);

    if (U_FAILURE(status)) {
        throw std::runtime_error(gppTr("CodePageChecker.findUnmappableChars", "ICU 转换发生意外错误: %1")
            .arg(u_errorName(status))
            .toStdString());
    }

    if (!m_unmappableCharsSet.empty()) {
        for (const UChar32 unmappableChar : m_unmappableCharsSet) {
            utf8::append(unmappableChar, m_unmappableCharsResult);
        }
        m_unmappableCharsSet.clear();
    }

    return m_unmappableCharsResult;
}
