# ProjectCachePage 页面流程

`ProjectCachePage` 是 GUI 中的缓存管理页面，用来查看缓存文件、跨文件搜索、聚合问题句、编辑单条缓存、删除文件或条目，以及在保存前批量替换缓存文本。

这个页面偏向“工作台”而不是普通设置页：左侧负责文件、搜索、问题聚合，右侧负责当前文件的条目概览和编辑入口。页面内部尽量使用 Ela 控件，列表内容则使用自绘 delegate，避免为大量缓存句子创建大量 QWidget。

## 文件拆分

- `ProjectCachePage.h` 声明页面状态、数据结构和私有方法，并按实现文件分组。
- `ProjectCachePage.cpp` 负责构造页面、搭建控件、连接信号。
- `ProjectCachePage_p.h` 保存拆分文件共享的 model role、字号常量和辅助函数声明。
- `ProjectCachePageDelegates.cpp` 自绘右侧缓存条目卡片和左侧搜索结果卡片。
- `ProjectCachePageFiles.cpp` 负责发现、排序、加载和写回缓存文件。
- `ProjectCachePageEntries.cpp` 负责渲染当前文件条目，并管理单条目的编辑窗口。
- `ProjectCachePageSearch.cpp` 负责全局搜索、问题聚合、跳转搜索结果和批量替换。
- `ProjectCachePageActions.cpp` 放置脏文件记录、按钮状态、主题样式、运行锁定、提示和确认弹窗逻辑。

## 创建与懒加载

构造函数只调用 `_setupUI()`，不会立刻扫描缓存目录。这样项目页创建和添加项目时不再因为缓存管理页解析大量 JSON 而明显卡顿。

缓存文件第一次加载发生在用户进入“缓存管理”页面时：

- `ProjectSettingsPage` 的“缓存管理”菜单被触发后，会调用 `ensureCacheFilesLoaded()`。
- `ensureCacheFilesLoaded()` 只在 `_cacheFilesLoaded == false` 时调用 `_loadCacheFiles()`。
- 页面右上角的“刷新”图标和公开方法 `refreshCacheFiles()` 会直接调用 `_loadCacheFiles()`，用于主动重新扫描磁盘状态。

`_setupUI()` 创建一个紧凑的双栏工作区：

- 顶部右侧图标按钮依次用于打开缓存文件夹、刷新文件列表、保存当前文件、保存全部脏文件。
- 左侧栏包含“文件”“搜索”“问题”三个分段导航按钮。
- 搜索页默认收起批量替换区域，避免占用搜索结果列表高度。
- 右侧区域显示当前文件名、单行统计摘要、本文件过滤控件、编辑/删除按钮和条目概览列表。

页面监听 `ElaTheme::themeModeChanged`。主题切换时会重新应用辅助文本颜色、分隔条样式和导航按钮颜色，并刷新右侧条目绘制。

## 缓存文件加载

`_loadCacheFiles()` 递归扫描 `projectDir / transCacheDirName` 目录下的 JSON 文件。缓存目录不存在时会尝试创建。

每个文件会记录：

- 相对路径。
- 文件大小。
- 修改时间。
- 句子数。
- 问题句数。
- 解析状态和解析错误。

统计句数和问题数时会读取 JSON 数组。解析失败不会打断文件列表刷新，文件项会显示“解析失败”，错误内容保存在 tooltip 中。用户尝试打开该文件时会用 `ElaMessageBar` 提示错误。

文件排序使用开启数字模式的 `QCollator`，并忽略大小写，尽量贴近 Windows 资源管理器的自然排序。因此 `1.json`、`2.json`、`10.json` 会按数字顺序显示，而不是 `1.json`、`10.json`、`2.json`。

刷新时的内存策略：

- `_cacheFiles` 每次都会按磁盘重新生成，所以已经从磁盘消失的问题或文件不会继续参与搜索和问题聚合。
- 未标记为脏的文件会丢弃旧的内存副本，下次读取以磁盘为准。
- 脏文件的内存副本会保留，避免用户尚未保存的修改在刷新或切换文件时丢失。
- 如果当前文件仍存在，会重新加载当前文件；当前文件是脏文件时优先保留内存副本。
- 如果当前文件已经不存在，会清空当前选择和右侧条目。

