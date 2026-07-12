module;

#include "GPPMacros.hpp"
#include <unicode/unistr.h>
#include <unicode/uchar.h>
#include <utf8cpp/utf8.h>
#include <toml.hpp>

module TextFull2Half;

import Tool;

namespace fs = std::filesystem;

TextFull2Half::TextFull2Half(const toml::value& projectConfig, const std::shared_ptr<spdlog::logger>& logger, PluginRunTime runTime)
    : m_logger(logger), m_runTime(runTime)
{
    try {

        bool reversePriority = false;
        const fs::path pluginConfigPath = [&]()
	        {
		        fs::path ret = textPluginConfigPath / std::format(L"TextFull2Half-{}.toml", ascii2Wide(pluginRunTimeNames[m_runTime]));
                if (!fs::exists(ret)) {
	                ret = textPluginConfigPath / L"TextFull2Half.toml";
                }
                else {
                    reversePriority = true;
                }
                return ret;
            }();
        const auto pluginConfig = toml::uparse(pluginConfigPath);

        m_replacePunctuation = parseToml<bool>(projectConfig, pluginConfig, "plugins.TextFull2Half.replacePunctuation", reversePriority);
        m_reverseConversion = parseToml<bool>(projectConfig, pluginConfig, "plugins.TextFull2Half.reverseConversion", reversePriority);

        createConversionMap();

        const std::string excludeChars = parseToml<std::string>(projectConfig, pluginConfig,
            "plugins.TextFull2Half.excludeChars", reversePriority);
        const icu::UnicodeString uExcludeChars = icu::UnicodeString::fromUTF8(excludeChars);
        for (int32_t i = 0; i < uExcludeChars.length();) {
            const UChar32 c = uExcludeChars.char32At(i);
            m_conversionMap.erase(c);
            i += U16_LENGTH(c);
        }

        const auto notConvertRegStrs = 
            parseToml<std::vector<std::string>>(projectConfig, pluginConfig, "plugins.TextFull2Half.notConvertRegs", reversePriority);
        for (const auto& regStr : notConvertRegStrs) {
            jpc::Regex reg(regStr, defaultRegCompileModifier);
            if (!reg) {
                throw std::invalid_argument(gppTr(
                    "TextFull2Half.TextFull2Half",
                    "TextFull2Half 正则编译错误: %1")
                    .arg(regStr)
                    .toStdString());
            }
            m_notConvertRegs.push_back(std::move(reg));
        }

        m_notConvertedCharsKey = std::format("TF2H-{}-notConvertedChars", pluginRunTimeNames[m_runTime]);

        m_logger->info(gppTr(
            "TextFull2Half.TextFull2Half",
            "TextFull2Half-%1 已加载 - 替换标点: %2, 反向替换: %3")
            .arg(pluginRunTimeNames[m_runTime])
            .arg(m_replacePunctuation ? "true" : "false")
            .arg(m_reverseConversion ? "true" : "false")
            .toStdString());
    }
    catch (const toml::exception& e) {
        throw std::runtime_error(gppTr(
            "TextFull2Half.TextFull2Half",
            "TextFull2Half-%1 配置文件解析错误: %2")
            .arg(pluginRunTimeNames[m_runTime])
            .arg(e.what())
            .toStdString());
    }
}

void TextFull2Half::createConversionMap() {
    // 数字转换
    for (char32_t i = 0; i < 10; ++i) {
        m_conversionMap[U'０' + i] = U'0' + i;
    }

    // 大写字母
    for (char32_t i = 0; i < 26; ++i) {
        m_conversionMap[U'Ａ' + i] = U'A' + i;
    }

    // 小写字母 
    for (char32_t i = 0; i < 26; ++i) {
        m_conversionMap[U'ａ' + i] = U'a' + i;
    }

    // 标点符号
    if (m_replacePunctuation) {
        m_conversionMap[U'。'] = U'.';
        m_conversionMap[U'，'] = U',';
        m_conversionMap[U'、'] = U'\\';
        m_conversionMap[U'；'] = U';';
        m_conversionMap[U'：'] = U':';
        m_conversionMap[U'？'] = U'?';
        m_conversionMap[U'！'] = U'!';
        m_conversionMap[U'（'] = U'(';
        m_conversionMap[U'）'] = U')';
        m_conversionMap[U'【'] = U'[';
        m_conversionMap[U'】'] = U']';
        m_conversionMap[U'《'] = U'<';
        m_conversionMap[U'》'] = U'>';
        m_conversionMap[U'「'] = U'[';
        m_conversionMap[U'」'] = U']';
        m_conversionMap[U'『'] = U'{';
        m_conversionMap[U'』'] = U'}';
        m_conversionMap[U'〔'] = U'[';
        m_conversionMap[U'〕'] = U']';
        m_conversionMap[U'｛'] = U'{';
        m_conversionMap[U'｝'] = U'}';
        m_conversionMap[U'［'] = U'[';
        m_conversionMap[U'］'] = U']';
        m_conversionMap[U'％'] = U'%';
        m_conversionMap[U'＋'] = U'+';
        m_conversionMap[U'－'] = U'-';
        m_conversionMap[U'＊'] = U'*';
        m_conversionMap[U'／'] = U'/';
        m_conversionMap[U'＝'] = U'=';
        m_conversionMap[U'＜'] = U'<';
        m_conversionMap[U'＞'] = U'>';
        m_conversionMap[U'＆'] = U'&';
        m_conversionMap[U'＄'] = U'$';
        m_conversionMap[U'＃'] = U'#';
        m_conversionMap[U'＠'] = U'@';
        m_conversionMap[U'＾'] = U'^';
        m_conversionMap[U'｜'] = U'|';
        m_conversionMap[U'｀'] = U'`';
        m_conversionMap[U'　'] = U' '; // 全角空格
        m_conversionMap[U'…'] = U'.';
        m_conversionMap[U'—'] = U'-';
        m_conversionMap[U'－'] = U'-';
        m_conversionMap[U'ー'] = U'-';
        m_conversionMap[U'・'] = U'·';
        m_conversionMap[U'′'] = U'\'';
        m_conversionMap[U'″'] = U'"';
        m_conversionMap[U'〜'] = U'~';
        m_conversionMap[U'～'] = U'~';
        m_conversionMap[U'￥'] = U'¥';
        m_conversionMap[U'￠'] = U'¢';
        m_conversionMap[U'￡'] = U'£';
    }

    // 反向转换(半角→全角)
    if (m_reverseConversion) {
        auto originalMap = m_conversionMap; // 保存原有映射
        m_conversionMap.clear(); // 清空map

        // 构建反向映射(值→键)
        for (const auto& [full, half] : originalMap) {
            m_conversionMap[half] = full;
        }

        // 特殊处理
        m_conversionMap[U'-'] = U'－'; // 半角横线优先转全角连字符
        m_conversionMap[U'.'] = U'。';
    }
}

