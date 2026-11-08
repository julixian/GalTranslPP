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
import xml.etree.ElementTree as elementTree


kScriptRoot = Path(__file__).resolve().parent
kTSFiles = (
    "GalTranslPP/qt_gpp_en.ts",
    "GPPCLI/qt_gppcli_en.ts",
    "GPPGUI/qt_gppgui_en.ts",
    "Updater/qt_gppupdater_en.ts",
)
kPlaceholder = re.compile(r"%L?(?:[1-9][0-9]?|n)(?![0-9])")
kChinese = re.compile(r"[\u3400-\u4dbf\u4e00-\u9fff\U00020000-\U000323af]")


def textOf(element):
    return "" if element is None else "".join(element.itertext())


def checkFile(relativePath):
    path = kScriptRoot / relativePath
    content = path.read_text(encoding="utf-8-sig")
    root = elementTree.fromstring(content)
    if root.tag != "TS":
        raise ValueError("不是 Qt TS 文件")
    # 报告链接定位到 TS 中的 message，方便直接编辑译文。
    messageLines = iter(
        line for line, value in enumerate(content.splitlines(), 1)
        if re.search(r"<message(?:\s|>)", value)
    )
    issues = []
    active = skipped = 0
    filename = ""
    sourceLines = {}
    english = root.get("language", "").lower().startswith("en")
    for context in root.findall("context"):
        for message in context.findall("message"):
            tsLine = next(messageLines, 1)
            locations = []
            # Qt 的 filename 可省略，带正负号的 line 相对该源码文件上一次位置。
            for location in message.findall("location"):
                filename = location.get("filename", filename)
                value = location.get("line", "")
                if value:
                    line = int(value)
                    if value.startswith(("+", "-")):
                        line += sourceLines.get(filename, 0)
                    sourceLines[filename] = line
                    locations.append(f"{filename}:{line}")
                else:
                    locations.append(filename)
            translation = message.find("translation")
            status = "" if translation is None else translation.get("type", "")
            if status in {"obsolete", "vanished"}:
                skipped += 1
                continue
            active += 1
            source = textOf(message.find("source"))
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
                    forms.extend(textOf(variant) for variant in form.findall("lengthvariant") or [form])
                expected = Counter(kPlaceholder.findall(source))
                for index, translated in enumerate(forms, 1):
                    label = f"第 {index} 个译文：" if len(forms) > 1 else ""
                    if not translated.strip():
                        errors.append(label + "空译文")
                        continue
                    actual = Counter(kPlaceholder.findall(translated))
                    missing, extra = expected - actual, actual - expected
                    if missing or extra:
                        details = []
                        if missing:
                            details.append("缺少 " + ", ".join(missing.elements()))
                        if extra:
                            details.append("多出 " + ", ".join(extra.elements()))
                        errors.append(label + "占位符不一致（" + "；".join(details) + "）")
                    if english and kChinese.search(translated):
                        reason = "译文与中文原文相同" if translated.strip() == source.strip() else "英文译文中保留中文"
                        reviews.append(label + reason + "（示例、路径或专名可保留，需人工判断）")
            if errors or reviews:
                issues.append({
                    "path": path,
                    "line": tsLine,
                    "context": textOf(context.find("name")),
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
    allIssues = []
    failed = False
    for relativePath in kTSFiles:
        try:
            active, skipped, issues = checkFile(relativePath)
        except (OSError, elementTree.ParseError, ValueError) as error:
            report.append(f"- {relativePath}：读取/解析失败：{error}")
            failed = True
            continue
        allIssues.extend(issues)
        errors = sum(bool(issue["errors"]) for issue in issues)
        reviews = sum(bool(issue["reviews"]) for issue in issues)
        report.append(f"- {relativePath}：有效 {active} 条，忽略旧条目 {skipped} 条；明确问题 {errors} 条，人工复核 {reviews} 条。")
    report.extend(["", "脚本检查翻译状态、空译文、复数形式、Qt 占位符和英文译文中的中文；措辞、语义和术语质量仍需人工审阅。", ""])
    for issue in allIssues:
        report.extend([
            f"## [{issue['path'].name}:{issue['line']}]({issue['path'].as_posix()}:{issue['line']}) · {issue['context']}",
            "",
        ])
        report.extend("- 明确问题：" + reason for reason in issue["errors"])
        report.extend("- 人工复核：" + reason for reason in issue["reviews"])
        if issue["locations"]:
            report.append("- 源码位置：" + "，".join(issue["locations"]))
        report.extend(["", "原文：", "", "````text", issue["source"], "````", "", "译文：", "", "````text", "\n---\n".join(issue["forms"]), "````", ""])
    if not allIssues and not failed:
        report.append("未发现上述问题。")
    output = "\n".join(report) + "\n"
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(output, encoding="utf-8")
        print("\n".join(report[:10]))
        print(f"\n完整报告：{args.report.resolve()}")
    else:
        print(output, end="")
    return 2 if failed else 1 if any(issue["errors"] for issue in allIssues) else 0


if __name__ == "__main__":
    raise SystemExit(main())