## 文件列表与当前文件

`_renderFileList()` 根据 `_cacheFiles` 重建左侧文件列表。每个文件项显示文件名和摘要：

- 正常文件显示“句数 · 问题数”。
- 解析失败的文件显示“解析失败”。
- 脏文件名前会加 `*`。

点击文件项会调用 `_loadCacheFile()`：

- 如果文件已在 `_loadedEntriesByFile` 中，且没有强制重载，会优先使用内存副本。
- 否则从磁盘读取 UTF-8 JSON 数组。
- 加载成功后刷新右侧条目列表、当前文件标题、统计摘要和按钮状态。

选中文件时不会弹“加载完成”提示，也不会留下常驻状态文字。只有读取或解析失败时才提示错误。

## 条目渲染与编辑

`_renderEntries()` 根据当前 `_entries` 重建右侧可见模型。本文件搜索框和“只看问题句”只影响可见列表，不改变底层 JSON 数组顺序。

每个可见 item 都通过 `JsonRowRole` 保存对应的 JSON 行号。自绘的 `CacheEntryDelegate` 使用 role 数据绘制紧凑卡片，内容包括：

- 句子序号。
- 说话人或名称预览。
- 问题文本。
- 模型名或翻译器名。
- 原文 `pre_processed_text`。
- 译文 `pre_translated_text`。

右侧条目列表的 item 高度在 `ProjectCachePage.cpp` 中通过 `_entryList->setItemHeight(112)` 设置；卡片内部布局在 `ProjectCachePageDelegates.cpp` 的 `CacheEntryDelegate` 中绘制，两边需要一起调整。

双击条目或点击“编辑选中条目”会打开 `_openEntryEditor()`。编辑窗口中：

- `original_text` 作为元信息只读显示。
- `pre_processed_text` 是原文，可编辑。
- `pre_translated_text` 是译文，可编辑。
- `problems` 只读。
- `translated_preview` 只读。

保存编辑窗口时，`_updateEntryField()` 只写回可编辑字段。如果内容确实变化，会更新 `_entries`、标记当前文件为脏文件、刷新对应列表项和右上角摘要。数据只有在用户点击保存后才写回磁盘。

## 搜索

全局搜索由 `_runGlobalSearch()` 实现。搜索会优先读取脏文件对应的内存副本，因此未保存的修改也能被搜到；其它文件则从磁盘读取。

搜索范围可以选择：

- 全部。
- 原文 `pre_processed_text`。
- 译文 `pre_translated_text`。
- 问题 `problems`。

搜索结果上限是 2000 条，用来避免大项目中输入时过度卡顿。每个命中项保存文件名、JSON 行号、句子序号、命中字段、原文预览、译文预览和问题预览。

`CacheSearchDelegate` 按接近 GalTransl 的顺序绘制搜索结果：

- 红色或蓝色标签，加文件名。
- `#index` 加问题文本或匹配字段。
- 原文预览。
- 译文预览。

左侧搜索结果列表的最小高度在 `ProjectCachePage.cpp` 中通过 `_searchResultList->setMinimumHeight(520)` 设置；每个搜索结果 item 高度通过 `_searchResultList->setItemHeight(120)` 设置；卡片内部布局在 `ProjectCachePageDelegates.cpp` 的 `CacheSearchDelegate` 中绘制。

点击搜索结果会加载对应文件，并在右侧条目列表中选中对应 JSON 行。跳转时会清空右侧本文件搜索框，并关闭“只看问题句”，确保目标条目可见。

## 问题聚合

问题页由 `_loadProblems()` 聚合。它只读取 GalTranslPP 的 `problems` 数组，不读取 GalTransl 旧格式中的单个 `problem` 字段。

问题聚合是按需执行的：

