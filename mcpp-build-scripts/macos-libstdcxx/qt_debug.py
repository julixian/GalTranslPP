"""复用已构建的 Qt，定位异常类型匹配和 C++ 运行库绑定问题。"""

import os
import json
import shlex
import shutil
import subprocess
import sys

from probe import OUTPUT, prepare
from qt_probe import TEST_SOURCE, run
import qt_probe


SOURCE = r'''
#include <QApplication>
#include <QCoreApplication>
#include <QFuture>
#include <QFutureInterface>
#include <QMetaType>
#include <QVariant>
#include <cxxabi.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <typeinfo>

struct Message { std::string text; };
Q_DECLARE_METATYPE(Message)
struct LocalError { int value; };

void address(const char* label, const void* pointer) {
    Dl_info info{};
    dladdr(pointer, &info);
    std::printf("%s=%p image=%s symbol=%s\n", label, pointer,
        info.dli_fname ? info.dli_fname : "?", info.dli_sname ? info.dli_sname : "?");
}

void type_info(const char* label, const std::type_info* type) {
    std::printf("%s: name=%s\n", label, type ? type->name() : "null");
    if (!type) return;
    address(label, type);
    address("type_info vtable", *(void* const*)type);
}

void bindings(const char* stage) {
    std::printf("\nBINDINGS %s\n", stage);
    for (const auto* symbol : {"__cxa_throw", "__cxa_begin_catch", "__cxa_get_globals",
            "__gxx_personality_v0", "_Unwind_RaiseException", "__emutls_get_address"})
        address(symbol, dlsym(RTLD_DEFAULT, symbol));
    address("bound rethrow_exception", (void*)&std::rethrow_exception);
    address("bound current_exception", (void*)&std::current_exception);
    type_info("expected runtime_error", &typeid(std::runtime_error));
    for (uint32_t index = 0; index < _dyld_image_count(); ++index) {
        const char* name = _dyld_get_image_name(index);
        if (std::strstr(name, "libstdc++") || std::strstr(name, "libc++") || std::strstr(name, "libQt"))
            std::printf("IMAGE %s\n", name);
    }
    std::fflush(stdout);
}

// catch (...) 用于区分“类型不匹配”和“展开根本到不了处理器”。
__attribute__((noinline)) int exceptions(const char* stage) {
    std::printf("\nEXCEPTIONS %s\n", stage);
    std::fflush(stdout);
    int failures = 0;
    try { throw std::runtime_error("direct"); }
    catch (const std::runtime_error& error) { std::printf("direct typed catch: %s\n", error.what()); }
    catch (...) { ++failures; type_info("direct catch-all", abi::__cxa_current_exception_type()); }
    auto error = std::make_exception_ptr(std::runtime_error("stored"));
    std::printf("exception_ptr stored, valid=%d\n", bool(error));
    std::fflush(stdout);
    try { std::rethrow_exception(error); }
    catch (const std::runtime_error& caught) { std::printf("stored typed catch: %s\n", caught.what()); }
    catch (...) { ++failures; type_info("stored catch-all", abi::__cxa_current_exception_type()); }
    try { std::rethrow_exception(std::make_exception_ptr(std::runtime_error("temporary"))); }
    catch (const std::runtime_error& caught) { std::printf("temporary typed catch: %s\n", caught.what()); }
    catch (...) { ++failures; type_info("temporary catch-all", abi::__cxa_current_exception_type()); }
    try { std::rethrow_exception(std::make_exception_ptr(LocalError{42})); }
    catch (const LocalError& caught) { std::printf("local typed catch: %d\n", caught.value); }
    catch (...) { ++failures; type_info("local catch-all", abi::__cxa_current_exception_type()); }
    std::printf("failures=%d\n", failures);
    std::fflush(stdout);
    return failures;
}

int main(int argc, char** argv) {
    int failures = 0;
    bindings("before application");
    failures += exceptions("before application");
    if (argc > 1 && std::strcmp(argv[1], "headers") == 0) return failures;
    if (argc > 1 && std::strcmp(argv[1], "core") == 0) {
        QCoreApplication app(argc, argv);
        bindings("after QCoreApplication");
        return failures + exceptions("after QCoreApplication");
    }
    QApplication app(argc, argv);
    bindings("after QApplication");
    failures += exceptions("after QApplication");
    QMetaType::registerConverter<Message, QString>([](const Message& value) {
        return QString::fromStdString(value.text);
    });
    const auto converted = QVariant::fromValue(Message{"converter passed"}).toString();
    std::printf("converter=%s\n", converted.toUtf8().constData());
    failures += exceptions("after converter");
    return failures;
}
'''


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    _, clang, _, sdk, _, includes, library = prepare()
    install = OUTPUT / "qtbase-install"
    project = OUTPUT / "qt-debug"
    project.mkdir(exist_ok=True)
    source = project / "main.cpp"
    source.write_text(SOURCE, encoding="utf-8")
    flags = ["--no-default-config", "-nostdinc++", "-stdlib=libstdc++", "-femulated-tls",
             "-std=c++23", "-O0", "-g", "-femit-dwarf-unwind=always", "--sysroot=" + sdk,
             "-mmacosx-version-min=15.0"]
    for include in [*includes, install / "include", *[install / "include" / name for name in ("QtCore", "QtGui", "QtWidgets")]]:
        flags.extend(["-isystem", str(include)])
    link = ["-fuse-ld=lld", "-nodefaultlibs", "-Wl,-no_compact_unwind", str(library),
            str(library.resolve().parent / "libgcc_s.1.1.dylib"),
            str(next(library.resolve().parent.rglob("libgcc.a"))), "-lSystem", "-lobjc",
            "-Wl,-rpath," + str(library.parent), "-Wl,-rpath," + str(install / "lib")]
    link.extend(str(install / "lib" / f"libQt6{name}.dylib") for name in ("Widgets", "Gui", "Core"))
    binary = project / "qt-exception-debug"
    run("debug-build", [clang, *flags, source, *link, "-o", binary])
    environment = dict(os.environ, QT_QPA_PLATFORM="offscreen")
    failures = []
    for mode in ("headers", "core", "gui"):
        try:
            run("debug-" + mode, [binary, mode], env=environment, timeout=60)
        except subprocess.CalledProcessError:
            failures.append(mode)
            run("debug-backtrace-" + mode, ["xcrun", "lldb", "--batch",
                "-o", "settings set target.env-vars QT_QPA_PLATFORM=offscreen",
                "-k", "thread backtrace all", "-k", "image lookup -n __gxx_personality_v0",
                "-k", "image lookup -n __cxa_get_globals", "-o", "run", "--", binary, mode], timeout=120)
    # 保留原来失败的 main 和 moc 生成代码，分别比较调试信息及 catch-all 的影响。
    original = project / "original.cpp"
    original.write_text(TEST_SOURCE, encoding="utf-8")
    run("debug-moc", [install / "libexec/moc", "-I" + str(install / "include"),
                      "-I" + str(install / "include/QtCore"), original, "-o", project / "main.moc"])
    for name, extra in (("original-debug", []), ("original-no-debug", ["-g0"]),
                        ("original-gnu", ["-g0", "-std=gnu++23", "-DQT_NO_DEBUG", "-arch", "arm64"])):
        target = project / name
        run("debug-build-" + name, [clang, *flags, *extra, original, *link, "-o", target])
        try:
            run("debug-run-" + name, [target, "--exceptions-only"], env=environment, timeout=60)
        except subprocess.CalledProcessError:
            failures.append(name)
            run("debug-backtrace-" + name, ["xcrun", "lldb", "--batch",
                "-o", "settings set target.env-vars QT_QPA_PLATFORM=offscreen",
                "-k", "thread backtrace all", "-k", "image lookup -n __gxx_personality_v0",
                "-k", "disassemble -n main", "-o", "run", "--", target, "--exceptions-only"], timeout=120)
    obj = project / "original.o"
    cmake_flags = [*flags, "--rtlib=compiler-rt", "-g0", "-std=gnu++23", "-arch", "arm64",
                   "-DQT_NO_DEBUG", "-DQT_CORE_LIB", "-DQT_GUI_LIB", "-DQT_WIDGETS_LIB"]
    run("debug-compile-cmake", [clang, *cmake_flags, "-c", original, "-o", obj])
    frameworks = [value for name in ("AppKit", "OpenGL", "ImageIO", "Metal", "IOKit",
                                    "DiskArbitration", "UniformTypeIdentifiers")
                  for value in ("-framework", name)]
    for name, arguments in (
            ("object-first", [obj, *link]),
            ("libraries-first", [*link, obj]),
            ("frameworks", [*link, obj, *frameworks]),
            ("cmake-link-options", [*link, obj, *frameworks, "-Wl,-search_paths_first", "-Wl,-headerpad_max_install_names"]),
            ("apple-linker", [*link, obj, *frameworks, "-fuse-ld=/usr/bin/ld"])):
        target = project / name
        run("debug-link-" + name, [clang, *cmake_flags, *arguments, "-o", target])
        try:
            run("debug-run-" + name, [target, "--exceptions-only"], env=environment, timeout=60)
        except subprocess.CalledProcessError:
            failures.append(name)
            run("debug-bindings-" + name, [clang.with_name("llvm-objdump"), "--macho", "--bind", target])
            run("debug-backtrace-" + name, ["xcrun", "lldb", "--batch",
                "-o", "settings set target.env-vars QT_QPA_PLATFORM=offscreen",
                "-k", "thread backtrace all", "-k", "image lookup -n __gxx_personality_v0",
                "-o", "run", "--", target, "--exceptions-only"], timeout=120)
    try:
        run("debug-cmake-probe", [sys.executable, qt_probe.__file__], timeout=1800)
    except subprocess.CalledProcessError:
        pass  # 这里保留原始失败产物，用同一目标文件检查 CMake 的编译与链接。
    cmake_project = OUTPUT / "qt-test"
    cmake_build = cmake_project / "build"
    cmake_obj = cmake_build / "CMakeFiles/qt-libstdcxx-probe.dir/main.cpp.o"
    moc_obj = cmake_build / "CMakeFiles/qt-libstdcxx-probe.dir/qt-libstdcxx-probe_autogen/mocs_compilation.cpp.o"
    for name, objects, extra in (
            ("direct-isysroot", [obj], ["-isysroot", sdk]),
            ("cmake-object", [moc_obj, cmake_obj], []),
            ("cmake-object-isysroot", [moc_obj, cmake_obj], ["-isysroot", sdk])):
        target = project / name
        run("debug-relink-" + name, [clang, *cmake_flags, *extra, *objects, *link, *frameworks, "-o", target])
        run("debug-driver-" + name, [clang, *cmake_flags, *extra, *objects, *link, *frameworks, "-o", target, "-###"])
        try:
            run("debug-run-" + name, [target, "--exceptions-only"], env=environment, timeout=60)
        except subprocess.CalledProcessError:
            failures.append(name)
    run("debug-export-commands", ["cmake", "-S", cmake_project, "-B", cmake_build,
                                  "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"])
    commands = json.loads((cmake_build / "compile_commands.json").read_text())
    command = next(item["command"] for item in commands if item["file"].endswith("/main.cpp"))
    run("debug-cmake-compile-driver", [*shlex.split(command), "-###"], cwd=cmake_build)
    run("debug-cmake-ir", [*shlex.split(command), "-S", "-emit-llvm", "-o", OUTPUT / "debug-cmake-main-ir.log"], cwd=cmake_build)
    ninja_commands = subprocess.check_output(["ninja", "-C", str(cmake_build), "-t", "commands", "qt-libstdcxx-probe"], text=True)
    link_command = ninja_commands.strip().splitlines()[-1].removeprefix(": && ").removesuffix(" && :")
    run("debug-cmake-link-driver", [*shlex.split(link_command), "-###"], cwd=cmake_build)
    cmake_binary = cmake_build / "qt-libstdcxx-probe"
    copied_binary = project / "copied-cmake-binary"
    shutil.copy2(cmake_binary, copied_binary)
    for name, candidate, directory in (
            ("original-cwd-test", cmake_binary, cmake_project),
            ("original-cwd-root", cmake_binary, None),
            ("copied-cwd-test", copied_binary, cmake_project),
            ("copied-cwd-root", copied_binary, None)):
        try:
            run("debug-run-" + name, [candidate, "--exceptions-only"], env=environment, cwd=directory, timeout=60)
        except subprocess.CalledProcessError:
            failures.append(name)
    for name, extra in (
            ("cmake-exact-replay", []),
            ("cmake-no-deduplicate", ["-Wl,-no_deduplicate"]),
            ("cmake-O0", ["-O0"]),
            ("cmake-O2", ["-O2"]),
            ("cmake-no-outlining", ["-Wl,-mllvm,-enable-linkonceodr-outlining=false"])):
        target = project / name
        run("debug-exact-link-" + name, [*shlex.split(link_command), *extra, "-o", target], cwd=cmake_build)
        try:
            run("debug-run-" + name, [target, "--exceptions-only"], env=environment, timeout=60)
        except subprocess.CalledProcessError:
            failures.append(name)
    exact_args = shlex.split(link_command)
    core_argument = str(install / "lib/libQt6Core.6.12.0.dylib")
    core_before_frameworks = [value for value in exact_args if value != core_argument]
    core_before_frameworks.insert(core_before_frameworks.index("-framework"), core_argument)
    aliases = [value.replace(".6.12.0.dylib", ".dylib") if value.startswith(str(install)) else value for value in exact_args]
    for name, arguments in (("core-before-frameworks", core_before_frameworks), ("unversioned-libraries", aliases)):
        target = project / name
        run("debug-exact-link-" + name, [*arguments, "-o", target], cwd=cmake_build)
        try:
            run("debug-run-" + name, [target, "--exceptions-only"], env=environment, timeout=60)
        except subprocess.CalledProcessError:
            failures.append(name)
    # 运行库放在 LINKER_FLAGS 中会早于目标文件，静态库可能因此没有抽取成员。
    runtime_arguments = [value for value in exact_args
                         if value.endswith(("/libstdc++.dylib", "/libgcc_s.1.1.dylib", "/libgcc.a"))
                         or value in ("-lSystem", "-lobjc")]
    gcc_archive = next(value for value in runtime_arguments if value.endswith("/libgcc.a"))
    runtime_cases = [("runtime-after-objects", runtime_arguments), ("gcc-after-objects", [gcc_archive])]
    runtime_cases.extend(("runtime-single-" + str(index), [value])
                         for index, value in enumerate(runtime_arguments))
    for name, moved in runtime_cases:
        arguments = [value for value in exact_args if value not in moved]
        last_object = max(index for index, value in enumerate(arguments) if value.endswith(".o"))
        arguments[last_object + 1:last_object + 1] = moved
        target = project / name
        run("debug-exact-link-" + name, [*arguments, "-o", target], cwd=cmake_build)
        try:
            run("debug-run-" + name, [target, "--exceptions-only"], env=environment, timeout=60)
        except subprocess.CalledProcessError:
            failures.append(name)
        run("debug-unwind-" + name, [clang.with_name("llvm-objdump"), "--macho", "--unwind-info", target])
    no_headerpad = [value for value in exact_args if value != "-Wl,-headerpad_max_install_names"]
    for name, arguments, extra in (
            ("no-headerpad", no_headerpad, []),
            ("large-headerpad", no_headerpad, ["-Wl,-headerpad,10000"]),
            ("no-chained-fixups", exact_args, ["-Wl,-no_fixup_chains"])):
        target = project / name
        run("debug-exact-link-" + name, [*arguments, *extra, "-o", target], cwd=cmake_build)
        try:
            run("debug-run-" + name, [target, "--exceptions-only"], env=environment, timeout=60)
        except subprocess.CalledProcessError:
            failures.append(name)
    for name, candidate in (("original", cmake_binary), ("working", project / "cmake-object")):
        run("debug-bind-" + name, [clang.with_name("llvm-objdump"), "--macho", "--bind", candidate])
        run("debug-weak-bind-" + name, [clang.with_name("llvm-objdump"), "--macho", "--weak-bind", candidate])
        run("debug-fixups-" + name, [clang.with_name("llvm-objdump"), "--macho", "--chained-fixups", candidate])
        run("debug-unwind-" + name, [clang.with_name("llvm-objdump"), "--macho", "--unwind-info", candidate])
        run("debug-dyld-" + name, ["xcrun", "lldb", "--batch",
            "-o", "settings set target.env-vars QT_QPA_PLATFORM=offscreen DYLD_PRINT_BINDINGS=1 DYLD_PRINT_WEAK_BINDINGS=1",
            "-k", "thread backtrace", "-o", "run", "--", candidate, "--exceptions-only"], timeout=120)
        debugger = project / (name + "-personality.lldb")
        debugger.write_text("\n".join([
            "settings set target.env-vars QT_QPA_PLATFORM=offscreen",
            "breakpoint set -n __gxx_personality_v0", "run",
            *[command for _ in range(4) for command in (
                "thread backtrace", "register read x0 x1 x2 x3 x4", "thread step-out",
                "register read x0", "continue")], "thread backtrace"]), encoding="utf-8")
        try:
            run("debug-personality-" + name, ["xcrun", "lldb", "--batch", "-s", debugger,
                "--", candidate, "--exceptions-only"], timeout=120)
        except subprocess.CalledProcessError:
            pass  # 进程提前结束后，后续调试命令自然无效，仍保留之前的采样。
        symbols = subprocess.check_output([clang.with_name("llvm-nm"), "--defined-only", candidate], text=True)
        (OUTPUT / ("debug-unwind-symbols-" + name + ".log")).write_text(
            "\n".join(line for line in symbols.splitlines()
                      if any(word in line for word in ("Unwind", "personality", "cxa_", "rethrow_exception"))), encoding="utf-8")
    archive_symbols = subprocess.check_output([clang.with_name("llvm-nm"), gcc_archive], text=True)
    (OUTPUT / "debug-gcc-archive.log").write_text(archive_symbols, encoding="utf-8")
    if failures:
        raise RuntimeError("异常测试失败：" + ", ".join(failures))


if __name__ == "__main__":
    main()
