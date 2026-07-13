<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="en_US">
<context>
    <name>ApiPool.loadApis</name>
    <message>
        <location filename="ApiPool.cpp" line="21"/>
        <source>令牌池新加载 %1 个 Api keys， 现共有 %2 个Api keys</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>ApiPool.reportProblem</name>
    <message>
        <location filename="ApiPool.cpp" line="78"/>
        <source>Api key [%1] 已被标记为不可用</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>ApiTool.makeApiTestPayload</name>
    <message>
        <location filename="ApiTool.cpp" line="182"/>
        <source>请用中文完整回复一句话：GPP Api 测试成功。</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>ApiTool.makeSystemProxies</name>
    <message>
        <location filename="ApiTool.cpp" line="289"/>
        <source>正在使用系统代理: [%1]</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>ApiTool.queryApiModels</name>
    <message>
        <location filename="ApiTool.cpp" line="503"/>
        <source>模型列表响应 JSON 解析失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ApiTool.cpp" line="514"/>
        <source>模型列表响应模型字段解析失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>CodePageChecker.CodePageChecker</name>
    <message>
        <location filename="CodePageChecker.cpp" line="47"/>
        <source>无法创建 ICU u8 转换器: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="CodePageChecker.cpp" line="54"/>
        <source>无法创建 ICU %1 转换器: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="CodePageChecker.cpp" line="66"/>
        <source>无法设置 ICU 回调函数: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>CodePageChecker.findUnmappableChars</name>
    <message>
        <location filename="CodePageChecker.cpp" line="95"/>
        <source>ICU 转换发生意外错误: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>ConditionTool.getCheckSeCondFunc</name>
    <message>
        <location filename="ConditionTool.ixx" line="162"/>
        <source>执行 Lua 条件函数 %1 时发生错误: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ConditionTool.ixx" line="172"/>
        <source>注册 Lua 脚本 [%1] 中的条件函数 %2 成功</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ConditionTool.ixx" line="180"/>
        <source>注册 Lua 脚本 [%1] 中的条件函数 %2 失败</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ConditionTool.ixx" line="209"/>
        <source>执行 Python 条件函数 %1 时发生错误: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ConditionTool.ixx" line="220"/>
        <source>注册 Python 脚本 [%1] 中的条件函数 %2 成功</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ConditionTool.ixx" line="228"/>
        <source>注册 Python 脚本 [%1] 中的条件函数 %2 失败</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>DictionaryGenerator.callLLMToGenerate</name>
    <message>
        <location filename="DictionaryGenerator.cpp" line="203"/>
        <source>没有可用的 Api key 了</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="217"/>
        <source>[线程 %1] [批次 %2] [请求 %3] 开始生成术语表:
%4</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="228"/>
        <source>[线程 %1] [批次 %2] [请求 %3]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="244"/>
        <source>[线程 %1] [批次 %2] [请求 %3] AI 字典生成成功:
%4</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="260"/>
        <source>发现重复术语: %1	%2	%3</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="273"/>
        <source>[线程 %1] [批次 %2] 在 %3 次请求后彻底失败，没有生成字典</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>DictionaryGenerator.generate</name>
    <message>
        <location filename="DictionaryGenerator.cpp" line="286"/>
        <source>没有输入文件，无法生成字典。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="297"/>
        <location filename="DictionaryGenerator.cpp" line="366"/>
        <source>任务终止，将不会生成字典文件</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="301"/>
        <source>阶段二: 搜索并选择信息量最大的文本块(单线程)...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="370"/>
        <source>阶段三: 启动 %1 个线程，向 AI 发送 %2 个任务...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="390"/>
        <source>任务终止，将保存已经生成的字典结果</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="392"/>
        <source>阶段四: 整理并保存结果...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="433"/>
        <source>任务终止，已保留完成审校的词条</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="439"/>
        <source>阶段四: 字典审校 Agent 完成，使用审校后的字典结果</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="453"/>
        <source>人名</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="454"/>
        <source>地名</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="470"/>
        <source>男性</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="471"/>
        <source>女性</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="475"/>
        <source>，与其它字典存在性别争议</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="495"/>
        <source>字典生成完成，共 %1 个词语，已保存到 [%2]</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>DictionaryGenerator.preprocessAndTokenize</name>
    <message>
        <location filename="DictionaryGenerator.cpp" line="49"/>
        <source>阶段一: 预处理和分词...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.cpp" line="123"/>
        <source>共分割成 %1 个文本块，开始进行分词 (使用依赖 Python 且未进行 GPU加速 的分词器这步会非常慢)...</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>DictionaryGeneratorReviewAgent.applyCommitResult</name>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="119"/>
        <source>提交结果 source_term=%1 与当前术语 %2 不匹配</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="128"/>
        <source>无效状态: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="135"/>
        <source>status=%1 时必须提供 final_target</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="142"/>
        <source>status=conflict 时必须提供 final_note</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="148"/>
        <source>status=merged 时必须提供 merge_into</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="192"/>
        <source>merge_into 指向未知术语: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>DictionaryGeneratorReviewAgent.executeToolCalls</name>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="650"/>
        <source>未知工具: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="657"/>
        <source>工具返回结果:
%1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>DictionaryGeneratorReviewAgent.parseAndApplyTurnResponse</name>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="698"/>
        <source>[线程 %1] [术语 %2] [轮次 %3] [请求 %4] 字典审校 Agent 工具调用明细:
%5</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="715"/>
        <source>执行工具调用 %1 个，进入下一轮。调用参数:
%2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="727"/>
        <source>选择跳过，该术语不会输出到最终字典</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="745"/>
        <source>提交审校结果:
%1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>DictionaryGeneratorReviewAgent.parseProtocolResponse</name>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="222"/>
        <source>字典审校 Agent 响应不是合法 JSON 对象</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="230"/>
        <source>无效的字典审校 Agent schema: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="239"/>
        <source>字典审校 Agent 响应缺少动作字段</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="244"/>
        <source>字典审校 Agent 返回了空工具调用</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="249"/>
        <source>字典审校 Agent 返回未知动作: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>DictionaryGeneratorReviewAgent.review</name>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="1036"/>
        <source>字典审校源文件路径数量(%1)与源文件视图数量(%2)不一致</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="1063"/>
        <source>字典审校 Agent 已停止，已保留完成审校的词条。最终保留术语数: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="1069"/>
        <source>字典审校 Agent 完成。最终保留术语数: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>DictionaryGeneratorReviewAgent.reviewTermGroup</name>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="769"/>
        <source>[线程 %1] [术语 %2] 字典审校 Agent 开始处理 `%3`，最多 %4 轮，候选译名 %5 个，候选备注 %6 个，粗候选累计出现 %7 次</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="818"/>
        <source>没有可用的 Api key 了</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="826"/>
        <source>[线程 %1] [术语 %2] [轮次 %3] [请求 %4] 字典审校 Agent 开始请求，上下文 %5 字节</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="837"/>
        <source>[线程 %1] [术语 %2] [轮次 %3] [请求 %4]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="853"/>
        <source>[线程 %1] [术语 %2] [轮次 %3] [请求 %4] 字典审校 Agent 成功响应，响应内容:
%5</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="875"/>
        <source>[线程 %1] [术语 %2] [轮次 %3] [请求 %4] 字典审校 Agent 响应处理成功，处理结果:
%5</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="890"/>
        <source>[线程 %1] [术语 %2] [轮次 %3] [请求 %4] 字典审校 Agent 响应处理失败，错误: %5，响应内容:
%6</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="903"/>
        <source>字典审校 Agent 响应处理失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="926"/>
        <source>[线程 %1] [术语 %2] 字典审校 Agent 因超过最大轮数 (%3 轮) 而失败，该术语不会输出到最终字典</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="935"/>
        <source>[线程 %1] [术语 %2] [轮次 %3] 字典审校 Agent 在 %4 次请求后彻底失败，该术语不会输出到最终字典</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>DictionaryGeneratorReviewAgent.runReviewWorkers</name>
    <message>
        <location filename="DictionaryGenerator.ReviewAgent.cpp" line="950"/>
        <source>字典审校 Agent 启动 %1 个审校线程处理 %2 个术语</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>EpubTranslator.EpubTranslator</name>
    <message>
        <location filename="EpubTranslator.cpp" line="57"/>
        <source>GalTransl++ EpubTranslator 启动...</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>EpubTranslator.epubBeforeRun</name>
    <message>
        <location filename="EpubTranslator.cpp" line="154"/>
        <source>已创建目录: [%1]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="172"/>
        <source>未找到 EPUB 文件</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="209"/>
        <source>正在解压 [%1] 到 [%2]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="283"/>
        <source>[文件 %1] 未找到对应的元数据</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="318"/>
        <source>[文件 %1] 元数据和翻译数据数量不匹配，无法重组 (%2 meta / %3 trans)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="364"/>
        <source>正在打包 %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="371"/>
        <source>无法创建 EPUB (zip) 文件: [%1]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="383"/>
        <source>无法为 mimetype 创建 zip_source_file</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="393"/>
        <source>无法将 mimetype 添加到 zip</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="402"/>
        <source>无法将 mimetype 设置为不压缩模式。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="409"/>
        <source>在源目录 [%1] 中未找到 mimetype 文件，生成的 EPUB 可能无效</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="433"/>
        <source>无法为文件 [%1] 创建 zip_source_file</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="442"/>
        <source>无法将文件 [%1] 添加到 zip</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="453"/>
        <source>关闭 zip 存档时出错: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="458"/>
        <source>已重建 EPUB 文件: [%1]</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>EpubTranslator.epubInit</name>
    <message>
        <location filename="EpubTranslator.cpp" line="90"/>
        <source>预处理正则 `%1` 编译失败</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="117"/>
        <source>预处理正则回调正则 `%1` 编译失败</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="EpubTranslator.cpp" line="142"/>
        <source>Epub 配置文件解析失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>EpubTranslator.~EpubTranslator</name>
    <message>
        <location filename="EpubTranslator.cpp" line="46"/>
        <source>所有任务已完成！EpubTranslator 结束</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>GptDictionary.checkDictUse</name>
    <message>
        <location filename="Dictionary.cpp" line="219"/>
        <source>GPT字典 `%1`-&gt;`%2` 未使用，但使用了 `%3`-&gt;`%4` 这一包含性字典</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="Dictionary.cpp" line="232"/>
        <location filename="Dictionary.cpp" line="270"/>
        <source>GPT字典 `%1`-&gt;`%2` 未使用</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>GptDictionary.getPrompt</name>
    <message>
        <location filename="Dictionary.cpp" line="91"/>
        <location filename="Dictionary.cpp" line="116"/>
        <source>内部错误: 无效的提示词类型</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>GptDictionary.loadFromFile</name>
    <message>
        <location filename="Dictionary.cpp" line="124"/>
        <source>GPT 字典文件 [%1] 不存在</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="Dictionary.cpp" line="159"/>
        <source>GPT 字典文件 [%1] 解析错误: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="Dictionary.cpp" line="165"/>
        <source>已加载 GPT 字典文件 [%1], 共 %2 个词条</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LuaJson.solObj2JsonValue</name>
    <message>
        <location filename="LuaManager.cpp" line="112"/>
        <source>LuaJson: key 必须是字符串</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LuaManager.registerCustomTypes</name>
    <message>
        <location filename="LuaManager.cpp" line="871"/>
        <location filename="LuaManager.cpp" line="882"/>
        <location filename="LuaManager.cpp" line="896"/>
        <location filename="LuaManager.cpp" line="911"/>
        <source>[%1] 未设置 %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="LuaManager.cpp" line="887"/>
        <source>[%1] 已配置 MeCab 分词器，首次使用时加载</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="LuaManager.cpp" line="901"/>
        <source>[%1] 已配置 spaCy 分词器，首次使用时加载</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="LuaManager.cpp" line="916"/>
        <source>[%1] 已配置 Stanza 分词器，首次使用时加载</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="LuaManager.cpp" line="923"/>
        <source>[%1] 已配置 pkuseg 分词器，首次使用时加载</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="LuaManager.cpp" line="930"/>
        <source>[%1] 中注册了无效的 tokenizerBackend: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LuaManager.registerFunction</name>
    <message>
        <location filename="LuaManager.cpp" line="254"/>
        <source>脚本不存在: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="LuaManager.cpp" line="278"/>
        <source>加载脚本 [%1] 失败: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="LuaManager.cpp" line="304"/>
        <source>在脚本 [%1] 中未找到函数 %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LuaTextPlugin.LuaTextPlugin</name>
    <message>
        <location filename="LuaTextPlugin.cpp" line="16"/>
        <source>LuaTextPlugin [%1] 获取 init 函数失败</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="LuaTextPlugin.cpp" line="28"/>
        <source>注册 LuaTextPlugin [%1] 中的 %2 函数成功</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="LuaTextPlugin.cpp" line="51"/>
        <source>调用 LuaTextPlugin [%1] init 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="LuaTextPlugin.cpp" line="60"/>
        <source>LuaTextPlugin [%1] 初始化完毕</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LuaTextPlugin.dPostRun</name>
    <message>
        <location filename="LuaTextPlugin.cpp" line="171"/>
        <source>调用 LuaTextPlugin [%1] dPostRun 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LuaTextPlugin.dPreRun</name>
    <message>
        <location filename="LuaTextPlugin.cpp" line="102"/>
        <source>调用 LuaTextPlugin [%1] dPreRun 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LuaTextPlugin.postRun</name>
    <message>
        <location filename="LuaTextPlugin.cpp" line="148"/>
        <source>调用 LuaTextPlugin [%1] postRun 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LuaTextPlugin.preRun</name>
    <message>
        <location filename="LuaTextPlugin.cpp" line="125"/>
        <source>调用 LuaTextPlugin [%1] preRun 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LuaTextPlugin.~LuaTextPlugin</name>
    <message>
        <location filename="LuaTextPlugin.cpp" line="79"/>
        <source>调用 LuaTextPlugin [%1] unload 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LuaToml.solObj2TomlValue</name>
    <message>
        <location filename="LuaManager.cpp" line="206"/>
        <source>LuaToml: key 必须是字符串</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LuaTranslator.LuaTranslator</name>
    <message>
        <location filename="LuaTranslator.ixx" line="57"/>
        <source>LuaTranslator [%1] 获取 init 函数失败。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="LuaTranslator.ixx" line="64"/>
        <source>LuaTranslator [%1] 获取 run 函数失败。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="LuaTranslator.ixx" line="85"/>
        <source>调用 LuaTranslator [%1] init 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="LuaTranslator.ixx" line="92"/>
        <source>LuaTranslator [%1] 初始化完毕</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LuaTranslator.run</name>
    <message>
        <location filename="LuaTranslator.ixx" line="39"/>
        <source>调用 LuaTranslator [%1] run 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LuaTranslator.~LuaTranslator</name>
    <message>
        <location filename="LuaTranslator.ixx" line="114"/>
        <location filename="LuaTranslator.ixx" line="124"/>
        <source>调用 LuaTranslator unload 函数时出现异常: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="LuaTranslator.ixx" line="129"/>
        <source>所有任务已完成！LuaTranslator [%1] 结束</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NLPTool.getMeCabTokenizeFunc</name>
    <message>
        <location filename="NLPTool.cpp" line="49"/>
        <source>正在检查 MeCab 环境...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NLPTool.cpp" line="62"/>
        <source>无法初始化 MeCab Model。请确保 BaseConfig/mecab/mecabrc 和 %1 存在
