<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="en_US" sourcelanguage="zh_CN">
<context>
    <name>APIPool.apiLogPrefix</name>
    <message>
        <location filename="APIPool.cpp" line="+96"/>
        <source>[线程 %1] [文件 %2] [模型 %3] [HTTP %4]</source>
        <translation>[Thread %1] [File %2] [Model %3] [HTTP %4]</translation>
    </message>
</context>
<context>
    <name>APIPool.loadApis</name>
    <message>
        <location line="-74"/>
        <source>令牌池新加载 %1 个 API keys， 现共有 %2 个API keys</source>
        <translation>Token pool loaded %1 API keys; total %2.</translation>
    </message>
</context>
<context>
    <name>APIPool.reportProblem</name>
    <message>
        <location line="+58"/>
        <source>API key [%1] 已被标记为不可用。</source>
        <translation>API key [%1] marked unavailable.</translation>
    </message>
</context>
<context>
    <name>APITool.performApiRequest</name>
    <message>
        <location filename="APITool.cpp" line="+90"/>
        <source>正在使用系统代理: [%1]</source>
        <translation>Using system proxy: [%1]</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>[线程 %1] 接收到流式数据块: %2</source>
        <translation>[Thread %1] received stream chunk: %2</translation>
    </message>
</context>
<context>
    <name>CodePageChecker.CodePageChecker</name>
    <message>
        <location filename="CodePageChecker.ixx" line="+59"/>
        <source>无法创建 ICU u8 转换器: %1</source>
        <translation>Failed to create ICU u8 converter: %1</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>无法创建 ICU %1 转换器: %2</source>
        <translation>Failed to create ICU %1 converter: %2</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>无法设置 ICU 回调函数: %1</source>
        <translation>Failed to set ICU callback: %1</translation>
    </message>
</context>
<context>
    <name>CodePageChecker.findUnmappableChars</name>
    <message>
        <location line="+71"/>
        <source>ICU 转换发生意外错误: %1</source>
        <translation>Unexpected ICU conversion error: %1</translation>
    </message>
</context>
<context>
    <name>ConditionTool.createGppCondition</name>
    <message>
        <location filename="ConditionTool.ixx" line="+59"/>
        <source>正则表达式编译失败: [%1]</source>
        <translation>Regex compile failed: [%1]</translation>
    </message>
</context>
<context>
    <name>ConditionTool.getCheckSeCondFunc</name>
    <message>
        <location line="+89"/>
        <source>执行Lua条件函数 %1 时发生错误: %2</source>
        <translation>Error running Lua condition %1: %2</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>注册Lua脚本 %1 中的条件函数 %2 失败</source>
        <translation>Failed to register condition %2 in Lua script %1</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>执行Python条件函数 %1 时发生错误: %2</source>
        <translation>Error running Python condition %1: %2</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>注册Python脚本 %1 中的条件函数 %2 失败</source>
        <translation>Failed to register condition %2 in Python script %1</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>未知的条件类型</source>
        <translation>Unknown condition type</translation>
    </message>
</context>
<context>
    <name>DictionaryGenerator.callLLMToGenerate</name>
    <message>
        <location filename="DictionaryGenerator.cpp" line="+282"/>
        <source>没有可用的 API key 了</source>
        <translation>No API key left</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>[线程 %1] 开始从段落中生成术语表:
%2</source>
        <translation>[Thread %1] generating glossary from text:
%2</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>字典生成——段落输入</source>
        <translation>Dict gen - segment input</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>[线程 %1] AI 字典生成成功:
 %2</source>
        <translation>[Thread %1] AI glossary generated:
 %2</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>发现重复术语: %1	%2	%3</source>
        <translation>Duplicate term found: %1	%2	%3</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>[线程 %1] AI 字典生成失败，已达到最大重试次数。</source>
        <translation>[Thread %1] AI glossary generation failed; max retries reached.</translation>
    </message>
</context>
<context>
    <name>DictionaryGenerator.finalizeCoarseCandidates</name>
    <message>
        <location line="+37"/>
        <source>，与其它字典存在性别争议</source>
        <translation>, gender differs from other dicts</translation>
    </message>
</context>
<context>
    <name>DictionaryGenerator.generate</name>
    <message>
        <location line="+14"/>
        <source>没有输入文件，无法生成字典。</source>
        <translation>No input files; cannot build dict.</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>任务终止，将不会生成字典文件。</source>
        <translation>Task stopped; no dict will be written.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>阶段三：启动 %1 个线程，向 AI 发送 %2 个任务...</source>
        <translation>Stage 3: starting %1 threads, sending %2 tasks to AI...</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>任务终止，将保存已经生成的字典结果。</source>
        <translation>Task stopped; saving generated dict.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>阶段四：整理并保存结果...</source>
        <translation>Step 4: sort and save...</translation>
    </message>
    <message>
        <location line="+35"/>
        <source>阶段四：Review Agent 审校完成，使用审校后的字典结果。</source>
        <translation>Step 4: Review Agent done; using reviewed dict.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>阶段四：Review Agent 失败，将回退到粗候选整理结果。错误: %1</source>
        <translation>Stage 4: Review Agent failed; falling back to rough candidates. Error: %1</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>字典生成完成，共 %1 个词语，已保存到 %2</source>
        <translation>Dictionary complete: %1 terms, saved to %2</translation>
    </message>
</context>
<context>
    <name>DictionaryGenerator.preprocessAndTokenize</name>
    <message>
        <location line="-456"/>
        <source>阶段一：预处理和分词...</source>
        <translation>Step 1: preprocess and tokenize...</translation>
    </message>
    <message>
        <location line="+76"/>
        <source>共分割成 %1 个文本块，开始进行分词(使用依赖 Python 且未进行 GPU加速 的分词器这步会非常慢)...</source>
        <translation>Split into %1 text chunks; starting tokenization (Python-based, no GPU, may be slow)...</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>原文: %1
分词实体结果: %2</source>
        <translation>Source: %1
Tokenized entities: %2</translation>
    </message>
</context>
<context>
    <name>DictionaryGenerator.solveSentenceSelection</name>
    <message>
        <location line="+45"/>
        <source>阶段二：搜索并选择信息量最大的文本块(单线程)...</source>
        <translation>Step 2: select richest blocks (single thread)...</translation>
    </message>
</context>
<context>
    <name>DictionaryReviewAgent.applyDecisionEntry</name>
    <message>
        <location filename="DictionaryReviewAgent.cpp" line="+908"/>
        <source>commit.source_term=%1 与当前术语 %2 不匹配</source>
        <translation>commit.source_term=%1 does not match current term %2</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>无效状态: %1</source>
        <translation>Invalid status: %1</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>status=%1 时必须提供 final_target</source>
        <translation>final_target required when status=%1</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>status=conflict 时必须提供 final_note</source>
        <translation>final_note required when status=conflict</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>status=merged 时必须提供 merge_into</source>
        <translation>merge_into required when status=merged</translation>
    </message>
    <message>
        <location line="+36"/>
        <source>建议合并目标 %1 不存在；已保留为冲突以供手动审校</source>
        <translation>Merge target %1 not found; kept as conflict for manual review</translation>
    </message>
</context>
<context>
    <name>DictionaryReviewAgent.review</name>
    <message>
        <location line="+48"/>
        <source>GenDict Review Agent 收到停止信号，剩余术语将使用本地回退。</source>
        <translation>GenDict Review Agent stopped; remaining terms use local fallback.</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>[线程 %1] GenDict Review Agent reviewing term %2/%3: %4, %5 target candidates, %6 note candidates, %7 occurrences.</source>
        <translation>[Thread %1] GenDict Review Agent reviewing term %2/%3: %4, %5 target candidates, %6 note candidates, %7 occurrences.</translation>
    </message>
    <message>
        <location line="+58"/>
        <source>GenDict Review Agent 术语 %1 第 %2/%3 轮返回无效响应，第 %4/%5 次重试。错误: %6。原始响应: %7</source>
        <translation>GenDict Review Agent term %1 round %2/%3 returned invalid response, retry %4/%5. Error: %6. Raw: %7</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>GenDict Review Agent 术语 %1 第 %2/%3 轮返回 action=&apos;%4&apos;。</source>
        <translation>GenDict Review Agent term %1 round %2/%3 returned action=&apos;%4&apos;.</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>GenDict Review Agent 术语 %1 第 %2/%3 轮返回空 tool_calls，第 %4/%5 次重试。</source>
        <translation>GenDict Review Agent term %1 round %2/%3 returned empty tool_calls, retry %4/%5.</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>GenDict Review Agent 术语 %1 请求了 %2 个工具调用: %3。</source>
        <translation>GenDict Review Agent term %1 requested %2 tool calls: %3.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>GenDict Review Agent 术语 %1 工具结果: %2。</source>
        <translation>GenDict Review Agent term %1 tool result: %2.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>GenDict Review Agent 术语 %1 工具结果:
%2</source>
        <translation>GenDict Review Agent term %1 tool results:
