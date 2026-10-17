"""检查相邻行的疑似串行，只标记问题，不修改译文。

作为 Text 插件：复制到项目目录，在 textPlugins 中加入
"dpostrun:%PROJECT_DIR%/CheckTranslationMixup.py"。

离线检查：python -X utf8 CheckTranslationMixup.py 项目目录 --output 检查结果.json
也可以传入 transl_cache 目录或单个缓存 JSON；离线模式只输出报告。
"""

from __future__ import annotations

import argparse
import json
import unicodedata
from dataclasses import dataclass
from difflib import SequenceMatcher
from functools import lru_cache
from pathlib import Path
from typing import TYPE_CHECKING, cast

if TYPE_CHECKING:
    import gpp_plugin_api as gpp


# 当前句之前、之后各检查多少条。同文件索引 40 的范围为 37～43，不含自身。
neighborLines = 3
# 完整包含邻句译文时，邻句至少包含多少个有效字符，当前译文至少多出多少字。
minMatchChars = 8
minExtraChars = 4
# 默认要求重复片段在当前译文中构成独立的句子，减少正常问答和短语复用的误报。
requireSentenceBoundary = True
# 原文是完整对白，而译文闭合引号外多出邻句时，采用更低的匹配长度。
minDialogueTailChars = 2
# 邻句原文的大部分本来就出现在当前原文中时，视为正常复述，不标记。
sourceOverlapRatio = 0.8

problemPrefix = "疑似串行: "
quotePairs = {"「": "」", "『": "』", "“": "”", "‘": "’", "（": "）"}
sentenceBoundaryChars = "。.!?;" + "".join(
    unicodedata.normalize("NFKC", opening + closing) for opening, closing in quotePairs.items()
)
logger = cast("gpp.spdlogLogger", None)


@dataclass(frozen=True)
class TextRow:
    index: int
    source: str
    translation: str


@lru_cache(maxsize=8192)
def normalizeText(text: str) -> str:
    text = unicodedata.normalize("NFKC", text.replace("<br>", ""))
    return "".join(char for char in text if char.isalnum())


def closingQuotePosition(text: str) -> int:
    opening = text[:1]
    closing = quotePairs.get(opening)
    if closing is None:
        return -1
    depth = 0
    for index, char in enumerate(text):
        if char == opening:
            depth += 1
        elif char == closing:
            depth -= 1
            if depth == 0:
                return index
    return -1


def dialogueTail(source: str, translation: str) -> str:
    source = source.strip()
    sourceEnd = closingQuotePosition(source)
    # 原文引号外本来有叙述时，不使用这条规则。
    if sourceEnd < 0 or normalizeText(source[sourceEnd + 1:]):
        return ""
    translation = translation.strip()
    translationEnd = closingQuotePosition(translation)
    return normalizeText(translation[translationEnd + 1:]) if translationEnd >= 0 else ""


def hasSentenceBoundary(text: str, fragment: str) -> bool:
    text = unicodedata.normalize("NFKC", text.replace("<br>", ""))
    positions = [index for index, char in enumerate(text) if char.isalnum()]
    normalized = "".join(text[index] for index in positions)
    offset = normalized.find(fragment)
    while offset >= 0:
        before = text[:positions[offset]].rstrip(" …—─~〜")
        after = text[positions[offset + len(fragment) - 1] + 1:].lstrip(" …—─~〜")
        if (not before or before[-1] in sentenceBoundaryChars) and (
            not after or after[0] in sentenceBoundaryChars
        ):
            return True
        offset = normalized.find(fragment, offset + 1)
    return False


def findPairProblem(current: TextRow, neighbor: TextRow) -> str | None:
    if current.translation.startswith("(Failed to translate)") or neighbor.translation.startswith("(Failed to translate)"):
        return None
    currentDst = normalizeText(current.translation)
    neighborDst = normalizeText(neighbor.translation)
    if not neighborDst or neighborDst not in currentDst or currentDst == neighborDst:
        return None

    tail = dialogueTail(current.source, current.translation)
    if len(neighborDst) >= minDialogueTailChars and neighborDst in tail:
        reason = "对白引号外包含邻句译文"
    elif len(neighborDst) >= minMatchChars and len(currentDst) - len(neighborDst) >= minExtraChars:
        if requireSentenceBoundary and not hasSentenceBoundary(current.translation, neighborDst):
            return None
        reason = "包含邻句的完整译文"
    else:
        return None

    # 排除重复的叫声、笑声，以及原文本来就有的相同表达。
    if len(neighborDst) >= minMatchChars and len(set(neighborDst)) <= 2:
        return None
    currentSrc = normalizeText(current.source)
    neighborSrc = normalizeText(neighbor.source)
    if not currentSrc or not neighborSrc or neighborSrc in currentSrc:
        return None
    overlap = SequenceMatcher(None, neighborSrc, currentSrc, autojunk=False).find_longest_match().size
    if overlap >= 3 and overlap / len(neighborSrc) >= sourceOverlapRatio:
        return None
    return reason


