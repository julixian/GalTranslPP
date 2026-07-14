<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="en_US" sourcelanguage="zh_CN">
<context>
    <name>AboutDialog</name>
    <message>
        <location filename="AboutDialog.cpp" line="+21"/>
        <source>关于</source>
        <translation>About</translation>
    </message>
    <message>
        <location line="+52"/>
        <source>版权所有 © 2025-2026</source>
        <translation>All rights reserved © 2025-2026</translation>
    </message>
    <message>
        <location line="+28"/>
        <source>跳转到 Github 发布页</source>
        <translation>Jump to github release page</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>检查更新</source>
        <translation>Check update</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>下载更新</source>
        <translation>Download update</translation>
    </message>
</context>
<context>
    <name>ApiSettingsPage</name>
    <message>
        <location filename="ApiSettingsPage.cpp" line="+38"/>
        <location line="+38"/>
        <source>Api 设置</source>
        <translation>Api settings</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>Api 使用策略</source>
        <translation>Api strategy</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>令牌策略，random随机轮询，fallback优先第一个，出现非额度/频率错误时使用下一个</source>
        <translation>Key strategy: random rotates; fallback starts first, then switches on non-quota/rate errors</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>Api 超时时间</source>
        <translation>Api timeout</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Api 请求超时时间，单位为秒</source>
        <translation>in seconds</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>增加新 Api</source>
        <translation>Add new Api</translation>
    </message>
    <message>
        <location line="+84"/>
        <source>请输入 Api key(Sakura引擎或有Extra keys时可不填)</source>
        <translation>API key (optional for Sakura or Extra keys)</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>请输入 Api url</source>
        <translation>Enter Api url</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>模型名称</source>
        <translation>Model name</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>请输入模型名称(Sakura引擎可不填)</source>
        <translation>Model name (optional for Sakura)</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>接口协议</source>
        <translation>Protocol</translation>
    </message>
    <message>
        <location line="+48"/>
        <source>启用</source>
        <translation>Enable</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>详细配置</source>
        <translation>Advanced</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Api 详细配置</source>
        <translation>Api details</translation>
    </message>
    <message>
        <location line="+65"/>
        <source>一行一个 key，保存时会接在首个 Api key 后面</source>
        <translation>One key per line; saved after the first Api key</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>sk-...
sk-...</source>
        <translation>sk-...
sk-...</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>模型</source>
        <translation>Model</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>获取模型</source>
        <translation>Fetch models</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>测试模型</source>
        <translation>Test model</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>思考等级</source>
        <translation>Thinking level</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>off/low/medium/high，具体效果由接口协议和模型支持情况决定</source>
        <translation>off/low/medium/high; exact behavior depends on protocol and model support</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>流式输出</source>
        <translation>Stream output</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>基础设置</source>
        <translation>Basic settings</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>温度</source>
        <translation>Temperature</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>勾选选框则使用自定义温度，否则使用供应商默认温度</source>
        <translation>Checked: custom temperature; unchecked: provider default</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>top_p</source>
        <translation>top_p</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>核采样(也是控制随机性的)</source>
        <translation>Another parameter to controll randomness</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>frequency_penalty</source>
        <translation>frequency_penalty</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>频率惩罚</source>
        <translation>Frequency penalty</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>presence_penalty</source>
        <translation>presence_penalty</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>存在惩罚</source>
        <translation>Penalizes the reuse of tokens</translation>
    </message>
    <message>
        <location line="+36"/>
        <source>JSON 对象，用于追加自定义 HTTP header</source>
        <translation>JSON object for appending custom HTTP headers</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>JSON 对象，用于追加或覆盖请求体顶层字段；使用模型专用思考参数时请将思考等级设为 off</source>
        <oldsource>JSON 对象，用于追加或覆盖请求 body 字段</oldsource>
        <translation>Body overrides; set thinking off for model options</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>高级设置</source>
        <translation>Advanced settings</translation>
    </message>
    <message>
        <location line="+31"/>
        <source>请求类型: 获取模型列表</source>
        <translation>Request type: fetch model list</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>请求方法: GET</source>
        <translation>Request method: GET</translation>
    </message>
    <message>
        <location line="+2"/>
        <location line="+28"/>
        <source>HTTP 状态: %1</source>
        <translation>HTTP status: %1</translation>
    </message>
    <message>
        <location line="-27"/>
        <location line="+28"/>
        <source>请求结果: %1</source>
        <translation>Request result: %1</translation>
    </message>
    <message>
        <location line="-28"/>
        <location line="+28"/>
        <source>成功</source>
        <translation>Success</translation>
    </message>
    <message>
        <location line="-28"/>
        <location line="+28"/>
        <source>失败</source>
        <translation>Failed</translation>
    </message>
    <message>
        <location line="-26"/>
        <source>解析到的模型: </source>
        <translation>Parsed models: </translation>
    </message>
    <message>
        <location line="+2"/>
        <source>(没有解析到模型)</source>
        <translation>(no models parsed)</translation>
    </message>
    <message>
        <location line="+9"/>
        <location line="+17"/>
        <source>错误信息: </source>
        <translation>Error message: </translation>
    </message>
    <message>
        <location line="-8"/>
        <source>请求类型: 测试模型回复</source>
        <translation>Request type: test model reply</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>请求方法: POST</source>
        <translation>Request method: POST</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>发出的请求体: </source>
        <translation>Request body sent: </translation>
    </message>
    <message>
        <location line="+6"/>
        <source>解析出的模型回复: </source>
        <translation>Parsed model reply: </translation>
    </message>
    <message>
        <location line="+1"/>
        <source>(空)</source>
        <translation>(empty)</translation>
    </message>
    <message>
        <location line="+47"/>
        <location line="+5"/>
        <location line="+8"/>
        <source>请求失败</source>
        <translation>Request failed</translation>
    </message>
    <message>
        <location line="-13"/>
        <source>Api url 不能为空</source>
        <translation>Api url cannot be empty</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>模型名称不能为空</source>
        <translation>Model name cannot be empty</translation>
    </message>
    <message>
        <location line="+69"/>
        <location line="+35"/>
        <location line="+5"/>
        <source>模型获取</source>
        <translation>Model fetch</translation>
    </message>
    <message>
        <location line="-40"/>
        <location line="+63"/>
        <location line="+4"/>
        <location line="+3"/>
        <source>模型测试</source>
        <translation>Model test</translation>
    </message>
    <message>
        <location line="-69"/>
        <source>正在获取模型列表...</source>
        <translation>Fetching model list...</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>正在测试模型请求...</source>
        <translation>Testing model request...</translation>
    </message>
    <message>
        <location line="+35"/>
        <source>获取到 %1 个模型</source>
        <translation>Fetched %1 models</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+5"/>
        <source>模型列表</source>
        <translation>Model list</translation>
    </message>
    <message>
        <location line="-1"/>
        <source>请求成功，但没有解析到模型</source>
        <translation>Request succeeded, but no models were parsed</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>模型列表请求失败</source>
        <translation>Model list request failed</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>模型请求成功</source>
        <translation>Model request succeeded</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>模型请求失败</source>
        <translation>Model request failed</translation>
    </message>
</context>
<context>
    <name>AppSettingsPage</name>
    <message>
        <location filename="AppSettingsPage.cpp" line="+26"/>
        <location line="+10"/>
        <source>应用设置</source>
        <translation>App Settings</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>主题设置</source>
        <translation>Theme Settings</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>日间模式</source>
        <translation>Light Mode</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>夜间模式</source>
        <translation>Dark Mode</translation>
    </message>
    <message>
        <location line="-7"/>
        <source>主题切换</source>
        <translation>Theme</translation>
    </message>
    <message>
        <location line="+35"/>
        <source>窗口效果</source>
        <translation>Window Effects</translation>
    </message>
    <message>
        <location line="+47"/>
        <source>导航栏模式选择</source>
        <translation>Navigation Bar Mode</translation>
    </message>
    <message>
        <location line="+34"/>
        <source>页面切换特效</source>
        <translation>Page Transition Effect</translation>
    </message>
    <message>
        <location line="+35"/>
        <source>GalTransl++ 设置</source>
        <oldsource>GalTransl 设置</oldsource>
        <translation>GalTransl settings</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>(DumpName/NameTrans)/GenDict任务成功后自动刷新人名表/项目GPT字典</source>
        <translation>Auto refresh NameTable/GptDict after (DumpName/NameTrans)/GenDict</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>新项目人名表默认打开模式</source>
        <translation>Default name table view mode for new projects</translation>
    </message>
    <message>
        <location line="+5"/>
        <location line="+29"/>
        <source>纯文本模式</source>
        <translation>Plain Text Mode</translation>
    </message>
    <message>
        <location line="-28"/>
        <location line="+29"/>
        <source>表格模式</source>
        <translation>Table Mode</translation>
    </message>
    <message>
        <location line="-6"/>
        <source>新项目字典默认打开模式</source>
        <translation>Default dictionary view mode for new projects</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>允许在项目仍在运行的情况下关闭程序</source>
        <translation>Allow exiting the app while a project is still running</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>危险！</source>
        <translation>Dangerous!</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>允许应用进程多开</source>
        <translation>Allow multiple program instance</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>通常建议由一个进程管理多个项目而不是多开进程</source>
        <translation>not suggested</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>自动检查更新</source>
        <translation>Automatically check for updates</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>检测到更新后自动下载</source>
        <translation>Automatically download detected updates</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>语言设置</source>
        <translation>Language</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+18"/>
        <source>重启生效</source>
        <translation>Takes effect after restart</translation>
    </message>
    <message>
        <location line="-1"/>
        <source>Python环境路径</source>
        <translation>Python Environment Path</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>浏览</source>
        <translation>Browse</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>选择Python.exe</source>
        <translation>Select Python.exe</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>错误</source>
        <translation>Error</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>目录下没有 python{ver}.zip 文件</source>
        <translation>python{ver}.zip was not found in this directory</translation>
    </message>
</context>
<context>
    <name>CommonGptDictsPage</name>
    <message>
        <location filename="CommonGptDictsPage.cpp" line="+34"/>
        <source>默认GPT字典设置</source>
        <translation>Default gptDict settings</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>通用GPT字典</source>
        <translation>Common gptDicts</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>导入字典页</source>
        <translation>Import dict page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>添加新字典页</source>
        <translation>Add new dict page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>保存所有页</source>
        <translation>Save all pages</translation>
    </message>
    <message>
        <location line="+27"/>
        <source>纯文本</source>
        <translation>Plain text</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>表模式</source>
        <translation>Table mode</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>默认启用</source>
        <translation>Default on</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>保存当前页</source>
        <translation>Save present page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>删除当前页</source>
        <translation>Remove current page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>重命名当前页</source>
        <translation>Rename current page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>撤回删除行</source>
        <translation>Cancel deleted row</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>刷新当前页</source>
        <translation>Refresh present page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>添加词条</source>
        <translation>Add new dict</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>删除词条</source>
        <translation>Remove present dict</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>编辑词条</source>
        <translation>Edit entry</translation>
    </message>
    <message>
        <location line="+41"/>
        <source>备注</source>
        <translation>Note</translation>
    </message>
    <message>
        <location line="+74"/>
        <source>保存失败</source>
        <translation>Fail to save</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>无法打开文件: %1</source>
        <translation>Cannot open file: %1</translation>
    </message>
    <message>
        <location line="+53"/>
        <location line="+287"/>
        <source>保存成功</source>
        <translation>Saved successfully</translation>
    </message>
    <message>
        <location line="-286"/>
        <source>字典 %1 已保存</source>
        <translation>Dict %1 saved</translation>
    </message>
    <message>
        <location line="+109"/>
        <source>刷新成功</source>
        <translation>Refreshed successfully</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>字典 %1 已刷新</source>
        <translation>Dict %1 refreshed</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>请输入新名称</source>
        <translation>Please type in new name</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>重命名字典</source>
        <translation>Rename dictionaryionary</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+44"/>
        <source>重命名失败</source>
        <translation>Fail to rename</translation>
    </message>
    <message>
        <location line="-44"/>
        <location line="+205"/>
        <source>字典名称不能为空，且不能包含点号、斜杠或反斜杠！</source>
        <translation>Dict name can not be empty or contain point symbol, slash, backslash!</translation>
    </message>
    <message>
        <location line="-196"/>
        <location line="+196"/>
        <location line="+9"/>
        <location line="+10"/>
        <source>新建失败</source>
        <translation>Fail to create</translation>
    </message>
    <message>
        <location line="-214"/>
        <location line="+175"/>
        <location line="+30"/>
        <source>字典 %1 已存在</source>
        <translation>Dict %1 exists</translation>
    </message>
    <message>
        <location line="-175"/>
        <source>重命名成功</source>
        <translation>Renamed successfully</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>字典 %1 已重命名为 %2</source>
        <translation>Dict %1 renamed to %2</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>字典 %1 重命名失败</source>
        <translation>Failed to rename dict %1</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>是</source>
        <translation>Yes</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>思考人生</source>
        <translation>Reflect on life</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>否</source>
        <translation>No</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>你确定要删除 %1 吗？</source>
        <translation>Delete %1?</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>将永久删除该字典文件，如有需要请先备份！</source>
        <translation>This deletes the dictionary file. Back up first.</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>删除成功</source>
        <translation>Deleted successfully</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>字典 %1 已从字典管理和磁盘中移除！</source>
        <translation>Dict %1 removed from list and disk!</translation>
    </message>
    <message>
        <location line="+50"/>
        <source>所有默认字典配置均已保存</source>
        <translation>All dicts are saved successfully</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>选择字典文件</source>
        <translation>Choose dict file</translation>
    </message>
    <message>
        <location line="+14"/>
        <location line="+9"/>
        <source>导入失败</source>
        <translation>Fail to import</translation>
    </message>
    <message>
        <location line="-9"/>
        <source>原文件删除失败</source>
        <translation>Fail to remove original file</translation>
    </message>
    <message>
        <location line="+16"/>
        <location line="+41"/>
        <source>创建成功</source>
        <translation>Created successfully</translation>
    </message>
    <message>
        <location line="-40"/>
        <location line="+41"/>
        <source>字典页 %1 已创建</source>
        <translation>Dict page %1 created</translation>
    </message>
    <message>
        <location line="-35"/>
        <source>请输入字典表名称</source>
        <translation>Please type in dict name</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>新建字典</source>
        <translation>New dictionary</translation>
    </message>
    <message>
        <location line="+27"/>
        <source>无法创建 %1 文件</source>
        <translation>Cannot create %1 file</translation>
    </message>
