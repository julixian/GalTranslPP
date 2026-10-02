# 高级 TransAgent 代码阅读路线

本文件按一个 worker 实际翻译批次的顺序说明高级路径。使用方式和协议选项见 [高级TransAgent.md](高级TransAgent.md)。

## 1. 从配置进入

入口在 `GalTranslPP/NormalJsonTranslator.Core.cpp` 的 `normalJsonInit()`：

- `common.agent.enabled` 决定是否使用 TransAgent。
- `common.agent.advancedEnabled` 写入 `m_agentAdvancedEnabled`，决定使用原生工具流程还是原有文本协议。
- `backend.apis` 的每一项构造 `TranslationApi`，高级协议选项跟着具体 API 保存。
- 提示词键在原来的 TransEngine 分支中选择：高级模式使用 `FORGALTSV_AGENT_ADVANCED_SYSTEM/USER` 或 `FORNOVELTSV_AGENT_ADVANCED_SYSTEM/USER`；仍按“项目 Prompt.toml → BaseConfig/Prompt.toml”读取，都缺键就抛出异常，没有代码中的默认提示词。

`TranslationApi` 以及下面的会话、回复类型定义在 `GalTranslPP/ApiTool.ixx`。

| API 字段 | 控制的行为 |
| --- | --- |
| `agentStrictTools` | OpenAI/Claude 工具参数 schema 的 strict 模式；Gemini 固定使用 any/ANY |
| `agentStateful` | Responses/Interactions 使用服务端 id 续接；关闭时回传客户端历史 |
| `agentNativeAutoCompaction` | Responses/Claude 在普通请求中启用服务端自动压缩 |
| `agentCompactThresholdTokens` | 原生自动压缩的 token 阈值；0 使用现有默认行为 |
| `agentGeminiInteractions` | Gemini 选择 Interactions 或 generateContent |

自动压缩字段已从 `agentNativeCompaction` 改为 `agentNativeAutoCompaction`，配置加载和 GUI 保存都使用新名字，旧名字不作为别名读取。

## 2. 初始化固定 worker

`NormalJsonTranslator.Run.cpp::normalJsonBeforeRun()` 创建 `NormalJsonTranslatorTransAgent`，传入 API 池、提示词、缓存、共享锁和工具限制，然后调用：

```cpp
m_transAgent->configureAdvanced(m_agentAdvancedEnabled, m_threadsNum);
```

`configureAdvanced()` 位于 `NormalJsonTranslator.TransAgent.Advanced.cpp`，完成两件事：

1. 分配 `m_workers`，根据 `apiStrategy` 给每个 worker 初选 API。
2. 建立 `m_nativeTools`，提交 schema 作为 `commit_translations` 的参数声明保存在工具列表中，供后面的请求发送。

`TransAgentWorker` 定义在 `NormalJsonTranslator.TransAgent.ixx`。每个 worker 保存一个可选的 `ApiAgentSession` 和一份 `rollingContext`，没有按文件创建新的 Agent。

线程池在 `normalJsonProcessFiles()` 中调用 `processFile(relFilePath, id + 1)`。同一线程陆续处理不同文件，使用同一份 `m_workers[threadId - 1]`，每个 worker 的会话只由对应线程访问。

## 3. 切批次并选择路径

`NormalJsonTranslator.File.cpp::processFile()` 负责加载缓存、预处理、切分批次，并调用：

```text
processFile
  → m_transAgent->translateBatch
      → 高级开关开启：translateAdvancedBatch
      → 高级开关关闭：原有批次文本流程
```

分支在 `NormalJsonTranslator.TransAgent.cpp::translateBatch()` 的开头。旧流程仍留在这个文件，高级流程集中在新增的 `NormalJsonTranslator.TransAgent.Advanced.cpp`。

## 4. 取得会话并追加当前批次

进入 `translateAdvancedBatch()` 后，先过滤已完成或空原文的句子，找到对应 worker，再执行 `selectApi(true)`：

