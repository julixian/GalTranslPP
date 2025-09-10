module;

#include <spdlog/spdlog.h>
#include <toml++/toml.hpp>
#include <unicode/unistr.h>
#include <unicode/uchar.h>

import std;
import Tool;
import IPlugin;
export module TextPostFull2Half;
namespace fs = std::filesystem;

export {
    class TextPostFull2Half : public IPlugin {
    private:
        bool m_replacePunctuation;
        bool m_reverseConversion;
        std::unordered_map<std::string, std::string> m_customMap;
        std::unordered_map<char32_t, char32_t> m_conversionMap;

        void createConversionMap();
        std::string convertText(const std::string& text);

    public:
        TextPostFull2Half(const fs::path& projectDir, std::shared_ptr<spdlog::logger> logger);
        virtual void run(Sentence* se) override;
        virtual ~TextPostFull2Half() = default;
    };
}

module :private;

TextPostFull2Half::TextPostFull2Half(const fs::path& projectDir, std::shared_ptr<spdlog::logger> logger)
    : IPlugin(projectDir, logger)
{
    try {
        auto projectConfig = toml::parse_file((projectDir / "config.toml").string());
        auto pluginConfig = toml::parse_file((pluginConfigsPath / "textPostPlugins/TextPostFull2Half.toml").string());

        m_replacePunctuation = parseToml<bool>(projectConfig, pluginConfig, "plugins.TextPostFull2Half.是否替换标点");
        m_reverseConversion = parseToml<bool>(projectConfig, pluginConfig, "plugins.TextPostFull2Half.反向替换");

        createConversionMap();
        m_logger->info("译后全角半角转换插件已加载 - 替换标点: {}, 反向替换: {}",
                      m_replacePunctuation, m_reverseConversion);
    } catch (const toml::parse_error& e) {
        m_logger->critical("配置文件解析错误: {}", e.description());
        throw;
    }
}

void TextPostFull2Half::createConversionMap() {
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
        m_conversionMap[U'～'] = U'~';
        m_conversionMap[U'｀'] = U'`';
        m_conversionMap[U'　'] = U' '; // 全角空格
        m_conversionMap[U'…'] = U'.'; m_conversionMap[U'…'] = U'.'; m_conversionMap[U'…'] = U'.'; // 转为3个点
        m_conversionMap[U'—'] = U'-';
        m_conversionMap[U'－'] = U'-';
        m_conversionMap[U'ー'] = U'-';
        m_conversionMap[U'・'] = U'·';
        m_conversionMap[U'·'] = U'·';
        m_conversionMap[U'′'] = U'\'';
        m_conversionMap[U'″'] = U'"';
        m_conversionMap[U'〜'] = U'~';  
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
            // 处理一个全角对应多个半角的情况(如省略号)
            if (full == U'…') continue; // 特殊处理
            
            m_conversionMap[half] = full;
        }
        
        // 特殊处理横线符号
        m_conversionMap[U'-'] = U'－'; // 半角横线优先转全角连字符
        
        // 特殊处理省略号
        if (originalMap.contains(U'…')) {
            m_conversionMap[U'.'] = U'…'; // 半角点→全角省略号
        }
    }
}

std::string TextPostFull2Half::convertText(const std::string& text) {
    std::string result;
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(text);
    for (int32_t i = 0; i < ustr.length();) {
        UChar32 c = ustr.char32At(i);
        i += U16_LENGTH(c);
        
        if (auto it = m_conversionMap.find(c); it != m_conversionMap.end()) {
            UChar32 converted = static_cast<UChar32>(it->second);
            icu::UnicodeString::fromUTF32(&converted, 1).toUTF8String(result);
        } else {
            UChar32 uc = static_cast<UChar32>(c);
            icu::UnicodeString::fromUTF32(&uc, 1).toUTF8String(result);
        }
    }
    return result;
}

void TextPostFull2Half::run(Sentence* se) {
    se->pre_translated_text = convertText(se->pre_translated_text);
}