std::string TextFull2Half::convertText(const std::string& text, Sentence* se, bool jumpTag) {

    std::string result;
    result.reserve(text.length());

    const uint8_t* s = (uint8_t*)text.c_str();
    const int32_t length = (int32_t)text.length();
    int32_t i = 0;
    UChar32 c;

    std::vector<uint8_t> notConvertFlags(text.length(), 0);
    if (!m_notConvertRegs.empty()) {
        jpcre2::VecOff startOffVec;
        jpcre2::VecOff endOffVec;
        jpc::RegexMatch rm;
        rm.setModifier("g").setMatchStartOffsetVector(&startOffVec).setMatchEndOffsetVector(&endOffVec).setSubject(&text);
        for (auto& reg : m_notConvertRegs) {
            if (rm.setRegexObject(&reg).match() > 0) {
                for (const auto& [startOff, endOff] : std::views::zip(startOffVec, endOffVec)) {
                    std::memset(notConvertFlags.data() + startOff, 1, endOff - startOff);
                }
            }
        }
        const std::string notConvertedChars = std::views::iota(0uz, notConvertFlags.size()) 
    	    | std::views::filter([&](const auto& index){ return notConvertFlags[index] != 0;}) 
    	    | std::views::transform([&](const auto& index) { return text[index]; }) | std::ranges::to<std::string>();
        if (!notConvertedChars.empty()) {
            se->otherInfo[m_notConvertedCharsKey] = notConvertedChars;
        }
    }

    while (i < length) {

        if (jumpTag && (text[i] == '<' || text[i] == '(')) {
            std::string_view sv = std::string_view(text).substr(i);
            if (sv.starts_with("<tab>")) {
                result.append("<tab>");
                i += 5;
                continue;
            }
            if (sv.starts_with("<br>")) {
                result.append("<br>");
                i += 4;
                continue;
            }
            if (sv.starts_with("(Failed to translate)")) {
                result.append("(Failed to translate)");
                i += 21;
                continue;
            }
        }

        if (notConvertFlags[i]) {
            U8_NEXT(s, i, length, c);
            utf8::unchecked::append(c, std::back_inserter(result));
            continue;
        }

        U8_NEXT(s, i, length, c);

        if (const auto it = m_conversionMap.find(c); it != m_conversionMap.end()) {
            if (c == U'…') {
                result.append("...");
            }
            else if (c == U'.' && i + 1 < length && text[i] == '.' && text[i + 1] == '.') {
                result.append("…");
                i += 2;
            }
            else {
                utf8::unchecked::append(it->second, std::back_inserter(result));
            }
        }
        else {
            utf8::unchecked::append(c, std::back_inserter(result));
        }
    }

    return result;
}

void TextFull2Half::dPreRun(Sentence* se) {
    if (m_runTime != PluginRunTime::DPre) {
        return;
    }
    se->preproc = convertText(se->preproc, se, false);
}

void TextFull2Half::preRun(Sentence* se) {
    if (m_runTime != PluginRunTime::Pre) {
        return;
    }
    se->preproc = convertText(se->preproc, se, true);
}

void TextFull2Half::postRun(Sentence* se) {
    if (m_runTime != PluginRunTime::Post) {
        return;
    }
    se->transview = convertText(se->transview, se, true);
}

void TextFull2Half::dPostRun(Sentence* se) {
    if (m_runTime != PluginRunTime::DPost) {
        return;
    }
    se->transview = convertText(se->transview, se, false);
}