</context>
<context>
    <name>CommonNormalDictsPage</name>
    <message>
        <location filename="CommonNormalDictsPage.cpp" line="+34"/>
        <source>默认译前字典设置</source>
        <translation>Common preDicts settings</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>内部错误</source>
        <translation>Internal error</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>未知通用字典模式</source>
        <translation>Unkown common dict mode</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>通用译前字典</source>
        <translation>Common preDicts</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>通用译后字典</source>
        <translation>Common postDicts</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>导入字典页</source>
        <translation>Import dict page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>添加新字典页</source>
        <translation>Add new dict page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>保存所有页</source>
        <translation>Save all pages</translation>
    </message>
    <message>
        <location line="+27"/>
        <source>纯文本</source>
        <translation>Plain text</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>表模式</source>
        <translation>Table mode</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>默认启用</source>
        <translation>Default on</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>保存当前页</source>
        <translation>Save present page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>删除当前页</source>
        <translation>Remove current page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>重命名当前页</source>
        <translation>Rename current page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>撤回删除行</source>
        <translation>Cancel deleted row</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>刷新当前页</source>
        <translation>Refresh present page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>添加词条</source>
        <translation>Add new dict</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>删除词条</source>
        <translation>Remove present dict</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>编辑词条</source>
        <translation>Edit entry</translation>
    </message>
    <message>
        <location line="+42"/>
        <source>条件</source>
        <translation>Rules</translation>
    </message>
    <message>
        <location line="+85"/>
        <source>保存失败</source>
        <translation>Fail to save</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>无法打开字典: %1</source>
        <translation>Cannot open dict: %1</translation>
    </message>
    <message>
        <location line="+55"/>
        <location line="+287"/>
        <source>保存成功</source>
        <translation>Saved successfully</translation>
    </message>
    <message>
        <location line="-286"/>
        <source>字典 %1 已保存</source>
        <translation>Dict %1 saved</translation>
    </message>
    <message>
        <location line="+107"/>
        <source>刷新成功</source>
        <translation>Refreshed successfully</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>字典 %1 已刷新</source>
        <translation>Dict %1 refreshed</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>请输入新名称</source>
        <translation>Please type in new name</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>重命名字典</source>
        <translation>Rename dictionaryionary</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+46"/>
        <source>重命名失败</source>
        <translation>Fail to rename</translation>
    </message>
    <message>
        <location line="-46"/>
        <location line="+207"/>
        <source>字典名称不能为空，且不能包含点号、斜杠或反斜杠！</source>
        <translation>Dict name can not be empty or contain point symbol, slash, backslash!</translation>
    </message>
    <message>
        <location line="-198"/>
        <location line="+198"/>
        <location line="+10"/>
        <location line="+9"/>
        <source>新建失败</source>
        <translation>Fail to create</translation>
    </message>
    <message>
        <location line="-216"/>
        <location line="+177"/>
        <location line="+31"/>
        <source>字典 %1 已存在</source>
        <translation>Dict %1 exists</translation>
    </message>
    <message>
        <location line="-178"/>
        <source>重命名成功</source>
        <translation>Renamed successfully</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>字典 %1 已重命名为 %2</source>
        <translation>Dict %1 renamed to %2</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>字典 %1 重命名失败</source>
        <translation>Failed to rename dict %1</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>是</source>
        <translation>Yes</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>思考人生</source>
        <translation>Reflect on life</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>否</source>
        <translation>No</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>你确定要删除 %1 吗？</source>
        <translation>Delete %1?</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>将永久删除该字典文件，如有需要请先备份！</source>
        <translation>This deletes the dictionary file. Back up first.</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>删除成功</source>
        <translation>Deleted successfully</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>字典 %1 已从字典管理和磁盘中移除！</source>
        <translation>Dict %1 removed from list and disk!</translation>
    </message>
    <message>
        <location line="+50"/>
        <source>所有默认字典配置均已保存</source>
        <translation>All dicts are saved successfully</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>选择字典文件</source>
        <translation>Choose dict file</translation>
    </message>
    <message>
        <location line="+14"/>
        <location line="+9"/>
        <source>导入失败</source>
        <translation>Fail to import</translation>
    </message>
    <message>
        <location line="-9"/>
        <source>原文件删除失败</source>
        <translation>Fail to remove original file</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>创建成功</source>
        <translation>Created successfully</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+41"/>
        <source>字典页 %1 已创建</source>
        <translation>Dict page %1 created</translation>
    </message>
    <message>
        <location line="-35"/>
        <source>请输入字典表名称</source>
        <translation>Please type in dict name</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>新建字典</source>
        <translation>New dictionary</translation>
    </message>
    <message>
        <location line="+27"/>
        <source>无法创建 %1 文件</source>
        <translation>Cannot create %1 file</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>新建成功</source>
        <translation>Created successfully</translation>
    </message>
</context>
<context>
    <name>CommonSettingsPage</name>
    <message>
        <location filename="CommonSettingsPage.cpp" line="+27"/>
        <source>一般设置</source>
        <translation>Common settings</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>单次请求翻译句子数量</source>
        <translation>Number per request to translate</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>最大线程数</source>
        <translation>Max threads num</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>翻译顺序</source>
        <translation>Translate order</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>name为文件名，size为大文件优先，多线程时大文件优先可以提高整体速度</source>
        <translation>name: filename; size: large files first (faster with threads)</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>文件名</source>
        <translation>name</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>文件大小</source>
        <translation>size</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>翻译到的目标语言</source>
        <translation>Target language</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>包括但不限于[zh-cn/zh-tw/en/ja/ko/ru/fr]</source>
        <translation>Include but not limited to[zh-cn/zh-tw/en/ja/ko/ru/fr]</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>单文件分割</source>
        <translation>Split single file</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>分割数量</source>
        <translation>Split num</translation>
    </message>
    <message>
        <location line="+40"/>
        <source>会把 onFileProcessed 延迟到翻译结束再执行。启用时建议将翻译顺序改为文件名排序</source>
        <translation>Delays onFileProcessed until translation ends. When enabled, filename order is recommended</translation>
    </message>
    <message>
        <location line="+33"/>
        <source>Agent 模式</source>
        <translation>Agent mode</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>当前仅 ForGalTsv、ForNovelTsv、GenDict 会实际启用</source>
        <translation>Currently only ForGalTsv, ForNovelTsv, and GenDict actually enable it</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>让模型可以调用一定的工具以获取更多上下文</source>
        <translation>Allows the model to call tools for more context</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>单块最大轮数</source>
        <translation>Max turns per block</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Agent 处理一个文本块时允许的最大问答轮数</source>
        <translation>Maximum Q&amp;A turns allowed when Agent processes one text block</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>压缩上下文阈值</source>
        <translation>Context compression threshold</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Agent 消息上下文超过该字节数后触发压缩</source>
        <translation>Compress Agent context above this byte count</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>单位为字节</source>
        <translation>Unit: bytes</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>工具搜索结果上限</source>
        <translation>Tool search result limit</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Agent 工具一次返回的搜索结果数量上限</source>
        <translation>Maximum search results returned by one Agent tool call</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>工具上下文行数上限</source>
        <translation>Tool context line limit</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>search_text 附近行数上限，0 表示不返回附近行</source>
        <translation>Line limit around search_text results; 0 returns no nearby lines</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>ProjectNote 路径</source>
        <translation>ProjectNote path</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Agent 可选读取的项目说明文件，需自己加 `get_project_note()` 的工具提示词</source>
        <translation>Optional project note file for Agent to read; add the `get_project_note()` tool prompt yourself</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>缓存保存间隔</source>
        <translation>Save interval</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>每翻译n次保存一次缓存</source>
        <translation>Save cache after every n translation rounds</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>携带上文数量</source>
        <translation>Num of context to attached</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>智能重试</source>
        <translation>Smart retry</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>额度检测</source>
        <translation>Check quota</translation>
    </message>
    <message>
        <location line="+273"/>
        <source>将换行符统一规范为 &amp;lt;br&amp;gt; 以方便检错和修复，也可以让如全角半角转化等插件方便忽略换行。具体替换时机详见使用说明，auto 为自动检测</source>
        <translation>Use &lt;br&gt; for checks/plugins. See docs for timing; auto detects.</translation>
    </message>
    <message>
        <location line="-520"/>
        <source>Num: 每n条分割一次，Equal: 每个文件均分n份，No: 关闭单文件分割</source>
        <translation>Num: split every n sentences; Equal: split every file to n parts equally</translation>
    </message>
    <message>
        <location line="-78"/>
        <source>根据模型从十几到一百多不等</source>
        <translation>Model-dependent: about 12-100+</translation>
    </message>
    <message>
        <location line="+101"/>
        <source>Num时，表示n句拆分一次；Equal时，表示每个文件均分拆成n部分</source>
        <translation>n</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>分割缓存查找距离</source>
        <oldsource>分割缓存贪婪查找</oldsource>
        <translation>Splitted cache search distance</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>将自身索引 ±N 的分割文件均视为当前分割文件的缓存</source>
        <translation>Treat split files within index +/-N as this file&apos;s cache</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>数值越大可能占用更多内存</source>
        <translation>Larger number may cause more memory usage</translation>
    </message>
    <message>
        <location line="+168"/>
        <source>最大请求次数</source>
        <translation>Max request count</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>对现代模型而言意义不大了，推荐值 ≤ 10</source>
        <translation>Less useful for modern models; recommended value ≤ 10</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>解析结果失败时尝试折半重翻与清空上下文</source>
        <translation>On parse failure, retry halves and clear context</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>运行时动态检测 key 额度，自动从 Api 池中删除额度不足的 key</source>
        <translation>Monitor key quota and remove depleted keys from the API pool</translation>
    </message>
    <message>
        <location line="+47"/>
        <source>日志级别</source>
        <translation>Log level</translation>
    </message>
    <message>
        <source>保存项目日志</source>
        <translation type="vanished">Save project logs</translation>
    </message>
    <message>
        <location line="-359"/>
        <source>单次请求翻译人名数量</source>
        <translation>Names per NameTrans request</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>NameTrans 每个线程单次处理的人名数量</source>
        <translation>Names per request per NameTrans thread</translation>
    </message>
    <message>
        <location line="+123"/>
        <source>连续重复块引用复用</source>
        <translation>Repeated block reference reuse</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>重复脚本块只翻译首次出现的片段，后续句子引用复制结果</source>
        <translation>Translate first repeated block only; later lines reuse it</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>重复块最小句数</source>
        <translation>Minimum repeated block size</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>连续 n 句的说话人和原文完全相同才建立引用</source>
        <translation>Reference only after n identical speakers and source lines</translation>
    </message>
    <message>
        <location line="+186"/>
        <source>解析不完整时重翻整段</source>
        <translation>Retry whole batch on incomplete parse</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>不开启则仅重翻漏掉的部分，开启可增加模型因串行而导致解析失败时的容错</source>
        <translation>Off: retry omissions only. On: tolerate serialization parse failures.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>默认关闭以节省token/防止因模型截断造成无限循环</source>
        <translation>Off by default to save tokens and avoid truncation loops</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>项目日志设置</source>
        <translation>Project logging settings</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>保存项目日志文件</source>
        <translation>Save project logs</translation>
    </message>
    <message>
        <location line="+42"/>
        <source>单个 log 文件大小限制</source>
        <oldsource>log 文件大小限制</oldsource>
        <translation>Single log file size limit</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>log 文件滚动数量上限</source>
        <translation>Maximum rolling quantity of log files</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>单次日志翻译文本行数上限</source>
        <translation>Max translated text lines per log entry</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>限制日志里 inputBlock 和解析结果显示的行数</source>
        <translation>Limits displayed lines of inputBlock and parse result in logs</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>实际请求和解析仍使用完整内容</source>
        <translation>Actual request and parsing still use full content</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>单次日志问题行数上限</source>
        <translation>Max problem lines per log entry</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>只限制日志里 Problems 显示的行数</source>
        <translation>Only limits displayed Problems lines in logs</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+14"/>
        <source>实际请求仍发送完整内容</source>
        <translation>Actual request still sends full content</translation>
    </message>
    <message>
        <location line="-2"/>
        <source>单次日志字典行数上限</source>
        <translation>Max dictionary lines per log entry</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>只限制日志里 Dict/Glossary 显示的行数</source>
        <translation>Only limits displayed Dict/Glossary lines in logs</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>分词器设置</source>
        <translation>Tokenizer settings</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>分词器后端</source>
        <translation>Tokenizer backend</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>除了MeCab，剩下的都依赖Python，速度会比较慢</source>
        <oldsource>除了MeCab，剩下的都依赖Python，所以速度变慢或内存占用变大是正常的</oldsource>
        <translation>spaCy and Stanza depend on Python</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>MeCab词典目录</source>
        <translation>MeCab dict dir</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>MeCab词典目录，程序自带一个日文词典</source>
        <oldsource>MeCab词典目录，程序自带一个</oldsource>
        <translation>Dir of MeCab&apos;s dictionary</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>选择MeCab词典目录</source>
        <translation>Choose MeCab dict dir</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>spaCy模型名称</source>
        <translation>spaCy model name</translation>
    </message>
    <message>
        <source>spaCy模型名称，新模型下载后需重启程序</source>
        <translation type="vanished">spaCy model name, you need reboot app after downloading a new model</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>sm模型的效果有点一言难尽，有条件的建议上trf模型</source>
        <translation>sm model is not so satisfying, use trf model if conditions allowing</translation>
    </message>
    <message>
        <location line="+13"/>
        <location line="+24"/>
        <source>打开模型列表网页</source>
        <translation>Browse model contents</translation>
    </message>
    <message>
        <location line="-13"/>
        <source>Stanza语言ID</source>
        <translation>Stanza lang id</translation>
    </message>
    <message>
        <source>Stanza语言ID，新模型下载后需重启程序</source>
        <translation type="vanished">Stanza lang id, you need reboot app after downloading a new model</translation>
    </message>
    <message>
        <location line="+110"/>
        <source>linebreakSymbol 不符合 toml 规范</source>
        <translation>linebreakSymbol is nonconforming</translation>
    </message>
    <message>
        <location line="-149"/>
        <location line="+27"/>
        <location line="+24"/>
        <source>浏览</source>
        <translation>browse</translation>
    </message>
    <message>
        <location line="-91"/>
        <source>用于生成字典和查错的分词器后端及其设置 (应选择适合原文的后端/模型/字典)</source>
        <translation>Tokenizer and settings for dictionary/error checks (match source language)</translation>
    </message>
    <message>
        <location line="+80"/>
        <source>感觉不如 spaCy</source>
        <translation>Less accurate than spaCy</translation>
    </message>
    <message>
        <location line="+35"/>
        <source>本项目所使用的换行符</source>
        <translation>Linebreak symbol used in this project</translation>
    </message>
    <message>
        <source>将换行符统一规范为 &amp;lt;br&amp;gt; 以方便检错和修复，也可以让如全角半角转化等插件方便忽略换行。&lt;br&gt;具体替换时机详见使用说明，auto为自动检测</source>
        <oldsource>将换行符统一规范为 &amp;lt;br&amp;gt; 以方便检错和修复，也可以让如全角半角转化等插件方便忽略换行，具体替换时机详见使用说明，auto为自动检测</oldsource>
        <translation type="vanished">All linebreak symbols will be replaced to &amp;lt;br&amp;gt; for programming use</translation>
    </message>
    <message>
        <location line="+74"/>
        <source>解析失败</source>
        <translation>Fail to analyze</translation>
    </message>