%2</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>GenDict Review Agent 术语 %1 选择 skip，使用本地回退 target=&apos;%2&apos;，note_chars=%3。</source>
        <translation>GenDict Review Agent term %1 chose skip; local fallback target=&apos;%2&apos;, note_chars=%3.</translation>
    </message>
    <message>
        <location line="+27"/>
        <source>GenDict Review Agent 术语 %1 commit 已接受: %2。</source>
        <translation>GenDict Review Agent term %1 commit accepted: %2.</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>GenDict Review Agent 术语 %1 commit 成功:
%2</source>
        <translation>GenDict Review Agent term %1 commit ok:
%2</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>GenDict Review Agent 术语 %1 commit 校验失败，第 %2/%3 次重试。错误: %4。原始响应: %5</source>
        <translation>GenDict Review Agent term %1 commit validation failed, retry %2/%3. Error: %4. Raw: %5</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>GenDict Review Agent 术语 %1 返回未知 action &apos;%2&apos;，第 %3/%4 次重试。</source>
        <translation>GenDict Review Agent term %1 returned unknown action &apos;%2&apos;, retry %3/%4.</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>GenDict Review Agent 术语 %1 已达到最大轮数限制(%2)，使用本地回退。</source>
        <translation>GenDict Review Agent term %1 reached max rounds (%2); using local fallback.</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>GenDict Review Agent 启动 %1 个审校 worker 处理 %2 个术语。</source>
        <translation>GenDict Review Agent started %1 review workers for %2 terms.</translation>
    </message>
    <message>
        <location line="+42"/>
        <source>原合并目标 %1 未被保留；已恢复为手动审校</source>
        <translation>Original merge target %1 was not kept; restored to manual review</translation>
    </message>
    <message>
        <location line="+46"/>
        <source>GenDict Review Agent 完成。最终保留术语数: %1。</source>
        <translation>GenDict Review Agent done. Final kept terms: %1.</translation>
    </message>
</context>
<context>
    <name>DictionaryReviewAgent.reviewTerm</name>
    <message>
        <location line="-300"/>
        <source>没有可用的 API key 了</source>
        <translation>No API key left</translation>
    </message>
</context>
<context>
    <name>EpubTranslator.EpubTranslator</name>
    <message>
        <location filename="EpubTranslator.cpp" line="+54"/>
        <source>GalTransl++ EpubTranslator 启动...</source>
        <translation>GalTransl++ EpubTranslator started...</translation>
    </message>
</context>
<context>
    <name>EpubTranslator.epubBeforeRun</name>
    <message>
        <location line="+101"/>
        <source>已创建目录: %1</source>
        <translation>Created dir: %1</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>未找到 EPUB 文件</source>
        <translation>EPUB file not found</translation>
    </message>
    <message>
        <location line="+37"/>
        <source>正在解压 %1 到 %2</source>
        <translation>Extracting %1 to %2</translation>
    </message>
    <message>
        <location line="+78"/>
        <source>[文件 %1] 未找到对应的元数据</source>
        <translation>[File %1] matching metadata not found</translation>
    </message>
    <message>
        <location line="+36"/>
        <source>[文件 %1] 元数据和翻译数据数量不匹配，无法重组(%2meta/%3trans)</source>
        <translation>[File %1] metadata/translation count mismatch; cannot rebuild (%2meta/%3trans)</translation>
    </message>
    <message>
        <location line="+47"/>
        <source>正在打包 %1</source>
        <translation>Packing %1</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>无法创建 EPUB (zip) 文件: %1</source>
        <translation>Failed to create EPUB (zip): %1</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>无法为 mimetype 创建 zip_source_file</source>
        <translation>Failed to create zip_source_file for mimetype</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>无法将 mimetype 添加到 zip</source>
        <translation>Failed to add mimetype to zip</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>无法将 mimetype 设置为不压缩模式。</source>
        <translation>Failed to store mimetype uncompressed.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>在源目录 %1 中未找到 mimetype 文件，生成的 EPUB 可能无效。</source>
        <translation>mimetype not found in source dir %1; EPUB may be invalid.</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>无法为文件 %1 创建 zip_source_file</source>
        <translation>Failed to create zip_source_file for %1</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>无法将文件 %1 添加到 zip</source>
        <translation>Failed to add file %1 to zip</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>关闭 zip 存档时出错: %1</source>
        <translation>Error closing zip archive: %1</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>已重建 EPUB 文件: %1</source>
        <translation>Rebuilt EPUB: %1</translation>
    </message>
</context>
<context>
    <name>EpubTranslator.epubInit</name>
    <message>
        <location line="-377"/>
        <source>预处理正则编译失败: %1</source>
        <translation>Preprocess regex compile failed: %1</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>预处理正则回调正则编译失败: [%1]</source>
        <translation>Preprocess callback regex compile failed: [%1]</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>Epub 配置文件解析失败: %1</source>
        <translation>Failed to parse Epub config: %1</translation>
    </message>
</context>
<context>
    <name>EpubTranslator.~EpubTranslator</name>
    <message>
        <location line="-100"/>
        <source>所有任务已完成！EpubTranslator 结束。</source>
        <oldsource>所有任务已完成！EpubTranslator结束。</oldsource>
        <translation>All tasks complete! EpubTranslator finished.</translation>
    </message>
</context>
<context>
    <name>GptDictionary.checkDictUse</name>
    <message>
        <location filename="Dictionary.cpp" line="+219"/>
        <source>GPT字典 %1-&gt;%2 未使用，但使用了 %3-&gt;%4 这一包含性字典</source>
        <translation>GPT dict %1-&gt;%2 unused, but inclusive dict %3-&gt;%4 used</translation>
    </message>
    <message>
        <location line="+13"/>
        <location line="+38"/>
        <source>GPT字典 %1-&gt;%2 未使用</source>
        <translation>GPT dict %1-&gt;%2 unused</translation>
    </message>
</context>
<context>
    <name>GptDictionary.getPrompt</name>
    <message>
        <location line="-178"/>
        <location line="+26"/>
        <source>无效的提示词类型</source>
        <translation>Invalid prompt type</translation>
    </message>
</context>
<context>
    <name>GptDictionary.loadFromFile</name>
    <message>
        <location line="+8"/>
        <source>GPT 字典文件不存在: %1</source>
        <translation>GPT dictionary file not found: %1</translation>
    </message>
    <message>
        <location line="+35"/>
        <source>GPT 字典文件解析错误: %1: %2</source>
        <translation>GPT dictionary parse error: %1: %2</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>已加载 GPT 字典: %1, 共 %2 个词条</source>
        <translation>Loaded GPT dictionary: %1, %2 entries</translation>
    </message>
</context>
<context>
    <name>LuaJson.solObj2JsonValue</name>
    <message>
        <location filename="LuaManager.cpp" line="+79"/>
        <source>LuaJson: key 必须是字符串</source>
        <translation>LuaJson: key must be string</translation>
    </message>
</context>
<context>
    <name>LuaManager.registerCustomTypes</name>
    <message>
        <location line="+701"/>
        <source>%1 已配置 MeCab 分词器，首次使用时加载。</source>
        <translation>%1 configured MeCab tokenizer; will load on first use.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>%1 已配置 spaCy 分词器，首次使用时加载。</source>
        <translation>%1 configured spaCy tokenizer; will load on first use.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>%1 已配置 Stanza 分词器，首次使用时加载。</source>
        <translation>%1 configured Stanza tokenizer; will load on first use.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>%1 已配置 pkuseg 分词器，首次使用时加载。</source>
        <translation>%1 configured pkuseg tokenizer; will load on first use.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>%1 中注册了无效的 tokenizerBackend: %2</source>
        <translation>Invalid tokenizerBackend in %1: %2</translation>
    </message>
</context>
<context>
    <name>LuaManager.registerFunction</name>
    <message>
        <location line="-572"/>
        <source>脚本不存在: %1</source>
        <translation>Script not found: %1</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>加载脚本 %1 失败: %2</source>
        <translation>Failed to load script %1: %2</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>在脚本 %1 中未找到函数 %2</source>
        <translation>Function %2 not found in script %1</translation>
    </message>
</context>
<context>
    <name>LuaTextPlugin.LuaTextPlugin</name>
    <message>
        <location filename="LuaTextPlugin.cpp" line="+13"/>
        <source>正在初始化 Lua 插件 %1</source>
        <translation>Initializing Lua plugin %1</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>%1 init函数初始化失败</source>
        <translation>%1 init function failed</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>%1 %2 函数注册成功</source>
        <translation>%1 %2 function registered</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>%1 init 函数执行失败: %2</source>
        <translation>%1 init function error: %2</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>%1 初始化成功</source>
        <translation>%1 initialized</translation>
    </message>
</context>
<context>
    <name>LuaTextPlugin.dPostRun</name>
    <message>
        <location line="+75"/>
        <source>%1 dPostRun函数执行失败: %2</source>
        <translation>%1 dPostRun function error: %2</translation>
    </message>
</context>
<context>
    <name>LuaTextPlugin.dPreRun</name>
    <message>
        <location line="-48"/>
        <source>%1 dPreRun 函数执行失败: %2</source>
        <translation>%1 dPreRun function error: %2</translation>
    </message>
