"""复用已构建的 Qt，定位异常类型匹配和 C++ 运行库绑定问题。"""

import os
import subprocess

from probe import OUTPUT, prepare
from qt_probe import TEST_SOURCE, run


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
    if failures:
        raise RuntimeError("异常测试失败：" + ", ".join(failures))


if __name__ == "__main__":
    main()