</context>
<context>
    <name>CustomFilePluginCfgPage</name>
    <message>
        <location filename="CustomFilePluginCfgPage.cpp" line="+19"/>
        <location line="+63"/>
        <source>自定义文件处理插件配置</source>
        <translation>Custom file plugin settings</translation>
    </message>
    <message>
        <location line="-52"/>
        <source>自定义文件处理插件路径</source>
        <translation>Custom file plugin path</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>浏览</source>
        <translation>Browser</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>选择自定义文件处理插件</source>
        <translation>Choose custom file plugin</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>基类继承</source>
        <translation>Inherit base class</translation>
    </message>
</context>
<context>
    <name>DefaultPromptPage</name>
    <message>
        <source>默认提示词管理</source>
        <translation type="vanished">Default prompts</translation>
    </message>
    <message>
        <source>解析失败</source>
        <translation type="vanished">Fail to analyze</translation>
    </message>
    <message>
        <source>默认提示词配置文件不符合 toml 规范</source>
        <translation type="vanished">Default prompts config file is nonconforming</translation>
    </message>
    <message>
        <source>agent用户</source>
        <translation type="vanished">User-agent</translation>
    </message>
    <message>
        <source>agent系统</source>
        <translation type="vanished">Sys-agent</translation>
    </message>
    <message>
        <source>保存成功</source>
        <translation type="vanished">Saved successfully</translation>
    </message>
    <message>
        <source>所有默认提示词配置已保存。</source>
        <translation type="vanished">All default prompts settings are saved.</translation>
    </message>
    <message>
        <source>默认 %1 提示词配置已保存。</source>
        <translation type="vanished">Default %1 prompt saved.</translation>
    </message>
    <message>
        <source>用户提示词</source>
        <translation type="vanished">User prompt</translation>
    </message>
    <message>
        <source>系统提示词</source>
        <translation type="vanished">System prompt</translation>
    </message>
    <message>
        <source>全部保存</source>
        <translation type="vanished">Save all</translation>
    </message>
    <message>
        <source>保存</source>
        <translation type="vanished">Save</translation>
    </message>
</context>
<context>
    <name>DefaultPromptsPage</name>
    <message>
        <location filename="DefaultPromptsPage.cpp" line="+21"/>
        <source>默认提示词管理</source>
        <translation>Default prompts</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>解析失败</source>
        <translation>Fail to analyze</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>默认提示词配置文件不符合 toml 规范</source>
        <translation>Default prompts config file is nonconforming</translation>
    </message>
    <message>
        <location line="+31"/>
        <source>用户提示词</source>
        <translation>User prompt</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>系统提示词</source>
        <translation>System prompt</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>agent用户</source>
        <translation>User-agent</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>agent系统</source>
        <translation>Sys-agent</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>全部保存</source>
        <translation>Save all</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>保存</source>
        <translation>Save</translation>
    </message>
    <message>
        <location line="+51"/>
        <location line="+25"/>
        <source>保存成功</source>
        <translation>Saved successfully</translation>
    </message>
    <message>
        <location line="-25"/>
        <source>所有默认提示词配置已保存。</source>
        <translation>All default prompts settings are saved.</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>默认 %1 提示词配置已保存。</source>
        <translation>Default %1 prompt saved.</translation>
    </message>
</context>
<context>
    <name>DictExSettingsPage</name>
    <message>
        <location filename="DictExSettingsPage.cpp" line="+16"/>
        <source>项目字典设置</source>
        <translation>Project dict settings</translation>
    </message>
    <message>
        <location line="+71"/>
        <source>选择要启用的译前字典</source>
        <translation>PreDicts to enable</translation>
    </message>
    <message>
        <location line="-6"/>
        <source>项目译前字典</source>
        <translation>Project PreDict</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>选择要启用的GPT字典</source>
        <translation>GptDicts to enable</translation>
    </message>
    <message>
        <location line="-6"/>
        <source>项目GPT字典</source>
        <translation>Project GptDict</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>选择要启用的译后字典</source>
        <translation>PostDicts to enable</translation>
    </message>
    <message>
        <location line="-6"/>
        <source>项目译后字典</source>
        <translation>Project PostDict</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>将译前字典用在name字段</source>
        <translation>Use predicts in name section</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>将译后字典用在name字段</source>
        <translation>Use postdicts in name section</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>将译前字典用在msg字段</source>
        <translation>Use predicts in msg section</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>将译后字典用在msg字段</source>
        <translation>Use postdicts in msg section</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>启用GPT字典替换name字段</source>
        <translation>Use gptdicts to replace name section</translation>
    </message>
</context>
<context>
    <name>DictSettingsPage</name>
    <message>
        <location filename="DictSettingsPage.cpp" line="+30"/>
        <source>项目字典设置</source>
        <translation>Project dicts settings</translation>
    </message>
    <message>
        <location line="+39"/>
        <source>纯文本</source>
        <translation>Plain text</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>表模式</source>
        <translation>Table mode</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>保存当前页</source>
        <translation>Save present page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>导入字典页</source>
        <translation>Import dict page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>撤回删除行</source>
        <translation>Cancel deleted row</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>刷新当前页</source>
        <translation>Refresh present page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>添加词条</source>
        <translation>Add new dict</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>删除词条</source>
        <translation>Remove present dict</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>编辑词条</source>
        <translation>Edit entry</translation>
    </message>
    <message>
        <location line="+56"/>
        <source>备注</source>
        <translation>Note</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>条件</source>
        <translation>Rules</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>刷新成功</source>
        <translation>Refreshed successfully</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>重新载入了 %1</source>
        <translation>Reloaded %1</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>生成字典删除失败</source>
        <translation>Failt to delete GenDict</translation>
    </message>
    <message>
        <location line="+73"/>
        <source>选择字典文件</source>
        <translation>Choose dict file</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>导入失败</source>
        <translation>Fail to import</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>字典文件中没有词条</source>
        <translation>No dicts in the file</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>导入成功</source>
        <translation>Import successfully</translation>
    </message>
    <message>
        <location line="+32"/>
        <source>%1 已保存</source>
        <translation>%1 saved</translation>
    </message>
    <message>
        <location line="+119"/>
        <source>项目GPT字典</source>
        <translation>Project GptDict</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>项目译前字典</source>
        <translation>Project PreDict</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>项目译后字典</source>
        <translation>Project PostDict</translation>
    </message>
    <message>
        <location line="-178"/>
        <source>从文件 %1 中导入了 %2 个词条</source>
        <translation>Imported %2 entries from %1</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>保存成功</source>
        <translation>Saved successfully</translation>
    </message>
</context>
<context>
    <name>DictionaryEntryDeleteDialog</name>
    <message>
        <location filename="DictionaryEntryDialog.cpp" line="+644"/>
        <source>否</source>
        <translation>No</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>思考人生</source>
        <translation>Reflect on life</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>是</source>
        <translation>Yes</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>你确定要删除选中的词条吗？</source>
        <translation>Delete selected entries?</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>已选中 %1 条词条，删除后可以使用撤回按钮恢复。</source>
        <translation>%1 entries selected. You can restore them with Undo after deletion.</translation>
    </message>