def problemMessage(current: TextRow, neighbor: TextRow, reason: str) -> str:
    direction = "前方" if neighbor.index < current.index else "后方"
    excerpt = neighbor.translation.replace("<br>", " ")[:80]
    return f"{problemPrefix}{reason}，{direction}句子 {neighbor.index}: {excerpt}"


def sentenceRow(sentence: gpp.Sentence) -> TextRow:
    return TextRow(sentence.index, sentence.preproc, sentence.transraw)


def checkSentence(sentence: gpp.Sentence) -> None:
    if not sentence.transCompleted or sentence.problemAnalyzeDisabled or not sentence.transraw:
        return
    current = sentenceRow(sentence)
    # 只刷新本插件的问题，保留其它插件和内置分析器的结果。
    problems = [problem for problem in sentence.problems if not problem.startswith(problemPrefix)]
    for attribute in ("prev", "next"):
        neighbor = getattr(sentence, attribute)
        for _ in range(neighborLines):
            if neighbor is None or neighbor.filename != sentence.filename:
                break
            if neighbor.transCompleted and not neighbor.problemAnalyzeDisabled and neighbor.transraw:
                row = sentenceRow(neighbor)
                if reason := findPairProblem(current, row):
                    problems.append(problemMessage(current, row, reason))
            neighbor = getattr(neighbor, attribute)
    sentence.problems = problems


def init(projectDir: Path) -> None:
    logger.info(f"CheckTranslationMixup 初始化成功，每句检查前后各 {neighborLines} 条")


def dPostRun(sentence: gpp.Sentence) -> None:
    checkSentence(sentence)
    # 重建时缓存逐句加载，跨批次时后面的译文也可能刚刚到齐，回查受影响的前句。
    previous = sentence.prev
    for _ in range(neighborLines):
        if previous is None:
            break
        checkSentence(previous)
        previous = previous.prev


def unload() -> None:
    # 不保存 Sentence 代理，避免句子数组销毁后仍持有 C++ 指针。
    normalizeText.cache_clear()


def scanCache(path: Path) -> dict:
    directory = path / "transl_cache" if (path / "transl_cache").is_dir() else path
    paths = sorted(directory.rglob("*.json")) if directory.is_dir() else [directory]
    results = []
    unreadable = []
    fileCount = rowCount = 0
    for cachePath in paths:
        try:
            data = json.loads(cachePath.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError) as error:
            # 翻译正在写入时可能暂时不可读，在报告中列出，供下次重新检查。
            unreadable.append({"file": str(cachePath), "error": str(error)})
            continue
        rows = [TextRow(item["index"], item["pre_processed_text"],
            item.get("translated_raw_text", "")) for item in data]
        rows.sort(key=lambda row: row.index)
        fileCount += 1
        rowCount += len(rows)
        for position, current in enumerate(rows):
            for neighbor in rows[max(0, position - neighborLines):position + neighborLines + 1]:
                # 未翻译行不会保存在缓存中，索引距离防止跨过尚未翻译的空档。
                if not 0 < abs(current.index - neighbor.index) <= neighborLines:
                    continue
                if reason := findPairProblem(current, neighbor):
                    results.append({"file": str(cachePath), "index": current.index,
                        "neighbor_index": neighbor.index, "problem": problemMessage(current, neighbor, reason),
                        "source": current.source, "translation": current.translation,
                        "neighbor_source": neighbor.source, "neighbor_translation": neighbor.translation})
    return {"neighbor_lines": neighborLines, "cache_files": fileCount, "cache_rows": rowCount,
        "suspect_rows": len({(item["file"], item["index"]) for item in results}),
        "unreadable_files": unreadable, "results": results}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path, help="项目目录、transl_cache 目录或单个缓存 JSON")
    parser.add_argument("--output", type=Path, help="报告路径；省略时输出到终端")
    args = parser.parse_args()
    report = scanCache(args.path)
    text = json.dumps(report, ensure_ascii=False, indent=2)
    if args.output:
        args.output.write_text(text, encoding="utf-8")
        print(f"已检查 {report['cache_files']} 个文件、{report['cache_rows']} 句，"
            f"疑似串行 {report['suspect_rows']} 句，不可读文件 {len(report['unreadable_files'])} 个；报告：{args.output}")
    else:
        print(text)


if __name__ == "__main__":
    main()