- `random` 初始化选定后保持 API，直到它不在池中。
- `fallback` 每批开始重新取池首 API；只有身份改变才重建会话。
- 请求错误允许重新选择 API；正常工具轮继续使用同一个 API。

`ApiPool.cpp::containsApi()` 和 `ApiTool.Agent.cpp::isSameApi()` 判断 API 是否仍可使用以及是否属于同一会话身份。身份包括协议、地址、凭据、模型及请求选项，不包括健康计数。

接着通过 `buildBaseMessages()` 生成当前批次的原文、文件备注、术语提示和滚动记忆，再通过 `appendAgentUserMessage()` 写入对应协议的原生历史。默认换文件也只追加消息，不重建会话。

## 5. 区分会话历史和本轮发送内容

`ApiAgentSession` 中几个字段的用途：

| 字段 | 用途 |
| --- | --- |
| `api` | 此会话固定使用的 API 配置快照 |
| `systemPrompt` | 系统提示词 |
| `history` | 客户端保存的完整原生输入、工具结果和模型返回内容 |
| `previousId` | Responses 或 Interactions 最新的服务端续接 id |
| `sentCount` | 最新成功响应之后，服务端已知历史的结束位置 |

`ApiTool.Agent.cpp::pendingAgentInput()` 判断发送多少内容：没有 `previousId` 就发送完整历史；有 id 就发送 `history[sentCount:]`，服务端根据 id 找回之前的部分。

所以 `history` 并没有因为使用 id 而丢掉。没有原生自动压缩时，GPP 仍能根据完整本地历史判断字节阈值。

## 6. 构造协议请求、发送并解析

调用入口是 `ApiTool.Agent.cpp::performAgentApiRequest()`：

```text
buildAgentPayload(session, tools)
  → applyApiPayloadOptions(payload, api)
  → Gemini Interactions 的思考参数转换 / Claude 自动压缩 beta 头
  → extraBody 覆盖
  → onPerformApi 插件处理最终请求正文
  → sendApiHttpRequest
  → json::parse
  → parseAgentReply
```

`buildAgentPayload()` 和 `parseAgentReply()` 按协议分别处理，不把原生会话全部转换成旧的文本 messages。

| 协议 | 请求与工具结果 | 回复与历史 |
| --- | --- | --- |
| Chat Completions | messages、tools、tool 消息 | assistant 消息及 tool_calls |
| Responses | input、instructions、function_call_output | 原样保存 output，包括 reasoning、compaction 等项目 |
| Claude | 顶层 system、messages、tool_result | 保存 content 块，包括 thinking、签名；收到有效 compaction 后只保留该块及之后的内容 |
| Gemini Interactions | input 步骤、function_result | 保存 steps 和最新 interaction id |
| Gemini generateContent | contents、functionResponse | 完整保存 model parts 和 thoughtSignature |

共用的 `applyApiPayloadOptions()` 在 `ApiTool.cpp`，继续处理模型名、思考等级、采样选项、Claude 默认 max_tokens 等。高级路径先暂存 extraBody，等协议参数准备完成后再覆盖，最后交给插件；不对插件返回的最终请求 JSON 加校验。

`sendApiHttpRequest()` 只负责非流式 HTTP。协议解析成功后才把本轮响应追加到历史并更新 `previousId/sentCount`，避免失败时留下半截历史。

返回的 `ApiAgentReply` 统一提供 `text` 和 `calls`。完整原生响应仍保存在 session.history，不会被这两个业务字段替换。

会话历史和推理始终保留，不在批次边界清空。OpenAI 使用 required，Gemini 使用 any/ANY，要求每轮调用工具；Claude 为兼容思考与模型限制保留 auto，由提示词要求工具提交。正文不再配置 JSON schema。

## 7. 执行工具并进入下一轮

`translateAdvancedBatch()` 遍历 `reply.calls`，按工具 id 回填结果：