</context>
<context>
    <name>DictionaryEntryDialog</name>
    <message>
        <location line="-570"/>
        <source>拖动调整顺序</source>
        <translation>Drag to reorder</translation>
    </message>
    <message>
        <location line="+40"/>
        <source>条件正则</source>
        <translation>Condition regex</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>相对句</source>
        <translation>Relative line</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>条件对象</source>
        <translation>Condition target</translation>
    </message>
    <message>
        <location line="+224"/>
        <source>编辑 GPT 字典词条</source>
        <translation>Edit GPT entry</translation>
    </message>
    <message>
        <location line="+24"/>
        <location line="+64"/>
        <source>原文（org）</source>
        <translation>Original (org)</translation>
    </message>
    <message>
        <location line="-63"/>
        <location line="+74"/>
        <source>译文（rep）</source>
        <translation>Replace (rep)</translation>
    </message>
    <message>
        <location line="-73"/>
        <source>注释（note）</source>
        <translation>Note (note)</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+186"/>
        <source>取消</source>
        <translation>Cancel</translation>
    </message>
    <message>
        <location line="-181"/>
        <location line="+186"/>
        <source>保存</source>
        <translation>Save</translation>
    </message>
    <message>
        <location line="-179"/>
        <location line="+187"/>
        <location line="+12"/>
        <location line="+9"/>
        <source>保存失败</source>
        <translation>Fail to save</translation>
    </message>
    <message>
        <location line="-207"/>
        <location line="+187"/>
        <source>原文（org）不能为空</source>
        <translation>Original (org) required</translation>
    </message>
    <message>
        <location line="-166"/>
        <source>编辑 Normal 字典词条</source>
        <translation>Edit Normal entry</translation>
    </message>
    <message>
        <location line="+41"/>
        <source>匹配设置</source>
        <translation>Match</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>启用正则（isReg）</source>
        <translation>Regex (isReg)</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>优先级（priority）</source>
        <translation>Priority</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>条件（conditions）</source>
        <translation>Rules (conditions)</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>添加条件</source>
        <translation>Add rule</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>删除条件</source>
        <translation>Delete rule</translation>
    </message>
    <message>
        <location line="+104"/>
        <source>第 %1 条条件的条件正则不能为空</source>
        <translation>Rule %1 regex required</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>第 %1 条条件的条件对象不能为空</source>
        <translation>Rule %1 target required</translation>
    </message>
</context>
<context>
    <name>DictionaryReader</name>
    <message>
        <location filename="DictionaryReader.cpp" line="+45"/>
        <location line="+9"/>
        <location line="+17"/>
        <location line="+29"/>
        <location line="+107"/>
        <location line="+9"/>
        <location line="+21"/>
        <location line="+6"/>
        <source>解析失败</source>
        <translation>Fail to analyze</translation>
    </message>
    <message>
        <location line="-197"/>
        <location line="+162"/>
        <source>%1 不符合 toml 规范</source>
        <translation>%1 is invalid TOML</translation>
    </message>
    <message>
        <location line="-153"/>
        <location line="+162"/>
        <source>%1 不是预期的 json 格式</source>
        <translation>%1 is not expected JSON</translation>
    </message>
    <message>
        <location line="-145"/>
        <location line="+166"/>
        <source>%1 不符合 json 规范</source>
        <translation>%1 is invalid JSON</translation>
    </message>
    <message>
        <location line="-137"/>
        <location line="+143"/>
        <source>%1 不是支持的格式</source>
        <translation>%1 is unsupported</translation>
    </message>
</context>
<context>
    <name>DictionarySearchBar</name>
    <message>
        <location filename="DictionarySearchBar.cpp" line="+25"/>
        <source>搜索字典...</source>
        <translation>Search dictionaries...</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+9"/>
        <source>全部</source>
        <translation>All</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>原文</source>
        <translation>original</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>译文</source>
        <translation>translation</translation>
    </message>
</context>
<context>
    <name>ElaInputDialog</name>
    <message>
        <location filename="ElaInputDialog.cpp" line="+16"/>
        <source>取消</source>
        <translation>Cancel</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>重置</source>
        <translation>Reset</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>确定</source>
        <translation>OK</translation>
    </message>
</context>
<context>
    <name>EpubCfgPage</name>
    <message>
        <location filename="EpubCfgPage.cpp" line="+23"/>
        <location line="+158"/>
        <source>Epub 输出配置</source>
        <translation>Epub output settings</translation>
    </message>
    <message>
        <location line="-147"/>
        <source>双语显示</source>
        <translation>Bilingual display</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>在每句译文下以设置的颜色和比例显示原文</source>
        <translation>Show source below translations using set color and scale</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>原文颜色</source>
        <translation>Color of original text</translation>
    </message>
    <message>
        <location line="+40"/>
        <source>缩小比例</source>
        <translation>Decreasing scale</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>预处理正则</source>
        <translation>PreProcess regex</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>提取正文后、送入翻译前应用的正则规则</source>
        <translation>Regex before translation</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>后处理正则</source>
        <translation>PostProcess Regex</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>翻译完成后、写回 Epub 前应用的正则规则</source>
        <translation>Regex after translation</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>说明</source>
        <translation>Description</translation>
    </message>
    <message>
        <location line="+25"/>
        <location line="+13"/>
        <source>解析失败</source>
        <translation>Fail to analyze</translation>
    </message>
    <message>
        <location line="-13"/>
        <source>Epub 预处理正则不符合 toml 规范</source>
        <translation>Invalid Epub preprocess TOML</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Epub 后处理正则不符合 toml 规范</source>
        <translation>Invalid Epub postprocess TOML</translation>
    </message>
    <message>
        <source>Epub预处理正则格式错误</source>
        <translation type="vanished">Epub PreProcRegex is nonconforming</translation>
    </message>
    <message>
        <source>Epub后处理正则格式错误</source>
        <translation type="vanished">Epub PostProcRegex is nonconforming</translation>
    </message>
</context>
<context>
    <name>GPPGUI.GPPGUI</name>
    <message>
        <location filename="GPPGUI.cpp" line="+83"/>
        <source>Updater 更新错误</source>
        <translation>Updater error</translation>
    </message>
    <message>
        <location line="+58"/>
        <source>无法创建共享内存段，程序即将退出。</source>
        <translation>Cannot create shared memory. Exiting.</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+62"/>
        <location line="+54"/>
        <source>错误</source>
        <translation>Error</translation>
    </message>
    <message>
        <location line="-55"/>
        <source>无法启动本地服务，程序即将退出。</source>
        <translation>Cannot start local service. Exiting.</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>缓存删除错误</source>
        <translation>Cache delete error</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>TOML 错误</source>
        <translation>TOML error</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>标准错误</source>
        <translation>Std error</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>遇到了未知的错误，程序即将退出。</source>
        <translation>Unknown error. Exiting.</translation>
    </message>
</context>
<context>
    <name>GPPGUI.main</name>
    <message>
        <source>Updater 更新错误</source>
        <translation type="vanished">Updater error</translation>
    </message>
    <message>
        <source>无法创建共享内存段，程序即将退出。</source>
        <translation type="vanished">Cannot create shared memory. Exiting.</translation>
    </message>
    <message>
        <source>错误</source>
        <translation type="vanished">Error</translation>
    </message>
    <message>
        <source>无法启动本地服务，程序即将退出。</source>
        <translation type="vanished">Cannot start local service. Exiting.</translation>
    </message>
    <message>
        <source>缓存删除错误</source>
        <translation type="vanished">Cache delete error</translation>
    </message>
    <message>
        <source>TOML 错误</source>
        <translation type="vanished">TOML error</translation>
    </message>
    <message>
        <source>标准错误</source>
        <translation type="vanished">Std error</translation>
    </message>
    <message>
        <source>遇到了未知的错误，程序即将退出。</source>
        <translation type="vanished">Unknown error. Exiting.</translation>
    </message>
</context>
<context>
    <name>GptDictModel</name>
    <message>
        <location filename="GptDictModel.cpp" line="+7"/>
        <source>原文</source>
        <translation>original</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>译文</source>
        <translation>translation</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>描述</source>
        <translation>note</translation>
    </message>
    <message>
        <location line="+49"/>
        <source>拖动调整顺序</source>
        <translation>Drag to reorder</translation>
    </message>
</context>
<context>
    <name>HomePage</name>
    <message>
        <location filename="HomePage.cpp" line="+280"/>
        <location line="+10"/>
        <source>主页</source>
        <translation>Home Page</translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <location filename="Mainwindow.cpp" line="+48"/>
        <source>解析错误</source>
        <translation>Fail to analyze</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>基本配置文件不符合 toml 规范！</source>
        <translation>Base config file is nonconforming!</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>不是哥们</source>
        <translation>What fuck</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>有病吧，你把我软件的配置文件删了！？</source>
        <translation>Where is the app&apos;s config file!?</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>取消</source>
        <translation>Cancel</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>最小化</source>
        <translation>Minimum</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>确认</source>
        <translation>Exit</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>退出</source>
        <translation>Exiting</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>确定要退出程序吗</source>
        <translation>Are you sure to exit application</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>警告</source>
        <translation>Warning</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>项目 %1 仍在运行，请先停止运行！</source>
        <oldsource> 仍在运行，请先停止运行！</oldsource>
        <translation>Project %1 is running; stop it first!</translation>
    </message>
    <message>
        <location line="+31"/>
        <source>成功</source>
        <translation>Success</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>初始化成功!</source>
        <translation>Initialized successfully!</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>检测到异常退出</source>
        <translation>Abnormal exit detected</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>请注意备份相关翻译缓存</source>
        <translation>Please back up the related translation cache</translation>
    </message>
    <message>
        <location line="+69"/>
        <location line="+167"/>
        <source>新建项目</source>
        <translation>New</translation>
    </message>
    <message>
        <location line="-166"/>
        <source>打开项目</source>
        <translation>Open</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>保存项目配置</source>
        <translation>Save</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>移除项目</source>
        <translation>Remove</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>删除项目</source>
        <translation>Delete</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>初始化成功！</source>
        <translation>Initialized successfully!</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>更新内容</source>
        <translation>Update content</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>召唤停靠窗口</source>
        <translation>Show dock widget</translation>
    </message>
    <message>
        <location line="+4"/>
        <location line="+62"/>
        <source>应用设置</source>
        <oldsource>设置</oldsource>
        <translation>App settings</translation>
    </message>
    <message>
        <location line="-58"/>
        <source>更改程序主题</source>
        <translation>Change theme</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>清空当前项目翻译日志</source>
        <translation>Clear log</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>主页</source>
        <translation>Home page</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>默认提示词管理</source>
        <translation>Default prompts</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>通用字典管理</source>
        <translation>Common dicts</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>通用译前字典</source>
        <translation>Common preDicts</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>通用GPT字典</source>
        <translation>Common gptDicts</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>通用译后字典</source>
        <translation>Common postDicts</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>项目管理</source>
        <translation>Projects</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>使用说明</source>
        <translation>Instructions</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>关于</source>
        <translation>About</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+6"/>
        <source>请稍候</source>
        <translation>Wait a minute</translation>
    </message>
    <message>
        <location line="-6"/>
        <source>正在检查更新...</source>
        <translation>Checking upate...</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>正在下载更新...</source>
        <translation>Downloading update...</translation>
    </message>
    <message>
        <location line="+39"/>
        <source>选择新项目的存放位置</source>
        <translation>Choose new dir to store project</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>请输入项目名称</source>
        <translation>Please type in project name</translation>
    </message>
    <message>
        <location line="+10"/>
        <location line="+6"/>
        <location line="+6"/>
        <location line="+14"/>
        <location line="+5"/>
        <location line="+43"/>
        <source>创建失败</source>
        <translation>Fail to create</translation>
    </message>
    <message>
        <location line="-74"/>
        <location line="+107"/>
        <source>已存在同名项目！</source>
        <translation>Project name exists!</translation>
    </message>
    <message>
        <location line="-101"/>
        <source>项目名称不能为空，且不能包含斜杠或反斜杠！</source>
        <translation>Project name can not be empty or contain slash/backslash!</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>目录下存在同名文件或文件夹！</source>
        <translation>File/dir with the same name has already existed!</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>无法创建新文件！</source>
        <translation>Can not create new file!</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>无法读取模板文件！</source>
        <translation>Can not read template file!</translation>
    </message>
    <message>
        <location line="+43"/>
        <source>无法写入配置文件！</source>
        <translation>Can not write into config file!</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>创建成功</source>
        <translation>Created successfully</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>请将待翻译的文件放入 gt_input 中！</source>
        <translation>Please put files you want to translate into gt_input dir!</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>选择已有项目的文件夹路径</source>
        <translation>Choose a dir of an existed project</translation>
    </message>
    <message>
        <location line="+8"/>
        <location line="+11"/>
        <source>打开失败</source>
        <translation>Fail to open</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>打开成功</source>
        <translation>Opened successfully</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>%1 已纳入项目管理！</source>
        <translation>%1 added to projects!</translation>
    </message>
    <message>
        <location line="+112"/>
        <source>项目 %1 已从项目管理和磁盘中移除！</source>
        <translation>Project %1 removed from list and disk!</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>项目 %1 配置信息已保存！</source>
        <translation>Project %1 config saved!</translation>
    </message>
    <message>
        <location line="-119"/>
        <location line="+4"/>
        <source>移除失败</source>
        <translation>Fail to remove</translation>
    </message>
    <message>
        <location line="-36"/>
        <source>目录下不存在 Config.toml 文件！</source>
        <translation>No Config.toml file exists in this directory!</translation>
    </message>
    <message>
        <location line="+32"/>
        <location line="+53"/>
        <location line="+60"/>
        <source>当前页面不是项目页面！</source>
        <translation>Current page is not a project page!</translation>
    </message>
    <message>
        <location line="-109"/>
        <location line="+53"/>
        <source>当前项目正在运行，请先停止运行！</source>
        <translation>Current project is still running, please stop it first!</translation>
    </message>
    <message>
        <location line="-48"/>
        <location line="+53"/>
        <source>是</source>
        <translation>Yes</translation>
    </message>
    <message>
        <location line="-52"/>
        <location line="+53"/>
        <source>思考人生</source>
        <translation>Reflect on life</translation>
    </message>
    <message>
        <location line="-52"/>
        <location line="+53"/>
        <source>否</source>
        <translation>No</translation>
    </message>
    <message>
        <location line="-48"/>
        <source>你确定要移除当前项目吗？</source>
        <translation>Are you sure to remove current project?</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>从项目管理中移除该项目，但不会删除其项目文件夹</source>
        <translation>Remove from project management, but won&apos;t delete its project dir</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>移除成功</source>
        <translation>Removed successfully</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>项目 %1 已从项目管理中移除！</source>
        <translation>Project %1 removed from list!</translation>
    </message>
    <message>
        <location line="+12"/>
        <location line="+4"/>
        <location line="+30"/>
        <source>删除失败</source>
        <translation>Fail to delete</translation>
    </message>
    <message>
        <location line="-18"/>
        <source>你确定要删除当前项目吗？                </source>
        <oldsource>你确定要删除当前项目吗？</oldsource>
        <translation>Are you sure to delete current project?</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>将删除该项目的项目文件夹，如果不备份，再次翻译将必须从头开始！</source>
        <translation>Delete project dir of this project, you will lost all translation cache!</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>删除成功</source>
        <translation>Deleted successfully</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>保存失败</source>
        <translation>Fail to save</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>保存成功</source>
        <translation>Saved successfully</translation>
    </message>
