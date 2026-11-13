"""在 Apple Silicon Mac 上构建应用，并组装便携压缩包和 DMG。"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import plistlib
import re
import shutil
import subprocess
import sys
import tarfile
import tomllib
import urllib.request
from pathlib import Path

kScriptRoot = Path(__file__).resolve().parents[2]
kToolsDir = kScriptRoot / "target/macos-tools"
kLogsDir = kScriptRoot / "target/macos-logs"
kConfig = tomllib.loads(Path(__file__).with_suffix(".toml").read_text(encoding="utf-8"))


def run(command, *, cwd=kScriptRoot, env=None):
    command = [str(arg) for arg in command]
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, env=env, check=True)


def download(url, destination):
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists():
        print("Download", url, flush=True)
        request = urllib.request.Request(url, headers={"User-Agent": "GalTranslPP-build"})
        with urllib.request.urlopen(request, timeout=120) as response:
            temporary = destination.with_suffix(destination.suffix + ".download")
            with temporary.open("wb") as output:
                shutil.copyfileobj(response, output)
            temporary.replace(destination)
    return destination


def unpack(archive, destination):
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as source:
        source.extractall(destination, filter="data")


def prepareTools():
    version = kConfig["tools"]["mcpp"]
    mcppRoot = kToolsDir / ("mcpp-" + version)
    binaries = list(mcppRoot.rglob("mcpp")) if mcppRoot.exists() else []
    if not any(path.is_file() for path in binaries):
        name = f"mcpp-{version}-macosx-arm64.tar.gz"
        url = f"https://github.com/mcpp-community/mcpp/releases/download/v{version}/{name}"
        archive = download(url, kToolsDir / name)
        checksum = download(url + ".sha256", kToolsDir / (name + ".sha256"))
        expected = checksum.read_text().split()[0]
        if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
            raise RuntimeError("mcpp 下载校验失败")
        unpack(archive, mcppRoot)
    mcpp = next(path for path in mcppRoot.rglob("mcpp") if path.is_file())
    os.environ["PATH"] = str(mcpp.parent) + os.pathsep + os.environ["PATH"]
    run([mcpp, "toolchain", "install", "llvm", kConfig["tools"]["llvm"]])
    run([mcpp, "self", "env"])

    qt = kToolsDir / "Qt" / kConfig["tools"]["qt"] / "macos"
    if not (qt / "bin/qmake").exists():
        run([sys.executable, "-m", "pip", "install", "aqtinstall"])
        run([sys.executable, "-m", "aqt", "install-qt", "mac", "desktop",
             kConfig["tools"]["qt"], "clang_64", "-O", kToolsDir / "Qt"])

    pythonRoot = kToolsDir / "python"
    if not (pythonRoot / "bin/python3").exists():
        release = kConfig["tools"]["python_release"]
        version = kConfig["tools"]["python"]
        name = f"cpython-{version}+{release}-aarch64-apple-darwin-install_only.tar.gz"
        archive = download(f"https://github.com/astral-sh/python-build-standalone/releases/download/{release}/{name}", kToolsDir / name)
        unpack(archive, kToolsDir)
    run([pythonRoot / "bin/python3", "-I", "-m", "pip", "--version"])
    return mcpp, qt, pythonRoot


def prepareDependencies(qt):
    vcpkg = kToolsDir / "vcpkg"
    if not (vcpkg / ".git").exists():
        run(["git", "clone", "https://github.com/microsoft/vcpkg.git", vcpkg])
    run(["git", "checkout", kConfig["tools"]["vcpkg"]], cwd=vcpkg)
    if not (vcpkg / "vcpkg").exists():
        run(["bash", "bootstrap-vcpkg.sh", "-disableMetrics"], cwd=vcpkg)
    triplet = "gpp-arm64-osx-release"
    # 上游 port 的 arm 限制也挡住了原生 Apple Silicon；仅在本次 macOS 构建中验证它。
    overlay = kToolsDir / "macos-ports/opencc"
    shutil.copytree(vcpkg / "ports/opencc", overlay, dirs_exist_ok=True)
    metadata = overlay / "vcpkg.json"
    port = json.loads(metadata.read_text(encoding="utf-8"))
    port["supports"] = "!uwp & (!arm | osx)"
    metadata.write_text(json.dumps(port, indent=2) + "\n", encoding="utf-8")
    run([vcpkg / "vcpkg", "install", "--triplet", triplet,
         "--overlay-ports=" + str(overlay.parent),
         "--x-install-root=" + str(kScriptRoot / "vcpkg_installed/macos")])
    # Clang 模块全局片段中的内部链接函数无法被后续模板实例化找到。
    # 只调整头文件内 ARM 两个助手的链接方式，函数体及二进制库保持一致。
    parseContext = kScriptRoot / "vcpkg_installed/macos" / triplet / "include/google/protobuf/parse_context.h"
    original = parseContext.read_text(encoding="utf-8")
    text = original.replace("static const char* VarintParseSlowArm(", "inline const char* VarintParseSlowArm(")
    if text != original:
        parseContext.write_text(text, encoding="utf-8")

    # 单独使用 Ela 的库目录，避开其顶层示例及 Linux 链接参数。
    elaSource = kToolsDir / "ela-config"
    elaSource.mkdir(exist_ok=True)
    (elaSource / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.24)
project(GppEla LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Widgets WidgetsPrivate)
add_subdirectory("${ELA_SOURCE}" ElaWidgetTools)
''', encoding="utf-8")
    elaBuild = kToolsDir / "ela-build"
    run(["cmake", "-S", elaSource, "-B", elaBuild, "-G", "Ninja",
         "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_PREFIX_PATH=" + str(qt),
         "-DCMAKE_OSX_DEPLOYMENT_TARGET=" + kConfig["build"]["deployment_target"],
         "-DELA_SOURCE=" + str(kScriptRoot / "3rdParty/ElaWidgetTools/ElaWidgetTools")])
    run(["cmake", "--build", elaBuild, "--parallel", str(kConfig["build"]["jobs"])])
    return kScriptRoot / "vcpkg_installed/macos" / triplet, elaBuild / "ElaWidgetTools"


def prepareArchiveLibrary():
    version = kConfig["tools"]["sevenzip"]
    name = "7z" + version.replace(".", "") + "-src.tar.xz"
    source = kToolsDir / ("7zip-" + version)
    if not (source / "CPP/7zip/Bundles/Format7zF/makefile").exists():
        unpack(download(f"https://github.com/ip7z/7zip/releases/download/{version}/{name}", kToolsDir / name), source)
    bundle = source / "CPP/7zip/Bundles/Format7zF"
    run(["make", "-f", "../../cmpl_mac_arm64.mak", "-j" + str(kConfig["build"]["jobs"])], cwd=bundle)
    return next(bundle.rglob("7z.so"))


def writeLocalConfig(qt, pythonRoot, dependencies, ela):
    path = kScriptRoot / "build-local.toml"
    original = path.read_text(encoding="utf-8")
    text = re.sub(r"(?ms)^\[macos\].*?(?=^\[|\Z)", "", original).rstrip()
    values = dict(qt_root=qt, python_root=pythonRoot, dependencies_root=dependencies, ela_root=ela)
    text += "\n[macos]\n" + "".join(f"{key} = {json.dumps(str(value))}\n" for key, value in values.items())
    if text != original:
        path.write_text(text, encoding="utf-8")


def macho(path):
    if not path.is_file() or path.is_symlink():
        return False
    with path.open("rb") as source:
        return source.read(4) in (b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\xfe\xed\xfa\xcf")


def deployLibraries(directory, searchRoots):
    """递归收集非系统 dylib，并把引用改成相对当前文件的路径。"""
    libraryDirectory = directory / "lib"
    libraryDirectory.mkdir(exist_ok=True)
    pythonDirectory = directory / "BaseConfig/Python"
    # python-build-standalone 自身已经可重定位，保留它内部的 @rpath 和库布局。
    # 这些扩展未必预留了扩展 Mach-O 载入命令的空间，不能统一改成更长的 @loader_path。
    pending = [path for path in directory.rglob("*")
               if not path.is_relative_to(pythonDirectory) and macho(path)]
    visited = set()
    while pending:
        binary = pending.pop()
        if binary in visited:
            continue
        visited.add(binary)
        # otool -L 也会列出 dylib 自己的 LC_ID_DYLIB；它不是需要寻找和复制的依赖。
        installNames = subprocess.check_output(["otool", "-D", binary], text=True)
        ownNames = {line.strip() for line in installNames.splitlines() if line.strip() and not line.endswith(":")}
        listing = subprocess.check_output(["otool", "-L", binary], text=True)
        for line in listing.splitlines()[1:]:
            dependency = line.strip().split(" (", 1)[0]
            if dependency in ownNames or dependency.startswith(("/System/", "/usr/lib/")):
                continue
            candidate = Path(dependency.replace("@loader_path", str(binary.parent))
                             .replace("@executable_path", str(directory)))
            if not candidate.exists():
                relative = dependency.removeprefix("@rpath/")
                candidates = [base / relative for base in [pythonDirectory / "lib", libraryDirectory, *searchRoots]]
                candidate = next((path for path in candidates if path.exists()), None)
                if candidate is None:
                    raise FileNotFoundError(f"{binary}: cannot resolve {dependency}")
            candidate = candidate.resolve()
            if candidate.is_relative_to(directory):
                destination = candidate
            elif ".framework/" in str(candidate):
                framework, suffix = str(candidate).split(".framework/", 1)
                framework = Path(framework + ".framework")
                deployed = libraryDirectory / framework.name
                if not deployed.exists():
                    shutil.copytree(framework, deployed, symlinks=True)
                    pending.extend(path for path in deployed.rglob("*") if macho(path))
                destination = deployed / suffix
            else:
                destination = libraryDirectory / candidate.name
                if not destination.exists():
                    shutil.copy2(candidate, destination)
                    pending.append(destination)
            relative = "@loader_path/" + os.path.relpath(destination, binary.parent)
            run(["install_name_tool", "-change", dependency, relative, binary])


def stagePackage(qt, pythonRoot, dependencies, ela, archiveLibrary):
    release = kScriptRoot / "Release"
    package = release / kConfig["package"]["name"]
    # 每次重新组装发布目录，升级 Qt/Python 后不沿用上一版的动态库。
    if package.resolve().parent != release.resolve():
        raise ValueError("package.name 必须是 Release 下的单个目录名")
    if package.exists():
        shutil.rmtree(package)
    app = package / "GalTranslPP.app"
    directory = app / "Contents/MacOS"
    directory.mkdir(parents=True, exist_ok=True)
    for name in ("GalTranslPP_GUI", "GalTranslPP_CLI", "Updater"):
        candidates = [p for p in (kScriptRoot / "target").glob(f"aarch64-macos/**/bin/**/{name}") if p.is_file()]
        if not candidates:
            raise FileNotFoundError("No built executable: " + name)
        shutil.copy2(max(candidates, key=lambda p: p.stat().st_mtime), directory / name)
    shutil.copytree(kScriptRoot / "Example/BaseConfig", directory / "BaseConfig", dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns("Python-3.12.10-embed-amd64*", "__pycache__"))
    shutil.copytree(pythonRoot, directory / "BaseConfig/Python", dirs_exist_ok=True, symlinks=True)
    shutil.copytree(dependencies / "share/opencc", directory / "BaseConfig/opencc", dirs_exist_ok=True)
    shutil.copytree(kScriptRoot / "Example/SampleProject", directory / "SampleProject", dirs_exist_ok=True)
    config = directory / "BaseConfig/GlobalConfig.toml"
    text = config.read_text(encoding="utf-8")
    text = re.sub(r'^pythonEnvPath = .*$', 'pythonEnvPath = "BaseConfig/Python"', text, flags=re.M)
    config.write_text(text, encoding="utf-8")
    shutil.copy2(archiveLibrary, directory / "7z.so")
    translations = directory / "translations"
    translations.mkdir(exist_ok=True)
    for member in ("GalTranslPP", "GPPGUI", "GPPCLI", "Updater"):
        for qm in (kScriptRoot / member).glob("qt_*_en.qm"):
            shutil.copy2(qm, translations / qm.name)
    for qm in (qt / "translations").glob("qt*_zh_CN.qm"):
        shutil.copy2(qm, translations / qm.name)
    for category in ("platforms", "imageformats", "styles", "iconengines", "tls"):
        source = qt / "plugins" / category
        if source.exists():
            shutil.copytree(source, directory / "plugins" / category, dirs_exist_ok=True)
    version = tomllib.loads((kScriptRoot / "mcpp.toml").read_text(encoding="utf-8"))["workspace"]["package"]["version"]
    resources = app / "Contents/Resources"
    resources.mkdir(exist_ok=True)
    # Qt 在 macOS 将 Prefix 相对于 Contents 解析。
    (resources / "qt.conf").write_text("[Paths]\nPrefix=MacOS\nPlugins=plugins\nTranslations=translations\n", encoding="utf-8")
    iconset = kToolsDir / "GalTranslPP.iconset"
    iconset.mkdir(exist_ok=True)
    for size in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            name = f"icon_{size}x{size}" + ("@2x" if scale == 2 else "") + ".png"
            run(["sips", "-s", "format", "png", "-z", size * scale, size * scale,
                 kScriptRoot / "GPPGUI/Resource/images/julixian_s.jpeg", "--out", iconset / name])
    run(["iconutil", "-c", "icns", iconset, "-o", resources / "GalTranslPP.icns"])
    with (app / "Contents/Info.plist").open("wb") as output:
        plistlib.dump(dict(CFBundleExecutable="GalTranslPP_GUI", CFBundleIdentifier="io.github.julixian.galtranslpp",
                          CFBundleName="GalTransl++", CFBundlePackageType="APPL", NSHighResolutionCapable=True,
                          CFBundleVersion=version, CFBundleShortVersionString=version, CFBundleIconFile="GalTranslPP.icns",
                          LSMinimumSystemVersion=kConfig["build"]["deployment_target"]), output)
    llvmLibraries = list((Path.home() / ".mcpp").rglob("libc++.1.dylib"))
    search = [qt / "lib", ela, pythonRoot / "lib", dependencies / "lib"]
    search.extend(path.parent for path in llvmLibraries)
    deployLibraries(directory, search)
    # 等资源和所有 Mach-O 都部署完成，再统一签名，避免后续复制或修改破坏资源封装。
    run([sys.executable, Path(__file__).with_name("sign-macos.py"), app])
    shutil.make_archive(str(package), "gztar", release, package.name)
    return package


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    phase = parser.add_mutually_exclusive_group()
    phase.add_argument("--prepare-only", dest="prepareOnly", action="store_true")
    phase.add_argument("--build-only", dest="buildOnly", action="store_true")
    args = parser.parse_args()
    if sys.platform != "darwin" or platform.machine() != "arm64":
        parser.error("请在 Apple Silicon Mac 上运行；Windows 不能验证 macOS 原生程序。")
    kToolsDir.mkdir(parents=True, exist_ok=True)
    kLogsDir.mkdir(parents=True, exist_ok=True)
    os.environ["MACOSX_DEPLOYMENT_TARGET"] = kConfig["build"]["deployment_target"]
    if args.buildOnly:
        settings = tomllib.loads((kScriptRoot / "build-local.toml").read_text(encoding="utf-8"))["macos"]
        qt, pythonRoot, dependencies, ela = (Path(settings[key]) for key in
            ("qt_root", "python_root", "dependencies_root", "ela_root"))
        mcpp = next(path for path in (kToolsDir / ("mcpp-" + kConfig["tools"]["mcpp"])).rglob("mcpp") if path.is_file())
        archiveLibrary = next((kToolsDir / ("7zip-" + kConfig["tools"]["sevenzip"])).rglob("7z.so"))
    else:
        mcpp, qt, pythonRoot = prepareTools()
        dependencies, ela = prepareDependencies(qt)
        archiveLibrary = prepareArchiveLibrary()
        writeLocalConfig(qt, pythonRoot, dependencies, ela)
    if args.prepareOnly:
        return
    for member in ("julixian.gpp-gui", "julixian.gpp-cli"):
        try:
            run([mcpp, "build", "-p", member, "--profile", kConfig["build"]["profile"],
                 "--toolchain", "llvm@" + kConfig["tools"]["llvm"],
                 "--jobs", kConfig["build"]["jobs"], "--no-color"])
        except subprocess.CalledProcessError:
            # mcpp 首次编译失败即停止；继续已有 Ninja 图，收集其它独立文件的错误。
            graphs = list((kScriptRoot / "target/aarch64-macos").glob("*/build.ninja"))
            if graphs:
                graph = max(graphs, key=lambda path: path.stat().st_mtime)
                subprocess.run(["ninja", "-C", str(graph.parent), "-k", "0", "-j", str(kConfig["build"]["jobs"])],
                               cwd=kScriptRoot, check=False)
            raise
    package = stagePackage(qt, pythonRoot, dependencies, ela, archiveLibrary)
    print("Portable package:", package)
    run([sys.executable, Path(__file__).with_name("make-dmg.py"), package])


if __name__ == "__main__":
    main()
