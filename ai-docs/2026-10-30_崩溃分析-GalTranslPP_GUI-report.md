# GalTranslPP GUI 2026-10-30 崩溃分析

直接原因已确认：Qt 6.12.0 的惯性滚动路径取得了空的屏幕对象，仍调用 `QScreen::physicalDotsPerInchY()`，导致空指针访问。屏幕对象为何为空、具体操作了哪个页面，尚不能由这份小转储确定。

## 范围与输入

分析范围见 [scope.md](../target/crash-analysis/20261030/scope.md)。用户授权分析以下本地文件，未上传转储：

- `C:\Users\julixian\Downloads\GalTranslPP_GUI_20261030_231138_43348.dmp`，393,038 字节。
- `C:\Users\julixian\Downloads\GalTranslPP_GUI.pdb`，70,791,168 字节。
- 调试器：Windows SDK CDB 10.0.26100.6584 x64。
- 转储时间：2026-10-30 23:11:38，Asia/Shanghai。
- 进程运行时间：620,811 秒，即 7 天 4 小时 26 分 51 秒。
- 主程序编译时间：2026-10-20 01:20:51；提供的 PDB 被调试器正常接受为匹配的私有符号。

## Evidence：调试证据

各证据的 `repro_command` 均为在项目根目录运行 `pwsh -NoProfile -File target/crash-analysis/20261030/reproduce.ps1`，需保持脚本内指定的本地输入文件和匹配 Qt 安装可用。`content_hash` 为 n/a；原始调试文本保存在以下链接中。

| E-id | source_ref | 观察 |
|---|---|---|
| E-001 | [analysis.txt](../target/crash-analysis/20261030/analysis.txt) | `NULL_CLASS_PTR_READ_c0000005_Qt6Gui.dll!QScreen::physicalDotsPerInchY`；读取地址 `0x8`，RCX 为 0；崩溃线程为主线程。 |
| E-002 | [scroller.txt](../target/crash-analysis/20261030/scroller.txt) | `Qt6Widgets+0x3e8519` 调用 `QWidget::screen()`；返回值直接作为 QScreen 指针传给 DPI 函数，没有空指针分支；异常返回地址为 `Qt6Widgets+0x3e852a`。 |
| E-003 | [stack.txt](../target/crash-analysis/20261030/stack.txt) | 调用链从鼠标事件经过 `QScroller::handleInput()` 进入 DPI 读取；主程序在 `main` 的事件循环中。 |
| E-004 | [modules.txt](../target/crash-analysis/20261030/modules.txt)、[scroller.txt](../target/crash-analysis/20261030/scroller.txt) | Qt6Gui.dll 为 6.12.0，时间戳 `6AB74F82`、ImageSize `00951000`、CheckSum `00945676`；本机加载的 Qt DLL 与转储这些字段相符。主程序 PDB 正常加载。 |

Qt 没有加载私有 PDB，因此调试器对非导出内部函数显示的是最近导出符号加偏移，例如 `QScroller::pixelPerMeter+0x62a`。不能把这些标签全部当成真实内部函数名。根据调用指令和源码逻辑，该区域对应滚动准备阶段的 DPI 更新。

## Findings：结论

### F-001：无屏幕对象时，Qt 滚动准备代码继续读取 DPI

- severity：n/a_re
- evidence_ids：E-001、E-002、E-003、E-004
- confidence：high
- location：`Qt6Gui+0xe2fde`；`D:\Qt\6.12.0\Src\qtbase\src\widgets\util\qscroller.cpp:987`。
- status：validated（转储中的直接崩溃机制；未进行运行时复现）。

本机对应源码：

```cpp
void QScrollerPrivate::setDpiFromWidget(QWidget *widget)
{
    const QScreen *screen = widget ? widget->screen() : QGuiApplication::primaryScreen();
    Q_ASSERT(screen);
    setDpi(QPointF(screen->physicalDotsPerInchX(), screen->physicalDotsPerInchY()));
}
```

发布版的断言不能提供运行时空指针保护。现场正在执行 `physicalDotsPerInchY()`，其 `this` 为 0，第一处访问对象内部数据的指令 `mov rcx, [rcx+8]` 即引发异常。

`QWidget::screen()` 源码（`qwidget.cpp:2496`）会尝试关联屏幕、初始屏幕、按位置查找屏幕，最后回退到主屏幕。`QGuiApplication::primaryScreen()` 在 Qt 屏幕列表为空时可返回空指针。转储未包含屏幕列表及目标控件的堆内存，不能确认具体是哪种状态导致取屏幕失败，也不能据此认定发生了显示器断开、远程连接切换或内存破坏。

### F-002：Ela 的鼠标拖动滚动会进入该 Qt 路径

- severity：n/a_re
- evidence_ids：E-002、E-003
- confidence：high（路径类型）；low（具体页面）。
- location：`ElaScrollArea.cpp:30`、`ElaBreadcrumbBar.cpp:54`、`ElaPivot.cpp:41`、`Development/ElaNavigationView.cpp:48`，均位于 `3rdParty/ElaWidgetTools/ElaWidgetTools`。
- status：validated（代码中注册该手势；目标控件身份未确认）。

这些控件调用 `QScroller::grabGesture(..., QScroller::LeftMouseButtonGesture)`，普通鼠标按下也可触发手势识别，并不要求用户进行长距离拖动。GPP 首页还在 `GPPGUI/HomePage.cpp:133` 显式开启了卡片区的鼠标手势，但这不证明本次崩溃发生于首页。

## Path：调用链

P-001，`path_type=callflow`，证据 E-001～E-003：

1. GUI 主线程接收鼠标事件。
2. Qt 手势识别调用 `QScroller::handleInput()`。
3. 滚动准备过程查询目标 QWidget 的屏幕，以更新滚动使用的 DPI。
4. 屏幕指针为空，代码未执行发布版判空处理。
5. `QScreen::physicalDotsPerInchY()` 读取 `this + 8`，发生 `0xC0000005`。

## 修复建议与限制

1. 从根源修复 Qt：在 `QScrollerPrivate::setDpiFromWidget()` 中先尝试控件屏幕，必要时回退主屏幕；仍无屏幕时保留已有 DPI 并返回，而不是继续解引用。初始化也经过此函数，需核对初始 DPI 默认值后再决定是否提供默认值。该方案需要重建和部署匹配的 QtWidgets.dll。
2. 若暂不重建 Qt，可在应用层评估屏幕不可用期间阻止该手势进入的方案，或关闭相关控件的鼠标拖动滚动作为绕过。关闭手势会改变交互，应覆盖所有注册入口，不能只关闭首页后声称问题已完全修复。
3. 建议向用户核对闪退前是否涉及远程桌面断开/重连、显示器或显示配置变化。这些只是排查线索，不是已确认的触发条件。
4. 若需定位具体控件和屏幕状态，需要包含堆内存的完整转储，或运行时复现；本次约 384 KiB 的小转储未保存所需对象数据。

本次没有修改应用或第三方功能代码，也没有以关闭某项交互来替代原因定位。

## Timeline：分析过程

1. 读取异常现场：确认主线程访问地址 `0x8`。
2. 验证主程序 PDB；初始缺少 Qt 映像时调用栈无法正确展开。
3. 配入本机匹配 Qt 6.12.0 DLL：还原鼠标手势到 QScroller 的调用链，排除仅凭初始残缺栈推测的绘制入口。
4. 对照汇编和 Qt 源码：确认空 QScreen 被传入 DPI 读取函数。
5. 检索 Ela 和 GUI 手势注册入口，记录修复方向及小转储的证据边界。