</context>
<context>
    <name>NJCfgPage</name>
    <message>
        <location filename="NJCfgPage.cpp" line="+14"/>
        <location line="+27"/>
        <source>NormalJson 输出配置</source>
        <translation>NormalJson output settings</translation>
    </message>
    <message>
        <location line="-16"/>
        <source>输出带原文</source>
        <translation>Output with src text</translation>
    </message>
</context>
<context>
    <name>NameTableModel</name>
    <message>
        <location filename="NameTableModel.cpp" line="+8"/>
        <source>原名</source>
        <translation>Original</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>译名</source>
        <translation>Translation</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>出现次数</source>
        <translation>Count</translation>
    </message>
</context>
<context>
    <name>NameTableSettingsPage</name>
    <message>
        <location filename="NameTableSettingsPage.cpp" line="+273"/>
        <source>人名替换表</source>
        <translation>NameTable</translation>
    </message>
    <message>
        <location line="-211"/>
        <source>解析失败</source>
        <translation>Fail to analyze</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>NameTable.toml 不符合 toml 规范</source>
        <translation>NameTable.toml is invalid TOML</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>纯文本</source>
        <translation>Plain text</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>表模式</source>
        <translation>Table mode</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>保存当前页</source>
        <translation>Save current page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>撤回删除行</source>
        <translation>Cancel deleted row</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>刷新当前页</source>
        <translation>Refresh current page</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>添加词条</source>
        <translation>Add new dict</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>删除词条</source>
        <translation>Remove current dict</translation>
    </message>
    <message>
        <location line="+65"/>
        <source>刷新成功</source>
        <translation>Refreshed successfully</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>重新载入了 NameTable.toml</source>
        <translation>Reloaded NameTable.toml</translation>
    </message>
    <message>
        <location line="+41"/>
        <source>已保存 NameTable.toml</source>
        <translation>Saved NameTable.toml</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>保存成功</source>
        <translation>Saved successfully</translation>
    </message>
</context>
<context>
    <name>NormalDictModel</name>
    <message>
        <location filename="NormalDictModel.cpp" line="+28"/>
        <source>原文</source>
        <translation>Original</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>译文</source>
        <translation>Translation</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>条件</source>
        <translation>Rules</translation>
    </message>
    <message>
        <location line="+34"/>
        <source>拖动调整顺序</source>
        <translation>Drag to reorder</translation>
    </message>
    <message>
        <location line="-34"/>
        <source>启用正则</source>
        <translation>Enable regex</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>优先级</source>
        <translation>Priority</translation>
    </message>
</context>
<context>
    <name>OtherSettingsPage</name>
    <message>
        <location filename="OtherSettingsPage.cpp" line="+24"/>
        <source>其它设置</source>
        <translation>Other settings</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>项目路径</source>
        <translation>Project path</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>打开文件夹</source>
        <translation>Open project dir</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>问题概览输出格式</source>
        <translation>Problem overview output format</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>翻译完成后输出 ProblemOverview 的文件格式</source>
        <translation>ProblemOverview output format</translation>
    </message>
    <message>
        <location line="+31"/>
        <source>移动项目</source>
        <translation>Move project</translation>
    </message>
    <message>
        <location line="+4"/>
        <location line="+12"/>
        <location line="+8"/>
        <source>移动失败</source>
        <translation>Fail to move</translation>
    </message>
    <message>
        <location line="-20"/>
        <source>项目仍在运行中，无法移动</source>
        <translation>Project is still running</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>请选择要移动到的文件夹</source>
        <translation>Please choose the dir you want to move to</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>移动成功</source>
        <translation>Moved successfully</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>%1 项目已移动到新文件夹</source>
        <oldsource> 项目已移动到新文件夹</oldsource>
        <translation>Project %1 moved to new folder</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>项目更名</source>
        <translation>Rename project</translation>
    </message>
    <message>
        <location line="+4"/>
        <location line="+11"/>
        <location line="+6"/>
        <location line="+8"/>
        <source>更名失败</source>
        <translation>Fail to rename</translation>
    </message>
    <message>
        <location line="-25"/>
        <source>项目仍在运行中，无法更名</source>
        <translation>Project is still running</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>请输入新的项目名称</source>
        <translation>Please type in new project name</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>新的项目名</source>
        <translation>New name</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>项目名不能为空且不能包含斜杠</source>
        <translation>Project name cannot be empty or contain slashes</translation>
    </message>
    <message>
        <location line="-36"/>
        <location line="+42"/>
        <source>目录下已有同名文件或文件夹</source>
        <translation>File/dir with the same name has already existed</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>更名成功</source>
        <translation>Renamed successfully</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>使用 ProblemOverview.json/.toml 中的 Sentence 替换 trans_cache 中的 Sentence</source>
        <translation>Replace Sentences in trans_cache with Sentences from ProblemOverview.json/.toml</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>选择翻译问题概览文件</source>
        <translation>Choose issue summary file</translation>
    </message>
    <message>
        <location line="+115"/>
        <source>保存项目配置</source>
        <translation>Save project settings</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>保存</source>
        <translation>Save</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>保存成功</source>
        <translation>Saved successfully</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>刷新项目配置</source>
        <translation>Refresh project configs</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>刷新</source>
        <translation>Refresh</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>你确定要刷新项目配置吗？</source>
        <translation>Are you sure to refresh project config?</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>GUI中未保存的数据将会被覆盖！</source>
        <translation>Data in GUI that not saved will be covered!</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>删除翻译缓存</source>
        <translation>Delete translation cache</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>删除</source>
        <translation>Delete</translation>
    </message>
    <message>
        <location line="+4"/>
        <location line="+29"/>
        <source>删除失败</source>
        <translation>Fail to delete</translation>
    </message>
    <message>
        <location line="-29"/>
        <source>项目仍在运行中，无法删除缓存</source>
        <translation>Project is still running</translation>
    </message>
    <message>
        <location line="-40"/>
        <location line="+46"/>
        <source>否</source>
        <translation>No</translation>
    </message>
    <message>
        <location line="-302"/>
        <source>项目移动/更名</source>
        <translation>Move/Rename project</translation>
    </message>
    <message>
        <location line="+80"/>
        <source>项目已更名为 %1</source>
        <translation>Project renamed to %1</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>导入翻译问题概览至翻译缓存</source>
        <translation>Import issue summary to trans_cache</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>导入</source>
        <translation>Import</translation>
    </message>
    <message>
        <location line="+4"/>
        <location line="+107"/>
        <source>导入失败</source>
        <translation>Fail to import</translation>
    </message>
    <message>
        <location line="-107"/>
        <source>项目仍在运行中，无法导入</source>
        <translation>Project is still running</translation>
    </message>
    <message>
        <location line="+35"/>
        <source>[文件 %1] 未在 cache 中找到，跳过导入</source>
        <translation>[File %1] not in cache; import skipped</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>[文件 %1] 无法解析，跳过导入</source>
        <translation>[File %1] cannot parse; import skipped</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>[文件 %1] 句子(index %2) 未在 cache 中找到，跳过导入</source>
        <translation>[File %1] sentence %2 not in cache; skipped</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>[文件 %1] 句子(index %2) 与 cache 中原文不匹配，可能产生意外结果，
概览原文: %3
缓存原文: %4</source>
        <translation>[File %1] sentence %2 differs from cache; result may vary
Preview: %3
Cache: %4</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>[文件 %1] 无法写入，跳过导入</source>
        <translation>[File %1] cannot write; import skipped</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>成功导入 %1 个句子至 trans_cache</source>
        <translation>Imported %1 sentences to trans_cache</translation>
    </message>
    <message>
        <location line="+9"/>
        <location line="+3"/>
        <source>导入完毕</source>
        <translation>Importing completes</translation>
    </message>
    <message>
        <location line="-3"/>
        <source>导入中出现的问题记录在 import_problems.log 中</source>
        <translation>Problems during importing saved to import_problems.log</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>开始翻译或关闭程序时会自动保存所有项目的配置，一般无需手动保存</source>
        <translation>All project settings auto-save on translation start or exit</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>项目 %1 的配置信息已保存</source>
        <translation>Project %1 config saved</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>刷新现有配置和字典，谨慎使用</source>
        <translation>Refresh config/dictionaries. Use carefully.</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>刷新失败</source>
        <translation>Fail to refresh</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>项目仍在运行中，无法刷新</source>
        <translation>Project is still running</translation>
    </message>
    <message>
        <location line="+6"/>
        <location line="+46"/>
        <source>思考人生</source>
        <translation>Reflect on life</translation>
    </message>
    <message>
        <location line="-45"/>
        <location line="+46"/>
        <source>是</source>
        <translation>Yes</translation>
    </message>
    <message>
        <location line="-18"/>
        <source>删除项目的翻译缓存，下次翻译将会重新从头开始</source>
        <translation>Translation all over again</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>你确定要删除项目翻译缓存吗？</source>
        <translation>Delete this project&apos;s translation cache?</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>再次翻译将会重新从头开始！</source>
        <translation>Next translation restarts from the beginning!</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>删除成功</source>
        <translation>Deleted successfully</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>项目 %1 的翻译缓存已删除</source>
        <translation>Project %1 cache deleted</translation>
    </message>