</context>
<context>
    <name>LuaTextPlugin.postRun</name>
    <message>
        <location line="+32"/>
        <source>%1 postRun 函数执行失败: %2</source>
        <translation>%1 postRun function error: %2</translation>
    </message>
</context>
<context>
    <name>LuaTextPlugin.preRun</name>
    <message>
        <location line="-16"/>
        <source>%1 preRun 函数执行失败: %2</source>
        <translation>%1 preRun function error: %2</translation>
    </message>
</context>
<context>
    <name>LuaTextPlugin.~LuaTextPlugin</name>
    <message>
        <location line="-31"/>
        <source>%1 unload 函数执行失败</source>
        <translation>%1 unload function failed</translation>
    </message>
</context>
<context>
    <name>LuaToml.solObj2TomlValue</name>
    <message>
        <location filename="LuaManager.cpp" line="-81"/>
        <source>LuaToml: key 必须是字符串</source>
        <translation>LuaToml: key must be string</translation>
    </message>
</context>
<context>
    <name>LuaTranslator.LuaTranslator</name>
    <message>
        <location filename="LuaTranslator.ixx" line="+50"/>
        <source>LuaTranslator 获取 init 函数失败。</source>
        <translation>LuaTranslator failed to get init.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>LuaTranslator 获取 run 函数失败。</source>
        <translation>LuaTranslator failed to get run.</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>初始化 LuaTranslator 时出现异常: %1</source>
        <translation>Exception initializing LuaTranslator: %1</translation>
    </message>
</context>
<context>
    <name>LuaTranslator.run</name>
    <message>
        <location line="-40"/>
        <source>开始运行 LuaTranslator...</source>
        <translation>Starting LuaTranslator...</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>LuaTranslator 运行时异常: %1</source>
        <translation>LuaTranslator runtime exception: %1</translation>
    </message>
</context>
<context>
    <name>LuaTranslator.~LuaTranslator</name>
    <message>
        <location line="+49"/>
        <source>卸载 LuaTranslator 时出现异常: %1</source>
        <translation>Exception unloading LuaTranslator: %1</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>所有任务已完成！LuaTranslator %1 结束。</source>
        <translation>All tasks complete! LuaTranslator %1 ended.</translation>
    </message>
</context>
<context>
    <name>NLPTool.getMeCabTokenizeFunc</name>
    <message>
        <location filename="NLPTool.cpp" line="+49"/>
        <source>正在检查 MeCab 环境...</source>
        <translation>Checking MeCab env...</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>无法初始化 MeCab Model。请确保 BaseConfig/mecabDict/mecabrc 和 %1 存在
错误信息: %2</source>
        <translation>Failed to init MeCab Model. Ensure BaseConfig/mecabDict/mecabrc and %1 exist
Error: %2</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>无法初始化 MeCab Tagger。请确保 BaseConfig/mecabDict/mecabrc 和 %1 存在
错误信息: %2</source>
        <translation>Failed to init MeCab Tagger. Ensure BaseConfig/mecabDict/mecabrc and %1 exist
Error: %2</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>MeCab 环境检查完毕。</source>
        <translation>MeCab env checked.</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>分词器解析失败，错误信息: %1</source>
        <translation>Tokenizer parse failed: %1</translation>
    </message>
</context>
<context>
    <name>NLPTool.getNLPTokenizeFunc</name>
    <message>
        <location line="+37"/>
        <source>需要重启程序以应用新安装的 NLP 模型</source>
        <translation>Restart required for new NLP model</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>Python NLP 函数调用失败，错误信息: %1</source>
        <translation>Python NLP call failed: %1</translation>
    </message>
</context>
<context>
    <name>NameTranslator.run</name>
    <message>
        <location filename="NameTranslator.cpp" line="+137"/>
        <source>NameTrans: 未找到人名表文件 %1</source>
        <translation>NameTrans: name table not found: %1</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>NameTrans: 开始处理人名表...</source>
        <translation>NameTrans: processing name table...</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>NameTrans: 解析人名表失败: %1</source>
        <translation>NameTrans: failed to parse name table: %1</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>NameTrans: 没有发现需要翻译的名字（所有条目均已有译名）。</source>
        <translation>NameTrans: no names to translate.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>NameTrans: 共发现 %1 个待翻译的名字。</source>
        <translation>NameTrans: found %1 names to translate.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>NameTrans: 启动 %1 个线程，每批处理 %2 个名字。</source>
        <translation>NameTrans: starting %1 threads, batch size %2.</translation>
    </message>
    <message>
        <location line="+65"/>
        <source>NameTrans: 处理完成，已更新 %1 个译名，保存至 %2</source>
        <translation>NameTrans: done, updated %1 names, saved to %2</translation>
    </message>
</context>
<context>
    <name>NameTranslator.translateBatch</name>
    <message>
        <location line="-169"/>
        <source>没有可用的 API key 了</source>
        <oldsource>NameTrans: 没有可用的 API key 了</oldsource>
        <translation>No API key left</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>[线程 %1] 正在翻译人名表:
%2</source>
        <translation>[Thread %1] translating name table:
%2</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>人名表翻译</source>
        <translation>Name table translation</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>[线程 %1] AI 翻译人名成功:
%2</source>
        <translation>[Thread %1] AI name translation succeeded:
%2</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>[线程 %1] NameTrans: 批次翻译失败，已达到最大重试次数。</source>
        <translation>[Thread %1] NameTrans: batch failed; max retries reached.</translation>
    </message>
</context>
<context>
    <name>NormalDictionary.loadFromFile</name>
    <message>
        <location filename="Dictionary.cpp" line="+117"/>
        <source>字典文件不存在: %1</source>
        <translation>Dictionary file not found: %1</translation>
    </message>
    <message>
        <location line="+32"/>
        <source>Normal 字典文件格式错误(正则表达式错误): %1  ——  %2</source>
        <translation>Normal dictionary format error (regex error): %1 -- %2</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>Normal 字典文件解析错误: %1: %2</source>
        <translation>Normal dictionary parse error: %1: %2</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>已加载 Normal 字典: %1, 共 %2 个词条</source>
        <translation>Loaded Normal dictionary: %1, %2 entries</translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.NormalJsonTranslator</name>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="+236"/>
        <source>GalTransl++ NormalJsonTranslator 启动...</source>
        <translation>GalTransl++ NormalJsonTranslator started...</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>未找到背景文本缓存 %1</source>
        <translation>Background text cache not found: %1</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>读取背景文本缓存 %1 失败</source>
        <translation>Failed to read background text cache %1</translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.applyAgentCommit</name>
    <message>
        <location filename="NormalJsonTranslator.Agent.cpp" line="+530"/>
        <source>commit 缺少句子 %1</source>
        <translation>commit missing sentence %1</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>commit 句子 %1 的 dst 为空</source>
        <translation>commit sentence %1 has empty dst</translation>
    </message>
    <message>
        <location line="+79"/>
        <source>[线程 %1] [文件 %2] Agent 术语 %3 未提供 line_ids，且本地未在当前 chunk 匹配到出现位置，本轮不记录 occurrence。</source>
        <translation>[Thread %1] [File %2] Agent term %3 gave no line_ids and no local match in current chunk; occurrence not recorded.</translation>
    </message>
    <message>
        <location line="+38"/>
        <source>Agent 请求重翻</source>
        <translation>Agent requests retranslation</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>[线程 %1] [文件 %2] Agent 术语账本本轮实际写入 %3 / %4 条，记录建议重翻 %5 条。</source>
        <translation>[Thread %1] [File %2] Agent term ledger wrote %3/%4 entries this round, with %5 retranslate suggestions.</translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.applyAgentRetranslateSuggestions</name>
    <message>
        <location line="+850"/>
        <source>Agent 建议重翻目标 %1 没有缓存文件，已跳过。</source>
        <translation>Agent retranslate target %1 has no cache file; skipped.</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Agent 建议重翻目标缓存 %1 读取失败: %2</source>
        <translation>Failed to read Agent retranslate target cache %1: %2</translation>
    </message>
    <message>
        <location line="+41"/>
        <source>Agent 已将 %1 条建议重翻记录写入缓存问题。</source>
        <translation>Agent wrote %1 retranslate suggestions to cache issues.</translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.buildAgentBaseMessages</name>
    <message>
        <location line="-1097"/>
        <source>Agent 模式不支持的 TransEngine</source>
        <translation>TransEngine not supported in Agent mode</translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.normalJsonAfterRun</name>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="+556"/>
        <source>

```
无问题概览
```
</source>
        <translation>

```
No issue summary
```
</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>翻译问题概览.toml</source>
        <translation>Issue summary.toml</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>翻译问题概览.json</source>
        <translation>Issue summary.json</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>已生成 翻译问题概览.json 和 翻译问题概览.toml 文件</source>
        <translation>Generated issue summary JSON/TOML files</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>

```
问题概览:
</source>
        <translation>

```
Issue summary:
</translation>
    </message>
    <message>
        <location line="+25"/>
        <source>问题概览结束
