"""在裁剪环境中实际提取、回填 PDF，不修改应用源码或发布包。"""

import argparse
import importlib.util
import json
import subprocess
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scripts", type=Path)
    args = parser.parse_args()
    scripts = args.scripts.resolve()
    sys.path.insert(0, str(scripts))
    # 与嵌入式解释器一致，关闭自动 site 初始化后只加入当前运行环境的安装目录。
    sys.path.append(str(Path(sys.prefix) / "lib/python3.12/site-packages"))
    assert importlib.util.find_spec("skimage") is None, "不应安装 scikit-image"
    assert importlib.util.find_spec("hyperscan") is None, "不应安装 hyperscan"
    import PDFConverter
    import pymupdf

    PDFConverter._load_babeldoc()
    print("BabelDOC 裁剪模块导入成功", flush=True)
    # 只豁免明确省略的两个依赖，其他元数据冲突仍然使检测失败。
    result = subprocess.run([sys.executable, "-I", "-m", "pip", "check"], text=True, capture_output=True)
    expected = {
        "babeldoc 0.6.4 requires hyperscan, which is not installed.",
        "babeldoc 0.6.4 requires scikit-image, which is not installed.",
    }
    errors = set(result.stdout.lower().splitlines()) - expected - {"no broken requirements found."}
    assert not errors and result.returncode in (0, 1), result.stdout + result.stderr
    print(result.stdout, flush=True)

    document = pymupdf.open()
    page = document.new_page()
    page.insert_text((72, 100), "A short story", fontsize=20)
    page.insert_textbox(
        pymupdf.Rect(72, 150, 520, 300),
        "Alice opened the door and walked into the quiet classroom. "
        "The afternoon sunlight fell across the empty desks. "
        "She put her book on the table and waited for her friend.",
        fontsize=12,
    )
    source = Path("babeldoc-source.pdf")
    document.save(source)
    document.close()
    translations = Path("babeldoc-translations.json")
    success, message = PDFConverter.extract_text_to_json(str(source.resolve()), str(translations.resolve()), "en")
    assert success, message
    lines = json.loads(translations.read_text(encoding="utf-8"))
    assert lines, "未提取到测试 PDF 文本"
    for line in lines:
        line["message"] = line["message"].replace("Alice", "Clara")
    translations.write_text(json.dumps(lines), encoding="utf-8")
    success, message = PDFConverter.reinject_json_to_pdf(
        str(source.resolve()), str(translations.resolve()), str(Path("babeldoc-output").resolve()), "en", True,
    )
    assert success, message
    mono = Path("babeldoc-output") / source.name
    dual = mono.with_name(source.stem + ".dual.pdf")
    assert mono.is_file() and dual.is_file(), message
    with pymupdf.open(mono) as result:
        assert "Clara" in "".join(page.get_text() for page in result), "回填 PDF 中没有找到替换后的文本"
    print(f"提取 {len(lines)} 段，回填单语、双语 PDF 成功", flush=True)


if __name__ == "__main__":
    main()