错误信息: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NLPTool.cpp" line="71"/>
        <source>无法初始化 MeCab Tagger。请确保 BaseConfig/mecab/mecabrc 和 %1 存在
错误信息: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NLPTool.cpp" line="78"/>
        <source>MeCab 环境检查完毕</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NLPTool.cpp" line="89"/>
        <source>分词器解析失败，错误信息: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NLPTool.getPythonNLPTokenizeFunc</name>
    <message>
        <location filename="NLPTool.cpp" line="134"/>
        <source>Python 模块 [%1] 的模型 %2 的 NLP 函数调用失败，错误信息: %3</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NameTranslator.run</name>
    <message>
        <location filename="NameTranslator.cpp" line="206"/>
        <source>NameTrans: 未找到人名表文件 %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NameTranslator.cpp" line="212"/>
        <source>NameTrans: 开始处理人名表...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NameTranslator.cpp" line="220"/>
        <source>NameTrans: 解析人名表失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NameTranslator.cpp" line="240"/>
        <source>NameTrans: 没有发现需要翻译的名字（所有条目均已有译名）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NameTranslator.cpp" line="245"/>
        <source>NameTrans: 共发现 %1 个待翻译的名字</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NameTranslator.cpp" line="253"/>
        <source>NameTrans: 启动 %1 个线程，每批处理 %2 个名字</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NameTranslator.cpp" line="293"/>
        <source>NameTrans 处理完成，已更新 %1 个译名，保存至 [%2]</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NameTranslator.translateBatch</name>
    <message>
        <location filename="NameTranslator.cpp" line="94"/>
        <source>没有可用的 Api key 了</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NameTranslator.cpp" line="106"/>
        <source>[线程 %1] [批次 %2] [请求 %3] 开始翻译人名，剩余 %4 个:
%5</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NameTranslator.cpp" line="119"/>
        <source>[线程 %1] [批次 %2] [请求 %3]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NameTranslator.cpp" line="135"/>
        <source>[线程 %1] [批次 %2] [请求 %3] 人名翻译成功响应，响应内容:
%4</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NameTranslator.cpp" line="167"/>
        <source>[线程 %1] [批次 %2] [请求 %3] 剩余 %4 个人名均被解析完毕，解析结果:
%5</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NameTranslator.cpp" line="179"/>
        <source>[线程 %1] [批次 %2] [请求 %3] 人名翻译响应解析不完整 (%4 / %5)，解析结果:
%6</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NameTranslator.cpp" line="193"/>
        <source>[线程 %1] [批次 %2] 人名翻译在 %3 次请求后彻底失败，共翻译 (%4 / %5) 个</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalDictionary.loadFromFile</name>
    <message>
        <location filename="Dictionary.cpp" line="284"/>
        <source>字典文件 [%1] 不存在</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="Dictionary.cpp" line="316"/>
        <source>Normal 字典文件 [%1] 正则表达式 `%2` 编译失败</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="Dictionary.cpp" line="347"/>
        <source>Normal 字典文件 [%1] 解析错误: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="Dictionary.cpp" line="353"/>
        <source>已加载 Normal 字典文件 [%1], 共 %2 个词条</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.NormalJsonTranslator</name>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="253"/>
        <source>GalTransl++ NormalJsonTranslator 启动...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="273"/>
        <source>未找到 rolling context 缓存文件 [%1]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="279"/>
        <source>读取 rolling context 缓存文件 [%1] 失败</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.normalJsonAfterRun</name>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="483"/>
        <source>

```
无问题概览
```
</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="495"/>
        <source>已生成 [ProblemOverview.%1] 文件</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="513"/>
        <source>

```
问题概览:
</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="538"/>
        <source>问题概览结束
```
</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="549"/>
        <source>rolling context 缓存已保存至 [%1]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="554"/>
        <source>rolling context 缓存 [%1] 保存失败</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="568"/>
        <source>重建过程中有句子未命中缓存 (%1 / %2 lines)，请检查日志以定位问题</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.normalJsonBeforeRun</name>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="95"/>
        <source>复制缓存文件夹时出现异常: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="103"/>
        <source>已创建目录: [%1]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="141"/>
        <source>第 %1 个对象缺少 message 字段。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="170"/>
        <source>读取文件 [%1] 时出错: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="180"/>
        <source>未找到有效的 Sentence</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="197"/>
        <source>解析原人名表失败</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="223"/>
        <source>已更新 NameTable.toml 文件</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="280"/>
        <source>解析 NameTable.toml 时出错: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="293"/>
        <source>检测到文件分割模式 (%1)，开始预处理输入文件...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="314"/>
        <source>文件 [%1] 已被分割成 %2 份，存入输入缓存</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="322"/>
        <source>分割文件 [%1] 时出错: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="341"/>
        <source>未知的文件分割模式: %1, 请使用 &apos;No&apos;, &apos;Equal&apos;, &apos;Num&apos;</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="371"/>
        <source>未知的排序模式: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="395"/>
        <source>连续重复块引用分析完成，阈值 %1，共配置引用 %2 句</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="405"/>
        <source>连续重复块引用分析完成，未发现长度不小于 %1 的重复块</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.normalJsonInit</name>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="320"/>
        <source>无效的 TransEngine: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="363"/>
        <source>ProjectNote 路径已注册: [%1]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="376"/>
        <source>Agent 模式在 TransEngine %1 下已自动关闭</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="383"/>
        <source>Agent 模式已启用</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="438"/>
        <location filename="NormalJsonTranslator.Core.cpp" line="454"/>
        <source>未找到字典文件 [%1]，已忽略</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="489"/>
        <source>apiStrategy 必须为 random 或 fallback</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="512"/>
        <source>backend.apis[%1] 未找到 Api 协议字段，默认使用 OpenAI 协议</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="519"/>
        <source>backend.apis[%1] apiurl 为空，已忽略</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="530"/>
        <source>backend.apis[%1] modelName 为空且不是 Sakura TransEngine，已忽略</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="591"/>
        <source>找不到可用的 Api key</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="605"/>
        <source>找不到 Prompt.toml 文件</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="622"/>
        <source>Prompt.toml 中缺少 %1 键</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="664"/>
        <source>内部错误: 未知的 TransEngine</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="687"/>
        <source>已配置 MeCab 分词器，首次使用时加载</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="696"/>
        <source>已配置 spaCy 分词器，首次使用时加载</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="705"/>
        <source>已配置 Stanza 分词器，首次使用时加载</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="713"/>
        <source>无效的 tokenizerBackend: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="800"/>
        <source>retranslKeys 正则表达式 `%1` 编译失败</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="818"/>
        <source>retranslKeys 的元素必须是字符串、表或表数组</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="835"/>
        <source>skipProblems 的内联表数组第一个元素必须是字符串</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="851"/>
        <source>skipProblems 的元素必须是字符串或表数组</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="862"/>
        <source>项目配置文件解析失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.normalJsonProcessFiles</name>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="600"/>
        <source>已将 %1 个文件任务分配到线程池，等待处理完成...</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.postProcess</name>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="874"/>
        <location filename="NormalJsonTranslator.Core.cpp" line="973"/>
        <source>翻译失败</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="983"/>
        <source>错误的 GPPCProblem 格式</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.processFile</name>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="27"/>
        <source>处理文件</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="30"/>
        <source>[线程 %1] 开始处理文件: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="80"/>
        <source>[线程 %1] [文件 %2] 解析失败: %3</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="173"/>
        <location filename="NormalJsonTranslator.File.cpp" line="259"/>
        <source>[线程 %1] 缓存文件 [%2] 解析失败: %3</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="311"/>
        <source>[线程 %1] [文件 %2] 共 %3 句，命中缓存/跳过 %4 句，需翻译 %5 句</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="328"/>
        <source>[线程 %1] [文件 %2] 有 %3 句未命中缓存，这些句子是: %4</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="375"/>
        <source>[线程 %1] [文件 %2] 已停止翻译</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="399"/>
        <source>[线程 %1] [文件 %2] 达到保存间隔，正在更新缓存文件...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="418"/>
        <source>[线程 %1] [文件 %2] 重建完成，正在进行最终保存...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="428"/>
        <source>[线程 %1] [文件 %2] 翻译完成，正在进行最终保存...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="460"/>
        <source>[线程 %1] [文件 %2] 处理完成</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="466"/>
        <source>[线程 %1] [文件 %2] 连续重复块引用模式启用，延后最终输出回填与文件回调</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="485"/>
        <source>文件 %1 尚未全部处理完成，跳过合并</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="491"/>
        <source>开始合并 %1 的缓存文件...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.File.cpp" line="498"/>
        <source>[线程 %1] [文件 %2] 合并处理完成</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.resolveRepeatedBlockReferences</name>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="722"/>
        <source>文件 [%1] 仍有未回填的连续重复块引用，跳过本轮最终输出</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="762"/>
        <source>连续重复块引用回填完成，共复制 (%1 / %2) 句</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="776"/>
        <source>文件 [%1] 尚未翻译完毕或分割输出尚未全部回填完成，跳过本轮合并</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.translateBatch</name>
    <message>
        <location filename="NormalJsonTranslator.Batch.cpp" line="48"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [请求 %4] 开始对半拆分句子重新请求...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Batch.cpp" line="73"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [请求 %4] 清空上下文后再次尝试...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Batch.cpp" line="118"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [请求 %4] 开始翻译，剩余 %5 句:
%6</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Batch.cpp" line="145"/>
        <source>没有可用的 Api key 了</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Batch.cpp" line="155"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [请求 %4]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Batch.cpp" line="172"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [请求 %4] 成功响应，响应内容:
%5</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Batch.cpp" line="195"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [请求 %4] 剩余 %5 句文本均被解析完毕，解析结果:
%6</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Batch.cpp" line="209"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [请求 %4] 解析失败或不完整 (%5 / %6), 解析结果:
%7</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Batch.cpp" line="223"/>
        <source>解析失败或不完整 (%1 / %2)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Batch.cpp" line="245"/>
        <source>[线程 %1] [文件 %2] [批次 %3] 在 %4 次请求后彻底失败，共翻译 (%5 / %6) 句</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslator.~NormalJsonTranslator</name>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="233"/>
        <source>所有任务已完成！NormalJsonTranslator 结束，总耗时 %1 秒</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslatorTransAgent.applyAgentSuggestions</name>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="1286"/>
        <source>Agent 建议目标 [%1] 没有缓存文件，已跳过</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="1299"/>
        <source>Agent 建议目标缓存 [%1] 读取失败: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="1333"/>
        <source>Agent 已将 %1 条建议写入缓存问题</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslatorTransAgent.applyCommit</name>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="871"/>
        <source>提交结果缺少句子 %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="879"/>
        <source>提交结果中句子 %1 的 dst 为空</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="990"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5] Agent 译文已提交，但术语账本/建议写入中途出现异常，本次不重新请求: %6</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="1011"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5] Agent 译文已提交，但 file note 写入中途出现异常，本次不重新请求: %6</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslatorTransAgent.executeToolCalls</name>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="655"/>
        <source>未知工具: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="662"/>
        <source>工具返回结果:
%1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslatorTransAgent.formatTermUpdateSuggestion</name>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="375"/>
        <source>注意！从此处生成/更新的术语 `%1` 的译名已由 `%2` 更新为 `%3`。请自行搜索全文以确认是否符合预期</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslatorTransAgent.parseAndApplyTurnResponse</name>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="693"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5] Agent 工具调用明细:
%6</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="711"/>
        <source>执行工具调用 %1 个，进入下一轮。调用参数:
%2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="725"/>
        <source>完成上下文压缩，进入下一轮</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="739"/>
        <source>该批次 %1 句译文均已提交，记录术语 %2 条，记录建议 %3 条，翻译结果:
%4</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslatorTransAgent.parseProtocolResponse</name>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="133"/>
        <source>翻译 Agent 响应不是合法 JSON 对象</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="142"/>
        <source>翻译 Agent 响应缺少动作字段</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="147"/>
        <source>翻译 Agent 返回了空工具调用</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="152"/>
        <source>翻译 Agent 返回未知动作: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslatorTransAgent.runSearchTextTool</name>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="463"/>
        <source>search_text.scope 非法: %1。允许值仅有 current_file|all_files|specified_file</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NormalJsonTranslatorTransAgent.translateBatch</name>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="1075"/>
        <source>[线程 %1] [文件 %2] [批次 %3] Agent 开始翻译，最多 %4 轮，共 %5 句:
%6</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="1093"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [轮次 %4] Agent 上下文接近上限，要求模型先压缩上下文</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="1121"/>
        <source>没有可用的 Api key 了</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="1130"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5] Agent 开始请求，剩余 %6 句，上下文 %7 字节</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="1145"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="1163"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5] Agent 成功响应，响应内容:
%6</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="1188"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5] Agent 响应处理成功，处理结果:
%6</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="1204"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5] Agent 响应处理失败，错误: %6，响应内容:
%7</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="1218"/>
        <source>Agent 响应处理失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="1248"/>
        <source>[线程 %1] [文件 %2] [批次 %3] Agent 因超过最大轮数 (%4 轮) 而失败，共翻译 (%5 / %6) 句</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.TransAgent.cpp" line="1260"/>
        <source>[线程 %1] [文件 %2] [批次 %3] [轮次 %4] Agent 在 %5 次请求后彻底失败，共翻译 (%6 / %7) 句</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PDFTranslator.PDFTranslator</name>
    <message>
        <location filename="PDFTranslator.cpp" line="25"/>
        <source>GalTransl++ PDFTranslator 启动...</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PDFTranslator.pdfBeforeRun</name>
    <message>
        <location filename="PDFTranslator.cpp" line="56"/>
        <source>已创建目录: [%1]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PDFTranslator.cpp" line="74"/>
        <source>未找到 PDF 文件</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PDFTranslator.cpp" line="88"/>
        <source>正在提取文件: [%1]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PDFTranslator.cpp" line="95"/>
        <source>成功提取元数据: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PDFTranslator.cpp" line="100"/>
        <source>提取元数据失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PDFTranslator.cpp" line="111"/>
        <source>[文件 %1] 未找到对应的元数据</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PDFTranslator.cpp" line="123"/>
        <source>正在回注文件: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PDFTranslator.cpp" line="131"/>
        <source>成功翻译文件: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PDFTranslator.cpp" line="136"/>
        <source>翻译文件失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PDFTranslator.pdfInit</name>
    <message>
        <location filename="PDFTranslator.cpp" line="44"/>
        <source>PDF 配置文件解析失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PDFTranslator.~PDFTranslator</name>
    <message>
        <location filename="PDFTranslator.cpp" line="15"/>
        <source>所有任务已完成！PDFTranslator 结束</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>ProblemAnalyzer.analyze</name>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="47"/>
        <source>翻译为空</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="60"/>
        <source>词频过高-&apos;%1&apos;%2次</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="77"/>
        <source>本有 %1 符号</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="84"/>
        <source>本无 %1 符号</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="97"/>
        <source>残留日文: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="108"/>
        <source>引入拉丁字母: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="119"/>
        <source>引入韩文: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="147"/>
        <source>引入繁体字: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="162"/>
        <source>丢失换行(%1/%2)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="176"/>
        <source>多加换行(%1/%2)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="192"/>
        <source>比原文严格长 %1 倍(%2/%3字符)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="207"/>
        <source>比原文长 %1 倍(%2/%3字符)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="258"/>
        <source>无法识别的语言</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="269"/>
        <source>引入(%1, %2)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="287"/>
        <source>非 %1 字符: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>ProblemAnalyzer.setProblemRule</name>
    <message>
        <location filename="ProblemAnalyzer.cpp" line="339"/>
        <source>未知问题: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PythonInterpreterInstance.daemonThreadFunc</name>
    <message>
        <location filename="PythonManager.cpp" line="228"/>
        <source>PythonInterpreterInstance 导入 gpp_plugin_api 时出现异常: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="247"/>
        <source>PythonInterpreterInstance 异常: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PythonMainInterpreterManager.PythonMainInterpreterManager</name>
    <message>
        <location filename="PythonManager.cpp" line="39"/>
        <source>Python 环境未初始化</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PythonMainInterpreterManager.daemonThreadFunc</name>
    <message>
        <location filename="PythonManager.cpp" line="133"/>
        <source>PythonMainInterpreterManager 导入 gpp_plugin_api 时出现异常: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="154"/>
        <source>PythonMainInterpreterManager 异常: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PythonMainInterpreterManager.registerNLPFunction</name>
    <message>
        <location filename="PythonManager.cpp" line="77"/>
        <source>正在加载模块 [%1] 的模型 %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="87"/>
        <source>模块 [%1] 的模型 %2 不可用</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="103"/>
        <source>加载模块 [%1] 的模型 %2 时出现异常: %3</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="113"/>
        <source>模块 [%1] 的模型 %2 已加载</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PythonManager.registerCustomTypes</name>
    <message>
        <location filename="PythonManager.cpp" line="372"/>
        <source>[%1] 已配置 MeCab 分词器，首次使用时加载</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="381"/>
        <source>[%1] 已配置 spaCy 分词器，首次使用时加载</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="391"/>
        <source>[%1] 已配置 Stanza 分词器，首次使用时加载</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="400"/>
        <source>[%1] 已配置 pkuseg 分词器，首次使用时加载</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="409"/>
        <source>[%1] 中注册了无效的 TokenizerBackend: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PythonManager.registerFunction</name>
    <message>
        <location filename="PythonManager.cpp" line="270"/>
        <source>脚本不存在: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="282"/>
        <source>加载模块 %1 时出现异常，子解释器无法开启</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="301"/>
        <source>为模块 %1 加载自定义类型时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="314"/>
        <source>模块 [%1] 插入失败</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="328"/>
        <location filename="PythonManager.cpp" line="336"/>
        <source>从脚本 [%1] 加载函数 %2 失败</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="346"/>
        <source>加载模块 [%1] 的函数 %2 时出现异常: %3</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PythonTextPlugin.PythonTextPlugin</name>
    <message>
        <location filename="PythonTextPlugin.cpp" line="18"/>
        <source>PythonTextPlugin [%1] 获取 init 函数失败</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonTextPlugin.cpp" line="30"/>
        <source>注册 PythonTextPlugin [%1] 中的 %2 函数成功</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonTextPlugin.cpp" line="49"/>
        <source>调用 PythonTextPlugin [%1] init 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonTextPlugin.cpp" line="58"/>
        <source>PythonTextPlugin [%1] 初始化完毕</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PythonTextPlugin.dPostRun</name>
    <message>
        <location filename="PythonTextPlugin.cpp" line="150"/>
        <source>调用 PythonTextPlugin [%1] dPostRun 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PythonTextPlugin.dPreRun</name>
    <message>
        <location filename="PythonTextPlugin.cpp" line="93"/>
        <source>调用 PythonTextPlugin [%1] dPreRun 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PythonTextPlugin.postRun</name>
    <message>
        <location filename="PythonTextPlugin.cpp" line="131"/>
        <source>调用 PythonTextPlugin [%1] postRun 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PythonTextPlugin.preRun</name>
    <message>
        <location filename="PythonTextPlugin.cpp" line="112"/>
        <source>调用 PythonTextPlugin [%1] preRun 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PythonTextPlugin.~PythonTextPlugin</name>
    <message>
        <location filename="PythonTextPlugin.cpp" line="74"/>
        <source>调用 PythonTextPlugin [%1] unload 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PythonTranslator.PythonTranslator</name>
    <message>
        <location filename="PythonTranslator.ixx" line="54"/>
        <source>PythonTranslator [%1] 获取 init 函数失败！</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonTranslator.ixx" line="62"/>
        <source>PythonTranslator [%1] 获取 run 函数失败！</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonTranslator.ixx" line="82"/>
        <source>调用 PythonTranslator [%1] init 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonTranslator.ixx" line="90"/>
        <source>PythonTranslator [%1] 初始化完毕</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PythonTranslator.run</name>
    <message>
        <location filename="PythonTranslator.ixx" line="36"/>
        <source>调用 PythonTranslator [%1] run 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PythonTranslator.~PythonTranslator</name>
    <message>
        <location filename="PythonTranslator.ixx" line="108"/>
        <source>调用 PythonTranslator [%1] unload 函数时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonTranslator.ixx" line="120"/>
        <source>所有任务已完成！PythonTranslator [%1] 结束</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>SkipTrans.SkipTrans</name>
    <message>
        <location filename="SkipTrans.cpp" line="23"/>
        <source>SkipTrans 不支持 %1 阶段运行</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="SkipTrans.cpp" line="59"/>
        <source>skipKeys 正则表达式 `%1` 编译失败</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="SkipTrans.cpp" line="77"/>
        <source>skipKeys 元素必须是字符串、表或表数组</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="SkipTrans.cpp" line="81"/>
        <source>插件 SkipTrans-%1 已加载, skipH: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="SkipTrans.cpp" line="87"/>
        <source>SkipTrans-%1 配置文件解析错误: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>SkipTrans.skipImpl</name>
    <message>
        <location filename="SkipTrans.cpp" line="118"/>
        <source>被第 %1 个 skipKeys 条件匹配到</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>TextFull2Half.TextFull2Half</name>
    <message>
        <location filename="TextFull2Half.cpp" line="54"/>
        <source>TextFull2Half 正则编译错误: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextFull2Half.cpp" line="65"/>
        <source>TextFull2Half-%1 已加载 - 替换标点: %2, 反向替换: %3</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextFull2Half.cpp" line="74"/>
        <source>TextFull2Half-%1 配置文件解析错误: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>TextLinebreakFix.TextLinebreakFix</name>
    <message>
        <location filename="TextLinebreakFix.cpp" line="26"/>
        <source>TextLinebreakFix 不支持 %1 阶段运行</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="64"/>
        <source>TextLinebreakFix-%1 无效的换行模式: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="85"/>
        <source>TextLinebreakFix-%1 已配置 MeCab 分词器，首次使用时加载</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="95"/>
        <source>TextLinebreakFix-%1 已配置 spaCy 分词器，首次使用时加载</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="106"/>
        <source>TextLinebreakFix-%1 已配置 Stanza 分词器，首次使用时加载</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="115"/>
        <source>TextLinebreakFix-%1 已配置 pkuseg 分词器，首次使用时加载</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="124"/>
        <source>TextLinebreakFix-%1 无效的 tokenizerBackend: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="134"/>
        <source>TextLinebreakFix-%1 分段字数阈值必须大于0</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="141"/>
        <source>TextLinebreakFix-%1 报错阈值必须大于0</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="148"/>
        <source>已加载插件 TextLinebreakFix-%1, 换行模式: %2, 优先阈值 %3, 分段字数阈值: %4, 强制修复: %5, 报错阈值: %6</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="159"/>
        <source>插件 TextLinebreakFix-%1 分词器已启用</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="165"/>
        <source>TextLinebreakFix-%1 插件配置文件解析错误: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>TextLinebreakFix.checkLineLength</name>
    <message>
        <location filename="TextLinebreakFix.cpp" line="214"/>
        <source>第 %1 行字数超出报错阈值[%2/%3]</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>TextLinebreakFix.fixLinebreak</name>
    <message>
        <location filename="TextLinebreakFix.cpp" line="233"/>
        <source>需要修复换行的句子[%1]: 原文 %2 行, 译文 %3 行</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="247"/>
        <location filename="TextLinebreakFix.cpp" line="512"/>
        <source>换行修复</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="248"/>
        <location filename="TextLinebreakFix.cpp" line="513"/>
        <source>原文 %1 行, 译文 %2 行, 修正后 %3 行</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="253"/>
        <source>译文[%1](%2行) -&gt; 修正后译文[%3](%4行)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="496"/>
        <source>译文分词结果</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="503"/>
        <source>无效的 TextLinebreakFix 模式</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="TextLinebreakFix.cpp" line="518"/>
        <source>句子[%1](%2行) -&gt; 修正后译文[%3](%4行)</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>buildContextHistory</name>
    <message>
        <location filename="NormalJsonTranslatorHelperTool.cpp" line="436"/>
        <source>未知的 PromptType</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>buildProblemOverviewFromCache</name>
    <message>
        <location filename="NormalJsonTranslator.Run.cpp" line="74"/>
        <source>构建问题概览时读取缓存文件 [%1] 失败: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>checkPythonDependencies</name>
    <message>
        <location filename="PythonManager.cpp" line="429"/>
        <source>正在检查依赖 %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="434"/>
        <source>依赖 %1 已安装</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="441"/>
        <source>检查依赖 %1 时出现异常: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="449"/>
        <source>依赖 %1 未安装，正在尝试安装</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="453"/>
        <source>将在 3s 后开始安装依赖，请勿关闭接下来出现的窗口！</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="456"/>
        <source>正在执行安装命令: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="462"/>
        <source>安装依赖 %1 的命令失败</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="469"/>
        <source>依赖 %1 安装成功</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="474"/>
        <source>依赖 %1 安装验证失败: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="484"/>
        <source>依赖 %1 检查完毕</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="PythonManager.cpp" line="486"/>
        <source>所有依赖均已安装</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>checkResponse</name>
    <message>
        <location filename="ApiPool.cpp" line="101"/>
        <source>%1 [HTTP %2]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ApiPool.cpp" line="117"/>
        <source>%1 Api 响应 JSON 解析失败。错误: %2，原始响应:
%3</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ApiPool.cpp" line="121"/>
        <location filename="ApiPool.cpp" line="163"/>
        <location filename="ApiPool.cpp" line="186"/>
        <location filename="ApiPool.cpp" line="216"/>
        <location filename="ApiPool.cpp" line="239"/>
        <source>空</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ApiPool.cpp" line="127"/>
        <source>Api 响应 JSON 解析失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ApiPool.cpp" line="139"/>
        <source>%1 切换到下一个 Api key</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ApiPool.cpp" line="159"/>
        <source>%1 Api key [%2] 疑似额度用尽，短期内多次报告将从池中移除。原始响应:
%3</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ApiPool.cpp" line="169"/>
        <source>Api key 疑似额度用尽: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ApiPool.cpp" line="170"/>
        <location filename="ApiPool.cpp" line="194"/>
        <location filename="ApiPool.cpp" line="223"/>
        <location filename="ApiPool.cpp" line="246"/>
        <source>响应为空</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ApiPool.cpp" line="182"/>
        <source>%1 Api key [%2] 没有可用模型，短期内多次报告将从池中移除。原始响应:
%3</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ApiPool.cpp" line="192"/>
        <source>Api key 没有模型 %1: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ApiPool.cpp" line="212"/>
        <source>%1 遇到频率限制或可再次请求错误，将等待 %2 秒后重新请求。原始响应:
%3</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ApiPool.cpp" line="222"/>
        <source>遇到频率限制或可再次请求错误: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ApiPool.cpp" line="236"/>
        <source>%1 遇到未知 Api 错误，原始响应:
%2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ApiPool.cpp" line="245"/>
        <source>遇到未知 Api 错误: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ApiPool.cpp" line="257"/>
        <source>%1 将切换到下一个 Api key</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>chooseCachePart</name>
    <message>
        <location filename="Tool.cpp" line="808"/>
        <source>无效的 CachePart %1</source>
        <oldsource>内部错误: 无效的 CachePart %1</oldsource>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>chooseStringRef</name>
    <message>
        <location filename="Tool.cpp" line="759"/>
        <source>无效的条件目标 None</source>
        <oldsource>内部错误: 无效的条件目标 None</oldsource>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="Tool.cpp" line="761"/>
        <source>无法获取字符串的无效条件目标 %1</source>
        <oldsource>内部错误: 无法获取字符串的无效条件目标 %1</oldsource>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>combineOutputFiles</name>
    <message>
        <location filename="NormalJsonTranslatorHelperTool.cpp" line="728"/>
        <source>开始合并文件: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslatorHelperTool.cpp" line="745"/>
        <source>试图合并 %1 时出错，缺少文件 %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslatorHelperTool.cpp" line="754"/>
        <source>文件 %1 合并完成，已保存到 %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>countGraphemes</name>
    <message>
        <location filename="Tool.cpp" line="231"/>
        <source>打开 UTF-8 文本失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="Tool.cpp" line="240"/>
        <source>创建字符边界迭代器失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="Tool.cpp" line="246"/>
        <source>设置字符边界迭代文本失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>createTranslator</name>
    <message>
        <location filename="ITranslator.cpp" line="145"/>
        <source>找不到配置文件 [%1]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ITranslator.cpp" line="175"/>
        <source>无效的日志等级: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ITranslator.cpp" line="202"/>
        <source>日志记录器初始化完成</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="ITranslator.cpp" line="225"/>
        <location filename="ITranslator.cpp" line="249"/>
        <source>无效的基类名称: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>fillBlockAndMap</name>
    <message>
        <location filename="NormalJsonTranslatorHelperTool.cpp" line="507"/>
        <source>内部错误: 不支持的 TransEngine 用于构建输入</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>json2Toml</name>
    <message>
        <location filename="Tool.ixx" line="403"/>
        <source>不支持的 JSON 数据类型: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>loadTokenizeCache</name>
    <message>
        <location filename="Tool.cpp" line="1105"/>
        <source>未找到分词缓存 [%1]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="Tool.cpp" line="1111"/>
        <source>读取分词缓存 [%1] 失败: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>parseApiProtocol</name>
    <message>
        <location filename="ApiTool.cpp" line="27"/>
        <source>无效的 Api 协议: %1 不在 {openai, claude,  gemini} 中</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>parseContent</name>
    <message>
        <location filename="NormalJsonTranslatorHelperTool.cpp" line="709"/>
        <source>内部错误: 不支持的 TransEngine 用于解析输出</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>parseToml</name>
    <message>
        <location filename="Tool.ixx" line="277"/>
        <source>无效的 TOML 路径: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="Tool.ixx" line="295"/>
        <source>无法在 TOML 中找到值: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>saveTokenizeCache</name>
    <message>
        <location filename="Tool.cpp" line="1123"/>
        <source>分词缓存已保存到 [%1]</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="Tool.cpp" line="1128"/>
        <source>分词缓存 [%1] 保存失败</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>splitIntoGraphemes</name>
    <message>
        <location filename="Tool.cpp" line="182"/>
        <source>打开 UTF-8 文本失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="Tool.cpp" line="192"/>
        <source>创建字符边界迭代器失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="Tool.cpp" line="199"/>
        <source>设置字符边界迭代文本失败: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>splitIntoTokens</name>
    <message>
        <location filename="NLPTool.cpp" line="160"/>
        <source>在原句剩余部分中找不到 token &apos;%1&apos;。</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>splitTsvLine</name>
    <message>
        <location filename="Tool.cpp" line="116"/>
        <source>内部错误: TSV 行切分不允许使用空分隔符</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>testApiConnection</name>
    <message>
        <location filename="ApiTool.cpp" line="551"/>
        <source>Api 响应 JSON 解析失败，%1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>toml2Json</name>
    <message>
        <location filename="Tool.ixx" line="363"/>
        <source>不支持的 TOML 数据类型: %1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>validateNormalJsonCoreConfig</name>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="26"/>
        <source>配置项 %1 无效: 当前值 %2，要求%3</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="71"/>
        <location filename="NormalJsonTranslator.Core.cpp" line="79"/>
        <location filename="NormalJsonTranslator.Core.cpp" line="87"/>
        <location filename="NormalJsonTranslator.Core.cpp" line="119"/>
        <location filename="NormalJsonTranslator.Core.cpp" line="143"/>
        <location filename="NormalJsonTranslator.Core.cpp" line="151"/>
        <location filename="NormalJsonTranslator.Core.cpp" line="167"/>
        <location filename="NormalJsonTranslator.Core.cpp" line="175"/>
        <location filename="NormalJsonTranslator.Core.cpp" line="183"/>
        <location filename="NormalJsonTranslator.Core.cpp" line="196"/>
        <location filename="NormalJsonTranslator.Core.cpp" line="221"/>
        <source>大于 0</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="95"/>
        <source>为 name 或 size</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="103"/>
        <source>为 No、Num 或 Equal</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="111"/>
        <source>为 toml 或 json</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="127"/>
        <source>大于等于 2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="135"/>
        <location filename="NormalJsonTranslator.Core.cpp" line="159"/>
        <location filename="NormalJsonTranslator.Core.cpp" line="212"/>
        <source>大于等于 0</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="NormalJsonTranslator.Core.cpp" line="204"/>
        <source>大于等于 1</source>
        <translation type="unfinished"></translation>
    </message>
</context>
</TS>