```
</source>
        <translation>Issue summary end
```
</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>背景文本缓存已保存至 %1</source>
        <translation>Background text cache saved to %1</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>背景文本缓存 %1 保存失败</source>
        <translation>Failed to save background text cache %1</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>重建过程中有句子未命中缓存 (%1/%2 lines)，请检查日志以定位问题。</source>
        <translation>Some lines missed cache during rebuild (%1/%2 lines); check logs.</translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.normalJsonBeforeRun</name>
    <message>
        <location line="-507"/>
        <source>复制缓存文件夹时出现异常: %1</source>
        <translation>Exception copying cache folder: %1</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>已创建目录: %1</source>
        <translation>Created dir: %1</translation>
    </message>
    <message>
        <location line="+40"/>
        <source>[文件 %1] 第 %2 个对象缺少 message 字段。</source>
        <translation>[File %1] object %2 missing message field.</translation>
    </message>
    <message>
        <location line="+32"/>
        <source>读取文件 %1 时出错: %2</source>
        <translation>Error reading file %1: %2</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>未找到有效的 Sentence</source>
        <translation>No valid Sentence found</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>解析原人名表失败</source>
        <translation>Failed to parse source name table</translation>
    </message>
    <message>
        <location line="+28"/>
        <source>已更新 人名替换表.toml 文件</source>
        <translation>Updated name replacement TOML</translation>
    </message>
    <message>
        <location line="+66"/>
        <source>发现原名 &apos;%1&apos; 的译名 &apos;%2&apos;</source>
        <translation>Found translation &apos;%2&apos; for original name &apos;%1&apos;</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>解析 人名替换表.toml 时出错: %1</source>
        <translation>Error parsing ?????.toml: %1</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>检测到文件分割模式 (%1)，开始预处理输入文件...</source>
        <translation>File split mode detected (%1); preprocessing input files...</translation>
    </message>
    <message>
        <location line="+25"/>
        <source>文件 %1 已被分割成 %2 份，存入输入缓存。</source>
        <translation>File %1 split into %2 parts in input cache.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>分割文件 %1 时出错: %2</source>
        <translation>Error splitting file %1: %2</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>未知的文件分割模式: %1, 请使用 &apos;No&apos;, &apos;Equal&apos;, &apos;Num&apos;</source>
        <translation>Unknown file split mode: %1; use &apos;No&apos;, &apos;Equal&apos;, or &apos;Num&apos;</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>未知的排序模式: %1</source>
        <translation>Unknown sort mode: %1</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>分析连续重复块时读取文件 %1 失败: %2</source>
        <translation>Failed to read file %1 for repeated-block analysis: %2</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>连续重复块引用分析完成，阈值 %1，共配置引用 %2 句。</source>
        <translation>Repeated-block reference analysis done, threshold %1, configured %2 sentence refs.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>连续重复块引用分析完成，未发现长度不小于 %1 的重复块。</source>
        <translation>Repeated-block reference analysis done; no repeat block length &gt;= %1.</translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.normalJsonInit</name>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="+44"/>
        <source>无效的 TransEngine: %1</source>
        <translation>Invalid TransEngine: %1</translation>
    </message>
    <message>
        <location line="+132"/>
        <source>apiStrategy 必须为 random 或 fallback</source>
        <translation>apiStrategy must be random or fallback</translation>
    </message>
    <message>
        <location line="+69"/>
        <source>找不到可用的 API key</source>
        <oldsource>找不到可用的 API key </oldsource>
        <translation>No API key available</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>找不到 Prompt.toml 文件</source>
        <translation>Prompt.toml not found</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>Prompt.toml 中缺少 %1 键</source>
        <translation>Prompt.toml missing key %1</translation>
    </message>
    <message>
        <location line="+46"/>
        <source>未知的 TransEngine</source>
        <translation>Unknown TransEngine</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>已配置 MeCab 分词器，首次使用时加载。</source>
        <translation>MeCab tokenizer set; loads on first use.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>已配置 spaCy 分词器，首次使用时加载。</source>
        <translation>spaCy tokenizer set; loads on first use.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>已配置 Stanza 分词器，首次使用时加载。</source>
        <translation>Stanza tokenizer set; loads on first use.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>无效的 tokenizerBackend: %1</source>
        <translation>Invalid tokenizerBackend: %1</translation>
    </message>
    <message>
        <location line="+59"/>
        <source>retranslKeys 正则表达式 [%1] 编译失败</source>
        <translation>retranslKeys regex [%1] compile failed</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>retranslKeys 的元素必须是字符串、表或表数组</source>
        <translation>retranslKeys items must be string/table/table array</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>skipProblems 的内联表数组第一个元素必须是字符串</source>
        <translation>First item in skipProblems inline table array must be string</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>skipProblems 的元素必须是字符串或表数组</source>
        <translation>skipProblems items must be string or table array</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>项目配置文件解析失败: %1</source>
        <translation>Project config parse failed: %1</translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.normalJsonProcessFiles</name>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="+178"/>
        <source>已将 %1 个文件任务分配到线程池，等待处理完成...</source>
        <translation>Assigned %1 file tasks to thread pool; waiting...</translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.postProcess</name>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="+18"/>
        <location line="+117"/>
        <source>翻译失败</source>
        <translation>Translation failed</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>错误的 GPPCProblem 格式</source>
        <translation>Bad GPPCProblem format</translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.processFile</name>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="+31"/>
        <source>处理文件</source>
        <translation>Processing file</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>[线程 %1] 开始处理文件: %2</source>
        <translation>[Thread %1] start processing file: %2</translation>
    </message>
    <message>
        <location line="+49"/>
        <source>[线程 %1] [文件 %2] 解析失败: %3</source>
        <translation>[Thread %1] [File %2] parse failed: %3</translation>
    </message>
    <message>
        <location line="+101"/>
        <location line="+82"/>
        <source>[线程 %1] 缓存文件 %2 解析失败: %3</source>
        <translation>[Thread %1] cache file %2 parse failed: %3</translation>
    </message>
    <message>
        <location line="+53"/>
        <source>[线程 %1] [文件 %2] 共 %3 句，命中缓存/跳过 %4 句，需翻译 %5 句。</source>
        <translation>[Thread %1] [File %2] %3 sentences, cache/skip hits %4, need translate %5.</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>[线程 %1] [文件 %2] 有 %3 句未命中缓存，这些句子是: %4</source>
        <translation>[Thread %1] [File %2] %3 sentences missed cache: %4</translation>
    </message>
    <message>
        <location line="+45"/>
        <source>[线程 %1] [文件 %2] 已停止翻译</source>
        <translation>[Thread %1] [File %2] translation stopped</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>[线程 %1] [文件 %2] 达到保存间隔，正在更新缓存文件...</source>
        <translation>[Thread %1] [File %2] save interval reached; updating cache...</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>[线程 %1] [文件 %2] 翻译完成，正在进行最终保存...</source>
        <translation>[Thread %1] [File %2] translation done; final save...</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>[线程 %1] [文件 %2] 处理完成。</source>
        <translation>[Thread %1] [File %2] processing done.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>[线程 %1] [文件 %2] 连续重复块引用模式启用，延后最终输出回填与文件回调。</source>
        <translation>[Thread %1] [File %2] repeated-block refs enabled; delaying final fill and callback.</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>文件 %1 尚未全部处理完成，跳过合并。</source>
        <translation>File %1 not fully processed; skipping merge.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>开始合并 %1 的缓存文件...</source>
        <translation>Merging cache files for %1...</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>[线程 %1] [文件 %2] 合并处理完成。</source>
        <translation>[Thread %1] [File %2] merge done.</translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.resolveRepeatedBlockReferences</name>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="+52"/>
        <source>连续重复块引用回填读取 %1 失败: %2</source>
        <translation>Failed to read %1 for repeated-block fill: %2</translation>
    </message>
    <message>
        <location line="+95"/>
        <source>连续重复块引用回填完成，共复制 %1 句。但还有 %2 句未找到被引用缓存，仅保留占位结果。</source>
        <translation>Repeated-block fill done, copied %1 sentences. %2 referenced cache sentences still missing; placeholders kept.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>连续重复块引用回填完成，共复制 %1 句。</source>
        <translation>Repeated-block fill done, copied %1 sentences.</translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.translateBatch</name>
    <message>
        <location filename="NormalJsonTranslator.Batch.cpp" line="+50"/>
        <source>[线程 %1] [文件 %2] 开始拆分批次进行重试...</source>
        <translation>[Thread %1] [File %2] splitting batch for retry...</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>[线程 %1] [文件 %2] 清空上下文后再次尝试...</source>
        <translation>[Thread %1] [File %2] retrying after clearing context...</translation>
    </message>
    <message>
        <location line="+42"/>
        <source>[线程 %1] [文件 %2] 开始翻译:
%3</source>
        <translation>[Thread %1] [File %2] translating:
%3</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>没有可用的 API key 了</source>
        <translation>No API key left</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>[线程 %1] [文件 %2] 成功响应，响应内容:
%3</source>
        <translation>[Thread %1] [File %2] response ok:
