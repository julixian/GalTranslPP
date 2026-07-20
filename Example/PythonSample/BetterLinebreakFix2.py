import gpp_plugin_api as gpp
from pathlib import Path
import json
import re
from typing import Callable, cast

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
maxLineLength = 28
linebreakSymbol = "@L"

def splitIntoTokens(sentence: str) -> list[str]:
    tokens = []
    if sentence in tokenizeCache:
        wordPosVec = tokenizeCache[sentence]
        tokens = gpp.utils.splitIntoTokens(wordPosVec, sentence)
    else:
        wordPosVec, _ = targetLangTokenizeFunc(sentence)
        tokens = gpp.utils.splitIntoTokens(wordPosVec, sentence)
        tokenizeCache[sentence] = wordPosVec
    return tokens

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


def init(projectDir: Path):
    """
    插件初始化函数，由 C++ 调用一次。
    """
    logger.info(f"BetterLinebreakFix 初始化成功")
    global tokenizeCachePath, tokenizeCache
    tokenizeCachePath = projectDir / "other_cache" / tokenizeCachePath
    if tokenizeCachePath.exists():
        with open(tokenizeCachePath, 'r', encoding='utf-8') as f:
            tokenizeCache = json.load(f)


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
        if len(segment) > maxLen:
            return True
    return False

def processSentence(se: gpp.Sentence):
    transView: str = se.transview
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
    currentLine: str = tokens[0]
    residualTokens = tokens[1:]

    for index, currentToken in enumerate(residualTokens):
        if dialogue:
            if not newLines:
                maxLen = maxLineLength
            else:
                maxLen = maxLineLength - 1
        else:
            maxLen = maxLineLength
        if len(currentLine) + len(currentToken) <= maxLen:
            if index + 1 < len(residualTokens):
                nextToken = residualTokens[index + 1]
                if len(currentLine) + len(currentToken) + len(nextToken) > maxLen:
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
    if len(newLines) >= 1 and len(currentLine) <= 2 and gpp.utils.isAllPunctuation(currentLine):
        #       ccc
        # word\n。」\z
        newLines[-1] = newLines[-1] + currentLine
    else:
        newLines.append(currentLine)
    se.transview = (linebreakSymbol + "　").join(newLines) if dialogue else linebreakSymbol.join(newLines)

def linkLine(se: gpp.Sentence):
    transView: str = se.transview
    dialogue = transView.startswith("「")
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
        if len(currentLine) + len(currentSegment) <= maxLen:
            currentLine += currentSegment
        else:
            newLines.append(currentLine)
            currentLine = currentSegment

    newLines.append(currentLine)
    se.transview = (linebreakSymbol + "　").join(newLines) if dialogue else linebreakSymbol.join(newLines)
    if se.orig.startswith("　") and not se.transview.startswith("　"):
        se.transview = "　" + se.transview


def dPostRun(se: gpp.Sentence):
    try:
        if not gpp.utils.hasCJK(se.transview) or se.orig.startswith("　　"):
            return
        if re.search("@[^L]", se.transview):
            return
        processSentence(se)
        linkLine(se)
    except Exception as e:
        logger.error(f"Error during BetterLinebreakFix dPostRun(): {e}")


def unload():
    with open(tokenizeCachePath, 'w', encoding='utf-8') as f:
        json.dump(tokenizeCache, f, ensure_ascii=False, indent=2)
    logger.info(f"BetterLinebreakFix 卸载成功，tokenizeCache 已保存至 {tokenizeCachePath}")
    
