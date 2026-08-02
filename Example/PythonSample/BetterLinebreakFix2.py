import gpp_plugin_api as gpp
from pathlib import Path
from dataclasses import dataclass
import json
import re
from typing import Callable, Optional, cast

tokenizeResult = tuple[list[tuple[str, str]], list[tuple[str, str]]]
tokenizeFuncType = Callable[[str], tokenizeResult]

# C++ 会把 logger 注入到当前模块。
logger = cast(gpp.spdlogLogger, None)

# Text 插件可以选择让 C++ 注入分词器函数。可配置 sourceLang / targetLang 两套。
# 可选后端: "MeCab" / "spaCy" / "Stanza" / "pkuseg"。
# MeCab: 需要 sourceLangMecabDictDir 或 targetLangMecabDictDir。
# spaCy: 需要 sourceLangSpaCyModelName 或 targetLangSpaCyModelName，例如 "ja_core_news_trf" / "zh_core_web_trf"。
# Stanza: 需要 sourceLangStanzaLang 或 targetLangStanzaLang，例如 "ja" / "zh"。
# pkuseg: 不需要额外模型名。
targetLangUseTokenizer = True
targetLangTokenizerBackend = "spaCy"
targetLangSpaCyModelName = "zh_core_web_trf"
targetLangTokenizeFunc = cast(tokenizeFuncType, None)

tokenizeCachePath = Path("tokenizeCache_betterLinebreakFix.json")
tokenizeCache = {}

excludePuncts = { "『", "「", "“", "‘", "'", "《", "〈", "（", "【", "〔", "〖", "≪" }
maxLineLength = 24
linebreakSymbol = "\\n"
dialogueNewLinePrefix = "\u2002"

@dataclass(frozen=True)
class InlineTokenRule:
    pattern: re.Pattern[str]
    displayWidth: Callable[[re.Match[str]], int]

# 行内特殊格式都在这里配置：匹配到的整段原文不会被分词或拆开。
# 格式变更时，只需修改/增加规则及其显示宽度算法。规则重叠时前者优先。
inlineTokenRules = (
    InlineTokenRule(
        re.compile(r"\{([^\{\}:]+?):([^\{\}:]+?)\}"),
        lambda match: len(match.group(1)),
    ),
    InlineTokenRule(
        re.compile(r"\{\{text_area_layer\}\}"),
        lambda match: 1,
    ),
    InlineTokenRule(
        re.compile(r"\{\{(.+?)\}\}"),
        lambda match: 2,
    ),
)


def findNextInlineToken(
    text: str, start: int = 0
) -> Optional[tuple[InlineTokenRule, re.Match[str]]]:
    result = None
    for rule in inlineTokenRules:
        match = rule.pattern.search(text, start)
        if match is None:
            continue
        if match.start() == match.end():
            raise ValueError(f"Inline token rule must not match empty text: {rule.pattern.pattern}")
        if result is None or match.start() < result[1].start():
            result = (rule, match)
    return result

def splitPlainTextIntoTokens(text: str) -> list[str]:
    tokens = []
    if text in tokenizeCache:
        wordPosVec = tokenizeCache[text]
        tokens = gpp.utils.splitIntoTokens(wordPosVec, text)
    else:
        wordPosVec, _ = targetLangTokenizeFunc(text)
        tokens = gpp.utils.splitIntoTokens(wordPosVec, text)
        tokenizeCache[text] = wordPosVec
    return tokens

def splitIntoTokens(text: str) -> list[str]:
    """仅对普通文本分词，行内特殊格式作为不可拆分的 token 保留原文。"""
    tokens = []
    cursor = 0
    while token := findNextInlineToken(text, cursor):
        _, match = token
        if match.start() > cursor:
            tokens.extend(splitPlainTextIntoTokens(text[cursor:match.start()]))
        tokens.append(match.group(0))
        cursor = match.end()
    if cursor < len(text):
        tokens.extend(splitPlainTextIntoTokens(text[cursor:]))
    return tokens

def displayLength(text: str) -> int:
    """计算引擎实际显示的字符数，而不是原始字符串长度。"""
    width = 0
    cursor = 0
    while token := findNextInlineToken(text, cursor):
        rule, match = token
        width += match.start() - cursor
        width += rule.displayWidth(match)
        cursor = match.end()
    return width + len(text) - cursor

def isDialogue(text: str) -> bool:
    text = text.strip()
    pairs = {
        "「": "」",
        "『": "』",
        "（": "）",
    }

    if len(text) < 2:
        return False

    left = text[0]
    right = pairs.get(left)

    if right is None or text[-1] != right:
        return False

    # 必须是同一组符号完整包住整句。
    # 例如：（xxx）xxxx（xxx）
    # 虽然首尾也是 （），但中间提前闭合过，所以不是 dialogue。
    depth = 0
    for i, ch in enumerate(text):
        if ch == left:
            depth += 1
        elif ch == right:
            depth -= 1

            if depth == 0 and i != len(text) - 1:
                return False

            if depth < 0:
                return False

    return depth == 0

