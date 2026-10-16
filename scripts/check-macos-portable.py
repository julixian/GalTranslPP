"""独立检测便携目录，不向应用源码插入测试入口。"""

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import time
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("--optional-dependencies", action="store_true")
    args = parser.parse_args()
    if sys.platform != "darwin" or platform.machine() != "arm64":
        parser.error("检测需要真实的 macOS ARM64 环境")
    report = {"platform": platform.platform(), "checks": [], "optional_dependencies": []}
    output = ROOT / "Release/macos-check-report.json"
    output.parent.mkdir(exist_ok=True)

    def record(name, command, *, cwd, timeout=120, group="checks", env=None):
        try:
            result = subprocess.run([str(x) for x in command], cwd=cwd, env=env,
                                    capture_output=True, text=True, timeout=timeout)
            item = dict(name=name, passed=result.returncode == 0, exit_code=result.returncode,
                        stdout=result.stdout, stderr=result.stderr)
        except Exception as error:
            item = dict(name=name, passed=False, error=str(error))
        report[group].append(item)
        output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        print(name, "PASS" if item["passed"] else "FAIL", flush=True)
        return item["passed"]

    # 改变安装位置并包含中文和空格，验证没有偷偷依赖构建机的 Python 路径。
    with tempfile.TemporaryDirectory(prefix="GPP 便携检测 ") as temporary:
        relocated = Path(temporary) / args.package.name
        shutil.copytree(args.package.resolve(), relocated, symlinks=True)
        directory = relocated / "GalTranslPP.app/Contents/MacOS"
        python_root = directory / "BaseConfig/Python"
        python = python_root / "bin/python3"
        env = {key: value for key, value in os.environ.items()
               if not key.startswith(("PYTHON", "DYLD_", "QT_"))}
        record("Python 标准库、动态扩展和中文路径", [python, "-I", "-c", '''
import sys, ssl, sqlite3, ctypes, json, lzma, bz2, zlib, pathlib, importlib.metadata
assert sys.version_info[:2] == (3, 12)
assert pathlib.Path(sys.prefix).resolve() == pathlib.Path(sys.executable).resolve().parents[1]
p = pathlib.Path('中文 文件.json')
p.write_text(json.dumps({'message': '你好'}), encoding='utf-8')
assert json.loads(p.read_text(encoding='utf-8'))['message'] == '你好'
p.unlink()
ssl.create_default_context()
sqlite3.connect(':memory:').execute('select 1').fetchone()
print(sys.version, sys.executable, sys.path, sep='\\n')
'''], cwd=directory, env=env)
        record("pip 可用", [python, "-I", "-m", "pip", "--version"], cwd=directory, env=env)
        record("pip 安装纯 Python 及原生 wheel", [python, "-I", "-m", "pip", "install", "packaging", "orjson"],
               cwd=directory, env=env, timeout=300)
        record("安装后导入", [python, "-I", "-c", "import packaging, orjson; print(orjson.loads(orjson.dumps({'name':'中文'})))"],
               cwd=directory, env=env)
        record("pip 依赖一致性", [python, "-I", "-m", "pip", "check"], cwd=directory, env=env)
        record("7-Zip 动态库入口", [python, "-I", "-c",
               "import ctypes; lib=ctypes.CDLL('./7z.so'); print(lib.CreateObject)"], cwd=directory, env=env)
        record("Python 子进程", [python, "-I", "-c", '''
import subprocess, sys
p = subprocess.run([sys.executable, '-X', 'utf8', '-u', '-'], input="print('中文输出')",
                   text=True, capture_output=True, check=True)
assert p.stdout.strip() == '中文输出'
print(p.stdout)
'''], cwd=directory, env=env)
        worker = directory / "check_multiprocessing.py"
        worker.write_text('''import multiprocessing as mp
def work(value): return value * value
if __name__ == '__main__':
    with mp.get_context('spawn').Pool(2) as pool:
        assert pool.map(work, [2, 3]) == [4, 9]
    print('spawn and shutdown passed')
''', encoding="utf-8")
        record("multiprocessing 启动及退出", [python, "-I", worker], cwd=directory, env=env)
        worker.unlink()
        log = ROOT / "target/macos-logs/gui-startup.log"
        log.parent.mkdir(parents=True, exist_ok=True)
        with log.open("wb") as stream:
            process = subprocess.Popen([directory / "GalTranslPP_GUI"], cwd=directory, env=env,
                                       stdout=stream, stderr=subprocess.STDOUT)
            try:
                time.sleep(12)
                alive = process.poll() is None
                report["checks"].append(dict(name="重定位后的 GUI 启动", passed=alive, exit_code=process.poll()))
                if alive:
                    subprocess.run(["screencapture", "-x", str(ROOT / "Release/macos-preview.png")], check=False)
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=15)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
        if args.optional_dependencies:
            config = tomllib.loads((ROOT / "build-macos.toml").read_text(encoding="utf-8"))
            for dependency in config["python"]["optional_dependencies"]:
                # 每个功能独立检测，失败保留 pip 原始输出；不把测试依赖塞进发布包。
                clone = Path(temporary) / ("Python-" + dependency)
                shutil.copytree(python_root, clone, symlinks=True)
                executable = clone / "bin/python3"
                installed = record(dependency + " 安装", [executable, "-I", "-m", "pip", "install", dependency],
                                   cwd=clone, env=env, timeout=1200, group="optional_dependencies")
                if installed:
                    record(dependency + " 导入", [executable, "-I", "-c", f"import {dependency}; print({dependency}.__file__)"],
                           cwd=clone, env=env, timeout=180, group="optional_dependencies")
                    record(dependency + " pip check", [executable, "-I", "-m", "pip", "check"],
                           cwd=clone, env=env, group="optional_dependencies")
                shutil.rmtree(clone)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    # 可选模型功能单独报告，不把“应用可启动”误报成“所有模型都已验证”。
    if not all(item["passed"] for item in report["checks"]):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
