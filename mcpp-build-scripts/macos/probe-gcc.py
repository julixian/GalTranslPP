"""独立验证 macOS 的 GCC、标准库功能及 mcpp 模块构建，不修改应用源码。"""

import json
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "target/macos-gcc-probe"
VERSION = "2026.10.16.2"


def run(name, command, *, cwd=None):
    print("+", " ".join(str(value) for value in command), flush=True)
    result = subprocess.run([str(value) for value in command], cwd=cwd,
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (OUTPUT / (name + ".log")).write_text(result.stdout, encoding="utf-8")
    print(result.stdout, flush=True)
    if result.returncode:
        raise subprocess.CalledProcessError(result.returncode, command)
    return result.stdout.strip()


def main():
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        raise SystemExit("需要真实的 macOS ARM64 环境")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    prefix = Path(run("brew-prefix", ["brew", "--prefix", "gcc"]))
    compiler = max(prefix.glob("bin/g++-*"), key=lambda path: int(path.name.split("-")[-1]))
    version = run("gcc-version", [compiler, "-dumpfullversion"])
    sdk = run("apple-sdk", ["xcrun", "--show-sdk-path"])
    run("gcc-info", [compiler, "-v"])
    run("gcc-libraries", [compiler, "-print-search-dirs"])
    modules = list(prefix.rglob("std.cc"))
    (OUTPUT / "std-modules.json").write_text(json.dumps([str(path) for path in modules], indent=2), encoding="utf-8")

    # Homebrew 的驱动带版本后缀；仅为 mcpp 整理一个临时入口，所有工具仍是同一套 GCC。
    layout = OUTPUT / "gcc"
    binaries = layout / "bin"
    binaries.mkdir(parents=True, exist_ok=True)
    major = version.split(".")[0]
    for name, source in {
        "g++": compiler,
        "gcc": prefix / ("bin/gcc-" + major),
        "gcc-ar": prefix / ("bin/gcc-ar-" + major),
        "gcc-ranlib": prefix / ("bin/gcc-ranlib-" + major),
        "gcc-nm": prefix / ("bin/gcc-nm-" + major),
        "ar": Path("/usr/bin/ar"),
        "ranlib": Path("/usr/bin/ranlib"),
        "nm": Path("/usr/bin/nm"),
        "as": Path("/usr/bin/as"),
    }.items():
        target = binaries / name
        target.unlink(missing_ok=True)
        target.symlink_to(source)
    if modules:
        include = layout / "include/c++"
        include.mkdir(parents=True, exist_ok=True)
        target = include / version
        target.unlink(missing_ok=True)
        target.symlink_to(modules[0].parent.parent, target_is_directory=True)

    archive_name = f"mcpp-{VERSION}-macosx-arm64.tar.gz"
    url = f"https://github.com/mcpp-community/mcpp/releases/download/v{VERSION}/{archive_name}"
    archive = OUTPUT / archive_name
    urllib.request.urlretrieve(url, archive)
    with tarfile.open(archive) as bundle:
        bundle.extractall(OUTPUT / "mcpp", filter="data")
    mcpp = next(path for path in (OUTPUT / "mcpp").rglob("mcpp") if path.is_file())

    body = '''
int main() {
    std::vector<int> values{1, 2, 3, 4, 5};
    int total = 0;
    for (auto chunk : values | std::views::as_const | std::views::chunk(2))
        for (int value : chunk) total += value;
    if (total != 15) return 1;
    const std::chrono::zoned_time local{std::chrono::current_zone(), std::chrono::system_clock::now()};
    std::cout << std::format("{:%Y-%m-%dT%H:%M:%S%Ez}", local) << '\\n';
    return 0;
}
'''
    # 放在仓库外，避免向上发现 GPP 工作区导致“最小项目”实际继承主项目配置。
    with tempfile.TemporaryDirectory(prefix="gpp-macos-gcc-") as temporary:
        project = Path(temporary)
        source = project / "main.cpp"
        source.write_text("#include <chrono>\n#include <format>\n#include <iostream>\n#include <ranges>\n#include <vector>\n" + body,
                          encoding="utf-8")
        run("gcc-headers-build", [compiler, "-std=c++23", source, "-o", project / "headers-test"])
        run("gcc-headers-run", [project / "headers-test"])
        source.write_text("import std;\n" + body, encoding="utf-8")
        manifest = f'''[package]
name = "gcc-probe"
version = "0.1.0"
standard = "c++23"
[toolchain]
default = {{ path = {json.dumps(str(layout))}, family = "gcc", sysroot = {json.dumps(sdk)} }}
[build]
sources = []
[targets.gcc-probe]
kind = "bin"
main = "main.cpp"
'''
        (project / "mcpp.toml").write_text(manifest, encoding="utf-8")
        try:
            run("mcpp-build", [mcpp, "build", "--no-color"], cwd=project)
            executables = [path for path in (project / "target").rglob("gcc-probe") if path.is_file()]
            run("mcpp-run", [max(executables, key=lambda path: path.stat().st_mtime)], cwd=project)
        finally:
            for name in ("build.ninja", "resolution.json", "mcpp.toml"):
                for path in project.rglob(name):
                    target = OUTPUT / "project" / path.relative_to(project)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(path, target)
    (OUTPUT / "passed.txt").write_text("GCC headers, import std, views and chrono passed\n", encoding="utf-8")


if __name__ == "__main__":
    main()