def hasLongPart(transView: str):
    maxLen = maxLineLength
    segments = transView.split(linebreakSymbol)
    segments = [s.strip() for s in segments if s.strip()]
    if not segments:
        return False
    dialogue = isDialogue(transView)
    for index, segment in enumerate(segments):
        if dialogue:
            if index == 0:
                maxLen = maxLineLength
            else:
                maxLen = maxLineLength - 1
        else:
            maxLen = maxLineLength
        if displayLength(segment) > maxLen:
            return True
    return False


def processSentence(se: gpp.Sentence):
    transView = se.transview
    if not hasLongPart(transView):
        return
    segments = transView.split(linebreakSymbol)
    segments = [s.strip() for s in segments if s.strip()]
    transView = "".join(segments)
    maxLen = maxLineLength
    tokens = splitIntoTokens(transView)
    if not tokens:
        return
    
    dialogue = isDialogue(transView)
    newLines = []
    currentLine = tokens[0]
    residualTokens = tokens[1:]

    for index, currentToken in enumerate(residualTokens):
        if dialogue:
            if not newLines:
                maxLen = maxLineLength
            else:
                maxLen = maxLineLength - 1
        else:
            maxLen = maxLineLength
        if displayLength(currentLine) + displayLength(currentToken) <= maxLen:
            if index + 1 < len(residualTokens):
                nextToken = residualTokens[index + 1]
                if displayLength(currentLine) + displayLength(currentToken) + displayLength(nextToken) > maxLen:
                    if currentToken in excludePuncts:
                        # c
                        #『\n...
                        newLines.append(currentLine)
                        currentLine = currentToken
                        continue
                    if gpp.utils.isAllPunctuation(nextToken) and nextToken not in excludePuncts:
                        if any(currentLine.endswith(excludePunct) for excludePunct in excludePuncts):
                            #  cccc  nnn
                            #『word\n。』
                            newLines.append(currentLine[:-1])
                            currentLine = currentLine[-1] + currentToken
                        else:
                            # cccc  n
                            # word\n，
                            newLines.append(currentLine)
                            currentLine = currentToken
                        continue
            currentLine += currentToken
        else:
            newLines.append(currentLine)
            currentLine = currentToken
    newLines.append(currentLine)
    
    se.transview = (linebreakSymbol + dialogueNewLinePrefix).join(newLines) if dialogue else linebreakSymbol.join(newLines)

def linkLine(se: gpp.Sentence):
    transView = se.transview
    dialogue = isDialogue(transView)
    maxLen = maxLineLength
    segments = transView.split(linebreakSymbol)
    segments = [s.strip() for s in segments if s.strip()]
    
    if not segments:
        return

    newLines = []
    currentLine = segments[0]

    for currentSegment in segments[1:]:
        if dialogue:
            if not newLines:
                maxLen = maxLineLength
            else:
                maxLen = maxLineLength - 1
        else:
            maxLen = maxLineLength
        if displayLength(currentLine) + displayLength(currentSegment) <= maxLen:
            currentLine += currentSegment
        else:
            newLines.append(currentLine)
            currentLine = currentSegment

    newLines.append(currentLine)
    se.transview = (linebreakSymbol + dialogueNewLinePrefix).join(newLines) if dialogue else linebreakSymbol.join(newLines)
    if se.orig.startswith("　") and not se.transview.startswith("　"):
        se.transview = "　" + se.transview



def init(projectDir: Path):
    logger.info(f"BetterLinebreakFix 初始化成功")
    global tokenizeCachePath, tokenizeCache
    tokenizeCachePath = projectDir / "other_cache" / tokenizeCachePath
    if not tokenizeCachePath.parent.exists():
        tokenizeCachePath.parent.mkdir(parents=True, exist_ok=True)
    if tokenizeCachePath.exists():
        with open(tokenizeCachePath, 'r', encoding='utf-8') as f:
            tokenizeCache = json.load(f)

def dPostRun(se: gpp.Sentence):
    try:
        if (se.filename == "cheat.sal.txt.json" or se.filename == "himekuri.sal.txt.json"):
            return
        if not gpp.utils.hasCJK(se.transview) or se.orig.startswith("　　"):
            return
        processSentence(se)
        linkLine(se)
    except Exception as e:
        logger.error(f"Error during BetterLinebreakFix dPostRun(): {e}")


def unload():
    with open(tokenizeCachePath, 'w', encoding='utf-8') as f:
        json.dump(tokenizeCache, f, ensure_ascii=False, indent=2)
    logger.info(f"BetterLinebreakFix 卸载成功，tokenizeCache 已保存至 {tokenizeCachePath}")
    