</context>
<context>
    <name>PASettingsPage</name>
    <message>
        <location filename="PASettingsPage.cpp" line="+29"/>
        <source>问题分析</source>
        <translation>Problem analyze</translation>
    </message>
    <message>
        <location line="+34"/>
        <source>词频过高</source>
        <translation>High word frequency</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>标点错漏</source>
        <translation>Error punctuations</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>丢失换行</source>
        <translation>Lose linebreak</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>多加换行</source>
        <translation>Redundant linebreak</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>比原文长</source>
        <translation>Longer than src</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>比原文长严格</source>
        <translation>Strictly longer</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>字典未使用</source>
        <translation>Dict unused</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>残留日文</source>
        <translation>Remain jp</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>引入拉丁字母</source>
        <translation>Intro latin</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>引入韩文</source>
        <translation>Intro hangul</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>语言不通</source>
        <translation>Incorrect lang</translation>
    </message>
    <message>
        <location line="-1"/>
        <source>引入繁体字</source>
        <translation>Intro traditional Chinese</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>非法字符</source>
        <translation>Invalid char</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>要发现的问题清单</source>
        <translation>Problem List</translation>
    </message>
    <message>
        <location line="+48"/>
        <source>标点查错</source>
        <translation>Punctuations to check</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>规定标点错漏要查哪些标点</source>
        <translation>Punctuations to check in &quot;Error punctuations&quot;</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>语言置信度</source>
        <translation>Lang probability</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>语言不通检测的语言置信度(0-1)，设置越高则检测越精准，但可能遗漏，反之亦然</source>
        <translation>Probability in &quot;incorrect lang&quot;, higher means preciser, but may omit</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>字符集</source>
        <translation>Code page</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>非法字符要检查的字符集</source>
        <translation>The code page to check in &quot;Invalid char&quot;</translation>
    </message>
    <message>
        <location line="+11"/>
        <location line="+71"/>
        <source>比较对象设置</source>
        <translation>Comparison field settings</translation>
    </message>
    <message>
        <location line="-42"/>
        <source>比较对象</source>
        <translation>Comparison fields</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>base 是比较基准字段，check 是被检查字段。</source>
        <translation>base is the reference field; check is the field being checked</translation>
    </message>
    <message>
        <location line="+41"/>
        <source>设置每个问题分析规则使用的 base/check 字段</source>
        <translation>Set the base/check fields used by each problem analysis rule</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>进入设置</source>
        <translation>Open settings</translation>
    </message>
    <message>
        <source>点击下方『语法示例』按钮以获取具体语法规则及作用</source>
        <oldsource>点击下方『语法示例』按钮以获取语法规则及作用</oldsource>
        <translation type="vanished">Push the button below to get detailed explanation of grammars and effects</translation>
    </message>
    <message>
        <location line="+57"/>
        <source>%1 不符合 toml 规范</source>
        <translation>%1 is invalid TOML</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>重翻关键字设定</source>
        <translation>RetranslKeys setting</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>查看 重翻关键字/跳过问题关键字 设定的语法示例</source>
        <translation>Look up example grammars of retranslKeys/skipProblems</translation>
    </message>
    <message>
        <location line="-10"/>
        <source>跳过问题关键字设定</source>
        <translation>Skip problems keys setting</translation>
    </message>
    <message>
        <location line="-40"/>
        <source>正则表达式数组，具体规则见下方语法示例</source>
        <translation>Regex array; see examples below</translation>
    </message>
    <message>
        <location line="+48"/>
        <source>语法示例</source>
        <translation>Grammars examples</translation>
    </message>
    <message>
        <location line="-20"/>
        <source>解析错误</source>
        <translation>Fail to analyze</translation>
    </message>
</context>
<context>
    <name>PDFCfgPage</name>
    <message>
        <location filename="PDFCfgPage.cpp" line="+17"/>
        <location line="+42"/>
        <source>PDF 输出配置</source>
        <translation>PDF output settings</translation>
    </message>
    <message>
        <location line="-31"/>
        <source>输出双语翻译文件</source>
        <translation>Output bilingual file</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>BabelDOC 目标语言码</source>
        <translation>BabelDOC target language code</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>影响 PDF 字体和排版，如 zh-CN/zh-TW/en/ja/ko</source>
        <translation>Affects PDF fonts and layout, e.g. zh-CN/zh-TW/en/ja/ko</translation>
    </message>
</context>
<context>
    <name>PluginItemWidget</name>
    <message>
        <location filename="PluginItemWidget.cpp" line="+17"/>
        <source>滤过插件</source>
        <oldsource>滤过插件，默认开启为 run 阶段</oldsource>
        <translation>Filter plugin</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>全角半角转换插件</source>
        <oldsource>全角半角转换插件，默认开启为 postRun 阶段</oldsource>
        <translation>Width conversion plugin</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>换行修复插件</source>
        <oldsource>换行修复插件，默认开启为 run 阶段</oldsource>
        <translation>Text linebreak fixing plugin</translation>
    </message>
</context>
<context>
    <name>PluginSettingsPage</name>
    <message>
        <location filename="PluginSettingsPage.cpp" line="+25"/>
        <source>插件设置</source>
        <translation>Plugin settings</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>文本插件设置</source>
        <oldsource>预处理插件设置</oldsource>
        <translation>Text plugin settings</translation>
    </message>
    <message>
        <location line="+78"/>
        <source>浏览</source>
        <translation>Browse</translation>
    </message>
    <message>
        <location line="+21"/>
        <location line="+27"/>
        <source>解析错误</source>
        <translation>Fail to analyze</translation>
    </message>
    <message>
        <location line="-26"/>
        <source>自定义文本处理插件不符合 toml 规范</source>
        <translation>Custom text plugin is nonconforming</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>选择自定义文本处理插件</source>
        <translation>Choose custom text plugin</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>%1 不符合 toml 规范</source>
        <oldsource> 不符合 toml 规范</oldsource>
        <translation>%1 is invalid TOML</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>自定义文本处理插件</source>
        <translation>Custom text plugins</translation>
    </message>
</context>
<context>
    <name>ProjectCachePage</name>
    <message>
        <location filename="ProjectCachePage.cpp" line="+31"/>
        <source>缓存管理</source>
        <oldsource>缓存与问题</oldsource>
        <translation>Cache</translation>
    </message>
    <message>
        <location line="+35"/>
        <source>打开缓存文件夹</source>
        <translation>Open Cache Folder</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>确认刷新</source>
        <translation>Confirm refresh</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>刷新会放弃所有未保存的缓存修改，确定要继续吗？</source>
        <translation>Refreshing will discard all unsaved cache changes. Continue?</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>保存当前文件</source>
        <translation>Save Current File</translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+21"/>
        <source>保存缓存</source>
        <translation>Save Cache</translation>
    </message>
    <message>
        <location line="-12"/>
        <source>已保存 %1</source>
        <oldsource>已保存 </oldsource>
        <translation>Saved %1</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>保存全部</source>
        <translation>Save All</translation>
    </message>
    <message>
        <location line="+31"/>
        <source>已保存 %1 个缓存文件</source>
        <oldsource> 个缓存文件</oldsource>
        <translation>Saved %1 cache files</translation>
    </message>
    <message>
        <location line="+55"/>
        <source>删除选中文件</source>
        <translation>Delete Selected Files</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>删除缓存文件</source>
        <translation>Delete Cache Files</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+259"/>
        <source>确认删除</source>
        <translation>Confirm Delete</translation>
    </message>
    <message>
        <location line="-258"/>
        <source>确定要删除选中的 %1 个缓存文件吗？</source>
        <oldsource>确定要删除选中的 </oldsource>
        <translation>Delete %1 selected cache files?</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>已删除 %1 个缓存文件</source>
        <oldsource>已删除 </oldsource>
        <translation>Deleted %1 cache files</translation>
    </message>
    <message>
        <location line="+193"/>
        <source>在当前文件中搜索 preproc/transraw/problems...</source>
        <translation>Search preproc / transraw / problems in current file...</translation>
    </message>
    <message>
        <location line="-244"/>
        <source>文件</source>
        <translation>Files</translation>
    </message>
    <message>
        <location line="-99"/>
        <source>刷新</source>
        <translation>Refresh</translation>
    </message>
    <message>
        <location line="+181"/>
        <source>搜索内容...</source>
        <translation>Search...</translation>
    </message>
    <message>
        <location line="+18"/>
        <location line="+40"/>
        <source>全部</source>
        <translation>All</translation>
    </message>
    <message>
        <location filename="ProjectCachePageSearch.cpp" line="+229"/>
        <source>批量替换</source>
        <translation>Batch Replace</translation>
    </message>
    <message>
        <location filename="ProjectCachePage.cpp" line="-12"/>
        <source>查找</source>
        <translation>Find</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>替换为</source>
        <translation>Replace with</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>transraw</source>
        <translation>transraw</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>preproc</source>
        <translation>preproc</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>预览</source>
        <translation>Preview</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>替换</source>
        <translation>Replace</translation>
    </message>
    <message>
        <location line="-152"/>
        <location filename="ProjectCachePageSearch.cpp" line="-99"/>
        <source>搜索</source>
        <translation>Search</translation>
    </message>
    <message>
        <location line="+193"/>
        <source>刷新问题</source>
        <translation>Refresh Problems</translation>
    </message>
    <message>
        <location line="-192"/>
        <location filename="ProjectCachePageSearch.cpp" line="+58"/>
        <location line="+145"/>
        <source>问题</source>
        <translation>Problems</translation>
    </message>
    <message>
        <location line="+225"/>
        <location filename="ProjectCachePageEntries.cpp" line="+32"/>
        <location line="+42"/>
        <source>未选择缓存文件</source>
        <translation>No cache file selected</translation>
    </message>
    <message>
        <location line="+31"/>
        <source>只看问题句</source>
        <translation>Problems only</translation>
    </message>
    <message>
        <location filename="ProjectCachePageActions.cpp" line="+90"/>
        <location filename="ProjectCachePage.cpp" line="+20"/>
        <source>删除选中条目</source>
        <translation>Delete Selected Entries</translation>
    </message>
    <message>
        <location filename="ProjectCachePage.cpp" line="+3"/>
        <source>删除缓存条目</source>
        <translation>Delete Cache Entries</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>确定要删除选中的 %1 个缓存条目吗？</source>
        <oldsource> 个缓存条目吗？</oldsource>
        <translation>Delete %1 selected cache entries?</translation>
    </message>
    <message>
        <location filename="ProjectCachePageFiles.cpp" line="+134"/>
        <source>文件 (%1)</source>
        <translation>Files (%1)</translation>
    </message>
    <message>
        <location filename="ProjectCachePageEntries.cpp" line="+105"/>
        <source>translated_view_text（只读）</source>
        <oldsource>translated_preview（只读）</oldsource>
        <translation>translated_preview (read-only)</translation>
    </message>
    <message>
        <location filename="ProjectCachePageActions.cpp" line="-18"/>
        <source>%1 句 · %2 已翻译 · %3 有问题 · %4 已选择</source>
        <translation>%1 lines · %2 translated · %3 with problems · %4 selected</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>删除选中条目 (%1)</source>
        <translation>Delete Selected Entries (%1)</translation>
    </message>
    <message>
        <location filename="ProjectCachePageSearch.cpp" line="+0"/>
        <source>问题 (%1)</source>
        <translation>Problems (%1)</translation>
    </message>
    <message>
        <location filename="ProjectCachePageEntries.cpp" line="-6"/>
        <source>original_text（元信息，只读）</source>
        <translation>original_text (metadata, read-only)</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>pre_processed_text（原文，可编辑）</source>
        <translation>pre_processed_text (source, editable)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>translated_raw_text（译文，可编辑）</source>
        <oldsource>pre_translated_text（译文，可编辑）</oldsource>
        <translation>pre_translated_text (translation, editable)</translation>
    </message>
    <message>
        <location line="+57"/>
        <source>已删除 %1 个条目，保存后生效</source>
        <translation>Deleted %1 entries; save to apply</translation>
    </message>
    <message>
        <location line="+98"/>
        <source>问题: %1</source>
        <translation>Issue: %1</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>原文: %1</source>
        <translation>Source: %1</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>译文: %1</source>
        <translation>Translation: %1</translation>
    </message>
    <message>
        <location filename="ProjectCachePageFiles.cpp" line="-17"/>
        <source>%1 句 · %2 问题</source>
        <translation>%1 lines · %2 problems</translation>
    </message>
    <message>
        <location filename="ProjectCachePageActions.cpp" line="+65"/>
        <location filename="ProjectCachePage.cpp" line="-175"/>
        <source>展开批量替换</source>
        <translation>Expand Batch Replace</translation>
    </message>
    <message>
        <location filename="ProjectCachePage.cpp" line="+154"/>
        <source>编辑选中条目</source>
        <translation>Edit Selected Entry</translation>
    </message>
    <message>
        <location filename="ProjectCachePageFiles.cpp" line="+2"/>
        <source>解析失败</source>
        <translation>Parse failed</translation>
    </message>
    <message>
        <location line="+37"/>
        <source>缓存文件不是 JSON 数组: %1</source>
        <oldsource>缓存文件不是 JSON 数组: </oldsource>
        <translation>Cache is not a JSON array: %1</translation>
    </message>
    <message>
        <location filename="ProjectCachePageEntries.cpp" line="-163"/>
        <source>problems（只读）</source>
        <translation>problems (read-only)</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>关闭</source>
        <translation>Close</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>保存修改</source>
        <translation>Save Changes</translation>
    </message>
    <message>
        <location filename="ProjectCachePageActions.cpp" line="-114"/>
        <source>完成</source>
        <translation>Done</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>失败</source>
        <translation>Failed</translation>
    </message>
    <message>
        <location line="+107"/>
        <source>收起批量替换</source>
        <translation>Collapse Batch Replace</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>项目正在运行中，只允许查看缓存。</source>
        <translation>The project is running. Cache is read-only.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>否</source>
        <translation>No</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>思考人生</source>
        <translation>Reflect on life</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>是</source>
        <translation>Yes</translation>
    </message>
    <message>
        <location filename="ProjectCachePageFiles.cpp" line="+33"/>
        <source>解析缓存失败: %1
