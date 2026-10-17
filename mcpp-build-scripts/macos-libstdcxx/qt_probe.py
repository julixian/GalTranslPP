"""在独立目录构建 QtBase，验证 libstdc++ ABI 及 macOS 平台插件。"""

import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

from probe import OUTPUT, prepare


def run(name, command, *, cwd=None, env=None, timeout=3600):
    command = [str(value) for value in command]
    print("+", shlex.join(command), flush=True)
    with (OUTPUT / (name + ".log")).open("w", encoding="utf-8") as log:
        # 长时间构建直接写日志，避免等待整个 Qt 构建完成后才看到输出。
        result = subprocess.run(command, cwd=cwd, env=env, text=True,
                                stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
    if result.returncode:
        print((OUTPUT / (name + ".log")).read_text(encoding="utf-8", errors="replace")[-24000:], flush=True)
        raise subprocess.CalledProcessError(result.returncode, command)
    print(name + " passed", flush=True)


TEST_SOURCE = r'''
#include <QApplication>
#include <QFutureInterface>
#include <QLabel>
#include <QMetaType>
#include <QPushButton>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <chrono>
#include <format>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef __GLIBCXX__
#error This test must use libstdc++ headers
#endif

struct Message { std::string text; };
Q_DECLARE_METATYPE(Message)

class Sender : public QObject {
    Q_OBJECT
signals:
    void delivered(Message value);
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    std::cout << "platform=" << app.platformName().toStdString() << std::endl;

    // 此接口把 std::function 交给 QtCore，能够直接检验两边的标准库 ABI。
    if (!QMetaType::registerConverter<Message, QString>([](const Message& value) {
        return QString::fromStdString(value.text);
    })) return 1;
    if (QVariant::fromValue(Message{"converter passed"}).toString() != "converter passed") return 2;
    std::cout << "std::function converter passed" << std::endl;

    QFutureInterface<void> future;
    future.reportStarted();
    future.reportException(std::make_exception_ptr(std::runtime_error("future exception passed")));
    future.reportFinished();
    bool caught = false;
    try { future.future().waitForFinished(); }
    catch (const std::runtime_error& error) {
        caught = true;
        std::cout << error.what() << std::endl;
    }
    if (!caught) return 3;

    std::vector<int> values{1, 2, 3, 4, 5};
    int sum = 0;
    for (auto chunk : values | std::views::as_const | std::views::chunk(2))
        for (int value : chunk) sum += value;
    if (sum != 15) return 4;
    const std::chrono::zoned_time local{std::chrono::current_zone(), std::chrono::system_clock::now()};
    const auto stamp = std::format("{:%Y-%m-%d %H:%M:%S %Z}", local);

    QWidget window;
    window.setWindowTitle("Qt + Clang + libstdc++");
    window.resize(560, 180);
    auto* layout = new QVBoxLayout(&window);
    layout->addWidget(new QLabel("Qt 6.12 / Clang 23 / libstdc++ 16"));
    layout->addWidget(new QLabel(QString::fromStdString(stamp)));
    auto* button = new QPushButton("Signal and slot test");
    layout->addWidget(button);
    int clicks = 0;
    QObject::connect(button, &QPushButton::clicked, &window, [&] { ++clicks; });

    Sender sender;
    bool delivered = false;
    QObject::connect(&sender, &Sender::delivered, &window, [&](Message value) {
        delivered = value.text == "queued signal passed";
        std::cout << value.text << std::endl;
    }, Qt::QueuedConnection);
    window.show();
    QTimer::singleShot(100, &window, [&] {
        button->click();
        emit sender.delivered(Message{"queued signal passed"});
    });
    QTimer::singleShot(500, &window, [&] {
        const bool saved = window.grab().save(app.platformName() + "-window.png");
        std::cout << "clicks=" << clicks << " screenshot=" << saved << std::endl;
        app.exit(delivered && clicks == 1 && saved ? 0 : 5);
    });
    return app.exec();
}

#include "main.moc"
'''


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    _, clang, _, sdk, _, includes, library = prepare()
    source = OUTPUT / "qtbase-src"
    build = OUTPUT / "qtbase-build"
    install = OUTPUT / "qtbase-install"
    test = OUTPUT / "qt-test"
    diagnostics = OUTPUT / "qt-diagnostics"
    test.mkdir(exist_ok=True)
    diagnostics.mkdir(exist_ok=True)

    if not source.is_dir():
        run("qt-clone", ["git", "clone", "--depth", "1", "--branch", "v6.12.0",
                         "https://github.com/qt/qtbase.git", source])
    cxxflags = ["--no-default-config", "-nostdinc++", "-stdlib=libstdc++", "--rtlib=compiler-rt"]
    for include in includes:
        cxxflags.extend(["-isystem", str(include)])
    ldflags = ["-fuse-ld=lld", "-nostdlib++", str(library), "-Wl,-rpath," + str(library.parent)]
    toolchain = OUTPUT / "qt-toolchain.cmake"
    toolchain.write_text("\n".join([
        f'set(CMAKE_{language}_COMPILER {json.dumps(str(compiler))} CACHE FILEPATH "")'
        for language, compiler in (("C", clang.with_name("clang")), ("CXX", clang),
                                   ("OBJC", clang.with_name("clang")), ("OBJCXX", clang))
    ] + [
        f'set(CMAKE_OSX_SYSROOT {json.dumps(sdk)} CACHE PATH "")',
        'set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")',
        'set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "")',
        'set(CMAKE_C_FLAGS_INIT "--no-default-config")',
        'set(CMAKE_OBJC_FLAGS_INIT "--no-default-config")',
        f'set(CMAKE_CXX_FLAGS_INIT {json.dumps(shlex.join(cxxflags))})',
        f'set(CMAKE_OBJCXX_FLAGS_INIT {json.dumps(shlex.join(cxxflags))})',
        *[f'set(CMAKE_{kind}_LINKER_FLAGS_INIT {json.dumps(shlex.join(ldflags))})'
          for kind in ("EXE", "SHARED", "MODULE")],
        ''
    ]), encoding="utf-8")
    shutil.copy2(toolchain, diagnostics)

    try:
        run("qt-configure", ["cmake", "-S", source, "-B", build, "-G", "Ninja",
                             "-DCMAKE_TOOLCHAIN_FILE=" + str(toolchain),
                             "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_CXX_STANDARD=23",
                             "-DCMAKE_INSTALL_PREFIX=" + str(install),
                             "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DBUILD_SHARED_LIBS=ON",
                             "-DFEATURE_framework=OFF", "-DQT_BUILD_TESTS=OFF",
                             "-DQT_BUILD_EXAMPLES=OFF", "-DQT_BUILD_BENCHMARKS=OFF"])
        run("qt-build", ["cmake", "--build", build, "--parallel", str(os.cpu_count() or 3)])
        run("qt-install", ["cmake", "--install", build])
        for name in ("Core", "Gui", "Widgets"):
            run("qt-dependencies-" + name.lower(), ["otool", "-L", install / "lib" / f"libQt6{name}.dylib"])
        run("qt-dependencies-cocoa", ["otool", "-L", install / "plugins/platforms/libqcocoa.dylib"])

        (test / "main.cpp").write_text(TEST_SOURCE, encoding="utf-8")
        (test / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.21)
project(QtLibstdcxxProbe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Widgets)
qt_add_executable(qt-libstdcxx-probe main.cpp)
target_link_libraries(qt-libstdcxx-probe PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
''', encoding="utf-8")
        run("qt-test-configure", ["cmake", "-S", test, "-B", test / "build", "-G", "Ninja",
                                  "-DCMAKE_TOOLCHAIN_FILE=" + str(toolchain),
                                  "-DCMAKE_PREFIX_PATH=" + str(install)])
        run("qt-test-build", ["cmake", "--build", test / "build", "--verbose"])
        binary = test / "build/qt-libstdcxx-probe"
        run("qt-test-dependencies", ["otool", "-L", binary])
        for platform in ("offscreen", "cocoa"):
            environment = dict(os.environ, QT_QPA_PLATFORM=platform, QT_DEBUG_PLUGINS="1")
            run("qt-test-" + platform, [binary], cwd=test, env=environment, timeout=60)
    finally:
        for name in ("CMakeCache.txt", "config.summary", "configure.cmake", "compile_commands.json"):
            for path in build.glob(name):
                shutil.copy2(path, diagnostics)
        if (build / "CMakeFiles/CMakeConfigureLog.yaml").is_file():
            shutil.copy2(build / "CMakeFiles/CMakeConfigureLog.yaml", diagnostics)
        for name in ("main.cpp", "CMakeLists.txt"):
            if (test / name).is_file():
                shutil.copy2(test / name, diagnostics)


if __name__ == "__main__":
    main()
