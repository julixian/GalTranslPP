import json
import sys
from pathlib import Path
import tomllib


def map_text(text: str, char_map: dict) -> str:
    """
    对字符串中的每个字符应用映射表。
    如果字符存在于 char_map 的 key 中，则替换成对应 value。
    否则保持不变。
    """
    return "".join(char_map.get(ch, ch) for ch in text)


def toml_quote(s: str) -> str:
    """
    生成 TOML 双引号字符串。
    json.dumps 的输出可兼容这里的 TOML 基本字符串需求。
    """
    return json.dumps(s, ensure_ascii=False)


def write_toml(data: dict, output_path: Path):
    """
    将形如:
    {
        "円": ["圆", 6138],
        "野乃": ["野乃", 2959]
    }
    的数据写回 TOML。
    """
    lines = []

    for key, value in data.items():
        if isinstance(value, list):
            items = []
            for item in value:
                if isinstance(item, str):
                    items.append(toml_quote(item))
                else:
                    items.append(str(item))

            line = f"{toml_quote(key)} = [{', '.join(items)}]"
            lines.append(line)
        else:
            # 如果遇到非 list 值，也简单写出
            if isinstance(value, str):
                line = f"{toml_quote(key)} = {toml_quote(value)}"
            else:
                line = f"{toml_quote(key)} = {value}"
            lines.append(line)

    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    # 默认文件名
    input_toml_path = Path("NameTable.toml")
    char_map_path = Path("charMap.json")
    output_toml_path = Path("NameTableMapped.toml")

    # 支持命令行参数:
    # python map_names.py input.toml charMap.json output.toml
    if len(sys.argv) >= 2:
        input_toml_path = Path(sys.argv[1])
    if len(sys.argv) >= 3:
        char_map_path = Path(sys.argv[2])
    if len(sys.argv) >= 4:
        output_toml_path = Path(sys.argv[3])

    # 读取 TOML
    with input_toml_path.open("rb") as f:
        name_table = tomllib.load(f)

    # 读取 charMap.json
    with char_map_path.open("r", encoding="utf-8") as f:
        char_map = json.load(f)

    # 处理每个人名
    for original_name, value in name_table.items():
        if not isinstance(value, list):
            continue

        if len(value) == 0:
            continue

        translated_name = value[0]

        if not isinstance(translated_name, str):
            continue

        mapped_name = map_text(translated_name, char_map)
        value[0] = mapped_name

    # 写出新的 TOML
    write_toml(name_table, output_toml_path)

    print(f"处理完成，已输出到: {output_toml_path}")


if __name__ == "__main__":
    main()