%2</source>
        <oldsource>解析缓存失败: </oldsource>
        <translation>Failed to parse cache: %1
%2</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>写入缓存失败: %1
%2</source>
        <oldsource>写入缓存失败: </oldsource>
        <translation>Cache write failed: %1
%2</translation>
    </message>
    <message>
        <location filename="ProjectCachePageSearch.cpp" line="-147"/>
        <source>原文</source>
        <translation>Source</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>译文</source>
        <translation>Translation</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>%1 条结果</source>
        <translation>%1 result(s)</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>搜索 (%1)</source>
        <translation>Search (%1)</translation>
    </message>
    <message>
        <location line="+8"/>
        <location line="+25"/>
        <source>请输入查找内容</source>
        <translation>Please enter text to find</translation>
    </message>
    <message>
        <location line="-20"/>
        <source>共 %1 处匹配，涉及 %2 个文件</source>
        <translation>%1 match(es) across %2 file(s)</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>
...</source>
        <translation>
...</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>无匹配内容</source>
        <translation>No matches</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>确认替换</source>
        <translation>Confirm Replace</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>确定要替换 %1 处内容吗？</source>
        <translation>Replace %1 occurrence(s)?</translation>
    </message>
    <message>
        <location line="+31"/>
        <source>已替换 %1 处，涉及 %2 个文件；保存后落盘。</source>
        <translation>Replaced %1 in %2 files; save to write.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>批量替换完成，记得保存修改</source>
        <translation>Batch replace done; save changes.</translation>
    </message>
    <message>
        <location line="+49"/>
        <source>点击搜索此问题</source>
        <translation>Click to search this problem</translation>
    </message>
</context>
<context>
    <name>ProjectSettingsPage</name>
    <message>
        <location filename="ProjectSettingsPage.cpp" line="+34"/>
        <source>项目设置主页</source>
        <translation>Project settings home</translation>
    </message>
    <message>
        <location line="+9"/>
        <location line="+238"/>
        <source>解析失败</source>
        <translation>Fail to analyze</translation>
    </message>
    <message>
        <location line="-210"/>
        <source>Toml 格式化错误</source>
        <translation>TOML formatting error</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>清理成功</source>
        <translation>Clear successfully</translation>
    </message>
    <message>
        <location line="-52"/>
        <source>项目 %1 的配置文件不符合 toml 规范</source>
        <translation>Project %1 config is invalid TOML</translation>
    </message>
    <message>
        <location line="+52"/>
        <source>已清空项目 %1 的日志输出窗口</source>
        <translation>Cleared log view for project %1</translation>
    </message>
    <message>
        <location line="+12"/>
        <location line="+9"/>
        <location line="+51"/>
        <source>Api设置</source>
        <translation>Api</translation>
    </message>
    <message>
        <location line="-50"/>
        <location line="+55"/>
        <source>一般设置</source>
        <translation>Common</translation>
    </message>
    <message>
        <location line="-54"/>
        <location line="+59"/>
        <source>问题分析</source>
        <translation>Problems</translation>
    </message>
    <message>
        <location line="-54"/>
        <source>基本设置</source>
        <translation>Base</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+52"/>
        <source>人名表</source>
        <translation>NameTable</translation>
    </message>
    <message>
        <location line="-51"/>
        <location line="+56"/>
        <source>项目字典</source>
        <translation>ProjectDicts</translation>
    </message>
    <message>
        <location line="-55"/>
        <location line="+60"/>
        <source>字典设置</source>
        <translation>DictSettings</translation>
    </message>
    <message>
        <location line="-59"/>
        <location line="+64"/>
        <source>提示词</source>
        <translation>Prompts</translation>
    </message>
    <message>
        <location line="-59"/>
        <source>翻译设置</source>
        <translation>Trans</translation>
    </message>
    <message>
        <location line="+4"/>
        <location line="+63"/>
        <source>插件管理</source>
        <translation>Plugins</translation>
    </message>
    <message>
        <location line="-62"/>
        <location line="+68"/>
        <source>缓存管理</source>
        <oldsource>缓存与问题</oldsource>
        <translation>Cache</translation>
    </message>
    <message>
        <location line="-67"/>
        <location line="+75"/>
        <source>开始翻译</source>
        <translation>Start</translation>
    </message>
    <message>
        <location line="-74"/>
        <location line="+79"/>
        <source>其他设置</source>
        <translation>Others</translation>
    </message>
    <message>
        <location line="+49"/>
        <source>项目仍在运行中，无法刷新配置</source>
        <translation>Project is still running</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>正在运行</source>
        <translation>Is running</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>项目 %1 的配置文件不符合规范</source>
        <translation>Project %1 config is invalid</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>刷新成功</source>
        <translation>Refreshed successfully</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>项目配置刷新成功</source>
        <translation>Refreshed successfully</translation>
    </message>
</context>
<context>
    <name>PromptSettingsPage</name>
    <message>
        <location filename="PromptSettingsPage.cpp" line="+20"/>
        <source>项目提示词设置</source>
        <translation>Project prompt settings</translation>
    </message>
    <message>
        <location line="+8"/>
        <location line="+10"/>
        <location line="+4"/>
        <source>解析失败</source>
        <translation>Fail to analyze</translation>
    </message>
    <message>
        <location line="-13"/>
        <source>项目 %1 的提示词配置文件不符合标准。</source>
        <oldsource> 的提示词配置文件不符合标准。</oldsource>
        <translation>Project %1 prompt config is invalid.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>默认提示词文件不符合 toml 规范</source>
        <translation>Default prompts file is nonconforming</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>找不到提示词文件</source>
        <translation>Prompt file not found</translation>
    </message>
    <message>
        <location line="+31"/>
        <source>用户提示词</source>
        <translation>User prompt</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>系统提示词</source>
        <translation>System prompt</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>agent用户</source>
        <translation>User-agent</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>agent系统</source>
        <translation>Sys-agent</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="ProjectCachePageDelegates.cpp" line="+218"/>
        <location line="+108"/>
        <source>原文</source>
        <translation>Src</translation>
    </message>
    <message>
        <location line="-107"/>
        <location line="+108"/>
        <source>译文</source>
        <translation>Dst</translation>
    </message>
    <message>
        <location line="-58"/>
        <source>问题</source>
        <translation>Problem</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>匹配</source>
        <translation>Match</translation>
    </message>
    <message>
        <location filename="TranslationWorkbenchPage.cpp" line="+206"/>
        <source>结果解析</source>
        <translation>Parse Result</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>模型请求</source>
        <translation>Model Request</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Agent</source>
        <translation>Agent</translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+55"/>
        <source>文件</source>
        <translation>File</translation>
    </message>
    <message>
        <location line="-49"/>
        <source>请求 %1</source>
        <translation>Request %1</translation>
    </message>
    <message>
        <location line="+50"/>
        <source>模型</source>
        <translation>Model</translation>
    </message>
    <message>
        <location filename="HomePage.cpp" line="-144"/>
        <source>AI 自动化翻译解决方案</source>
        <translation>AI-powered translation solution</translation>
    </message>
    <message>
        <location line="+54"/>
        <source>启动</source>
        <translation>Launch</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>获取</source>
        <translation>Get</translation>
    </message>
    <message>
        <location filename="ApiSettingsPage.cpp" line="-175"/>
        <location line="+7"/>
        <source>解析失败</source>
        <translation>Fail to analyze</translation>
    </message>
    <message>
        <location line="-6"/>
        <source>%1 必须是 JSON 对象</source>
        <translation>%1 must be a JSON object</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>%1 不是合法 JSON: %2</source>
        <translation>%1 is not valid JSON: %2</translation>
    </message>
</context>
<context>
    <name>SkipTransCfgPage</name>
    <message>
        <location filename="SkipTransCfgPage.cpp" line="+122"/>
        <location line="+102"/>
        <source>跳过翻译设置</source>
        <translation>SkipTrans settings</translation>
    </message>
    <message>
        <location line="-91"/>
        <source>跳过 H 关键字</source>
        <translation>Skip H keys</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>关键字列表以 base64 编码形式存储在 SkipTrans.toml 中</source>
        <translation>List of hKeys(base64) stores in SkipTrans.toml</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>编辑</source>
        <translation>Edit</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>编辑 H 关键词</source>
        <translation>Edit H keywords</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>H 关键词</source>
        <translation>H keywords</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>每行一个关键字，保存时会自动写回 base64</source>
        <translation>One keyword per line; saved back as base64 automatically</translation>
    </message>
    <message>
        <location line="+28"/>
        <source>语法与 retranslKeys 完全相同</source>
        <translation>The grammars is completely same as retranslKeys&apos;</translation>
    </message>
    <message>
        <location line="+32"/>
        <source>解析错误</source>
        <translation>Fail to analyze</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>skipKeys 不符合 toml 规范</source>
        <translation>skipKeys is nonconforming</translation>
    </message>
</context>
<context>
    <name>StartSettingsPage</name>
    <message>
        <location filename="StartSettingsPage.cpp" line="+366"/>
        <location line="+46"/>
        <source>回到底部并继续输出</source>
        <translation>Back to bottom and resume</translation>
    </message>
    <message>
        <location line="-376"/>
        <source>启动设置</source>
        <translation>Start settings</translation>
    </message>
    <message>
        <location line="+167"/>
        <location line="+72"/>
        <source>```
问题概览:</source>
        <translation>```
