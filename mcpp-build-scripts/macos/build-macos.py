"""在 Apple Silicon Mac 上构建并组装便携版；暂不生成 DMG。"""

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

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "target/macos-tools"
LOGS = ROOT / "target/macos-logs"
CONFIG = tomllib.loads(Path(__file__).with_suffix(".toml").read_text(encoding="utf-8"))


def run(command, *, cwd=ROOT, env=None):
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


def prepare_tools():
    version = CONFIG["tools"]["mcpp"]
    mcpp_root = TOOLS / ("mcpp-" + version)
    binaries = list(mcpp_root.rglob("mcpp")) if mcpp_root.exists() else []
    if not any(path.is_file() for path in binaries):
        name = f"mcpp-{version}-macosx-arm64.tar.gz"
        url = f"https://github.com/mcpp-community/mcpp/releases/download/v{version}/{name}"
        archive = download(url, TOOLS / name)
        checksum = download(url + ".sha256", TOOLS / (name + ".sha256"))
        expected = checksum.read_text().split()[0]
        if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
            raise RuntimeError("mcpp 下载校验失败")
        unpack(archive, mcpp_root)
    mcpp = next(path for path in mcpp_root.rglob("mcpp") if path.is_file())
    os.environ["PATH"] = str(mcpp.parent) + os.pathsep + os.environ["PATH"]
    run([mcpp, "toolchain", "install", "llvm", CONFIG["tools"]["llvm"]])
    run([mcpp, "self", "env"])

    qt = TOOLS / "Qt" / CONFIG["tools"]["qt"] / "macos"
    if not (qt / "bin/qmake").exists():
        run([sys.executable, "-m", "pip", "install", "aqtinstall"])
        run([sys.executable, "-m", "aqt", "install-qt", "mac", "desktop",
             CONFIG["tools"]["qt"], "clang_64", "-O", TOOLS / "Qt"])

    python_root = TOOLS / "python"
    if not (python_root / "bin/python3").exists():
        release = CONFIG["tools"]["python_release"]
        version = CONFIG["tools"]["python"]
        name = f"cpython-{version}+{release}-aarch64-apple-darwin-install_only.tar.gz"
        archive = download(f"https://github.com/astral-sh/python-build-standalone/releases/download/{release}/{name}", TOOLS / name)
        unpack(archive, TOOLS)
    run([python_root / "bin/python3", "-I", "-m", "pip", "--version"])
    return mcpp, qt, python_root


def prepare_dependencies(qt):
    vcpkg = TOOLS / "vcpkg"
    if not (vcpkg / ".git").exists():
        run(["git", "clone", "https://github.com/microsoft/vcpkg.git", vcpkg])
    run(["git", "checkout", CONFIG["tools"]["vcpkg"]], cwd=vcpkg)
    if not (vcpkg / "vcpkg").exists():
        run(["bash", "bootstrap-vcpkg.sh", "-disableMetrics"], cwd=vcpkg)
    triplet = "gpp-arm64-osx-release"
    # 上游 port 的 arm 限制也挡住了原生 Apple Silicon；仅在本次 macOS 构建中验证它。
    overlay = TOOLS / "macos-ports/opencc"
    shutil.copytree(vcpkg / "ports/opencc", overlay, dirs_exist_ok=True)
    metadata = overlay / "vcpkg.json"
    port = json.loads(metadata.read_text(encoding="utf-8"))
    port["supports"] = "!uwp & (!arm | osx)"
    metadata.write_text(json.dumps(port, indent=2) + "\n", encoding="utf-8")
    run([vcpkg / "vcpkg", "install", "--triplet", triplet,
         "--overlay-ports=" + str(overlay.parent),
         "--x-install-root=" + str(ROOT / "vcpkg_installed/macos")])
    # Clang 模块全局片段中的内部链接函数无法被后续模板实例化找到。
    # 只调整头文件内 ARM 两个助手的链接方式，函数体及二进制库保持一致。
    parse_context = ROOT / "vcpkg_installed/macos" / triplet / "include/google/protobuf/parse_context.h"
    original = parse_context.read_text(encoding="utf-8")
    text = original.replace("static const char* VarintParseSlowArm(", "inline const char* VarintParseSlowArm(")
    if text != original:
        parse_context.write_text(text, encoding="utf-8")

    # 单独使用 Ela 的库目录，避开其顶层示例及 Linux 链接参数。
    ela_source = TOOLS / "ela-config"
    ela_source.mkdir(exist_ok=True)
    (ela_source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.24)
