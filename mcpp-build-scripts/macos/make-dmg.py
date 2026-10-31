"""把已构建的应用包装成拖拽安装 DMG，并验证映像中的链接及执行权限。"""

import argparse
import hashlib
import json
import os
import plistlib
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def run(command):
    command = [str(value) for value in command]
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)


def symlinks(directory):
    return {
        path.relative_to(directory).as_posix(): os.readlink(path)
        for path in directory.rglob("*") if path.is_symlink()
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path, help="包含 GalTranslPP.app 的目录")
    parser.add_argument("--check", action="store_true", help="复制映像中的应用并执行便携检测")
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("DMG 制作和挂载检测需要 macOS")

    app = args.package.resolve() / "GalTranslPP.app"
    if not app.is_dir():
        parser.error("找不到应用包：" + str(app))
    release = ROOT / "Release"
    release.mkdir(exist_ok=True)
    image = release / "GPPGUI_MACOS_ARM64.dmg"
    report_path = release / "macos-dmg-report.json"
    report = {"image": str(image), "checks": []}

    try:
        # 直接运行成功不代表下载后能通过 Gatekeeper；分别记录包签名与系统发布策略的结果。
        report["signature_assessments"] = []
        for name, command in [
            ("应用包签名", ["codesign", "--verify", "--deep", "--strict", "--verbose=4", str(app)]),
            ("Gatekeeper", ["spctl", "--assess", "--type", "execute", "--verbose=4", str(app)]),
        ]:
            result = subprocess.run(command, capture_output=True, text=True)
            report["signature_assessments"].append({
                "name": name,
                "passed": result.returncode == 0,
                "exit_code": result.returncode,
                "stdout": result.stdout,
                "stderr": result.stderr,
            })
            print(name, "PASS" if result.returncode == 0 else "FAIL", flush=True)
            print(result.stdout + result.stderr, flush=True)
        report["release_ready"] = all(item["passed"] for item in report["signature_assessments"])
        if not report["signature_assessments"][0]["passed"]:
            raise RuntimeError("应用包签名校验失败，停止生成 DMG；需要先修正应用包布局并重新签名。")
        with tempfile.TemporaryDirectory(prefix="gpp-dmg-") as temporary:
            temporary = Path(temporary)
            staging = temporary / "image"
            staging.mkdir()
            # ditto 保留应用包中的相对符号链接、权限和 macOS 文件属性。
            run(["ditto", app, staging / app.name])
            (staging / "Applications").symlink_to("/Applications", target_is_directory=True)
            run([
                "hdiutil", "create", "-volname", "GalTransl++", "-srcfolder", staging,
                "-fs", "HFS+", "-format", "UDZO", "-ov", image,
            ])
            run(["hdiutil", "verify", image])
            report["checks"].append({"name": "DMG 校验", "passed": True})

            mount = temporary / "mounted"
            mount.mkdir()
            attachment = subprocess.check_output([
                "hdiutil", "attach", "-readonly", "-nobrowse", "-plist",
                "-mountpoint", str(mount), str(image),
            ])
            try:
                entities = plistlib.loads(attachment)["system-entities"]
                mount = Path(next(entity["mount-point"] for entity in entities if "mount-point" in entity))
                mounted_app = mount / app.name
                links = symlinks(mounted_app)
                report["symlink_count"] = len(links)
                report["checks"].append({
                    "name": "应用符号链接完整保留",
                    "passed": links == symlinks(app),
                })
                report["checks"].append({
                    "name": "Applications 拖拽目标",
                    "passed": (mount / "Applications").is_symlink()
                    and os.readlink(mount / "Applications") == "/Applications",
                })
                executables = ["GalTranslPP_GUI", "GalTranslPP_CLI", "Updater", "BaseConfig/Python/bin/python3"]
                report["checks"].append({
                    "name": "应用及 Python 执行权限",
                    "passed": all(os.access(mounted_app / "Contents/MacOS" / name, os.X_OK) for name in executables),
                })

                if args.check:
                    # 当前便携版会写配置，按实际安装流程先复制到可写目录，再启动检测。
                    installed = temporary / "安装检测"
                    run(["ditto", mounted_app, installed / app.name])
                    result = subprocess.run([
                        sys.executable, str(Path(__file__).with_name("check-macos-portable.py")), str(installed),
                    ], cwd=ROOT)
                    report["checks"].append({
                        "name": "从 DMG 复制后运行便携检测",
                        "passed": result.returncode == 0,
                    })
            finally:
                run(["hdiutil", "detach", mount])

        with image.open("rb") as stream:
            report["sha256"] = hashlib.file_digest(stream, "sha256").hexdigest()
        report["size"] = image.stat().st_size
        if not all(check["passed"] for check in report["checks"]):
            raise RuntimeError("DMG 检测失败，详情见 " + str(report_path))
        print("DMG:", image, flush=True)
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