- 首次切换到“问题”页时，如果 `_problemsLoaded == false`，会自动调用 `_loadProblems()`。
- 问题页上的“刷新问题”按钮只重新聚合问题列表，不重新扫描整个缓存文件列表。
- 顶部“刷新”图标会重新扫描缓存文件；如果当前停留在问题页，会在扫描后重新聚合问题。

问题列表按出现次数降序排列，次数相同则按文本排序。点击某个问题会切换到搜索页，把搜索范围设为 `problems`，并用该问题文本执行搜索。

## 两个刷新入口的区别

页面目前有两个刷新入口，职责不同：

- 顶部“刷新”图标：重新扫描磁盘缓存文件、更新文件列表、重新统计每个文件的句数和问题数；如果当前有搜索词或正在问题页，会同步刷新搜索或问题聚合。
- 问题页“刷新问题”：只基于当前文件列表重新聚合问题，适合用户刚刚编辑、删除或批量替换后更新问题分类。

这样做是为了避免每次只想刷新问题时都重新扫描整个缓存目录。

## 批量替换

批量替换区域默认收起，因为它会占用左侧栏高度。预览和执行替换都复用 `_collectReplaceDetails()`。

替换遵循翻译器实际使用的缓存字段：

- `src` 只匹配并修改 `pre_processed_text`。
- `dst` 只匹配并修改 `pre_translated_text`。
- `all` 同时作用于上述两个字段。

`original_text` 是元信息，永远不会被批量替换修改。

执行替换前会使用 `ElaContentDialog` 确认。确认后只修改受影响文件的内存 JSON，并把这些文件标记为脏文件，然后刷新文件列表、当前条目列表和搜索结果。数据只有在保存或保存全部后才会真正写回磁盘。

## 保存、删除与运行锁定

页面在执行破坏性操作或写操作前会检查 `GUIConfig.isRunning`。项目运行中时，编辑、删除、替换和保存都会被阻止；浏览缓存和以只读方式打开编辑窗口仍然可用。

保存当前文件时，会把当前 `_entries` 以两空格缩进写回 JSON。保存全部时，会遍历 `_dirtyFiles` 中仍有内存副本的文件。保存成功后会刷新文件列表和统计摘要。

删除缓存文件会从磁盘删除选中文件，并移除对应的内存副本和脏文件标记。如果删除的是当前文件，会清空右侧条目。删除缓存条目只修改当前内存 JSON，保存后才落盘。

确认弹窗使用 `ElaContentDialog`，保持和其它 Ela 设置页一致。成功、警告和错误反馈使用 `ElaMessageBar`，页面不保留常驻的绿色或红色状态文字，避免占用布局高度。

## 关键字段约束

- 原文显示、搜索和编辑使用 `pre_processed_text`。
- 译文显示、搜索和编辑使用 `pre_translated_text`。
- `original_text` 是元信息，只读，不参与批量替换。
- 问题来源只使用 GalTranslPP 的 `problems` 数组。
- 说话人优先使用 `name_preview`，其次使用 `name` 或 `names`。
- 模型名或翻译器名使用 `translated_by`。

## 调整 UI 时需要同步的位置

- 右侧条目 item 高度：`ProjectCachePage.cpp` 中的 `_entryList->setItemHeight(112)`。
- 右侧条目卡片布局：`ProjectCachePageDelegates.cpp` 中的 `CacheEntryDelegate`。
- 搜索结果 item 高度：`ProjectCachePage.cpp` 中的 `_searchResultList->setItemHeight(120)`。
- 搜索结果列表最小高度：`ProjectCachePage.cpp` 中的 `_searchResultList->setMinimumHeight(520)`。
- 搜索结果卡片布局：`ProjectCachePageDelegates.cpp` 中的 `CacheSearchDelegate`。
- 左侧栏宽度：`ProjectCachePage.cpp` 中 `sidebarWidget` 的最小和最大宽度。
- 主分隔条样式：`ProjectCachePage_p.h` / `ProjectCachePageDelegates.cpp` 相关辅助样式函数中的 `splitterStyle()`。
- model role 常量：`ProjectCachePage_p.h`。这些 role 是模型填充和 delegate 绘制之间的共享契约，修改时需要两边一起检查。
