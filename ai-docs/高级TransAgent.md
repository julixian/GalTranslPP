# 使用高级 TransAgent

高级模式使用原生工具调用，并按 worker 保留跨文件会话。请求依赖接口默认的非流式行为，代码不设置 stream 字段，也不干预自定义 body 对它的覆盖。总开关默认关闭，关闭时继续使用原有批次文本协议。Agent 仅用于 ForGalTsv、ForNovelTsv；GenDict 使用普通字典生成流程。

按实际调用顺序熟悉实现，可阅读 [高级TransAgent代码链路.md](高级TransAgent代码链路.md)。

## 开启

在 GUI 的「Agent 模式」下开启「高级 Agent」，或修改项目 `Config.toml` 中已有的表：

```toml
[common.agent]
enabled = true
advancedEnabled = true
```

使用 `ForGalTsv` 或 `ForNovelTsv`。API 行内的高级选项默认如下：

```toml
agentStrictTools = true
agentStateful = true
agentNativeAutoCompaction = false
agentCompactThresholdTokens = 0
agentGeminiInteractions = true
```

中转或模型不支持某项能力时，在「Api 设置 → 高级 Agent」中关闭对应选项。程序不发探测请求，也不静默更换协议。

## 协议能力

| 协议 | 工具调用 | 工具选择 | 会话与推理 |
| --- | --- | --- | --- |
| `openai` | `tools` / `tool_calls` / `tool` 消息 | `required` | 客户端保存完整消息 |
| `openaires` | `function_call` / `function_call_output` | `required` | 保存原始 output；可用 `previous_response_id` 续接 |
| `claude` | `tool_use` / `tool_result`，可开启 `strict` | `auto`，提示词要求工具提交 | 保存完整内容块，包括 thinking、签名、redacted_thinking |
| `gemini` + Interactions | `function_call` / `function_result` 步骤 | `any` | 保存步骤和 thought signature；可用 `previous_interaction_id` 续接 |
| `gemini` + generateContent | `functionCall` / `functionResponse` | `ANY` | 保存完整 model parts，包括 thoughtSignature |

高级模式只接受 `commit_translations` 工具提交译文，压缩摘要只接受 `compact_context` 工具。回复正文保留在历史和日志中，不再作为提交入口，也不发送正文 JSON schema。正常轮次允许查询或提交，不会强制每一轮都提交；只返回正文时提醒模型调用工具并继续下一轮，连续未调用工具达到 `maxRequestCount` 后结束当前批次，任意工具调用都会清零该计数。

`agentStrictTools` 控制 OpenAI/Claude 的工具参数 strict 约束；Gemini 固定使用要求函数调用并约束参数结构的 any/ANY 模式。工具参数 schema 不替代提交时检查当前句子是否全部有译文。Claude 的手动思考和部分新模型不支持强制工具调用，因此保留 auto，通过提示词和业务反馈要求工具提交。

`agentStateful` 仅用于 Responses 和 Gemini Interactions，会启用服务端存储；关闭后发送客户端保存的完整历史。API、模型、凭据或请求选项改变时重建会话，不迁移服务端 id 或签名。

原生会话历史和推理始终保留，跨批次和跨文件继续使用同一 worker 的会话；仅在 API 身份改变或压缩重建时清空，不裁剪签名块。

`agentNativeAutoCompaction` 控制原生自动压缩，仅用于 Responses 和 Claude，默认关闭。Responses 使用 `context_management` 自动压缩；Claude 使用 `compact-2026-01-12` beta 的阈值压缩，需模型和中转支持。不支持时关闭该选项，程序不通过请求错误自动探测能力。

Claude 返回有效 compaction 块后，客户端会删除它之前的历史，只保留最新有效压缩块及之后的回复、推理和工具调用，后续请求直接发送这份缩短的历史。只有整个响应解析成功且压缩摘要为非空字符串时才清理；压缩失败返回空摘要时保留旧历史。

在 Chat Completions 或 Gemini 上设置为 true 不会发送原生自动压缩参数，仍使用字节阈值。Responses/Claude 设置为 true 时会直接传参数；不支持的模型或中转可能拒绝请求，按现有 API 错误流程处理，不自动关闭开关。中转若忽略该字段，也不能据此认为已经启用压缩。

API 属性 `agentCompactThresholdTokens` 只控制原生自动压缩，单位为 tokens。设为 0 时，Responses 沿用本项目的默认阈值 100000；Claude 省略 trigger，采用服务端默认的 150000。正数对 Claude 同样生效，会写入 `trigger = {type: "input_tokens", value: ...}`；Claude 官方要求至少 50000。可通过 `extraBody` 覆盖原生参数。

