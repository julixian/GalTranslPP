"""检查本项目四个 Qt TS 文件的翻译，仅使用 Python 标准库，不修改 TS 文件。

运行：python CheckTranslations.py
保存完整报告：python CheckTranslations.py --report target/ts-translations.md
明确问题返回 1，文件读取/解析失败返回 2；只有人工复核项时返回 0。
"""

import argparse
from collections import Counter
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parent
TS_FILES = (
    "GalTranslPP/qt_gpp_en.ts",
    "GPPCLI/qt_gppcli_en.ts",
    "GPPGUI/qt_gppgui_en.ts",
    "Updater/qt_gppupdater_en.ts",
)
PLACEHOLDER = re.compile(r"%L?(?:[1-9][0-9]?|n)(?![0-9])")
CHINESE = re.compile(r"[\u3400-\u4dbf\u4e00-\u9fff\U00020000-\U000323af]")


def text_of(element):
    return "" if element is None else "".join(element.itertext())


def check_file(relative_path):
    path = ROOT / relative_path
    content = path.read_text(encoding="utf-8-sig")
    root = ET.fromstring(content)
    if root.tag != "TS":
        raise ValueError("不是 Qt TS 文件")
    # 报告链接定位到 TS 中的 message，方便直接编辑译文。
    message_lines = iter(
        line for line, value in enumerate(content.splitlines(), 1)
        if re.search(r"<message(?:\s|>)", value)
    )
    issues = []
    active = skipped = 0
    filename = ""
    source_lines = {}
    english = root.get("language", "").lower().startswith("en")
    for context in root.findall("context"):
        for message in context.findall("message"):
            ts_line = next(message_lines, 1)
            locations = []
            # Qt 的 filename 可省略，带正负号的 line 相对该源码文件上一次位置。
            for location in message.findall("location"):
                filename = location.get("filename", filename)
                value = location.get("line", "")
                if value:
                    line = int(value)
                    if value.startswith(("+", "-")):
                        line += source_lines.get(filename, 0)
                    source_lines[filename] = line
                    locations.append(f"{filename}:{line}")
                else:
                    locations.append(filename)
            translation = message.find("translation")
            status = "" if translation is None else translation.get("type", "")
            if status in {"obsolete", "vanished"}:
                skipped += 1
                continue
            active += 1
            source = text_of(message.find("source"))
            errors, reviews = [], []
            forms = []
            if translation is None:
                errors.append("缺少 translation")
            else:
                if status == "unfinished":
                    errors.append("标记为 unfinished")
                plural = translation.findall("numerusform")
                if message.get("numerus") == "yes" and not plural:
                    errors.append("缺少复数译文 numerusform")
                for form in plural or [translation]:
                    forms.extend(text_of(variant) for variant in form.findall("lengthvariant") or [form])
                expected = Counter(PLACEHOLDER.findall(source))
                for index, translated in enumerate(forms, 1):
                    label = f"第 {index} 个译文：" if len(forms) > 1 else ""
                    if not translated.strip():
                        errors.append(label + "空译文")
                        continue
                    actual = Counter(PLACEHOLDER.findall(translated))
                    missing, extra = expected - actual, actual - expected
                    if missing or extra:
                        details = []
                        if missing:
                            details.append("缺少 " + ", ".join(missing.elements()))
                        if extra:
                            details.append("多出 " + ", ".join(extra.elements()))
                        errors.append(label + "占位符不一致（" + "；".join(details) + "）")
                    if english and CHINESE.search(translated):
                        reason = "译文与中文原文相同" if translated.strip() == source.strip() else "英文译文中保留中文"
                        reviews.append(label + reason + "（示例、路径或专名可保留，需人工判断）")
            if errors or reviews:
                issues.append({
                    "path": path,
                    "line": ts_line,
                    "context": text_of(context.find("name")),
                    "locations": locations,
                    "source": source,
                    "forms": forms,
                    "errors": errors,
                    "reviews": reviews,
                })
    return active, skipped, issues


def main():
    # Windows 下输出重定向时也保持 UTF-8，避免中文报告乱码。
    sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--report", type=Path, help="另存 Markdown 完整报告")
    args = parser.parse_args()
    report = ["# Qt TS 翻译检查", "", "只读取当前四个 TS 文件，跳过 obsolete/vanished；中文残留需要人工复核。", ""]
    all_issues = []
    failed = False
    for relative_path in TS_FILES:
        try:
            active, skipped, issues = check_file(relative_path)
        except (OSError, ET.ParseError, ValueError) as error:
            report.append(f"- {relative_path}：读取/解析失败：{error}")
            failed = True
            continue
        all_issues.extend(issues)
        errors = sum(bool(issue["errors"]) for issue in issues)
        reviews = sum(bool(issue["reviews"]) for issue in issues)
        report.append(f"- {relative_path}：有效 {active} 条，忽略旧条目 {skipped} 条；明确问题 {errors} 条，人工复核 {reviews} 条。")
    report.extend(["", "脚本检查翻译状态、空译文、复数形式、Qt 占位符和英文译文中的中文；措辞、语义和术语质量仍需人工审阅。", ""])
    for issue in all_issues:
        report.extend([
            f"## [{issue['path'].name}:{issue['line']}]({issue['path'].as_posix()}:{issue['line']}) · {issue['context']}",
            "",
        ])
        report.extend("- 明确问题：" + reason for reason in issue["errors"])
        report.extend("- 人工复核：" + reason for reason in issue["reviews"])
        if issue["locations"]:
            report.append("- 源码位置：" + "，".join(issue["locations"]))
        report.extend(["", "原文：", "", "````text", issue["source"], "````", "", "译文：", "", "````text", "\n---\n".join(issue["forms"]), "````", ""])
    if not all_issues and not failed:
        report.append("未发现上述问题。")
    output = "\n".join(report) + "\n"
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(output, encoding="utf-8")
        print("\n".join(report[:10]))
        print(f"\n完整报告：{args.report.resolve()}")
    else:
        print(output, end="")
    return 2 if failed else 1 if any(issue["errors"] for issue in all_issues) else 0


if __name__ == "__main__":
    raise SystemExit(main())