%3</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>解析失败或不完整 (%1 / %2)</source>
        <translation>Parse failed or incomplete (%1 / %2)</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>[线程 %1] [文件 %2] 解析失败或不完整 (%3 / %4), 进行第 %5 次重试..., 解析结果: 
%6</source>
        <translation>[Thread %1] [File %2] parse failed/incomplete (%3 / %4), retry %5... Result:
%6</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>[线程 %1] [文件 %2] 成功解析 %3 句，解析结果: 
%4</source>
        <translation>[Thread %1] [File %2] parsed %3 sentences. Result:
%4</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>[线程 %1] [文件 %2] 批次翻译在 %3 次重试后彻底失败，共翻译 %4 / %5 句。</source>
        <translation>[Thread %1] [File %2] batch failed after %3 retries, translated %4/%5 sentences.</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>批次翻译在 %1 次重试后彻底失败，共翻译 %2 / %3 句。</source>
        <translation>Batch failed after %1 retries, translated %2/%3 sentences.</translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.translateBatchAgent</name>
    <message>
        <location filename="NormalJsonTranslator.Agent.cpp" line="+661"/>
        <source>[线程 %1] [文件 %2] Agent 开始拆分批次进行重试...</source>
        <translation>[Thread %1] [File %2] Agent splitting batch for retry...</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>[线程 %1] [文件 %2] Agent 清空上下文后再次尝试...</source>
        <translation>[Thread %1] [File %2] Agent retrying after clearing context...</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>[线程 %1] [文件 %2] Agent 开始翻译，当前 chunk %3-%4，待提交 %5 句，最多 %6 轮:
%7</source>
        <translation>[Thread %1] [File %2] Agent translating chunk %3-%4, %5 pending, max %6 rounds:
%7</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>[线程 %1] [文件 %2] Agent 第 %3/%4 轮，请求上下文约 %5 字符。</source>
        <translation>[Thread %1] [File %2] Agent round %3/%4, context about %5 chars.</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>[线程 %1] [文件 %2] Agent 第 %3/%4 轮请求消息（实际发送给模型）:
%5</source>
        <translation>[Thread %1] [File %2] Agent round %3/%4 request sent to model:
%5</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>[线程 %1] [文件 %2] Agent 上下文超过 hardContextChars，回退到最近摘要重建消息。</source>
        <translation>[Thread %1] [File %2] Agent context over hardContextChars; rebuilding from latest summary.</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>[线程 %1] [文件 %2] Agent 上下文接近上限，要求模型先压缩上下文。</source>
        <translation>[Thread %1] [File %2] Agent context near limit; asking model to compress first.</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>没有可用的 API key 了</source>
        <translation>No API key left</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>Agent 响应解析失败: %1</source>
        <translation>Agent response parse failed: %1</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>[线程 %1] [文件 %2] Agent 响应解析失败，第 %3 次重试。原始响应: %4
错误: %5</source>
        <translation>[Thread %1] [File %2] Agent response parse failed, retry %3. Raw: %4
Error: %5</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>[线程 %1] [文件 %2] Agent 第 %3/%4 轮返回 action=&apos;%5&apos;。</source>
        <translation>[Thread %1] [File %2] Agent round %3/%4 returned action=&apos;%5&apos;.</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>[线程 %1] [文件 %2] Agent 第 %3/%4 轮原始响应:
%5</source>
        <translation>[Thread %1] [File %2] Agent round %3/%4 raw response:
%5</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>[线程 %1] [文件 %2] Agent 请求 %3 个工具调用: %4。</source>
        <translation>[Thread %1] [File %2] Agent requested %3 tool calls: %4.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>[线程 %1] [文件 %2] Agent 工具调用明细:
%3</source>
        <translation>[Thread %1] [File %2] Agent tool call details:
%3</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>[线程 %1] [文件 %2] Agent 工具执行完成，返回 %3 项结果，继续下一轮。</source>
        <translation>[Thread %1] [File %2] Agent tools done, returned %3 results; next round.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>[线程 %1] [文件 %2] Agent 工具返回结果:
%3</source>
        <translation>[Thread %1] [File %2] Agent tool results:
%3</translation>
    </message>
    <message>
        <location line="+25"/>
        <source>[线程 %1] [文件 %2] Agent 已压缩上下文，摘要长度 %3 字符。</source>
        <translation>[Thread %1] [File %2] Agent compressed context, summary length %3 chars.</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>[线程 %1] [文件 %2] Agent commit 内容:
translations=%3
term_updates=%4
rewrite_requests=%5
file_note_patch=%6
rolling_context=%7</source>
        <translation>[Thread %1] [File %2] Agent commit:
translations=%3
term_updates=%4
rewrite_requests=%5
file_note_patch=%6
rolling_context=%7</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>[线程 %1] [文件 %2] Agent commit 成功，提交 %3 句，术语更新 %4 条，建议重翻请求 %5 条，新的 rolling_context 长度 %6 字符。</source>
        <translation>[Thread %1] [File %2] Agent commit ok: %3 sentences, %4 term updates, %5 retranslate requests, new rolling_context %6 chars.</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>Agent commit 校验失败: %1</source>
        <translation>Agent commit validation failed: %1</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>[线程 %1] [文件 %2] Agent commit 校验失败，第 %3 次重试。错误: %4</source>
        <translation>[Thread %1] [File %2] Agent commit validation failed, retry %3. Error: %4</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>Agent 返回未知 action &apos;%1&apos;</source>
        <translation>Agent returned unknown action &apos;%1&apos;</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>[线程 %1] [文件 %2] Agent 返回未知 action &apos;%3&apos;，第 %4 次重试。</source>
        <translation>[Thread %1] [File %2] Agent returned unknown action &apos;%3&apos;, retry %4.</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>[线程 %1] [文件 %2] Agent 单个 chunk 在 %3 轮内仍未产出 commit，判定本批次失败，不再重试。当前 chunk %4-%5，待提交 %6 句。</source>
        <translation>[Thread %1] [File %2] Agent chunk produced no commit in %3 rounds; batch failed. Chunk %4-%5, %6 pending.</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>[线程 %1] [文件 %2] Agent 批次因超过最大轮数而失败，共翻译 %3 / %4 句。</source>
        <translation>[Thread %1] [File %2] Agent batch failed by max rounds, translated %3/%4 sentences.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Agent 批次因超过最大轮数而失败，共翻译 %1 / %2 句。</source>
        <translation>Agent batch failed by max rounds, translated %1/%2 sentences.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>[线程 %1] [文件 %2] Agent 批次在 %3 次重试后彻底失败，共翻译 %4 / %5 句。</source>
        <translation>[Thread %1] [File %2] Agent batch failed after %3 retries, translated %4/%5 sentences.</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Agent 批次在 %1 次重试后彻底失败，共翻译 %2 / %3 句。</source>
        <translation>Agent batch failed after %1 retries, translated %2/%3 sentences.</translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.~NormalJsonTranslator</name>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="-696"/>
        <source>所有任务已完成！NormalJsonTranslator 结束。</source>
        <oldsource>所有任务已完成！NormalJsonTranslator结束。</oldsource>
        <translation>All tasks complete! NormalJsonTranslator finished.</translation>
    </message>
</context>
<context>
    <name>PDFTranslator.PDFTranslator</name>
    <message>
        <location filename="PDFTranslator.cpp" line="+25"/>
        <source>GalTransl++ PDFTranslator 启动...</source>
        <translation>GalTransl++ PDFTranslator started...</translation>
    </message>
</context>
<context>
    <name>PDFTranslator.pdfBeforeRun</name>
    <message>
        <location line="+31"/>
        <source>已创建目录: %1</source>
        <translation>Created dir: %1</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>未找到 PDF 文件</source>
        <translation>PDF file not found</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>正在提取文件: %1</source>
        <translation>Extracting file: %1</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>成功提取元数据: %1</source>
        <translation>Metadata extracted: %1</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>提取元数据失败: %1</source>
        <translation>Metadata extraction failed: %1</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>未找到与 %1 对应的元数据，跳过</source>
        <translation>Metadata for %1 not found; skipping</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>正在回注文件: %1</source>
        <translation>Injecting file: %1</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>成功翻译文件: %1</source>
        <translation>Translated file successfully: %1</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>翻译文件失败: %1</source>
        <translation>File translation failed: %1</translation>
    </message>
</context>
<context>
    <name>PDFTranslator.pdfInit</name>
    <message>
        <location line="-89"/>
        <source>PDF 配置文件解析失败: %1</source>
        <translation>PDF config parse failed: %1</translation>
    </message>
</context>
<context>
    <name>PDFTranslator.~PDFTranslator</name>
    <message>
        <location line="-29"/>
        <source>所有任务已完成！PDFTranslator 结束。</source>
        <oldsource>所有任务已完成！PDFTranslator结束。</oldsource>
        <translation>All tasks complete! PDFTranslator finished.</translation>
    </message>