没有启用原生压缩的高级会话以及原有文本流程，都使用 `compactContextThresholdBytes`。高级模式在每轮请求前检查客户端原生历史的 JSON 字节数；超过阈值后，专门请求模型通过 `compact_context` 总结完整历史。成功收到非空 `rolling_context` 后，清除旧消息、服务端会话 id 和推理签名，用新摘要、文件备注、术语和当前未提交批次重建会话。摘要无效时直接重建，沿用上次成功提交的滚动记忆；请求错误先经过统一重试与轮转，压缩请求重试耗尽后也按此方式重建。重建不会删除已经保存的译文、术语和备注。压缩占用一个 Agent 轮次；刚重建的批次在历史增加前不会反复触发压缩。

ID 续接与原生压缩是独立功能。Responses 和 Gemini Interactions 即使使用服务端 id，客户端仍保存整个会话的已发送输入、工具结果及返回内容。没有原生压缩时，字节阈值检查这份完整历史，而非当次发送的新增消息。它只是本地估算，不等于包含隐藏推理在内的服务端 token 数。开启原生压缩的 Responses、Claude 则由服务端按有效上下文 tokens 判断阈值，客户端不使用字节数触发摘要重建。

Chat Completions、Gemini generateContent 和普通模型的 Interactions 当前没有这里所用的原生压缩参数，使用上述摘要流程。Interactions 的服务端会话续接和上下文缓存不等于自动摘要压缩。

Gemini 的 `agentGeminiInteractions = false` 使用 generateContent；2.5 等采用 thinkingBudget 的旧模型应选择该路径。思考等级继续沿用已有模型规则。Interactions v1 的 generation_config 不使用温度、top_p 或惩罚参数，这条路径不传递 GUI 中对应的采样选项；自定义请求字段仍可通过 extraBody 配置。

## worker 和 API 选择

每个 worker 初始化时根据 `apiStrategy` 选择 API。换文件只追加当前批次，不重建会话，由模型判断此前上下文是否相关。

- `random`：保持已选 API，直到它被移出池。
- `fallback`：每批开始前重新取 API；报错后也允许轮转，API 改变才重建会话。
- 正常的一批工具循环固定使用同一 API。重建后发送未提交批次、当前文件备注、术语和 worker 滚动记忆。
- 每个原生工具调用都有对应结果，包括参数解析失败和重复提交。成功提交后也回填本轮其它调用的结果，再结束批次。

原生历史只在本轮运行的内存中保存。可持久化记忆仍是术语账本、文件备注和翻译缓存。

高级 Agent 的 HTTP 错误、协议 JSON 解析失败、明确拒答或截断，均由 API 层生成 `ApiError`，分别保存错误类型、说明、原始响应和 HTTP 状态。普通 batch、默认 Agent 和高级 Agent 共用协议错误判断；`handleApiError` 只负责日志、等待和重试策略，展示时先放说明，再附原始响应。每个模型轮次重新计算 `maxRequestCount`；普通错误计数并等待 2 秒，限流类错误等待 1～64 秒且不计数，无效 key/额度与模型无权限类错误记录 API 健康且不计数。因此最大请求次数不是所有 HTTP 调用的硬上限。fallback 可按现有策略调整 API，身份改变时重建会话；random 保持原 API 直到被移出池。

工具参数或提交不完整时，错误按调用 id 回填，让模型在下一轮修正；只返回正文时提醒调用工具。它们占用 `maxTurnsPerChunk`，不计入 HTTP 请求重试数；连续无工具调用另行计数，上限同样使用 `maxRequestCount`，有工具调用即清零。请求重试耗尽、连续无工具调用达到上限或达到最大轮数，当前批次标记失败；压缩摘要失败则沿用上次有效滚动记忆重建会话后继续。

`checkQuota` 在高级模式生效；`smartRetry` 的折半及失败清空上下文、`enhanceJailbreak` 的 assistant 预填充仅用于原有文本流程，高级路径不使用它们。旧的“携带上文数量”配置和历史译文注入代码已删除，滚动记忆与 Agent 原生历史继续保留。

高级模式中的 `rollingContext` 作为 worker 的摘要备份继续保留。每次有效提交更新它，并在批次提示词中携带；API 更换以及会话压缩或重建时用它接续。正常会话的主要上下文来自原生历史或服务端 id，它不替代原生推理记忆。手动摘要成功时也会更新它；摘要失败则使用上次有效值重建。外层文件流程仍通过原有滚动记忆缓存保存中断时的文件记忆。

## 工具

| 工具 | 数据来源 |
| --- | --- |
| `list_files` | 源文件列表和行数 |
| `read_source` / `search_source` | 预处理后的只读原文，包含 `name`、`src` |
| `read_cache` / `search_cache` | 已保存的内存缓存或磁盘缓存，包含 `name`、`src`、`dst` |
| `read_dictionary` / `search_dictionary` | 配置的 GPT 字典，包含 `src`、`dst`、`note` |
| `read_terms` / `search_terms` | 共享术语账本，包含 `src`、`dst`、状态和出现位置 |
| `read_file_note` / `read_project_note` | 文件备注和用户配置的项目备注 |
| `commit_translations` | 提交当前批次译文、术语更新、文件备注和滚动记忆 |

