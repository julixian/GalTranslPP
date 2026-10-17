"""独立检测 Clang + libstdc++ 的头文件、模块及 mcpp 标准库包入口。"""

import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "target/macos-libstdcxx"
MCPP_VERSION = "2026.10.17.1"
LLVM_VERSION = "23.1.3"
RESULTS = []


def run(name, command, *, cwd=None):
    command = [str(value) for value in command]
    print("+", " ".join(command), flush=True)
    result = subprocess.run(command, cwd=cwd, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (OUTPUT / (name + ".log")).write_text(result.stdout, encoding="utf-8")
    print(result.stdout, flush=True)
    RESULTS.append(dict(name=name, command=command, exit_code=result.returncode))
    (OUTPUT / "results.json").write_text(json.dumps(RESULTS, ensure_ascii=False, indent=2), encoding="utf-8")
    if result.returncode:
        raise subprocess.CalledProcessError(result.returncode, command)
    return result.stdout.strip()


def prepare():
    name = f"mcpp-{MCPP_VERSION}-macosx-arm64.tar.gz"
    url = f"https://github.com/mcpp-community/mcpp/releases/download/v{MCPP_VERSION}/{name}"
    archive = OUTPUT / name
    urllib.request.urlretrieve(url, archive)
    with urllib.request.urlopen(url + ".sha256", timeout=120) as response:
        checksum = response.read().decode().split()[0]
    if hashlib.sha256(archive.read_bytes()).hexdigest() != checksum:
        raise RuntimeError("mcpp 下载校验失败")
    with tarfile.open(archive) as bundle:
        bundle.extractall(OUTPUT / "mcpp", filter="data")
    mcpp = next(path for path in (OUTPUT / "mcpp").rglob("mcpp") if path.is_file())
    run("install-llvm", [mcpp, "toolchain", "install", "llvm", LLVM_VERSION])
    compilers = [path for path in (Path.home() / ".mcpp/registry").rglob("clang++")
                 if LLVM_VERSION in path.parts and path.is_file()]
    clang = next(path for path in compilers if (path.parent / "clang-scan-deps").is_file())
    run("clang-version", [clang, "--version"])
    prefix = Path(run("gcc-prefix", ["brew", "--prefix", "gcc"]))
    gcc = max(prefix.glob("bin/g++-*"), key=lambda path: int(path.name.split("-")[-1]))
    version = run("gcc-version", [gcc, "-dumpfullversion"])
    sdk = run("apple-sdk", ["xcrun", "--show-sdk-path"])
    module = next(prefix.rglob("std.cc"))
    headers = module.parent.parent
    config = next(headers.rglob("c++config.h"))
    includes = [headers, config.parent.parent, headers / "backward"]
    library = Path(run("libstdcxx", [gcc, "-print-file-name=libstdc++.dylib"]))
    if not library.is_file():
        raise FileNotFoundError(library)
    run("libstdcxx-dependencies", ["otool", "-L", library])
    return mcpp, clang, version, sdk, module, includes, library


BODY = r'''
int main() {
    std::vector<int> values{1, 2, 3, 4, 5};
    int total = 0;
    for (auto chunk : values | std::views::as_const | std::views::chunk(2))
        for (int value : chunk) total += value;
    if (total != 15) return 1;
    std::expected<int, std::string> result = total;
    if (!result || *result != 15) return 2;
    try { throw std::runtime_error("exception passed"); }
    catch (const std::exception& error) { std::cout << error.what() << '\n'; }
    const std::chrono::zoned_time local{std::chrono::current_zone(), std::chrono::system_clock::now()};
    std::cout << std::format("{:%Y-%m-%dT%H:%M:%S%Ez}", local) << '\n';
}
'''


def main():
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        raise SystemExit("需要真实的 macOS ARM64 环境")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    mcpp, clang, version, sdk, module, includes, library = prepare()
    flags = ["--no-default-config", "-std=c++23", "-nostdinc++", "-stdlib=libstdc++",
             "--sysroot=" + sdk, "-mmacosx-version-min=14.4", "--rtlib=compiler-rt"]
    for include in includes:
        flags.extend(["-isystem", str(include)])
    link = ["-fuse-ld=lld", "-nostdlib++", str(library), "-Wl,-rpath," + str(library.parent)]

    # 仓库外的临时项目不会继承 GPP 工作区，也不修改用户的任何源码。
    with tempfile.TemporaryDirectory(prefix="gpp-clang-libstdcxx-") as temporary:
        project = Path(temporary)
        source = project / "main.cpp"
        source.write_text("#include <chrono>\n#include <expected>\n#include <format>\n#include <iostream>\n"
                          "#include <ranges>\n#include <stdexcept>\n#include <vector>\n" + BODY, encoding="utf-8")
        binary = project / "headers-test"
        run("headers-build", [clang, *flags, source, *link, "-o", binary])
        run("headers-run", [binary])
        run("headers-dependencies", ["otool", "-L", binary])

        provider = project / "libstdcxx"
        provider.mkdir()
        shutil.copy2(module, provider / "std.cppm")
        shutil.copy2(module.with_name("std.compat.cc"), provider / "std.compat.cppm")
        source.write_text("import std;\n" + BODY, encoding="utf-8")
        pcm = project / "std.pcm"
        run("module-precompile", [clang, *flags, "--precompile", provider / "std.cppm", "-o", pcm])
        run("module-codegen", [clang, *flags, "-c", pcm, "-o", project / "std.o"])
        binary = project / "modules-test"
        run("modules-build", [clang, *flags, "-fmodule-file=std=" + str(pcm), source, project / "std.o",
                              *link, "-o", binary])
        run("modules-run", [binary])
        run("modules-dependencies", ["otool", "-L", binary])

        provider_manifest = f'''[package]
namespace = "probe"
name = "libstdcxx"
version = {json.dumps(version)}
standard = "c++23"
provides = ["mcpp:c++-abi=libstdc++"]
requires = ["mcpp:compiler=llvm"]
[build]
sources = []
include_dirs = {json.dumps([str(path) for path in includes])}
cxxflags = ["-stdlib=libstdc++", "--rtlib=compiler-rt"]
ldflags = {json.dumps([str(library), '-Wl,-rpath,' + str(library.parent)])}
std-module = "std.cppm"
std-compat-module = "std.compat.cppm"
std-module-flags = ["--no-default-config", "-nostdinc++"]
'''
        (provider / "mcpp.toml").write_text(provider_manifest, encoding="utf-8")
        manifest = f'''[package]
name = "clang-libstdcxx-probe"
version = "0.1.0"
standard = "c++23"
[toolchain]
default = "llvm@{LLVM_VERSION}"
[dependencies]
"probe.libstdcxx" = {{ path = "libstdcxx" }}
[build]
sources = []
macos_deployment_target = "14.4"
[targets.clang-libstdcxx-probe]
kind = "bin"
main = "main.cpp"
'''
        (project / "mcpp.toml").write_text(manifest, encoding="utf-8")
        try:
            run("mcpp-build", [mcpp, "build", "--no-color"], cwd=project)
            executables = [path for path in (project / "target").rglob("clang-libstdcxx-probe") if path.is_file()]
            binary = max(executables, key=lambda path: path.stat().st_mtime)
            run("mcpp-run", [binary], cwd=project)
            run("mcpp-dependencies", ["otool", "-L", binary])
        finally:
            for name in ("build.ninja", "resolution.json", "mcpp.toml"):
                for path in project.rglob(name):
                    destination = OUTPUT / "project" / path.relative_to(project)
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(path, destination)


if __name__ == "__main__":
    main()
