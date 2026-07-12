module GPPDefines;

namespace fs = std::filesystem;

const fs::path baseConfigPath = L"BaseConfig";
const fs::path globalConfigPath = baseConfigPath / L"GlobalConfig.toml";
const fs::path defaultPromptPath = baseConfigPath / L"Prompt.toml";
const fs::path defaultDictPath = baseConfigPath / L"Dicts";
const fs::path defaultGptDictPath = defaultDictPath / L"gpt";
const fs::path defaultPreDictPath = defaultDictPath / L"pre";
const fs::path defaultPostDictPath = defaultDictPath / L"post";
const fs::path pluginConfigsPath = baseConfigPath / L"PluginConfigs";
const fs::path filePluginConfigPath = pluginConfigsPath / L"FilePlugins";
const fs::path textPluginConfigPath = pluginConfigsPath / L"TextPlugins";
const std::wstring transCacheDirName = L"transl_cache";
const std::wstring otherCacheDirName = L"other_cache";

const std::string defaultRegCompileModifier = "mnS"; // m: 多行, n: unicode 支持, s: DotAll, S: jit编译
const std::string defaultRegReplaceModifier = "gxE"; // g: gloabl, x: ${n:-replace}/${n:+trueText:falseText} 语法支持, E: 未匹配的引用返回空字符串代替
