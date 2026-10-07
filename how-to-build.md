# GalTranslPP 编译指南

## 1. 环境配置

在开始编译之前，请确保你的开发环境满足以下要求：

- **操作系统**: Windows 10 或 Windows 11，适配其它系统需要自行修改代码和构建脚本
- **版本控制工具**: [git](https://git-scm.com/)
- **主要构建工具**: [xlings](https://github.com/openxlings/xlings)、[mcpp](https://github.com/mcpp-community/mcpp)
- **辅助构建工具**: [Visual Studio Build Tools](https://visualstudio.microsoft.com/zh-hans/downloads/#build-tools-for-visual-studio-2026)
(不需要下完整 IDE，需确保选中 `使用 C++ 的桌面开发` 的工作负载)、[Python3](https://www.python.org/)
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

### 1.2 vcpkg 包管理器

本项目大部分 C++ 依赖库使用 vcpkg 管理。

```cmd
# 1. 克隆 vcpkg 仓库到任意位置
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg

# 2. 执行引导脚本进行安装
.\bootstrap-vcpkg.bat
```

其它环境配置都较为简单或网上教程繁多，在此不再赘述。

## 2. Qt 框架

- 1、  访问 [Qt 官方网站](https://www.qt.io/download-qt-installer-oss) 下载并运行 Qt 社区开源版本的在线安装器 (需要注册 Qt 账户)。
- 2、  在安装器的组件选择页面，确保勾选以下组件:
  - `Qt` → `Qt 6.11.1 (或更高，但不保证兼容性)` → `MSVC 2022 64-bit`

## 3. 拉取项目源码

将 GalTranslPP 主仓库连同子模块依赖克隆至本地。

```cmd
git clone --recursive https://github.com/julixian/GalTranslPP.git
cd GalTranslPP
```

## 4. 为构建脚本配置本机工具路径

请完成 `mcpp-build-scripts\gpp-build\gpp-build.ixx` 头部的 `本机工具路径配置`。

## 5. 编译项目

使用 mcpp 构建 CLI/GUI。
```cmd
mcpp build -p GPPCLI
mcpp build -p GPPGUI
```

首次构建会先安装 vcpkg 依赖，所以耗时会比较长。

默认的 release profile 会使用最高优化等级，你也可以尝试使用 fast-release profile。
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
