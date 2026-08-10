# GalTranslPP 编译指南

## 1. 环境配置

在开始编译之前，请确保你的开发环境满足以下要求：

- **操作系统**: Windows 10 或 Windows 11
- **IDE**: [Visual Studio 2026](https://visualstudio.microsoft.com/zh-hans/downloads/)
  - **必需工作负载**: `使用 C++ 的桌面开发`
  - **必需工具集**: `MSVC v14.50` (v14.51/v14.52 目前有 bug 会构建失败，需要自行在单个组件中勾选 v14.50 版本的工具集)
- **辅助构建工具**: [CMake](https://cmake.org/download/)
- **版本控制工具**: [git](https://git-scm.com/)

![MSVC_1450](images/MSVC_1450.png?raw=true)

## 2. 安装核心依赖

### 2.1 vcpkg 包管理器

vcpkg 用于管理项目所需的 C++ 库。

```cmd
# 1. 克隆 vcpkg 仓库到任意位置
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg

# 2. 执行引导脚本进行安装
.\bootstrap-vcpkg.bat

# 3. 运行 vcpkg integreate install 来将 vcpkg 绑定到 Visual Studio
```

### 2.2 Qt 框架

- 1、  访问 [Qt 官方网站](https://www.qt.io/download-qt-installer-oss) 下载并运行 Qt 社区开源版本的在线安装器 (需要注册 Qt 账户)。
- 2、  在安装器的组件选择页面，确保勾选以下组件:
  - `Qt` → `Qt 6.11.1 (或更高，但不保证兼容性)` → `MSVC 2022 64-bit`

## 3. 获取项目源码

将 GalTranslPP 主仓库连同子模块依赖克隆至本地。

```cmd
git clone --recurse-submodules https://github.com/julixian/GalTranslPP.git
cd GalTranslPP
```

## 4. 编译依赖

### 4.1 配置 Visual Studio 与 Qt

- 1、  **安装 VS 插件**:
  - 启动 Visual Studio，在顶部菜单栏选择 `扩展` → `管理扩展`。
  - 搜索并安装 **"Qt Visual Studio Tools"** 插件。
  - 根据提示重启 Visual Studio 以完成安装。
- 2、  **关联 Qt 版本**:
  - 重启后，在菜单栏选择 `扩展` → `Qt VS Tools` → `Qt Versions`。
  - 点击 `Import`，并选择你安装的 Qt MSVC 目录 (例如: `D:\Qt\6.11.1\msvc2022_64`)，并将其设置为默认版本。

### 4.2 编译 ElaWidgetTools

- 1、  修改 `CMakeLists.txt` 中的 `QT_SDK_DIR` 为你安装的 Qt MSVC 目录。
- 2、  运行 `build.bat`。
- 3、  **确认编译产物**:
  - 确保 `3rdParty\ElaWidgetTools\Install\ElaWidgetTools\include` 文件夹存在，程序会用到里面的头文件
  - 确保 `3rdParty\ElaWidgetTools\Install\ElaWidgetTools\lib\ElaWidgetTools.lib` 文件存在
  - 确保 `3rdParty\ElaWidgetTools\Install\ElaWidgetTools\bin\ElaWidgetTools.dll` 文件存在

## 5. 编译 GalTranslPP (主项目)

- 1、  使用 Visual Studio 打开根目录下的 `GalTranslPP.slnx` 解决方案文件。

- 2、  将 配置从默认的 Debug 切换至 Release。

- 3、  在解决方案资源管理器中右键你想编译的项目，如 `GPPGUI`，点击生成，VS 会自动编译其它依赖并生成最终二进制文件。

## 6. 完成与运行

编译成功后，所有可执行文件将生成于 `Release\` 目录下。  

还需将一些文件复制到文件夹内程序才可正常运行。  

- 0、 先将项目根目录的 `Example\BaseConfig` 文件夹内的 `Python-3.12.10-embed-amd64.zip` 文件解压到同一文件夹下

### 6.1 GPPCLI

- 1、 运行项目根目录下的 `Release.bat`

### 6.2 GPPGUI

- 1、 运行项目根目录下的 `Release.bat`
- 2、  打开 Qt 专属控制台，如 Qt 6.11.1 (MSVC 2022 64-bit)，输入命令 

```cmd
windeployqt path/to/GalTranslPP_GUI.exe
```

至此所有步骤均已完成。