- `read_source/search_source` 读取预处理原文。
- `read_cache/search_cache` 读取已有译文缓存。
- `read_dictionary/search_dictionary` 读取翻译字典。
- `read_terms/search_terms` 读取共享术语账本。
- `list_files/read_file_note/read_project_note` 读取文件列表与备注。
- `commit_translations` 提交当前批次。

普通读取与搜索集中在 `NormalJsonTranslator.TransAgent.Tools.cpp::runReadTool()`。原生和文本 Agent 都调用它，共用工具名称、参数以及结果格式。`fields` 控制返回列，`match_fields` 控制搜索列；`file/id` 始终返回。`offset/limit` 是分页，`ids` 是句子或记录 id；搜索还可以带前后文。原有四套文本工具实现已经删除。

工具参数解析或执行失败时，错误作为对应调用的工具结果回传。`appendAgentToolResults()` 将统一结果转换成各协议要求的原生格式，再进入下一轮。一个响应包含多个工具调用时，即使其中一次提交成功，也先给其他调用回填结果。

仅接受 `commit_translations` 工具提交，正文只用于历史和日志。没有工具调用时，追加提醒要求调用查询或提交工具，再进入下一轮，不尝试解析正文 JSON。连续无工具调用达到 `m_maxRequestCount` 后结束当前批次，任意工具调用都会清零该计数；它与 HTTP 请求重试计数分别维护。

## 8. 提交译文并回到外层

高级流程中的 `commit` lambda 先处理严格 schema 下的空备注和空术语出现记录，再复用：

```text
parseProtocolResponse
  → applyCommit
  → 更新 worker.rollingContext
  → 回填工具结果
  → 返回 processFile
```

`applyCommit()` 位于原有的 `NormalJsonTranslator.TransAgent.cpp`。它检查当前待翻译句子是否全部有译文，再写入 Sentence、术语账本及文件备注。

返回外层后，`processFile()` 继续后处理、记录进度和保存翻译缓存。原生历史由 worker 在本次运行中保存，持久化记忆由现有缓存和备注流程保存。

## 9. 压缩及错误如何插入主循环

每轮请求前先检查压缩：

- 开启 `agentNativeAutoCompaction` 且协议为 Responses/Claude：请求附带原生自动压缩参数，服务端根据 tokens 判断阈值，客户端保存并继续使用返回的原生压缩内容，不单独调用 `/responses/compact`。Claude 响应解析成功且包含非空摘要的 compaction 块时，删除该块之前的本地历史，只保留最新有效压缩块及后续内容；空摘要不会清除旧历史。
- 其他情况：检查 `history.dump().size()` 是否超过 `compactContextThresholdBytes`，专门发送只有 `compact_context` 工具的请求，摘要必须通过该工具返回。
- 摘要成功：用新 rolling_context 重建会话，再追加当前未提交批次。
- 摘要无效或压缩请求重试耗尽：沿用上次有效滚动记忆重建，不继续保留越来越长的历史重试摘要。

重建通过 `rebuildSession()` 清除旧历史、服务端 id 和 sentCount；通过 `appendBatch()` 重新带入摘要、文件备注、术语和当前批次。重建不会删除已提交的译文与持久化记忆。

请求失败统一返回 `ApiError`，`type`、`message`、`rawResponse`、`statusCode` 分别保存分类、说明、原始响应和 HTTP 状态。`ApiTool.Error.cpp::parseApiResponse()` 共用 JSON 解析及协议错误判断，普通 batch 和原生 Agent 各自提取文本或工具调用。业务循环检测 `response.content`，失败时把 `response.content.error()` 交给 `ApiPool.cpp::handleApiError()`；后者只按分类记录日志、等待、更新 API 健康或调整 fallback 顺序，不再匹配错误文本。原始响应保留到展示时再拼接，不会覆盖 JSON 解析异常等具体原因。

建议第一次阅读按第 1—8 节顺序跟完一个“搜索原文 → 工具回填 → 提交译文”的正常批次，再回来看第 9 节的压缩和重试分支。
