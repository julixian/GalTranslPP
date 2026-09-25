# GalTranslPP 编译指南（mcpp）

本文对应当前的 mcpp 构建分支。以下命令在 Windows PowerShell 中从仓库根目录执行，除特别说明外均使用 x64。

## 1. 准备环境

- Windows 10/11，Visual Studio 2026 或 Build Tools，安装“使用 C++ 的桌面开发”工作负载和 Windows SDK。mcpp 使用自己的 LLVM/Clang 22.1.8 编译项目，但仍需要 Visual Studio 提供的 MSVC STL、运行库和 Windows SDK。
- Git、Python 3、CMake，以及可在终端执行的 `mcpp`。mcpp 会按根目录 `mcpp.toml` 的 `[toolchain]` 选择 `llvm@22.1.8`；先确保该版本在本机可用。
- [vcpkg](https://github.com/microsoft/vcpkg)。项目使用 manifest 模式，不需要 `vcpkg integrate install`。
- [Qt](https://www.qt.io/download) 6.11.1 的 **MSVC 2022 64-bit** 组件。其他版本尚未在本构建配置中验证。

**现阶段尽量不要在开发路径中使用非 ASCII 字符。** 仓库、vcpkg、Qt 的安装位置，以及它们的父目录和 Windows 用户目录，建议使用例如 `D:\dev\GalTranslPP`、`D:\tools\vcpkg` 这样的路径。mcpp 当前在 Windows 上的一些路径处理仍受系统 ANSI 代码页影响；路径含无法由当前代码页表示的字符时，配置或构建可能失败。这个限制是当前工具链的兼容性建议，并非项目源码要求只能使用英文。

## 2. 获取源码和安装 vcpkg 依赖

在一个只含 ASCII 字符的开发目录中获取当前 mcpp 分支及子模块：

```powershell
git clone --branch codex/mcpp-sync-main --recurse-submodules https://github.com/julixian/GalTranslPP.git D:\dev\GalTranslPP
Set-Location D:\dev\GalTranslPP
```

如果已经克隆了仓库，切换到该分支后执行 `git submodule update --init --recursive`。

单独安装 vcpkg，然后在**本项目根目录**运行 manifest 安装：

```powershell
git clone https://github.com/microsoft/vcpkg.git D:\tools\vcpkg
& D:\tools\vcpkg\bootstrap-vcpkg.bat
& D:\tools\vcpkg\vcpkg.exe install --triplet gpp-x64-windows-release
```

依赖由根目录的 `vcpkg.json`、`vcpkg-configuration.json`、`ports/` 和 `triplets/gpp-x64-windows-release.cmake` 决定。安装结果在本项目的 `vcpkg_installed/gpp-x64-windows-release/`，无需手工填写全局 vcpkg 路径。自定义的 MeCab 和 proxy 补丁也由项目 overlay port 应用。

## 3. 配置 Qt 并构建 ElaWidgetTools

将实际 Qt 安装目录分别写到以下两处：

1. 根目录 `mcpp-build-scripts/qt-root.txt`：只写一行 Qt 根目录，例如 `D:/Qt/6.11.1/msvc2022_64`。mcpp 的 Qt 头文件、库和工具从这里读取。
2. `3rdParty/ElaWidgetTools/build.py` 顶部的 `QT_ROOT`：设为同一个目录。ElaWidgetTools 自己的 CMake 构建从这里读取。

从仓库根目录构建并安装 ElaWidgetTools：

```powershell
python 3rdParty/ElaWidgetTools/build.py
```

脚本默认同时构建 ElaWidgetTools 示例；不需要示例时可加 `--no-example`。CMake 自行选择本机可用的 Visual Studio 生成器和工具集。完成后检查 `3rdParty/ElaWidgetTools/Install/ElaWidgetTools/` 下的 `include/`、`lib/` 和 `bin/`。这是 GUI 的构建输入，mcpp 不会替你运行此脚本。

## 4. 构建项目

在仓库根目录执行：

```powershell
mcpp build -p GPPGUI --profile release
```

根目录 `mcpp.toml` 将 `GalTranslPP`（核心库）、`GPPVersion`、`GPPCLI`、`GPPGUI` 和 `Updater` 声明为 workspace members。`-p` 指定要构建的成员；mcpp 会先构建它声明的依赖。GUI 的 `gpp.updater` 是构建依赖，所以构建 GUI 也会构建 Updater，并把其程序复制到 GUI 对应的发布目录。

五个成员也可以分别构建，例如：

```powershell
mcpp build -p GalTranslPP --profile release
mcpp build -p GPPVersion --profile release
mcpp build -p GPPCLI --profile release
mcpp build -p Updater --profile release
```

`release` 是各成员的默认配置，省略 `--profile release` 也可以。开发迭代时可用 `--profile fast-release`：关闭高级优化和 LTO，但 CLI、GUI 及库仍生成调试信息；Updater 在这两种配置下均不生成 PDB。两种配置都会发布到相同的 `Release/` 目录，因此切换配置后请留意已有文件。

各成员的 `build.mcpp` 会安排 Qt 翻译文件更新与生成、GUI 的 moc/rcc、构建产物和所需运行时 DLL 的复制。DLL 由程序实际导入关系递归确定；**Qt DLL 和插件不在自动复制范围内**。

## 5. 补齐运行数据并运行

首次打包前，把 `Example/BaseConfig/Python-3.12.10-embed-amd64.zip` 解压到同目录下名为 `Python-3.12.10-embed-amd64` 的文件夹，然后运行：

```powershell
Expand-Archive Example/BaseConfig/Python-3.12.10-embed-amd64.zip Example/BaseConfig/Python-3.12.10-embed-amd64
python Release.py
```

`Release.py` 复制基础配置、OpenCC 数据、`7z.dll` 和 CLI 示例项目。它不负责构建，也不会复制 Qt 运行库。主要程序目录是 `Release/GPPCLI/`、`Release/GPPGUI/`；GUI 还会发布到 `Release/GUICORE/`。若预先创建了相应的 `Release/*_PRIVATE/` 目录，构建脚本也会复制对应程序及运行文件。

GUI 运行前，再使用**同一套 Qt** 的 `windeployqt.exe` 部署 Qt DLL、插件等文件，例如：

```powershell
& D:\Qt\6.11.1\msvc2022_64\bin\windeployqt.exe Release\GPPGUI\GalTranslPP_GUI.exe
```

若需要运行 `Release/GUICORE/` 或其他独立目录中的 GUI，也分别对其中的可执行文件运行 `windeployqt`。CLI 如因 QtCore 缺失而无法启动，也对 `Release/GPPCLI/GalTranslPP_CLI.exe` 执行相同工具。
