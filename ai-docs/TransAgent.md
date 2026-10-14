# 使用 TransAgent

Agent 使用原生工具调用，并按 worker 保留跨批次、跨文件会话。支持的翻译格式依次为 ForGalJson、ForNovelJson、ForGalTsv、ForNovelTsv。JSON 系列的 Agent 使用 JSONLINE 输入，TSV 系列使用 TSV 输入；均通过原生工具提交译文，小说格式不包含姓名。

## 开启和提示词

在 GUI 中开启「Agent 模式」，或设置项目配置：

```toml
[common.agent]
enabled = true
allowPython = false
maxTurnsPerChunk = 50
compactContextThresholdBytes = 400000
searchResultLimit = 80
contextLinesLimit = 20
projectNotePath = "ProjectNote.md"
```

对应提示词键为 `FORGALJSON_AGENT_SYSTEM/USER`、`FORNOVELJSON_AGENT_SYSTEM/USER`、`FORGALTSV_AGENT_SYSTEM/USER`、`FORNOVELTSV_AGENT_SYSTEM/USER`。读取顺序为项目 `Prompt.toml`、`BaseConfig/Prompt.toml`，都缺少所需键时抛出异常。

SYSTEM 保存固定角色、翻译要求、工具规则和提交规则。USER 仅包含当前文件、句子范围、原文、问题、词典、文件备注和术语摘要，不要求每批生成或携带 `rolling_context`。`Prompt_MyCustom.toml` 是默认提示词的中文版本，Sakura 保持原样。

## API 选项

「Api 设置 → Agent」提供以下选项：

| 配置 | 行为 |
| --- | --- |
| `agentStrictTools` | `off`、`all`、`commit` 分别表示不严格、全部工具严格、仅提交工具严格；用于 OpenAI/Claude |
| `agentStateful` | Responses/Interactions 使用服务端 id 续接，否则发送客户端历史 |
| `agentNativeAutoCompaction` | Responses/Claude 启用原生自动压缩 |
| `agentCompactThresholdTokens` | 原生压缩的 token 阈值，0 使用当前协议的默认处理 |
| `agentGeminiInteractions` | 在配置加载时将 Gemini 选择为独立的 `GeminiInterAction` 协议 |

流式输出沿用 API 的流式选项，收集完整响应后统一解析工具调用。完整协议内容（包括推理块和签名）保存在会话历史中。Responses/Interactions 的 `previousId` 和 `sentCount` 决定本轮只发送哪些新增消息。

每个 API 的 `fastPause` 默认关闭。暂停时等待当前请求完成，处理并回填该轮工具结果；进入下一轮前检查暂停，不再发起新请求。开启后立即响应暂停，中断 HTTP 请求和正在执行的 Python 工具。若等待期间请求失败且已暂停，直接结束，不继续重试。

## 会话、压缩与重试

每个 worker 初始化时选择 API。同一 worker 换文件时继续会话；API 身份改变时重建。`random` 保持当前 API 直到被移出池，`fallback` 在批次开始及请求失败时允许重新选择 API。

未启用原生自动压缩时，按 `history.dump().size()` 判断字节阈值。超出阈值后进入专门的压缩轮次，只提供 `compact_context`，通过其 `rolling_context` 参数获取摘要。

- 摘要有效：清空旧历史、服务端 id 和发送位置，向新会话追加一次 `Conversation summary:\n` 加摘要，再追加当前批次。
- 摘要无效或压缩请求重试耗尽：清空摘要和会话，重新追加当前批次。
- 正常提交不更新摘要，也不要求输出摘要。压缩不会删除已保存的译文、术语和备注。

Responses/Claude 启用原生自动压缩时，不走上述字节阈值摘要流程。收到有效压缩块后，客户端仅保留最新压缩块及其后的历史；Responses 保持最新服务端续接 id。日志中的上下文字节数来自本地历史，只是估算值。

API 错误交给 `handleApiError` 处理重试、等待和 API 健康。工具参数或执行错误按调用 id 回填，供模型下一轮修正。仅返回正文时提醒模型调用工具，连续无工具调用达到 `maxRequestCount` 后结束批次。

开启 `smartRetry` 后，两次请求失败会触发对半重试：先恢复当前批次开始前的会话历史和服务端续接位置，再依次翻译两半，保留此前批次的上下文。

## 工具和提交

| 工具 | 用途 |
| --- | --- |
| `list_files` | 查看源文件和行数 |
| `read_source` / `search_source` | 查看、搜索预处理后的原文 |
| `read_cache` / `search_cache` | 查看、搜索已保存的译文 |
| `read_dictionary` / `search_dictionary` | 查询 GPT 字典 |
| `read_terms` / `search_terms` | 查询共享术语账本 |
| `read_file_note` / `read_project_note` | 查看文件或项目备注 |
| `run_python` | 开启 `allowPython` 后使用当前 Python 环境分析数据 |
| `commit_translations` | 提交当前批次译文、术语、备注和人工复核建议 |
| `compact_context` | 仅压缩轮次提供，用于提交会话摘要 |

读取和搜索使用 `file`、`ids`、`fields`、`offset`、`limit`；搜索另有 `query`、`match_fields` 和前后文数量。`offset` 为跳过的记录数，`file=""` 表示当前文件，`file="*"` 表示所有文件。`limit=0` 使用配置上限；字典和术语忽略 file。

`commit_translations` 参数包括 `translations`、`term_updates`、`agent_suggest`、`file_note_patch`。每条译文包含 `id` 和 `dst`，术语使用 `src` 和 `dst`。空备注保留旧值，空更新数组表示无更新。建议写入缓存 `other_info["gpp.agent.suggestion"]`。

代码入口和调用顺序见 [TransAgent代码链路.md](TransAgent代码链路.md)。
