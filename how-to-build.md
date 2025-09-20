# GalTransl++ 编译指南

## 1. 环境要求

- Windows 10/11
- [Visual Studio 2026（工具集v143+v145）](https://visualstudio.microsoft.com/insiders/?rwnlp=zh-hans)
- [git](https://git-scm.com/)

## 2. 安装vcpkg

```cmd
git clone https://github.com/microsoft/vcpkg
cd vcpkg
.\bootstrap-vcpkg.bat
```

然后将刚才克隆的vcpkg的路径添加到环境变量Path中

## 3. 安装Qt（编译GPPGUI和ElaWidgetTools时需要）

- 1、 下载Qt的社区开源版本(LGPL协议)的在线安装器(需注册登录): [https://www.qt.io/download-qt-installer-oss](https://www.qt.io/download-qt-installer-oss)
- 2、 选择安装组件:
  - `Qt 6.9.2` → `MSVC 2022 64-bit`

## 4. 克隆GalTranslPP项目

```cmd
git clone https://github.com/julixian/GalTranslPP.git
cd GalTranslPP
```

## 5. ElaWidgetTools（GPPGUI的依赖项）

### 5.1 克隆ElaWidgetTools项目

```cmd
git clone https://github.com/Liniyous/ElaWidgetTools.git
```

### 5.2 用Visual Studio 2026打开ElaWidgetTools项目

可以打开文件夹也可以打开CMakeLists.txt

### 5.3 安装`Qt Visual Studio Tools`插件

VS选项卡中`扩展` → `管理扩展` → 搜索"Qt Visual Studio Tools"  
点击安装后过一会VS会在上方提示你关闭VS完成插件安装，之后重新用  
Visual Studio 2026打开ElaWidgetTools项目

### 5.4 配置Qt路径

- 1、 在VS选项卡中: `扩展` → `Qt VS Tools` → `Qt Versions`
- 2、 添加Qt安装路径，如: `C:\Qt\6.9.2\msvc2022_64`，并设置为默认版本

### 5.5 编译ElaWidgetTools

在VS工具栏的第二个下拉选项栏把生成目标从`Qt-Debug`改为`Qt-Release`，  
然后在选项卡的`生成`中点击`全部生成`即开始编译。

### 5.6 将编译产物移动到GalTranslPP项目

ElaWidgetTools的编译产物在ElaWidgetTools根目录下的out\build\release\ElaWidgetTools内，  
将其中的ElaWidgetTools.lib和ElaWidgetTools.exp移动到GalTranslPP根目录的lib文件夹(没有则自行创建)，
待会编译完GPPGUI后把ElaWidgetTools.dll复制到Release\GPPGUI

## 6. 编译GalTranslPP项目

用Visual Studio 2026打开`GalTranslPP.sln`  

如[README.md](README.md)所示：  

> **其它注意事项**
>
>由于我的开发环境基本绑定 windows系统，我自己也没有linux设备，所以即使在项目中使用的winapi数量很少也很好替换，  
>但是跨平台的事我自己是不会主动考虑的。  
>另外由于我所使用的环境较新，也可能会有一些比较罕见的问题。  
>目前已知项目依赖 `mecab:x64-windows` 在VS2026(工具集 v145)下不过编，但是VS2022(工具集 v143)能过，安装依赖可能需要切回VS2022  

故目前编译项目依赖需要在三个模块的属性中临时切换工具集为v143，  
然后在选项卡的`生成`-`批生成`中任意编译一个模块比如GPPGUI，  
依赖编译完成后进入正式编译时会乱码报错，  
此时将工具集切换回v145即可正常编译。
