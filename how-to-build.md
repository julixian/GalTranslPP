# GalTranslPP 编译指南

## 1. 环境配置

在开始编译之前，请确保你的开发环境满足以下要求：

- **操作系统**: Windows 10 或 Windows 11，适配其它系统需要自行修改代码和构建脚本
- **版本控制工具**: [git](https://git-scm.com/)
- **主要构建工具**: [xlings](https://github.com/openxlings/xlings)、[mcpp](https://github.com/mcpp-community/mcpp)
- **辅助构建工具**: [Visual Studio Build Tools](https://visualstudio.microsoft.com/zh-hans/downloads/#build-tools-for-visual-studio-2026) (不需要下完整 IDE，需确保选中 `使用 C++ 的桌面开发` 的工作负载)，vcpkg 依赖与 ElaWidgetTools 用它编译。

其余依赖由构建自动准备，无需手动安装、解压或配置路径：Qt 6.11.1、vcpkg 及 `vcpkg.json` 中的库（含 OpenCC 数据）、CMake、ElaWidgetTools、7z.dll，以及从 `Example\BaseConfig` 中的压缩包解出的嵌入式 Python。

### 1.1 构建工具介绍

本项目使用 `mcpp` 作为构建系统，作为 2026 新兴的构建系统，其安装非常简单，仅需两步即可。

首先使用 powershell 安装作为高级包管理工具的 xlings，让 mcpp 可以方便的安装、管理。
```powershell
irm https://raw.githubusercontent.com/openxlings/xlings/main/tools/other/quick_install.ps1 | iex
```

重启 cmd/powershell 后，在项目根目录（见第 2 节）使用 xlings 安装 mcpp，版本由 `.xlings.json` 指定。
```cmd
xlings install
```

## 2. 拉取项目源码

将 GalTranslPP 主仓库连同子模块依赖克隆至本地。

```cmd
git clone --recursive https://github.com/julixian/GalTranslPP.git
cd GalTranslPP
```

## 3. 编译项目

```cmd
mcpp build --workspace
```

首次构建会下载 Qt、安装 vcpkg 依赖并编译 ElaWidgetTools，所以耗时会比较长；之后的构建复用这些结果。

默认的 release profile 会使用最高优化等级，你也可以尝试使用 fast-release profile。
```cmd
mcpp build --workspace --profile fast-release
```

## 4. 运行

```cmd
mcpp run -p GPPGUI
mcpp run -p GPPCLI
```

运行所需的文件已由构建放在程序旁，无需 `windeployqt`，也无需复制或解压文件：Qt 运行库、插件与翻译，vcpkg、ElaWidgetTools 与 Python 的 DLL，7z.dll、Updater、`BaseConfig` 与 `SampleProject`。

## 5. 打包

在仓库根目录执行（在成员目录下执行 `mcpp pack --format release` 结果相同）：

```cmd
mcpp pack -p GPPCLI --format release
mcpp pack -p GPPGUI --format release
```

结果写入 `Release\`，布局与此前相同（`mcpp build` 与 `mcpp run` 不写 `Release\`）：
- `Release\GPPCLI`：CLI 及其运行所需的全部文件，含 `BaseConfig` 与 `SampleProject`。
- `Release\GPPGUI`：GUI 及其运行所需的全部文件，含 `Updater.exe` 与 `BaseConfig`。
- `Release\GUICORE`：GUI 的更新包。不含 `GlobalConfig.toml`、`mecab` 与 Python，含 `opencc`，Updater 为 `Updater_new.exe`。
- `Release\.pdb`：程序的 PDB。

发布目录默认不含 MSVC C++ 运行库（`vcruntime140.dll`、`msvcp140.dll`）：根目录 `mcpp.toml` 声明 `cxx_runtime = "host-coupled"`，由运行的机器提供，安装了 Visual Studio 或 [VC++ 运行库](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist)（x64）即可。

> 注：以下均可选，不配置即用默认。
> - mcpp：版本由 `.xlings.json` 指定，在项目目录内执行 `xlings install mcpp -y` 同样安装该版本。
> - 工具链：根目录 `mcpp.toml` 的 `[toolchain]`；mcpp:plugins 的版本：`mcpp-build-scripts/gpp-build/mcpp.toml`。
> - Qt：默认为根目录 `mcpp.toml` 声明的 `xim:qt-base`。改用本机 Qt（MSVC 2022 64-bit）：注释该行，设置环境变量 `QT_ROOT_DIR` 或 `gpp-build.ixx` 的 `qt_root`。
> - vcpkg、CMake：默认由 xlings 提供。改用本机版本：设置 `gpp-build.ixx` 的 `vcpkg_root`、`cmake_executable`。vcpkg 的缓存目录见 `VCPKG_DOWNLOADS`、`VCPKG_DEFAULT_BINARY_CACHE`。
> - ElaWidgetTools 的编译结果：缓存在 `%LOCALAPPDATA%\mcpp-plugins\deps-cmake`，源码与编译器不变时直接复用；环境变量 `MCPP_DEPS_CMAKE_CACHE` 可指定其它目录，设为 `off` 则不缓存。
> - MSVC C++ 运行库随程序发布：把根目录 `mcpp.toml` 的 `cxx_runtime` 改为 `"toolchain-coupled"`，构建与打包会把编译器自带的运行库放到程序旁。
> - 额外发布目录：写在 `Release\GPPCLI_PRIVATE.txt` 或 `Release\GPPGUI_PRIVATE.txt` 的第一行，打包时同时写入该目录（不含 `BaseConfig` 与 `SampleProject`）。
