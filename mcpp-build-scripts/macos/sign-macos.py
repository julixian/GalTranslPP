"""保留便携相对路径，整理包内资源后由内向外进行 ad-hoc 签名。"""

import argparse
import plistlib
import shutil
import subprocess
import sys
from pathlib import Path


def run(command):
    print("+", " ".join(str(value) for value in command), flush=True)
    subprocess.run(command, check=True)


def macho(path):
    if path.is_symlink() or not path.is_file():
        return False
    with path.open("rb") as stream:
        return stream.read(4) in (b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\xfe\xed\xfa\xcf")


def signApp(app):
    contents = app / "Contents"
    directory = contents / "MacOS"
    resources = contents / "Resources"
    resources.mkdir(exist_ok=True)
    # MacOS 是代码目录，普通数据不能在这里被当成嵌套代码签名。
    # 数据仍随应用携带；相对链接让现有 BaseConfig 等路径和写入行为保持一致。
    for name in ("BaseConfig", "SampleProject", "translations"):
        source = directory / name
        if source.is_dir() and not source.is_symlink():
            shutil.move(source, resources / name)
            source.symlink_to("../Resources/" + name, target_is_directory=True)

    with (contents / "Info.plist").open("rb") as stream:
        executable = directory / plistlib.load(stream)["CFBundleExecutable"]
    # 先签 Python 扩展、动态库和辅助程序；主程序在最后签整个 .app 时一并处理。
    for binary in sorted(app.rglob("*")):
        if binary != executable and macho(binary):
            run(["codesign", "--force", "--sign", "-", binary])
    frameworks = sorted(app.rglob("*.framework"), key=lambda path: len(path.parts), reverse=True)
    for framework in frameworks:
        if not framework.is_symlink():
            run(["codesign", "--force", "--sign", "-", framework])
    run(["codesign", "--force", "--sign", "-", app])
    run(["codesign", "--verify", "--deep", "--strict", "--verbose=4", app])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path)
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("应用签名需要 macOS")
    signApp(args.app.resolve())


if __name__ == "__main__":
    main()