read/search 的参数统一为 `file`、`ids`、`fields`、`offset`、`limit`。搜索另有 `query`、`match_fields`、`context_before`、`context_after`。`fields` 只控制返回列，`match_fields` 控制匹配列；`file` 和 `id` 始终返回。源文工具不会混入缓存译文。

`file = ""` 表示当前文件，`"*"` 表示全部文件；字典和术语忽略 file。`ids = []` 表示不限 id，`fields = []` 返回全部列，`match_fields = []` 搜索全部数据列。`limit = 0` 使用配置的结果上限。`offset` 表示跳过多少条结果，0 表示从第一条开始，不是页码或句子 id；搜索时跳过的是匹配结果。字典和术语的 id 是当前快照内的条目序号。

`read_project_note` 在正常高级轮次始终声明，与是否配置备注路径无关；压缩轮次仅声明 `compact_context`。配置加载时，仅当 `projectNotePath` 对应的文件实际存在，才注册其路径；未注册或文件后来被删除时，工具返回 `available=false` 和空内容。

例如查原文中的角色名，只返回说话人和原文：

```json
{
  "file": "*",
  "ids": [],
  "fields": ["name", "src"],
  "offset": 0,
  "limit": 10,
  "query": "アリス",
  "match_fields": ["name", "src"],
  "context_before": 2,
  "context_after": 2
}
```

## 提示词与代码入口

`Example/BaseConfig/Prompt.toml` 提供 `FORGALTSV_AGENT_ADVANCED_SYSTEM/USER` 和 `FORNOVELTSV_AGENT_ADVANCED_SYSTEM/USER`。项目可用同名键覆盖；项目和 BaseConfig 都没有所需键时直接抛出缺键异常，代码没有内置默认提示词。高级提示词只描述翻译任务，工具参数由 API 声明，不需要手写文本动作协议。可用的批次占位符与原 Agent 相同。

高级提示词以原有 Agent 提示词为基础，保留完整翻译要求、示例、术语优先级、文件备注和滚动记忆规则，仅将文本协议改为原生工具调用。`Prompt_MyCustom.toml` 中对应的高级键保留该文件自己的中文风格要求和示例。系统提示词通过各协议的系统指令字段发送；用户提示词由 `buildBaseMessages()` 填入目标语言、当前原文、词典、问题提示、文件备注、术语和滚动记忆后加入会话。工具声明只负责工具名称、用途及参数结构，不代替业务提示词。`agent_suggest` 也通过提交工具参数保留，用于把句子级翻译疑点记录到缓存。

原有文本 Agent 的读、搜索、备注工具与高级模式共用 `runReadTool()`：名称和参数全部对齐，旧 `read_lines/search_text/search_term/get_file_note/get_project_note` 不作为别名保留。自定义文本提示词需要更新工具声明和调用示例；文本 `action=tool_calls|commit|compact_context` 协议保持现有形式。

原生 Agent 请求和内容提取在 `GalTranslPP/ApiTool.Agent.cpp`；共用错误分类及协议错误判断在 `GalTranslPP/ApiTool.Error.cpp`；worker 会话、工具声明和执行在 `GalTranslPP/NormalJsonTranslator.TransAgent.Advanced.cpp`。旧批次流程继续在 `NormalJsonTranslator.TransAgent.cpp`。HTTP 发送共用 `sendApiHttpRequest`，失败统一交给 `handleApiError`。插件返回的最终请求 JSON 直接发送，不做二次校验。

## 协议依据

本次已通过 `mcpp build -p GPPGUI -p GPPCLI --profile fast-release -j 4`，并检查示例 TOML、TS 翻译和内置 ZIP 配置。未模拟响应，也未调用真实模型 API。

- [OpenAI Docs：Function calling](https://developers.openai.com/api/docs/guides/function-calling)、[Reasoning](https://developers.openai.com/api/docs/guides/reasoning)、[Compaction](https://developers.openai.com/api/docs/guides/compaction)。
- [Claude：Thinking](https://platform.claude.com/docs/en/build-with-claude/thinking)、[工具选择限制](https://platform.claude.com/docs/en/agents-and-tools/tool-use/define-tools#forcing-tool-use)、[Threshold compaction](https://platform.claude.com/docs/en/build-with-claude/compaction-threshold)。
- [Gemini Interactions v1](https://ai.google.dev/api/interactions-api-v1)、[generateContent thought signatures](https://ai.google.dev/gemini-api/docs/generate-content/thought-signatures)。
