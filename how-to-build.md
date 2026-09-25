# GalTranslPP 编译指南

## 1. 环境配置

在开始编译之前，请确保你的开发环境满足以下要求：

- **操作系统**: Windows 10 或 Windows 11，适配其它系统需要自行修改代码和构建脚本
- **版本控制工具**: [git](https://git-scm.com/)
- **主要构建工具**: [xlings](https://github.com/openxlings/xlings)、[mcpp](https://github.com/mcpp-community/mcpp)
- **辅助构建工具**: [CMake](https://cmake.org/download/)、[Python3](https://www.python.org/)、
[Visual Studio Build Tools](https://visualstudio.microsoft.com/zh-hans/downloads/#build-tools-for-visual-studio-2026) (理论上不需要下完整 IDE，只需确保选中 `使用 C++ 的桌面开发` 的工作负载即可)。
- **包管理工具**: [vcpkg](https://github.com/microsoft/vcpkg)

### 1.1 构建工具介绍

本项目使用 `mcpp` 作为构建系统，作为 2026 新兴的构建系统，其安装非常简单，仅需两步即可。

首先使用 powershell 安装作为高级包管理工具的 xlings，让 mcpp 可以方便的安装、管理。
```powershell
irm https://raw.githubusercontent.com/openxlings/xlings/main/tools/other/quick_install.ps1 | iex
```

重启 cmd/powershell 后使用 xlings 安装 mcpp。
```cmd
xlings install mcpp -y
```

如果你确实是通过 xlings 安装的 mcpp 的话，建议在安装完之后先运行
```cmd
mcpp self init
```
然后将 `.mcpp/config.toml` 中的 `[xlings.binary]` 从 `bundled` 改为 `system` 并将 `[xlings.home]` 改为 `.xlings` 所在目录。

### 1.2 vcpkg 包管理器

本项目大部分 C++ 依赖库使用 vcpkg 管理。

```cmd
# 1. 克隆 vcpkg 仓库到任意位置
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg

# 2. 执行引导脚本进行安装
.\bootstrap-vcpkg.bat

# 3. 将 vcpkg 目录加入 PATH 环境变量，或记住 vcpkg.exe 文件路径，将之后命令中的 vcpkg 替换为绝对路径
```

其它环境配置基本都比较简单或网上有很多教程，在此不再赘述。

## 2 Qt 框架

- 0、  如果你只想编译 CLI，可以忽略这一步
- 1、  访问 [Qt 官方网站](https://www.qt.io/download-qt-installer-oss) 下载并运行 Qt 社区开源版本的在线安装器 (需要注册 Qt 账户)。
- 2、  在安装器的组件选择页面，确保勾选以下组件:
  - `Qt` → `Qt 6.11.1 (或更高，但不保证兼容性)` → `MSVC 2022 64-bit`

## 3. 拉取项目源码

将 GalTranslPP 主仓库连同子模块依赖克隆至本地。

```cmd
git clone --recursive https://github.com/julixian/GalTranslPP.git
cd GalTranslPP
```

## 4. 编译依赖

### 4.1 编译 ElaWidgetTools

- 0、  如果你只想编译 CLI，可以忽略这一步
- 1、  修改 `build.py` 中的 `QT_ROOT` 为你安装的 Qt MSVC 目录。
- 2、  运行 `build.py`。
- 3、  **确认编译产物**:
  - 确保 `3rdParty\ElaWidgetTools\Install\ElaWidgetTools\include` 文件夹存在，程序会用到里面的头文件
  - 确保 `3rdParty\ElaWidgetTools\Install\ElaWidgetTools\lib\ElaWidgetTools.lib` 文件存在
  - 确保 `3rdParty\ElaWidgetTools\Install\ElaWidgetTools\bin\ElaWidgetTools.dll` 文件存在

### 4.2 编译 vcpkg 依赖包

- 1、  运行以下命令即可
```cmd
vcpkg install --triplet gpp-x64-windows-release
```

## 5. 编译主项目

- 1、  如果要编译 `GPPGUI`，则需在 `mcpp-build-scripts/qt-root.txt` 的第一行中再次写入你安装的 Qt MSVC 目录。

- 2、  使用 mcpp 构建 CLI/GUI。
```cmd
mcpp build -p GPPCLI
mcpp build -p GPPGUI
```

默认的 release profile 会使用最高优化等级，如果觉得构建速度太慢可以尝试使用 fast-release profile。
```cmd
mcpp build -p GPPCLI --profile fast-release
mcpp build -p GPPGUI --profile fast-release
```

## 6. 完成与运行

构建成功后，所有可执行文件将被部署于 `Release\` 目录下。  

还需将一些文件复制到文件夹内程序才可正常运行。  

- 0、 先将项目根目录的 `Example\BaseConfig` 文件夹内的 `Python-3.12.10-embed-amd64.zip` 文件解压到同一文件夹下
- 1、 运行项目根目录下的 `Release.py`
- 2、 打开 Qt 专属控制台，如 Qt 6.11.1 (MSVC 2022 64-bit)，输入命令 

```cmd
windeployqt path/to/GalTranslPP_CLI.exe
windeployqt path/to/GalTranslPP_GUI.exe
```

至此所有步骤均已完成。
