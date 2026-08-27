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

lineEndProhibitedPuncts = { "『", "「", "“", "‘", "《", "〈", "（", "【", "〔", "〖", "≪" }
symmetricQuoteMarks = { "'", '"', "＇", "＂" }
kGameMaxLineLength = 29
kCommonMaxLineLength = kGameMaxLineLength
linebreakSymbol = "\r\n"
dialogueNewLinePrefix = "　"
checkIsDialogueByName = True

@dataclass(frozen=True)
class InlineTokenRule:
    pattern: re.Pattern[str]
    displayWidth: Callable[[re.Match[str]], int]

# 行内特殊格式都在这里配置：匹配到的整段原文不会被分词或拆开。
# 格式变更时，只需修改/增加规则及其显示宽度算法。规则重叠时前者优先。
inlineTokenRules = (
    InlineTokenRule(
        re.compile(r"\[([^\[\]/]+?)/([^\[\]/]+?)\]"),
        lambda match: len(match.group(1)),
    ),
    InlineTokenRule(
        re.compile(r"\\[A-Za-z\d\+;]+"),
        lambda match: 0,
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


def isAsciiWordChar(ch: Optional[str]) -> bool:
    # .isalnum() 不接受符号
    return ch is not None and ch.isascii() and (ch.isalnum() or ch == "_")


def classifySymmetricQuotes(text: str) -> dict[int, str]:
    """按原句字符位置将对称引号分为 infix / open / close。"""
    roles = {}
    quoteIsOpen = {mark: False for mark in symmetricQuoteMarks}
    for index, ch in enumerate(text):
        if ch not in symmetricQuoteMarks:
            continue

        previousChar = text[index - 1] if index > 0 else None
        nextChar = text[index + 1] if index + 1 < len(text) else None

        # don't / Let's 等英文词内用法两边都不允许断行。
        if (
            ch == "'"
            and isAsciiWordChar(previousChar)
            and isAsciiWordChar(nextChar)
        ):
            roles[index] = "infix"
            continue

        previousIsWord = previousChar is not None and previousChar.isalnum()
        nextIsWord = nextChar is not None and nextChar.isalnum()
        if not previousIsWord and nextIsWord:
            roles[index] = "open"
            quoteIsOpen[ch] = True
        elif previousIsWord and not nextIsWord:
            roles[index] = "close"
            quoteIsOpen[ch] = False
        elif quoteIsOpen[ch]:
            roles[index] = "close"
            quoteIsOpen[ch] = False
        else:
            roles[index] = "open"
            quoteIsOpen[ch] = True
    return roles


def isPunctuationOrWhitespace(ch: str) -> bool:
    return gpp.utils.isAllWhitespace(ch) or gpp.utils.isAllPunctuation(ch)


def classifyEllipses(text: str) -> dict[int, str]:
    """按原句字符位置将连续省略号分为 left / right / neutral。"""
    roles = {}
    for match in re.finditer(r"…+", text):
        start, end = match.span()
        if start == 0:
            role = "right"
        elif end == len(text):
            role = "left"
        else:
            leftIsSeparator = isPunctuationOrWhitespace(text[start - 1])
            rightIsSeparator = isPunctuationOrWhitespace(text[end])
            if leftIsSeparator and rightIsSeparator:
                role = "neutral"
            elif leftIsSeparator:
                role = "right"
            else:
                # 右边是分隔符，或左右都是正文时，均粘左。
                role = "left"
        for index in range(start, end):
            roles[index] = role
    return roles


def shouldGlueTokenBoundary(
    leftToken: str,
    rightToken: str,
    boundaryOffset: int,
    symmetricQuoteRoles: dict[int, str],
    ellipsisRoles: dict[int, str],
) -> bool:
    """返回两个 token 的边界是否禁止断行。"""
    if not leftToken or not rightToken:
        return False

    leftChar = leftToken[-1]
    rightChar = rightToken[0]
    if leftChar == "…" and rightChar == "…":
        return True
    if leftChar == "…" and ellipsisRoles.get(boundaryOffset - 1) == "right":
        return True
    if rightChar == "…" and ellipsisRoles.get(boundaryOffset) == "left":
        return True
    if leftChar in lineEndProhibitedPuncts:
        return True
    if leftChar in symmetricQuoteMarks:
        return symmetricQuoteRoles.get(boundaryOffset - 1) in {"infix", "open"}
    if rightChar in symmetricQuoteMarks:
        return symmetricQuoteRoles.get(boundaryOffset) in {"infix", "close"}
    return (
        rightChar != "…"
        and gpp.utils.isAllPunctuation(rightChar)
        and rightChar not in lineEndProhibitedPuncts
    )


def mergePunctuationTokens(tokens: list[str]) -> list[str]:
    """把左标点粘到后文，把右标点和普通标点粘到前文。"""
    if not tokens:
        return []

    text = "".join(tokens)
    symmetricQuoteRoles = classifySymmetricQuotes(text)
    ellipsisRoles = classifyEllipses(text)

    mergedTokens = []
    currentToken = tokens[0]
    boundaryOffset = len(tokens[0])
    for index in range(1, len(tokens)):
        if shouldGlueTokenBoundary(
            tokens[index - 1],
            tokens[index],
            boundaryOffset,
            symmetricQuoteRoles,
            ellipsisRoles,
        ):
            currentToken += tokens[index]
        else:
            mergedTokens.append(currentToken)
            currentToken = tokens[index]
        boundaryOffset += len(tokens[index])
    mergedTokens.append(currentToken)

    return mergedTokens


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


def balancedGroupEnd(text: str, start: int) -> Optional[int]:
    """返回从 start 开始的完整括号组的结束位置（左闭右开）。"""
    pairs = {
        "「": "」",
        "『": "』",
        "（": "）",
    }
    closingSymbols = set(pairs.values())

    if start >= len(text) or text[start] not in pairs:
        return None

    stack = []
    for index in range(start, len(text)):
        ch = text[index]
        if ch in pairs:
            stack.append(pairs[ch])
        elif ch in closingSymbols:
            if not stack or ch != stack[-1]:
                return None
            stack.pop()
            if not stack:
                return index + 1

    return None


def checkIsDialogue(se: gpp.Sentence) -> bool:
    if checkIsDialogueByName:
        if not se.name and not se.names:
            return False
        return True
    transView = se.transview
    transView = transView.strip()
    if len(transView) < 2 or transView[0] not in {"「", "『", "（"}:
        return False

    firstEnd = balancedGroupEnd(transView, 0)
    if firstEnd is None:
        return False

    # 一般结构：一个完整括号组包住整句。
    if firstEnd == len(transView):
        return True

    # 附注结构：完整对话后紧跟一个完整的全角括号组。
    if transView[0] not in {"「", "『"} or transView[firstEnd] != "（":
        return False
    commentEnd = balancedGroupEnd(transView, firstEnd)
    return commentEnd == len(transView)


def hasLongPart(transView: str, isDialogue: bool):
    gameMaxLen = kGameMaxLineLength
    segments = transView.split(linebreakSymbol)
    segments = [s.strip() for s in segments if s.strip()]
    if not segments:
        return False
    for index, segment in enumerate(segments):
        if isDialogue:
            if index == 0:
                gameMaxLen = kGameMaxLineLength
            else:
                gameMaxLen = kGameMaxLineLength - 1
        else:
            gameMaxLen = kGameMaxLineLength
        if displayLength(segment) > gameMaxLen:
            return True
    return False


def processSentence(se: gpp.Sentence, isDialogue: bool):
    transView = se.transview
    if not hasLongPart(transView, isDialogue):
        return
    segments = transView.split(linebreakSymbol)
    segments = [s.strip() for s in segments if s.strip()]
    transView = "".join(segments)
    tokens = mergePunctuationTokens(splitIntoTokens(transView))
    if not tokens:
        return

    newLines = []
    currentLine = ""
    for token in tokens:
        commonMaxLen = kCommonMaxLineLength
        if isDialogue and newLines:
            commonMaxLen -= 1

        if currentLine and displayLength(currentLine) + displayLength(token) > commonMaxLen:
            newLines.append(currentLine)
            currentLine = token
        else:
            currentLine += token
    if currentLine:
        newLines.append(currentLine)
    
    se.transview = (linebreakSymbol + dialogueNewLinePrefix).join(newLines) if isDialogue else linebreakSymbol.join(newLines)


def linkLine(se: gpp.Sentence, isDialogue: bool):
    transView = se.transview
    segments = transView.split(linebreakSymbol)
    segments = [s.strip() for s in segments if s.strip()]
    if not segments:
        return

    newLines = []
    currentLine = segments[0]

    for currentSegment in segments[1:]:
        commonMaxLen = kCommonMaxLineLength
        if isDialogue and newLines:
            commonMaxLen -= 1

        if displayLength(currentLine) + displayLength(currentSegment) <= commonMaxLen:
            currentLine += currentSegment
        else:
            newLines.append(currentLine)
            currentLine = currentSegment
    newLines.append(currentLine)

    se.transview = (linebreakSymbol + dialogueNewLinePrefix).join(newLines) if isDialogue else linebreakSymbol.join(newLines)



def init(projectDir: Path):
    logger.info(f"BetterLinebreakFix 初始化成功")
    global tokenizeCachePath, tokenizeCache
    tokenizeCachePath = projectDir / "other_cache" / tokenizeCachePath
    if not tokenizeCachePath.parent.exists():
        tokenizeCachePath.parent.mkdir(parents=True, exist_ok=True)
    if tokenizeCachePath.exists():
        with open(tokenizeCachePath, 'r', encoding='utf-8') as f:
            tokenizeCache = json.load(f)

def dPostRunImpl(se: gpp.Sentence):
    try:
        if "＆" in se.name or "\\f" in se.transview:
            return
        if not gpp.utils.hasCJK(se.transview):
            return
        isDialogue = checkIsDialogue(se)
        processSentence(se, isDialogue)
        linkLine(se, isDialogue)
    except Exception as e:
        logger.error(f"Error during BetterLinebreakFix dPostRun(): {e}")

def dPostRun(se: gpp.Sentence):
    dPostRunImpl(se)

def unload():
    with open(tokenizeCachePath, 'w', encoding='utf-8') as f:
        json.dump(tokenizeCache, f, ensure_ascii=False, indent=2)
    logger.info(f"BetterLinebreakFix 卸载成功，tokenizeCache 已保存至 {tokenizeCachePath}")
    