project(GppEla LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Widgets WidgetsPrivate)
add_subdirectory("${ELA_SOURCE}" ElaWidgetTools)
''', encoding="utf-8")
    ela_build = TOOLS / "ela-build"
    run(["cmake", "-S", ela_source, "-B", ela_build, "-G", "Ninja",
         "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_PREFIX_PATH=" + str(qt),
         "-DCMAKE_OSX_DEPLOYMENT_TARGET=" + CONFIG["build"]["deployment_target"],
         "-DELA_SOURCE=" + str(ROOT / "3rdParty/ElaWidgetTools/ElaWidgetTools")])
    run(["cmake", "--build", ela_build, "--parallel", str(CONFIG["build"]["jobs"])])
    return ROOT / "vcpkg_installed/macos" / triplet, ela_build / "ElaWidgetTools"


def prepare_archive_library():
    version = CONFIG["tools"]["sevenzip"]
    name = "7z" + version.replace(".", "") + "-src.tar.xz"
    source = TOOLS / ("7zip-" + version)
    if not (source / "CPP/7zip/Bundles/Format7zF/makefile").exists():
        unpack(download(f"https://github.com/ip7z/7zip/releases/download/{version}/{name}", TOOLS / name), source)
    bundle = source / "CPP/7zip/Bundles/Format7zF"
    run(["make", "-f", "../../cmpl_mac_arm64.mak", "-j" + str(CONFIG["build"]["jobs"])], cwd=bundle)
    return next(bundle.rglob("7z.so"))


def write_local_config(qt, python_root, dependencies, ela):
    path = ROOT / "build-local.toml"
    original = path.read_text(encoding="utf-8")
    text = re.sub(r"(?ms)^\[macos\].*?(?=^\[|\Z)", "", original).rstrip()
    values = dict(qt_root=qt, python_root=python_root, dependencies_root=dependencies, ela_root=ela)
    text += "\n[macos]\n" + "".join(f"{key} = {json.dumps(str(value))}\n" for key, value in values.items())
    if text != original:
        path.write_text(text, encoding="utf-8")


def macho(path):
    if not path.is_file() or path.is_symlink():
        return False
    with path.open("rb") as source:
        return source.read(4) in (b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\xfe\xed\xfa\xcf")


def deploy_libraries(directory, search_roots):
    """递归收集非系统 dylib，并把引用改成相对当前文件的路径。"""
    library_directory = directory / "lib"
    library_directory.mkdir(exist_ok=True)
    pending = [path for path in directory.rglob("*") if macho(path)]
    visited = set()
    while pending:
        binary = pending.pop()
        if binary in visited:
            continue
        visited.add(binary)
        listing = subprocess.check_output(["otool", "-L", binary], text=True)
        for line in listing.splitlines()[1:]:
            dependency = line.strip().split(" (", 1)[0]
            if dependency.startswith(("/System/", "/usr/lib/")):
                continue
            python_directory = directory / "BaseConfig/Python"
            executable_directory = python_directory / "bin" if binary.is_relative_to(python_directory) else directory
            candidate = Path(dependency.replace("@loader_path", str(binary.parent))
                             .replace("@executable_path", str(executable_directory)))
            if not candidate.exists():
                relative = dependency.removeprefix("@rpath/")
                candidates = [base / relative for base in [library_directory, *search_roots]]
                candidate = next((path for path in candidates if path.exists()), None)
                if candidate is None:
                    raise FileNotFoundError(f"{binary}: cannot resolve {dependency}")
            candidate = candidate.resolve()
            if candidate.is_relative_to(directory):
                destination = candidate
            elif ".framework/" in str(candidate):
                framework, suffix = str(candidate).split(".framework/", 1)
                framework = Path(framework + ".framework")
                deployed = library_directory / framework.name
                if not deployed.exists():
                    shutil.copytree(framework, deployed, symlinks=True)
                    pending.extend(path for path in deployed.rglob("*") if macho(path))
                destination = deployed / suffix
            else:
                destination = library_directory / candidate.name
                if not destination.exists():
                    shutil.copy2(candidate, destination)
                    pending.append(destination)
            relative = "@loader_path/" + os.path.relpath(destination, binary.parent)
            run(["install_name_tool", "-change", dependency, relative, binary])
        # 修改 Mach-O 后原签名失效；便携版使用本地 ad-hoc 签名。
        run(["codesign", "--force", "--sign", "-", binary])


def stage_package(qt, python_root, dependencies, ela, archive_library):
    release = ROOT / "Release"
    package = release / CONFIG["package"]["name"]
    # 每次重新组装发布目录，升级 Qt/Python 后不沿用上一版的动态库。
    if package.resolve().parent != release.resolve():
        raise ValueError("package.name 必须是 Release 下的单个目录名")
    if package.exists():
        shutil.rmtree(package)
    app = package / "GalTranslPP.app"
    directory = app / "Contents/MacOS"
    directory.mkdir(parents=True, exist_ok=True)
    for name in ("GalTranslPP_GUI", "GalTranslPP_CLI", "Updater"):
        candidates = [p for p in (ROOT / "target").glob(f"aarch64-macos/**/bin/**/{name}") if p.is_file()]
        if not candidates:
            raise FileNotFoundError("No built executable: " + name)
        shutil.copy2(max(candidates, key=lambda p: p.stat().st_mtime), directory / name)
    shutil.copytree(ROOT / "Example/BaseConfig", directory / "BaseConfig", dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns("Python-3.12.10-embed-amd64*", "__pycache__"))
    shutil.copytree(python_root, directory / "BaseConfig/Python", dirs_exist_ok=True, symlinks=True)
    shutil.copytree(dependencies / "share/opencc", directory / "BaseConfig/opencc", dirs_exist_ok=True)
    shutil.copytree(ROOT / "Example/SampleProject", directory / "SampleProject", dirs_exist_ok=True)
    config = directory / "BaseConfig/GlobalConfig.toml"
    text = config.read_text(encoding="utf-8")
    text = re.sub(r'^pythonEnvPath = .*$', 'pythonEnvPath = "BaseConfig/Python"', text, flags=re.M)
    config.write_text(text, encoding="utf-8")
    shutil.copy2(archive_library, directory / "7z.so")
    translations = directory / "translations"
    translations.mkdir(exist_ok=True)
    for member in ("GalTranslPP", "GPPGUI", "GPPCLI", "Updater"):
        for qm in (ROOT / member).glob("qt_*_en.qm"):
            shutil.copy2(qm, translations / qm.name)
    for qm in (qt / "translations").glob("qt*_zh_CN.qm"):
        shutil.copy2(qm, translations / qm.name)
    for category in ("platforms", "imageformats", "styles", "iconengines", "tls"):
        source = qt / "plugins" / category
        if source.exists():
            shutil.copytree(source, directory / "plugins" / category, dirs_exist_ok=True)
    version = tomllib.loads((ROOT / "mcpp.toml").read_text(encoding="utf-8"))["workspace"]["package"]["version"]
    resources = app / "Contents/Resources"
    resources.mkdir(exist_ok=True)
    # Qt 在 macOS 将 Prefix 相对于 Contents 解析。
    (resources / "qt.conf").write_text("[Paths]\nPrefix=MacOS\nPlugins=plugins\nTranslations=translations\n", encoding="utf-8")
    iconset = TOOLS / "GalTranslPP.iconset"
    iconset.mkdir(exist_ok=True)
    for size in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            name = f"icon_{size}x{size}" + ("@2x" if scale == 2 else "") + ".png"
            run(["sips", "-s", "format", "png", "-z", size * scale, size * scale,
                 ROOT / "GPPGUI/resource/images/julixian_s.jpeg", "--out", iconset / name])
    run(["iconutil", "-c", "icns", iconset, "-o", resources / "GalTranslPP.icns"])
    with (app / "Contents/Info.plist").open("wb") as output:
        plistlib.dump(dict(CFBundleExecutable="GalTranslPP_GUI", CFBundleIdentifier="io.github.julixian.galtranslpp",
                          CFBundleName="GalTransl++", CFBundlePackageType="APPL", NSHighResolutionCapable=True,
                          CFBundleVersion=version, CFBundleShortVersionString=version, CFBundleIconFile="GalTranslPP.icns",
                          LSMinimumSystemVersion=CONFIG["build"]["deployment_target"]), output)
    llvm_libraries = list((Path.home() / ".mcpp").rglob("libc++.1.dylib"))
    search = [qt / "lib", ela, python_root / "lib", dependencies / "lib"]
    search.extend(path.parent for path in llvm_libraries)
    deploy_libraries(directory, search)
    shutil.make_archive(str(package), "gztar", release, package.name)
    return package


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    phase = parser.add_mutually_exclusive_group()
    phase.add_argument("--prepare-only", action="store_true")
    phase.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    if sys.platform != "darwin" or platform.machine() != "arm64":
        parser.error("请在 Apple Silicon Mac 上运行；Windows 不能验证 macOS 原生程序。")
    TOOLS.mkdir(parents=True, exist_ok=True)
    LOGS.mkdir(parents=True, exist_ok=True)
    os.environ["MACOSX_DEPLOYMENT_TARGET"] = CONFIG["build"]["deployment_target"]
    if args.build_only:
        settings = tomllib.loads((ROOT / "build-local.toml").read_text(encoding="utf-8"))["macos"]
        qt, python_root, dependencies, ela = (Path(settings[key]) for key in
            ("qt_root", "python_root", "dependencies_root", "ela_root"))
        mcpp = next(path for path in (TOOLS / ("mcpp-" + CONFIG["tools"]["mcpp"])).rglob("mcpp") if path.is_file())
        archive_library = next((TOOLS / ("7zip-" + CONFIG["tools"]["sevenzip"])).rglob("7z.so"))
    else:
        mcpp, qt, python_root = prepare_tools()
        dependencies, ela = prepare_dependencies(qt)
        archive_library = prepare_archive_library()
        write_local_config(qt, python_root, dependencies, ela)
    if args.prepare_only:
        return
    for member in ("GPPGUI", "GPPCLI"):
        run([mcpp, "build", "-p", member, "--profile", CONFIG["build"]["profile"],
             "--toolchain", "llvm@" + CONFIG["tools"]["llvm"],
             "--jobs", CONFIG["build"]["jobs"], "--no-color"])
    package = stage_package(qt, python_root, dependencies, ela, archive_library)
    print("Portable package:", package)


if __name__ == "__main__":
    main()
