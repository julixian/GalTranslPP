import argparse
from pathlib import Path
import shutil
import stat
import sys


ROOT = Path(__file__).resolve().parent
TARGET_DIRS = (
    # ".vs",
    "GalTranslPP/x86",
    "GalTranslPP/x64",
    "GPPCLI/x86",
    "GPPCLI/x64",
    "GPPGUI/x86",
    "GPPGUI/x64",
    "GPPVersion/x86",
    "GPPVersion/x64",
    "Updater/x86",
    "Updater/x64"
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
    if not path.is_dir() or is_link(path) or not resolved.is_relative_to(root) or resolved == root:
        raise ValueError(f"拒绝清理不属于项目的 VS 生成目录：{path}")


def clean_vs(root: Path, dry_run: bool = False) -> int:
    root = root.resolve(strict=True)
    targets = [root / relative for relative in TARGET_DIRS if (root / relative).exists()]
    if not targets:
        print("没有找到 VS 生成目录。")
        return 0

    for path in targets:
        # 仅清理清单内的目录，删除前核对其实际位置。
        validate_target(root, path)
        relative = path.relative_to(root)
        if dry_run:
            print(f"将清理：{relative}")
        else:
            print(f"正在清理：{relative}", flush=True)
            shutil.rmtree(path)

    print(f"{'预览' if dry_run else '清理'}完成，共 {len(targets)} 个 VS 生成目录。")
    return len(targets)


def main() -> int:
    parser = argparse.ArgumentParser(description="清理本项目的 .vs 及各成员的 x86、x64 生成目录。")
    parser.add_argument("--dry-run", action="store_true", help="只列出待清理目录，不删除文件")
    args = parser.parse_args()
    try:
        clean_vs(ROOT, args.dry_run)
    except (OSError, ValueError) as error:
        print(f"清理失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