</context>
<context>
    <name>ProblemAnalyzer.analyze</name>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="+40"/>
        <source>翻译为空</source>
        <translation>Empty translation</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>词频过高-&apos;%1&apos;%2次</source>
        <translation>High freq-&apos;%1&apos; %2 times</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>本有 %1 符号</source>
        <translation>Original has %1 symbol</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>本无 %1 符号</source>
        <translation>Original lacks %1 symbol</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>残留日文: %1</source>
        <translation>Japanese remains: %1</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>引入拉丁字母: %1</source>
        <translation>Latin letters introduced: %1</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>引入韩文: %1</source>
        <translation>Korean introduced: %1</translation>
    </message>
    <message>
        <location line="+28"/>
        <source>引入繁体字: %1</source>
        <translation>Traditional Chinese introduced: %1</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>丢失换行(%1/%2)</source>
        <translation>Lost line breaks (%1/%2)</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>多加换行(%1/%2)</source>
        <translation>Extra line breaks (%1/%2)</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>比原文严格长 %1 倍(%2/%3字符)</source>
        <translation>Strictly %1x longer than source (%2/%3 chars)</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>比原文长 %1 倍(%2/%3字符)</source>
        <translation>%1x longer than source (%2/%3 chars)</translation>
    </message>
    <message>
        <location line="+52"/>
        <source>无法识别的语言</source>
        <translation>Unknown language</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>引入(%1, %2)</source>
        <translation>Introduced (%1, %2)</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>非 %1 字符: %2</source>
        <translation>Non-%1 chars: %2</translation>
    </message>
</context>
<context>
    <name>ProblemAnalyzer.loadProblems</name>
    <message>
        <location line="+59"/>
        <source>未知问题: %1</source>
        <translation>Unknown problem: %1</translation>
    </message>
</context>
<context>
    <name>ProblemAnalyzer.overwriteCompareObj</name>
    <message>
        <location line="+14"/>
        <location line="+7"/>
        <source>未知缓存键: %1</source>
        <translation>Unknown cache key: %1</translation>
    </message>
    <message>
        <location line="+48"/>
        <source>不支持的问题比较对象覆写: %1</source>
        <translation>Unsupported problem compare object override: %1</translation>
    </message>
</context>
<context>
    <name>PythonInterpreterInstance.daemonThreadFunc</name>
    <message>
        <location filename="PythonManager.cpp" line="+275"/>
        <source>导入 gpp_plugin_api 时出现异常: %1</source>
        <translation>Exception importing gpp_plugin_api: %1</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>PythonInterpreterInstance 异常: %1</source>
        <translation>PythonInterpreterInstance exception: %1</translation>
    </message>
</context>
<context>
    <name>PythonMainInterpreterManager.PythonMainInterpreterManager</name>
    <message>
        <location line="-259"/>
        <source>Python 环境未初始化</source>
        <translation>Python env not initialized</translation>
    </message>
</context>
<context>
    <name>PythonMainInterpreterManager.daemonThreadFunc</name>
    <message>
        <location line="+145"/>
        <source>导入 gpp_plugin_api 时出现异常: %1</source>
        <translation>Exception importing gpp_plugin_api: %1</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>PythonMainInterpreterManager 异常: %1</source>
        <translation>PythonMainInterpreterManager exception: %1</translation>
    </message>
</context>
<context>
    <name>PythonMainInterpreterManager.registerNLPFunction</name>
    <message>
        <location line="-130"/>
        <source>模块 %1 的模型 %2 已在内存中，直接获取</source>
        <translation>Module %1 model %2 already in memory; using it</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>正在加载模块 %1 的模型 %2</source>
        <translation>Loading module %1 model %2</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>模块 %1 的模型 %2 未安装，正在尝试安装</source>
        <translation>Module %1 model %2 not installed; trying install</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>将在 3s 后开始安装模型，请勿关闭接下来出现的窗口！</source>
        <translation>Model install starts in 3s; do not close the next window!</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>正在执行安装命令: %1</source>
        <translation>Running install command: %1</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>安装模型 %1 的命令失败</source>
        <translation>Install command for model %1 failed</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>模块 %1 的模型 %2 安装成功</source>
        <translation>Module %1 model %2 installed</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>模块 %1 的模型 %2 安装失败</source>
        <translation>Module %1 model %2 install failed</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>加载模块 %1 的模型 %2 时出现异常: %3</source>
        <translation>Exception loading module %1 model %2: %3</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>模块 %1 的模型 %2 已加载</source>
        <translation>Module %1 model %2 loaded</translation>
    </message>
</context>
<context>
    <name>PythonManager.registerCustomTypes</name>
    <message>
        <location line="+259"/>
        <source>%1 已配置 MeCab 分词器，首次使用时加载。</source>
        <translation>%1 configured MeCab tokenizer; will load on first use.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>%1 已配置 spaCy 分词器，首次使用时加载。</source>
        <translation>%1 configured spaCy tokenizer; will load on first use.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>%1 已配置 Stanza 分词器，首次使用时加载。</source>
        <translation>%1 configured Stanza tokenizer; will load on first use.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>%1 已配置 pkuseg 分词器，首次使用时加载。</source>
        <translation>%1 configured pkuseg tokenizer; will load on first use.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>%1 中注册了无效的 tokenizerBackend: %2</source>
        <translation>Invalid tokenizerBackend in %1: %2</translation>
    </message>
</context>
<context>
    <name>PythonManager.registerFunction</name>
    <message>
        <location line="-136"/>
        <source>脚本不存在: %1</source>
        <translation>Script not found: %1</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>加载模块 %1 时出现异常，子解释器无法开启</source>
        <translation>Exception loading module %1; subinterpreter cannot start</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>为模块 %1 加载自定义类型时出现异常: %2</source>
        <translation>Exception loading custom types for module %1: %2</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>模块 %1 插入失败</source>
        <translation>Failed to insert module %1</translation>
    </message>
    <message>
        <location line="+14"/>
        <location line="+8"/>
        <source>从脚本 %1 加载函数 %2 失败</source>
        <translation>Failed to load function %2 from script %1</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>加载模块 %1 的函数 %2 时出现异常: %3</source>
        <translation>Exception loading function %2 from module %1: %3</translation>
    </message>
</context>
<context>
    <name>PythonTextPlugin.PythonTextPlugin</name>
    <message>
        <location filename="PythonTextPlugin.cpp" line="+15"/>
        <source>正在初始化 Python 插件 %1</source>
        <translation>Initializing Python plugin %1</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>%1 init 函数初始化失败</source>
        <translation>%1 init function failed</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>%1 %2 函数注册成功</source>
        <translation>%1 %2 function registered</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>%1 init 函数执行失败: %2</source>
        <translation>%1 init function error: %2</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>%1 初始化成功</source>
        <translation>%1 initialized</translation>
    </message>
</context>
<context>
    <name>PythonTextPlugin.dPostRun</name>
    <message>
        <location line="+88"/>
        <source>%1 postRun 函数执行失败: %2</source>
        <translation>%1 postRun function error: %2</translation>
    </message>
</context>
<context>
    <name>PythonTextPlugin.dPreRun</name>
    <message>
        <location line="-54"/>
        <source>%1 dPreRun 函数执行失败: %2</source>
        <translation>%1 dPreRun function error: %2</translation>
    </message>
</context>
<context>
    <name>PythonTextPlugin.postRun</name>
    <message>
        <location line="+36"/>
        <source>%1 postRun 函数执行失败: %2</source>
        <translation>%1 postRun function error: %2</translation>
    </message>
</context>
<context>
    <name>PythonTextPlugin.preRun</name>
    <message>
        <location line="-18"/>
        <source>%1 preRun 函数执行失败: %2</source>
        <translation>%1 preRun function error: %2</translation>
    </message>
</context>
<context>
    <name>PythonTextPlugin.~PythonTextPlugin</name>
    <message>
        <location line="-36"/>
        <source>%1 unload 函数执行失败: %2</source>
        <translation>%1 unload function error: %2</translation>
    </message>
</context>
<context>
    <name>PythonTranslator.PythonTranslator</name>
    <message>
        <location filename="PythonTranslator.ixx" line="+53"/>
        <source>PythonTranslator 获取 init 函数失败！</source>
        <translation>PythonTranslator failed to get init!</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>PythonTranslator 获取 run 函数失败！</source>
        <translation>PythonTranslator failed to get run!</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>初始化 PythonTranslator 时出现异常: %1</source>
        <translation>Exception initializing PythonTranslator: %1</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>PythonTranslator 已加载模块: %1</source>
        <translation>PythonTranslator loaded module: %1</translation>
    </message>
</context>
<context>
    <name>PythonTranslator.run</name>
    <message>
        <location line="-55"/>
        <source>开始运行 PythonTranslator...</source>
        <translation>Starting PythonTranslator...</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>PythonTranslator 运行时异常: %1</source>
        <translation>PythonTranslator runtime exception: %1</translation>
    </message>
</context>
<context>
    <name>PythonTranslator.~PythonTranslator</name>
    <message>
        <location line="+69"/>
        <source>卸载 PythonTranslator 时出现异常: %1</source>
        <translation>Exception unloading PythonTranslator: %1</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>所有任务已完成！PythonTranslator %1 结束。</source>
        <translation>All tasks complete! PythonTranslator %1 ended.</translation>
    </message>