Issue summary:</translation>
    </message>
    <message>
        <location line="-56"/>
        <source>[GUI] 日志窗口缓存超过 5MB，有旧缓存被丢弃。完整日志请查看项目 logs/*.log。
</source>
        <oldsource>[GUI] 日志窗口缓存超过 5MB，有旧缓存被丢弃。完整日志请查看项目 logs/*.log。</oldsource>
        <translation>[GUI] Log cache exceeded 5MB; old entries dropped. Full logs: logs/*.log.</translation>
    </message>
    <message>
        <location line="+57"/>
        <location line="+1"/>
        <source>问题概览结束
```</source>
        <translation>Issue summary end
```</translation>
    </message>
    <message>
        <location line="+74"/>
        <source>日志输出</source>
        <translation>log output</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>日志输出已暂停，点击右侧按钮
回到底部并补发缓存</source>
        <translation>Logs paused. Use the right button to resume and flush.</translation>
    </message>
    <message>
        <location line="+34"/>
        <location line="+24"/>
        <source>继续输出(%1)</source>
        <oldsource>继续输出</oldsource>
        <translation>Resume (%1)</translation>
    </message>
    <message>
        <location line="+53"/>
        <source>文件处理器设置</source>
        <translation>File plugin config</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>工作线程数:</source>
        <translation>Working threads:</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>已用时间:</source>
        <translation>Used time:</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>剩余时间:</source>
        <translation>Remaining time:</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>翻译模式:</source>
        <translation>Translation mode:</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>开始翻译</source>
        <translation>Start translating</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>停止翻译</source>
        <translation>Stop translation</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>详情</source>
        <translation>Details</translation>
    </message>
    <message>
        <location line="-473"/>
        <source>翻译中</source>
        <translation>Translating</translation>
    </message>
    <message>
        <location line="+333"/>
        <source>文件处理器:</source>
        <translation>File plugin:</translation>
    </message>
    <message>
        <location line="+167"/>
        <source>文件格式错误</source>
        <translation>File format error</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>自定义文件插件的格式必须是 .lua 或 .py 格式。</source>
        <translation>Custom plugin must be *.lua or *.py format.</translation>
    </message>
    <message>
        <location line="+46"/>
        <source>停止中</source>
        <translation>Stopping</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>正在等待最后一批翻译完成，请稍候...</source>
        <translation>Waiting for the last batch of translations...</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>项目 %1 的翻译任务失败，请检查日志输出。</source>
        <translation>Project %1 translation failed; check logs.</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+4"/>
        <location line="+8"/>
        <source>翻译失败</source>
        <translation>Translation failed</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>项目 %1 连工厂函数都失败了，玩毛啊</source>
        <translation>Project %1 factory creation failed.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>项目 %1 的生成任务已完成。</source>
        <translation>Project %1 generation done.</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+2"/>
        <location line="+8"/>
        <location line="+2"/>
        <source>生成完成</source>
        <oldsource>项目 </oldsource>
        <translation>Project </translation>
    </message>
    <message>
        <location line="+27"/>
        <source>项目 %1 的翻译任务已终止</source>
        <translation>Project %1 translation stopped</translation>
    </message>
    <message>
        <location line="-30"/>
        <source>请在 show_normal 文件夹中查收项目 %1 的预处理结果。</source>
        <translation>Check project %1 preproc results in show_normal.</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>请在 gt_output 文件夹中查收项目 %1 的翻译结果。</source>
        <translation>Check project %1 translation in gt_output.</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+2"/>
        <source>翻译完成</source>
        <translation>Translation completed</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>项目 %1 的翻译任务停止成功。</source>
        <translation>Project %1 stopped.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>翻译停止</source>
        <translation>Translation stopped</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>停止成功</source>
        <translation>Stopped succeesfully</translation>
    </message>
</context>
<context>
    <name>TF2HCfgPage</name>
    <message>
        <location filename="TF2HCfgPage.cpp" line="+20"/>
        <location line="+90"/>
        <source>全角半角转换设置</source>
        <translation>Convert settings</translation>
    </message>
    <message>
        <location line="-79"/>
        <source>转换标点符号</source>
        <translation>Convert punctuations</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>反向替换</source>
        <translation>Reverse converting</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>关闭为全转半，开启为半转全</source>
        <translation>Off: full to half; on: half to full</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>不转换的字符</source>
        <translation>Exclude chars</translation>
    </message>
    <message>
        <location line="+46"/>
        <source>解析失败</source>
        <translation>Fail to analyze</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>notConvertRegs 不符合 toml 规范</source>
        <translation>notConvertRegs is nonconforming</translation>
    </message>
</context>
<context>
    <name>TLFCfgPage</name>
    <message>
        <location filename="TLFCfgPage.cpp" line="+24"/>
        <location line="+222"/>
        <source>换行修复设置</source>
        <translation>Linebreak fix settings</translation>
    </message>
    <message>
        <location line="-213"/>
        <source>优先标点</source>
        <translation>Prefer punctuations</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>保持位置</source>
        <translation>Keep positions</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>固定字数</source>
        <translation>Fixed char count</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>平均</source>
        <translation>Average</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>换行模式</source>
        <translation>Fix Mode</translation>
    </message>
    <message>
        <location line="+34"/>
        <source>分段字数阈值</source>
        <translation>Segment threshold</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>仅在固定字数模式有效</source>
        <translation>Only applied in &quot;Fixed char count&quot; mode</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>强制修复</source>
        <translation>Force to fix</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>报错阈值</source>
        <translation>Error threshold</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>单行字符数超过此阈值时报错</source>
        <translation>Flag lines exceeding this character count</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>分词器设置</source>
        <translation>Tokenizer settings</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>使用分词器</source>
        <translation>Use tokenizer</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>分词器后端</source>
        <translation>Tokenizer backend</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>应选择适合目标语言的后端/模型/字典</source>
        <translation>Should choose backend/model/dict fit to target language</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>MeCab词典目录</source>
        <translation>MeCab dict dir</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>MeCab中文词典需手动下载</source>
        <oldsource>MeCab词典需手动下载</oldsource>
        <translation>Download MeCab Chinese dictionary manually</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>选择MeCab词典目录</source>
        <translation>Choose MeCab dict dir</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>spaCy模型名称</source>
        <translation>spaCy model name</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>spaCy模型名称，新模型下载后需重启程序</source>
        <translation>spaCy model; restart after download</translation>
    </message>
    <message>
        <location line="-16"/>
        <location line="+26"/>
        <location line="+24"/>
        <source>浏览</source>
        <translation>Browse</translation>
    </message>
    <message>
        <location line="-183"/>
        <source>仅检查</source>
        <translation>Check only</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>优先阈值</source>
        <translation>Prefer threshold</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>仅在 优先标点 模式有效，值越高，换行的相对位置的可以变动以去匹配标点的限度就越大</source>
        <translation>Prefer punctuation only: higher values allow farther break moves</translation>
    </message>
    <message>
        <location line="+64"/>
        <source>可能可以获得更好的换行效果，其中 pkuseg 的安装需要电脑上有 MS C++ Build Tools</source>
        <oldsource>可能可以获得更好的换行效果，其中 pkuseg 需要电脑上有 MS C++ Build Tools</oldsource>
        <translation>May get better performance (installing pkuseg needs MS C++ Build Tools on your PC)</translation>
    </message>
    <message>
        <location line="+72"/>
        <location line="+24"/>
        <source>打开模型列表网页</source>
        <oldsource>浏览模型目录</oldsource>
        <translation>Browse model contents</translation>
    </message>
    <message>
        <location line="-12"/>
        <source>Stanza语言ID</source>
        <translation>Stanza lang id</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Stanza语言ID，新模型下载后需重启程序</source>
        <translation>Stanza language ID; restart after download</translation>
    </message>
</context>
<context>
    <name>TranslationWorkbenchPage</name>
    <message>
        <location filename="TranslationWorkbenchPage.cpp" line="+64"/>
        <source>翻译工作台</source>
        <translation>Translation Workbench</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>等待翻译任务</source>
        <translation>Waiting for translation task</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>清除筛选</source>
        <translation>Clear Filter</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>成功句流</source>
        <translation>Success Stream</translation>
    </message>
    <message>
        <location line="+26"/>
        <location line="+247"/>
        <source>最近错误</source>
        <translation>Recent Errors</translation>
    </message>
    <message>
        <location line="-242"/>
        <location line="+247"/>
        <source>文件进度</source>
        <translation>File Progress</translation>
    </message>
    <message>
        <location line="-18"/>
        <source>空闲</source>
        <translation>Idle</translation>
    </message>
    <message>
        <location line="+1"/>
        <source> · 当前文件: %1</source>
        <oldsource> · 当前文件: </oldsource>
        <translation> · Current file: %1</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>%1%2 · %3/%4 句 · %5 问题 · %6 成功事件 · %7 错误</source>
        <translation>%1%2 · %3/%4 lines · %5 problem(s) · %6 success event(s) · %7 error(s)</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>已筛选文件: %1</source>
        <oldsource>已筛选文件: </oldsource>
        <translation>Filtered files: %1</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>最近错误 (%1)</source>
        <translation>Recent Errors (%1)</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>文件进度 (%1)</source>
        <translation>File Progress (%1)</translation>
    </message>
</context>
<context>
    <name>TranslatorWorker</name>
    <message>
        <location filename="TranslatorWorker.cpp" line="+219"/>
        <source>[GalTransl++ 系统错误] %1</source>
        <oldsource>[系统错误] %1</oldsource>
        <translation>[GalTransl++ System] %1</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>[GalTransl++ 参数错误] %1</source>
        <oldsource>[参数错误] %1</oldsource>
        <translation>[GalTransl++ Arg] %1</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>[GalTransl++ 运行时错误] %1</source>
        <oldsource>[运行时错误] %1</oldsource>
        <translation>[GalTransl++ Runtime] %1</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>[GalTransl++ 标准错误] %1</source>
        <oldsource>[标准错误] %1</oldsource>
        <translation>[GalTransl++ Std] %1</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>[GalTransl++ 未知错误]</source>
        <oldsource>[未知错误]</oldsource>
        <translation>[GalTransl++ Unknown]</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>[GalTransl++ info] 翻译任务%1。</source>
        <oldsource>翻译任务%1。</oldsource>
        <translation>[GalTransl++ info] Translation task %1.</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>已正常停止</source>
        <translation>stopped normally</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>已正常完成</source>
        <translation>completed normally</translation>
    </message>
</context>
<context>
    <name>UpdateChecker</name>
    <message>
        <location filename="UpdateChecker.cpp" line="+437"/>
        <location line="+2"/>
        <source>更新检测失败</source>
        <translation>Failed to check update</translation>
    </message>
    <message>
        <location line="-284"/>
        <location line="+231"/>
        <source>网络连接失败，请检查网络设置。</source>
        <translation>Failed to connect to internet.</translation>
    </message>
    <message>
        <location line="-187"/>
        <source>获取更新信息失败。</source>
        <translation>Failed to get update information.</translation>
    </message>
    <message>
        <location line="-15"/>
        <source>检测到新版本</source>
        <translation>New version detected</translation>
    </message>
    <message>
        <location line="-117"/>
        <location line="+16"/>
        <source>请稍候</source>
        <translation>Please wait</translation>
    </message>
    <message>
        <location line="+0"/>
        <location line="+39"/>
        <source>正在检查更新...</source>
        <translation>Checking for update...</translation>
    </message>
    <message>
        <location line="+50"/>
        <source>当前已是最新版本</source>
        <translation>You are already on the latest version</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>最新版本: %1</source>
        <translation>Latest: %1</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>更新信息中缺少版本号。</source>
        <translation>Update info has no version.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>版本号解析失败。</source>
        <translation>Version parse failed</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>发布页中未找到 GUICORE.7z 更新包。</source>
        <translation>GUICORE.7z was not found on the release page.</translation>
    </message>
    <message>
        <location line="+71"/>
        <source>不兼容更新</source>
        <translation>Incompatible update</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>更新下载已完成</source>
        <translation>Download completed</translation>
    </message>
    <message>
        <location line="+114"/>
        <source>下载完成</source>
        <translation>Download completed</translation>
    </message>
    <message>
        <location line="-345"/>
        <location line="+345"/>
        <source>点击以关闭程序并安装更新</source>
        <translation>Click to exit app and apply update</translation>
    </message>
    <message>
        <location line="-237"/>
        <source>检测到新版本！</source>
        <translation>New version detected!</translation>
    </message>
    <message>
        <location line="+117"/>
        <source>最新版含有不兼容当前版本的内容，请确认 GitHub 发布页更新日志后再下载。</source>
        <translation>Latest release is incompatible. Check its GitHub changelog first.</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>无法创建更新包临时文件。</source>
        <translation>Failed to create temporary update package file.</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>下载更新</source>
        <translation>Downloading update</translation>
    </message>
    <message>
        <location line="-273"/>
        <location line="+273"/>
        <source>正在下载更新包...</source>
        <translation>Downloading update package...</translation>
    </message>
    <message>
        <location line="-267"/>
        <source>下载已完成</source>
        <translation>Download completed</translation>
    </message>
    <message>
        <location line="+265"/>
        <source>下载更新...</source>
        <translation>Downloading update...</translation>
    </message>
    <message>
        <location line="-164"/>
        <source>版本检测</source>
        <translation>Version check</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>当前已是最新的版本</source>
        <translation>Current version is the latest</translation>
    </message>
    <message>
        <location line="+173"/>
        <source>未知大小</source>
        <translation>Unknown size</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>下载更新... %1/%2</source>
        <translation>Downloading update... %1/%2</translation>
    </message>
    <message>
        <location line="+47"/>
        <source>更新包校验失败，请重新下载。</source>
        <translation>Update check failed; download again.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>无法保存更新包。</source>
        <translation>Failed to save update package.</translation>
    </message>
    <message>
        <location line="+45"/>
        <location line="+2"/>
        <source>更新下载失败</source>
        <translation>Download failed</translation>
    </message>
    <message>
        <location line="-43"/>
        <location line="+11"/>
        <source>更新下载成功</source>
        <translation>Download completed</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>将在程序关闭后自动安装更新</source>
        <translation>Update will be applied automatically after exiting</translation>
    </message>
</context>
</TS>
