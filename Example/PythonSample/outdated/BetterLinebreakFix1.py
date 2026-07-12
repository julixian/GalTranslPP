import gpp_plugin_api as gpp
from pathlib import Path
import re
import json
from typing import Callable, cast

# Sentence 的声明详见 Example/LuaSample/SampleTextPlugin.lua

# 全局变量，由 C++ 注入
# python 中的 logger 和 toknizeFunc 不在 utils 中而是在当前模块的全局变量中
logger = cast(gpp.spdlogLogger, None)

# sourceLangUseTokenizer = True
targetLangTokenizerBackend = "spaCy"
# sourceLangMecabDictDir = "..."
# sourceLangSpaCyModelName = "ja_core_news_sm"
targetLangSpaCyModelName = "zh_core_web_trf"
# targetLangStanzaLang = "..."
# sourceLangTokenizeFunc = None
# ...

targetLangUseTokenizer = True
# 如果使用提供的分词器则必须先定义 targetLangTokenizeFunc
targetLangTokenizeFunc = cast(Callable[[str], tuple[list[tuple[str, str]], list[tuple[str, str]]]], None)

tokenizeCachePath = Path(__file__).resolve().parent / r"other_cache\tokenizeCache_linelink.json"
tokenizeCache = {}
if tokenizeCachePath.exists():
    with open(tokenizeCachePath, 'r', encoding='utf-8') as f:
        tokenizeCache = json.load(f)

excludePuncts = { "『", "「", "“", "‘", "'", "《", "〈", "（", "【", "〔", "〖", "≪" }

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
    logger.info(f"LineLink 初始化成功，projectDir: {projectDir}")

maxLineLength = 32
linebreakSymbol = "\\n"
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
    # if "[" in transView:
    #     return
    if not hasLongPart(transView):
        return
    segments = transView.split(linebreakSymbol)
    segments = [s.strip() for s in segments if s.strip()]
    transView = "".join(segments)
    maxLen = maxLineLength
    tokens = splitIntoTokens(transView)
    
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
                        #『\n...
                        newLines.append(currentLine)
                        currentLine = currentToken
                        continue
                    removed = gpp.utils.removePunctuation(nextToken)
                    if not removed and nextToken not in excludePuncts:
                        if any(currentLine.endswith(excludePunct) for excludePunct in excludePuncts):
                            #『word\n。』
                            newLines.append(currentLine[:-1])
                            currentLine = currentLine[-1] + currentToken
                        else:
                            # word\n，
                            newLines.append(currentLine)
                            currentLine = currentToken
                        continue
            currentLine += currentToken
        else:
            newLines.append(currentLine)
            currentLine = currentToken
    if len(newLines) >= 1 and len(currentLine) <= 2 and not gpp.utils.removePunctuation(currentLine):
        # word\n。」
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
        # 判断：如果 (当前行 + 下一段) 的长度 <= maxLen
        if len(currentLine) + len(currentSegment) <= maxLen:
            # 可以合并（直接拼接）
            currentLine += currentSegment
        else:
            # 不能合并了，把当前行存入结果列表
            newLines.append(currentLine)
            # 开启新的一行
            currentLine = currentSegment

    # 3. 循环结束后，别忘了把最后一行加进去
    newLines.append(currentLine)
    # 4. 将新分好的段落连接起来返回
    se.transview = (linebreakSymbol + "　").join(newLines) if dialogue else linebreakSymbol.join(newLines)
    if se.orig.startswith("　") and not se.transview.startswith("　"):
        se.transview = "　" + se.transview


def dPostRun(se: gpp.Sentence):
    """
    处理每个句子的主函数。
    se 是一个 C++ Sentence 对象的代理。
    """
    try:
        if not gpp.utils.extractCJK(se.transview) or se.orig.startswith("　　"):
            return
        processSentence(se)
        #linkLine(se)
        #se.transview = se.transview.replace(linebreakSymbol, "\n")
    except Exception as e:
        logger.error(f"Error during LineLink run(): {e}")


def unload():
    with open(tokenizeCachePath, 'w', encoding='utf-8') as f:
        json.dump(tokenizeCache, f, ensure_ascii=False, indent=2)
    logger.info(f"LineLink tokenizeCache 已保存至 {tokenizeCachePath}")
    