</context>
<context>
    <name>SkipTrans.SkipTrans</name>
    <message>
        <location filename="SkipTrans.cpp" line="+23"/>
        <source>SkipTrans 不支持 %1 阶段运行</source>
        <translation>SkipTrans does not support %1 stage</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>skipKeys 正则表达式 [%1] 编译失败</source>
        <translation>skipKeys regex [%1] compile failed</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>skipKeys 元素必须是字符串、表或表数组</source>
        <translation>skipKeys items must be string/table/table array</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>插件 SkipTrans-%1 已加载, skipH: %2</source>
        <translation>Plugin SkipTrans-%1 loaded, skipH: %2</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>SkipTrans-%1 配置文件解析错误: %2</source>
        <translation>SkipTrans-%1 config parse error: %2</translation>
    </message>
</context>
<context>
    <name>SkipTrans.skipImpl</name>
    <message>
        <location line="+31"/>
        <source>被第 %1 个 skipKeys 条件匹配到</source>
        <translation>Matched by skipKeys condition %1</translation>
    </message>
</context>
<context>
    <name>TextFull2Half.TextFull2Half</name>
    <message>
        <location filename="TextFull2Half.cpp" line="+48"/>
        <source>TextFull2Half 正则编译错误: %1</source>
        <translation>TextFull2Half regex compile error: %1</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>TextFull2Half-%1 已加载 - 替换标点: %2, 反向替换: %3</source>
        <translation>TextFull2Half-%1 loaded - replace punctuation: %2, reverse: %3</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>TextFull2Half-%1 配置文件解析错误: %2</source>
        <translation>TextFull2Half-%1 config parse error: %2</translation>
    </message>
</context>
<context>
    <name>TextLinebreakFix.TextLinebreakFix</name>
    <message>
        <location filename="TextLinebreakFix.cpp" line="+24"/>
        <source>TextLinebreakFix 不支持 %1 阶段运行</source>
        <translation>TextLinebreakFix does not support %1 stage</translation>
    </message>
    <message>
        <location line="+33"/>
        <source>TextLinebreakFix-%1 无效的换行模式: %2</source>
        <translation>TextLinebreakFix-%1 invalid line break mode: %2</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>TextLinebreakFix-%1 已配置 MeCab 分词器，首次使用时加载。</source>
        <translation>TextLinebreakFix-%1 configured MeCab tokenizer; will load on first use.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>TextLinebreakFix-%1 已配置 spaCy 分词器，首次使用时加载。</source>
        <translation>TextLinebreakFix-%1 configured spaCy tokenizer; will load on first use.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>TextLinebreakFix-%1 已配置 Stanza 分词器，首次使用时加载。</source>
        <translation>TextLinebreakFix-%1 configured Stanza tokenizer; will load on first use.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>TextLinebreakFix-%1 已配置 pkuseg 分词器，首次使用时加载。</source>
        <translation>TextLinebreakFix-%1 configured pkuseg tokenizer; will load on first use.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>TextLinebreakFix-%1 无效的 tokenizerBackend: %2</source>
        <translation>TextLinebreakFix-%1 invalid tokenizerBackend: %2</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>TextLinebreakFix-%1 分段字数阈值必须大于0</source>
        <translation>TextLinebreakFix-%1 segment char threshold must be &gt; 0</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>TextLinebreakFix-%1 报错阈值必须大于0</source>
        <translation>TextLinebreakFix-%1 error threshold must be &gt; 0</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>已加载插件 TextLinebreakFix-%1, 换行模式: %2, 优先阈值 %3, 分段字数阈值: %4, 强制修复: %5, 报错阈值: %6</source>
        <translation>Loaded plugin TextLinebreakFix-%1, mode: %2, priority %3, segment threshold: %4, force fix: %5, error threshold: %6</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>插件 TextLinebreakFix-%1 分词器已启用</source>
        <translation>Plugin TextLinebreakFix-%1 tokenizer enabled</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>TextLinebreakFix-%1 插件配置文件解析错误: %2</source>
        <translation>TextLinebreakFix-%1 plugin config parse error: %2</translation>
    </message>
</context>
<context>
    <name>TextLinebreakFix.checkLineLength</name>
    <message>
        <location line="+49"/>
        <source>第 %1 行字数超出报错阈值[%2/%3]</source>
        <translation>Line %1 exceeds error threshold [%2/%3]</translation>
    </message>
</context>
<context>
    <name>TextLinebreakFix.fixLinebreak</name>
    <message>
        <location line="+18"/>
        <source>需要修复换行的句子[%1]: 原文 %2 行, 译文 %3 行</source>
        <translation>Sentence [%1] needs line break fix: source %2 lines, translation %3 lines</translation>
    </message>
    <message>
        <location line="+13"/>
        <location line="+254"/>
        <source>换行修复</source>
        <translation>Line break fix</translation>
    </message>
    <message>
        <location line="-253"/>
        <location line="+254"/>
        <source>原文 %1 行, 译文 %2 行, 修正后 %3 行</source>
        <translation>Source %1 lines, translation %2 lines, fixed %3 lines</translation>
    </message>
    <message>
        <location line="-249"/>
        <source>译文[%1](%2行) -&gt; 修正后译文[%3](%4行)</source>
        <translation>Translation [%1](%2 lines) -&gt; fixed translation [%3](%4 lines)</translation>
    </message>
    <message>
        <location line="+232"/>
        <source>译文分词结果</source>
        <translation>Translation tokens</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>无效的 TextLinebreakFix 模式</source>
        <translation>Invalid TextLinebreakFix mode</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>句子[%1](%2行) -&gt; 修正后译文[%3](%4行)</source>
        <translation>Sentence [%1](%2 lines) -&gt; fixed translation [%3](%4 lines)</translation>
    </message>
</context>
<context>
    <name>buildContextHistory</name>
    <message>
        <location filename="NormalJsonTranslatorHelperTool.cpp" line="+517"/>
        <source>未知的 PromptType</source>
        <translation>Unknown PromptType</translation>
    </message>
</context>
<context>
    <name>checkPythonDependencies</name>
    <message>
        <location filename="PythonManager.cpp" line="+80"/>
        <source>正在检查依赖 %1</source>
        <translation>Checking dependency %1</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>依赖 %1 已安装</source>
        <translation>Dependency %1 installed</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>检查依赖 %1 时出现异常: %2</source>
        <translation>Exception checking dependency %1: %2</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>依赖 %1 未安装，正在尝试安装</source>
        <translation>Dependency %1 not installed; trying install</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>将在 3s 后开始安装依赖，请勿关闭接下来出现的窗口！</source>
        <translation>Dependency install starts in 3s; do not close the next window!</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>正在执行安装命令: %1</source>
        <translation>Running install command: %1</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>安装依赖 %1 的命令失败</source>
        <translation>Install command for dependency %1 failed</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>依赖 %1 安装成功</source>
        <translation>Dependency %1 installed</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>依赖 %1 安装验证失败: %2</source>
        <translation>Dependency %1 validation failed: %2</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>依赖 %1 检查完毕</source>
        <translation>Dependency %1 check complete</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>所有依赖均已安装</source>
        <translation>All dependencies installed</translation>
    </message>
</context>
<context>
    <name>checkResponse</name>
    <message>
        <location filename="APIPool.cpp" line="+54"/>
        <source>%1 API 响应 JSON 解析失败，进行第 %2 次重试。错误: %3，原始响应: %4</source>
        <translation>%1 API response JSON parse failed, retry %2. Error: %3, raw: %4</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>API 响应 JSON 解析失败: %1</source>
        <translation>API response JSON parse failed: %1</translation>
    </message>
    <message>
        <location line="+9"/>
        <location line="+109"/>
        <source>[线程 %1] 将切换到下一个 API key(如果有多个API key的话)</source>
        <translation>[Thread %1] switching to next API key (if multiple keys exist)</translation>
    </message>
    <message>
        <location line="-87"/>
        <source>%1 API key [%2] 疑似额度用尽，短期内多次报告将从池中移除。响应: %3</source>
        <translation>%1 API key [%2] may be exhausted; repeated reports will remove it. Response: %3</translation>
    </message>
    <message>
        <location line="+9"/>
        <location line="+22"/>
        <location line="+26"/>
        <source>响应为空</source>
        <translation>Empty response</translation>
    </message>
    <message>
        <location line="-49"/>
        <source>API key 疑似额度用尽: %1</source>
        <translation>API key may be exhausted: %1</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>%1 API key [%2] 没有可用模型，短期内多次报告将从池中移除。响应: %3</source>
        <translation>%1 API key [%2] has no available model; repeated reports will remove it. Response: %3</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>API key 没有模型 %1: %2</source>
        <translation>API key has no model %1: %2</translation>
    </message>
    <message>
        <location line="+21"/>
        <location line="+24"/>
        <source>空</source>
        <translation>Empty</translation>
    </message>
    <message>
        <location line="-27"/>
        <source>%1 遇到频率限制或可重试错误，将等待 %2 秒后重试。响应: %3</source>
        <translation>%1 hit rate limit or retryable error; retrying in %2s. Response: %3</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>遇到频率限制或可重试错误: %1</source>
        <translation>Rate limit or retryable error: %1</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>%1 遇到未知 API 错误，进行第 %2 次重试。响应: %3</source>
        <translation>%1 unknown API error, retry %2. Response: %3</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>未知 API 错误</source>
        <translation>Unknown API error</translation>
    </message>
