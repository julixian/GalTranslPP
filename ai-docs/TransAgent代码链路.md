# TransAgent 代码阅读路线

## 配置与初始化

`NormalJsonTranslator.Core.cpp::normalJsonInit()` 读取 `common.agent.enabled`，按 ForGalJson、ForNovelJson、ForGalTsv、ForNovelTsv 选择对应的 Agent 提示词。Gemini 开启 Interactions 配置时，在加载 API 时选择 `ApiProtocol::GeminiInterAction`。

`NormalJsonTranslator.Run.cpp::normalJsonBeforeRun()` 创建 `NormalJsonTranslatorTransAgent`，随后调用 `configureWorkers(m_threadsNum, m_agentAllowPython, m_inputDir, m_outputDir)`。该函数位于 `NormalJsonTranslator.TransAgent.Session.cpp`，分配固定的 worker 会话槽、初选 API，并生成原生工具声明。

每个 `TransAgentWorker` 保存可选的 `AgentApiSession` 和压缩摘要 `rollingContext`。同一工作线程只访问对应的 worker 槽。

## 批次与请求

```text
NormalJsonTranslator::processFile
  → NormalJsonTranslatorTransAgent::translateBatch
  → 选择或重建 worker 会话
  → buildUserPrompt / appendAgentUserMessage
  → performAgentApiRequest
      → buildAgentPayload
      → applyApiPayloadOptions / 插件
      → sendApiHttpRequest
      → parseApiResponse
      → parseAgentReply
```

`buildInputBlock()` 按翻译格式构造 JSONLINE 或 TSV 输入。`buildUserPrompt()` 只替换 USER 的批次占位符，SYSTEM 由会话单独保存。压缩摘要只在重建会话时单独注入，不加入每批 USER。

`ApiTool.Agent.cpp` 负责按 Chat Completions、Responses、Claude、Gemini、Gemini Interactions 构造请求和解析响应。`AgentApiSession::history` 保存协议原生历史，`previousId` 保存最新服务端续接 id，`sentCount` 标记服务端已知历史的结束位置。`pendingAgentInput()` 在续接时只发送新增历史。

流式请求在 HTTP 层收集完整响应后进入同一解析流程。响应通过检查后，才更新会话历史及续接位置。

## 工具与提交

`translateBatch()` 遍历原生工具调用，解析每个调用的参数，交给 `runReadTool()`、Python 工具或提交逻辑。读取和搜索实现在 `NormalJsonTranslator.TransAgent.Tools.cpp`。

提交直接将 `commit_translations` 的 JSON 参数交给 `applyCommit()`，不再封装文本动作协议。`applyCommit()` 校验当前批次译文、写入句子，再更新术语账本、备注和建议。空备注与空术语位置数组在提交前按工具约定处理。

工具异常转换为对应调用的 `error`。`appendAgentToolResults()` 将结果转为协议原生消息。即使一次提交成功，也先回填同轮其他工具结果，再返回外层进行后处理和缓存保存。

## 压缩与重试

没有原生自动压缩时，批次循环检查历史字节数。压缩轮次只声明 `compact_context`，读取其 `rolling_context` 参数。摘要有效时重建会话并注入一次摘要，无效或请求重试耗尽时清空摘要重新开始当前批次。

原生自动压缩由 Responses/Claude 的请求选项和响应解析维护，不调用文本摘要流程。

HTTP 错误经 `handleApiError()` 重试。智能对半重试恢复 `sessionBeforeBatch` 后递归处理两个子批次，不清空此前上下文。工具错误、连续无工具调用和最大轮数分别按现有批次循环处理。

使用说明见 [TransAgent.md](TransAgent.md)。
