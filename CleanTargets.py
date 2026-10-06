"""清理本项目七个固定目录的 target 构建缓存，仅使用 Python 标准库。

清理：python CleanTargets.py
预览：python CleanTargets.py --dry-run
始终以脚本所在目录为项目根目录，不清理 mcpp 全局缓存或 x64/x86 目录。
"""

import argparse
from pathlib import Path
import shutil
import stat
import sys


ROOT = Path(__file__).resolve().parent
# 根目录必须排在首位，先清 Ninja 记录，再清成员的生成目录。
TARGET_DIRS = (
    "target",
    "3rdParty/ElaWidgetTools/target",
    "GalTranslPP/target",
    "GPPCLI/target",
    "GPPGUI/target",
    "GPPVersion/target",
    "Updater/target",
)


def is_link(path: Path) -> bool:
    # Windows 的目录联接也属于重解析点，不能跟随它清理仓库外的文件。
    info = path.lstat()
    return stat.S_ISLNK(info.st_mode) or bool(
        getattr(info, "st_file_attributes", 0)
        & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    )


def validate_target(root: Path, path: Path) -> None:
    resolved = path.resolve(strict=True)
    if path.name != "target" or is_link(path) or not resolved.is_relative_to(root) or resolved == root:
        raise ValueError(f"拒绝清理不属于项目的 target 目录：{path}")


def clean_targets(root: Path, dry_run: bool = False) -> int:
    root = root.resolve(strict=True)
    targets = [root / relative for relative in TARGET_DIRS if (root / relative).exists()]
    if not targets:
        print("没有找到 target 构建缓存。")
        return 0

    for path in targets:
        # 实际删除前再次核对路径；删除失败立即停止，避免继续清理成员缓存。
        validate_target(root, path)
        relative = path.relative_to(root)
        if dry_run:
            print(f"将清理：{relative}")
        else:
            print(f"正在清理：{relative}", flush=True)
            shutil.rmtree(path)

    print(f"{'预览' if dry_run else '清理'}完成，共 {len(targets)} 个 target 目录。")
    return len(targets)


def main() -> int:
    parser = argparse.ArgumentParser(description="清理本项目内的 target 构建缓存。")
    parser.add_argument("--dry-run", action="store_true", help="只列出待清理目录，不删除文件")
    args = parser.parse_args()
    try:
        clean_targets(ROOT, args.dry_run)
    except (OSError, ValueError) as error:
        print(f"清理失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