</context>
<context>
    <name>chooseCachePart</name>
    <message>
        <location filename="Tool.cpp" line="+323"/>
        <source>无效的 CachePart: %1</source>
        <translation>Invalid CachePart: %1</translation>
    </message>
</context>
<context>
    <name>chooseStringRef</name>
    <message>
        <location line="-49"/>
        <source>无效的条件目标: None</source>
        <translation>Invalid condition target: None</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>无法获取字符串的无效条件目标: %1</source>
        <translation>Cannot get string for invalid condition target: %1</translation>
    </message>
</context>
<context>
    <name>combineOutputFiles</name>
    <message>
        <location filename="NormalJsonTranslatorHelperTool.cpp" line="+267"/>
        <source>开始合并文件: %1</source>
        <translation>Merging file: %1</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>合并文件 %1 时出错: %2</source>
        <translation>Error merging file %1: %2</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>试图合并 %1 时出错，缺少文件 %2</source>
        <translation>Error merging %1: missing file %2</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>文件 %1 合并完成，已保存到 %2</source>
        <translation>File %1 merged, saved to %2</translation>
    </message>
</context>
<context>
    <name>countGraphemes</name>
    <message>
        <location filename="Tool.cpp" line="+146"/>
        <source>创建字符边界迭代器失败: %1</source>
        <translation>Failed to create character boundary iterator: %1</translation>
    </message>
</context>
<context>
    <name>createTranslator</name>
    <message>
        <location filename="ITranslator.cpp" line="+173"/>
        <source>找不到配置文件</source>
        <translation>Config file not found</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>无效的日志等级</source>
        <translation>Invalid log level</translation>
    </message>
    <message>
        <location line="+33"/>
        <source>日志器初始化完成。</source>
        <translation>Logger initialized.</translation>
    </message>
    <message>
        <location line="+23"/>
        <location line="+24"/>
        <source>无效的基类名称: %1</source>
        <translation>Invalid base class name: %1</translation>
    </message>
</context>
<context>
    <name>executeAgentToolCall</name>
    <message>
        <location filename="NormalJsonTranslator.Agent.cpp" line="-709"/>
        <source>search_text.scope 非法: %1。允许值仅有 current_file|all_files|specified_file</source>
        <translation>Invalid search_text.scope: %1. Allowed: current_file|all_files|specified_file</translation>
    </message>
</context>
<context>
    <name>executeAgentToolCalls</name>
    <message>
        <location line="+274"/>
        <source>未知工具: %1</source>
        <translation>Unknown tool: %1</translation>
    </message>
</context>
<context>
    <name>fillBlockAndMap</name>
    <message>
        <location filename="NormalJsonTranslatorHelperTool.cpp" line="-235"/>
        <source>不支持的 TransEngine 用于构建输入</source>
        <translation>Unsupported TransEngine for input build</translation>
    </message>
</context>
<context>
    <name>formatAgentRetranslateProblem</name>
    <message>
        <location filename="NormalJsonTranslator.Agent.cpp" line="-728"/>
        <source>可能需要重翻</source>
        <translation>May need retranslation</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>可能需要重翻: %1</source>
        <translation>May need retranslation: %1</translation>
    </message>
</context>
<context>
    <name>formatAgentTermChangeReason</name>
    <message>
        <location line="+8"/>
        <source>术语「%1」译名由「%2」更新为「%3」</source>
        <translation>Term &quot;%1&quot; translation changed from &quot;%2&quot; to &quot;%3&quot;</translation>
    </message>
</context>
<context>
    <name>getMostCommonChar</name>
    <message>
        <location filename="Tool.cpp" line="-74"/>
        <source>创建字符边界迭代器失败: %1</source>
        <translation>Failed to create character boundary iterator: %1</translation>
    </message>
</context>
<context>
    <name>getTraditionalChineseExtractor</name>
    <message>
        <location line="+308"/>
        <source>使用 OpenCC 进行繁体中文检测</source>
        <translation>Using OpenCC for TC detection</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>OpenCC 不可用，尝试回退到基于 ICU 的繁体中文检测</source>
        <translation>OpenCC unavailable; falling back to ICU TC detection</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>基于 ICU 的繁体中文检测不可用</source>
        <translation>ICU TC detection unavailable</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>基于 ICU 的简体中文检测不可用</source>
        <translation>ICU SC detection unavailable</translation>
    </message>
    <message>
        <location line="+69"/>
        <source>使用基于 ICU 的繁体中文检测</source>
        <translation>Using ICU TC detection</translation>
    </message>
</context>
<context>
    <name>loadTokenizeCache</name>
    <message>
        <location line="+15"/>
        <source>未找到分词缓存 %1</source>
        <translation>Tokenize cache not found: %1</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>读取分词缓存 %1 失败: %2</source>
        <translation>Failed to read tokenize cache %1: %2</translation>
    </message>
</context>
<context>
    <name>parseAgentProtocolResponse</name>
    <message>
        <location filename="NormalJsonTranslator.Agent.cpp" line="-305"/>
        <source>Agent 响应不是合法 JSON 对象</source>
        <translation>Agent response is not a JSON object</translation>
    </message>
</context>
<context>
    <name>parseContent</name>
    <message>
        <location filename="NormalJsonTranslatorHelperTool.cpp" line="+176"/>
        <source>不支持的 TransEngine 用于解析输出</source>
        <translation>Unsupported TransEngine for output parsing</translation>
    </message>
</context>
<context>
    <name>parseReviewProtocolResponse</name>
    <message>
        <location filename="DictionaryReviewAgent.cpp" line="-1024"/>
        <source>Review Agent 响应不是合法 JSON 对象</source>
        <translation>Review Agent response is not a JSON object</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>无效的 Review Agent schema: %1</source>
        <translation>Invalid Review Agent schema: %1</translation>
    </message>
</context>
<context>
    <name>parseToml</name>
    <message>
        <location filename="Tool.ixx" line="+227"/>
        <source>无效的 TOML 路径: %1</source>
        <translation>Invalid TOML path: %1</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>无法在 TOML 中找到值: %1</source>
        <translation>Value not found in TOML: %1</translation>
    </message>
</context>
<context>
    <name>saveTokenizeCache</name>
    <message>
        <location filename="Tool.cpp" line="+15"/>
        <source>分词缓存已保存到 %1</source>
        <translation>Tokenize cache saved to %1</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>分词缓存 %1 保存失败</source>
        <translation>Failed to save tokenize cache %1</translation>
    </message>
</context>
<context>
    <name>splitIntoGraphemes</name>
    <message>
        <location line="-387"/>
        <source>创建字符边界迭代器失败: %1</source>
        <translation>Failed to create character boundary iterator: %1</translation>
    </message>
</context>
<context>
    <name>splitIntoTokens</name>
    <message>
        <location line="+107"/>
        <source>在原句剩余部分中找不到 token &apos;%1&apos;。</source>
        <translation>Token &apos;%1&apos; not found in remaining source sentence.</translation>
    </message>
</context>
<context>
    <name>splitTsvLine</name>
    <message>
        <location line="-275"/>
        <source>TSV 行切分不允许使用空分隔符</source>
        <translation>TSV split cannot use empty delimiter</translation>
    </message>
</context>
<context>
    <name>toml2Json</name>
    <message>
        <location filename="NormalJsonTranslatorHelperTool.cpp" line="+207"/>
        <location line="+30"/>
        <source>不支持的 TOML 数据类型</source>
        <translation>Unsupported TOML data type</translation>
    </message>
</context>
<context>
    <name>validateNormalJsonCoreConfig</name>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="-185"/>
        <source>配置项 %1 无效：当前值 %2，要求%3</source>
        <translation>Invalid config %1: current %2, required %3</translation>
    </message>
    <message>
        <location line="+41"/>
        <location line="+8"/>
        <location line="+8"/>
        <location line="+24"/>
        <location line="+24"/>
        <location line="+8"/>
        <location line="+8"/>
        <location line="+22"/>
        <location line="+8"/>
        <location line="+9"/>
        <location line="+8"/>
        <source>大于 0</source>
        <translation>greater than 0</translation>
    </message>
    <message>
        <location line="-103"/>
        <source>为 name 或 size</source>
        <translation>be name or size</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>为 No、Num 或 Equal</source>
        <translation>be No, Num or Equal</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>大于等于 2</source>
        <translation>at least 2</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>大于等于 0</source>
        <translation>at least 0</translation>
    </message>
    <message>
        <location line="+37"/>
        <source>Agent 模式当前仅支持 ForGalTsv / ForNovelTsv / GenDict</source>
        <translation>Agent mode only supports ForGalTsv / ForNovelTsv / GenDict</translation>
    </message>
    <message>
        <location line="+39"/>
        <source>小于等于 agent.hardContextChars</source>
        <translation>no more than agent.hardContextChars</translation>
    </message>
</context>
</TS>
