import argparse
from pathlib import Path

# 现在 SExtractor 的选项 『段落: 强制合并』 与 『addSpace=』 参数组合可能已经能处理这种需求了

DEFAULT_SIGNATURE = "sext_sign"
_LINE_ENDINGS = (
    "\r\n",
    "\n",
    "\r",
    "\v",
    "\f",
    "\x1c",
    "\x1d",
    "\x1e",
    "\x85",
    "\u2028",
    "\u2029",
)


def _content_without_line_ending(line: str) -> str:
    for line_ending in _LINE_ENDINGS:
        if line.endswith(line_ending):
            return line[: -len(line_ending)]
    return line


def remove_signature_lines(
    folder: str | Path,
    encoding: str,
    signature: str = DEFAULT_SIGNATURE,
    *,
    recursive: bool = True,
) -> dict[Path, int]:
    folder = Path(folder)
    if not folder.is_dir():
        raise NotADirectoryError(f"文件夹不存在或不是目录: {folder}")
    if any(line_ending in signature for line_ending in _LINE_ENDINGS):
        raise ValueError("签名不能包含换行符")

    paths = folder.rglob("*") if recursive else folder.iterdir()
    files = sorted(
        (path for path in paths if path.is_file() and not path.is_symlink()),
        key=lambda path: str(path).casefold(),
    )
    changed_files: dict[Path, int] = {}

    for path in files:
        original_bytes = path.read_bytes()
        text = original_bytes.decode(encoding)
        lines = text.splitlines(keepends=True)
        retained_lines = [
            line for line in lines
            if _content_without_line_ending(line) != signature
        ]
        removed_count = len(lines) - len(retained_lines)
        if removed_count == 0:
            continue

        path.write_bytes("".join(retained_lines).encode(encoding))
        changed_files[path] = removed_count

    return changed_files


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="删除指定文件夹内完全等于签名的整行，并保留原有换行符。",
    )
    parser.add_argument("folder", type=Path, help="要遍历的文件夹")
    parser.add_argument("encoding", help="文件编码，例如 utf-8、shift_jis 或 gbk")
    parser.add_argument("signature", nargs="?", default=DEFAULT_SIGNATURE, help="要删除的整行签名")
    parser.add_argument("--no-recursive", action="store_true", help="只处理文件夹直属文件")
    args = parser.parse_args(argv)

    changed_files = remove_signature_lines(
        args.folder,
        args.encoding,
        args.signature,
        recursive=not args.no_recursive,
    )
    removed_count = sum(changed_files.values())
    print(f"处理完成，修改 {len(changed_files)} 个文件，删除 {removed_count} 行")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
