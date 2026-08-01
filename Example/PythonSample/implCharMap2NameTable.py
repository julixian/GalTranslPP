import json
import sys
from collections.abc import Mapping, Sequence
from pathlib import Path

import tomlkit
from tomlkit.items import Array


def map_text(text: str, char_map: Mapping[str, str]) -> str:
    """
    对字符串中的每个字符应用映射表。
    如果字符存在于 char_map 的 key 中，则替换成对应 value。
    否则保持不变。
    """
    return "".join(char_map.get(ch, ch) for ch in text)


def _resolve_path(project_dir: Path, path: str | Path) -> Path:
    path = Path(path)
    return path if path.is_absolute() else project_dir / path


def convert_name_table(
    project_dir: str | Path,
    *,
    input_path: str | Path = "NameTable.toml",
    char_map_path: str | Path = "charMap.json",
    output_path: str | Path = "NameTableMapped.toml",
) -> Path:
    project_dir = Path(project_dir)
    input_toml_path = _resolve_path(project_dir, input_path)
    char_map_json_path = _resolve_path(project_dir, char_map_path)
    output_toml_path = _resolve_path(project_dir, output_path)

    # tomlkit 会保留注释、键顺序和原有排版。
    with input_toml_path.open("r", encoding="utf-8") as f:
        name_table = tomlkit.load(f)

    # 读取 charMap.json
    with char_map_json_path.open("r", encoding="utf-8") as f:
        char_map = json.load(f)

    # 处理每个人名
    for original_name, value in name_table.items():
        if not isinstance(value, Array):
            continue

        if len(value) == 0:
            continue

        translated_name = value[0]

        if not isinstance(translated_name, str):
            continue

        mapped_name = map_text(translated_name, char_map)
        value[0] = mapped_name

    with output_toml_path.open("w", encoding="utf-8") as f:
        tomlkit.dump(name_table, f)
    return output_toml_path


def main(argv: Sequence[str] | None = None) -> int:
    """Command-line entry point compatible with the original argument order."""
    args = list(sys.argv[1:] if argv is None else argv)
    input_path = args[0] if len(args) >= 1 else "NameTable.toml"
    char_map_path = args[1] if len(args) >= 2 else "charMap.json"
    output_path = args[2] if len(args) >= 3 else "NameTableMapped.toml"

    resolved_output_path = convert_name_table(
        Path.cwd(),
        input_path=input_path,
        char_map_path=char_map_path,
        output_path=output_path,
    )

    print(f"处理完成，已输出到: {resolved_output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
