# 必须: 导入 C++ 绑定的模块
import gpp_plugin_api as gpp
from pathlib import Path
import re
import json

# Sentence 的声明详见 Example/Lua/MySampleTextPlugin.lua

# 全局变量，由 C++ 注入
# python 中的 logger 和 toknizeFunc 不在 utils 中而是在当前模块的全局变量中
logger = None

# sourceLang_useTokenizer = True
targetLang_tokenizerBackend = "spaCy"
# sourceLang_meCabDictDir = "..."
# sourceLang_spaCyModelName = "ja_core_news_sm"
targetLang_spaCyModelName = "zh_core_web_trf"
# targetLang_stanzaLang = "..."
# sourceLang_tokenizeFunc = None
# ...

targetLang_useTokenizer = True
# 如果使用提供的分词器则必须先定义 tokenizeFunc
targetLang_tokenizeFunc = None

tokenizeCachePath = Path(__file__).resolve().parent / Path(r"other_cache\tokenizeCache_linelink.json")
tokenizeCache = {}
if tokenizeCachePath.exists():
    with open(tokenizeCachePath, 'r', encoding='utf-8') as f:
        tokenizeCache = json.load(f)

excludePuncts = { "『", "「", "“", "‘", "'", "《", "〈", "（", "【", "〔", "〖", "≪" }

def splitIntoTokens(sentence: str):
    tokens = []
    if sentence in tokenizeCache:
        wordPosVec = tokenizeCache[sentence]
        tokens = gpp.utils.splitIntoTokens(wordPosVec, sentence)
    else:
        wordPosVec, entityVec = targetLang_tokenizeFunc(sentence)
        tokens = gpp.utils.splitIntoTokens(wordPosVec, sentence)
        tokenizeCache[sentence] = wordPosVec
    return tokens

def init(project_dir: Path):
    """
    插件初始化函数，由 C++ 调用一次。
    """
    logger.info(f"LineLink 初始化成功，projectDir: {project_dir}")

MAX_LEN = 32
LINEBREAK_SYMBOL = "\\n"
def has_long_part(transView: str):
    max_len = MAX_LEN
    segments = transView.split(LINEBREAK_SYMBOL)
    segments = [s.strip() for s in segments if s.strip()]
    if not segments:
        return False
    dialogue = segments[0].startswith("「")
    for index, segment in enumerate(segments):
        if dialogue:
            if index == 0:
                max_len = MAX_LEN
            else:
                max_len = MAX_LEN - 1
        else:
            max_len = MAX_LEN
        if len(segment) > max_len:
            return True
    return False

def processSentence(se: gpp.Sentence):
    transView: str = se.translated_preview
    # if "[" in transView:
    #     return
    if not has_long_part(transView):
        return
    segments = transView.split(LINEBREAK_SYMBOL)
    segments = [s.strip() for s in segments if s.strip()]
    transView = "".join(segments)
    max_len = MAX_LEN
    tokens = splitIntoTokens(transView)
    
    dialogue = transView.startswith("「")
    new_lines = []
    current_line: str = tokens[0]
    residual_tokens = tokens[1:]

    for index, current_token in enumerate(residual_tokens):
        if dialogue:
            if not new_lines:
                max_len = MAX_LEN
            else:
                max_len = MAX_LEN - 1
        else:
            max_len = MAX_LEN
        if len(current_line) + len(current_token) <= max_len:
            if index + 1 < len(residual_tokens):
                next_token = residual_tokens[index + 1]
                if len(current_line) + len(current_token) + len(next_token) > max_len:
                    if current_token in excludePuncts:
                        #『\n...
                        new_lines.append(current_line)
                        current_line = current_token
                        continue
                    removed = gpp.utils.removePunctuation(next_token)
                    if not removed and next_token not in excludePuncts:
                        if any(current_line.endswith(excludePunct) for excludePunct in excludePuncts):
                            #『word\n。』
                            new_lines.append(current_line[:-1])
                            current_line = current_line[-1] + current_token
                        else:
                            # word\n，
                            new_lines.append(current_line)
                            current_line = current_token
                        continue
            current_line += current_token
        else:
            new_lines.append(current_line)
            current_line = current_token
    if len(new_lines) >= 1 and len(current_line) <= 2 and not gpp.utils.removePunctuation(current_line):
        # word\n。」
        new_lines[-1] = new_lines[-1] + current_line
    else:
        new_lines.append(current_line)
    se.translated_preview = (LINEBREAK_SYMBOL + "　").join(new_lines) if dialogue else LINEBREAK_SYMBOL.join(new_lines)

def linkLine(se: gpp.Sentence):
    transView: str = se.translated_preview
    dialogue = transView.startswith("「")
    max_len = MAX_LEN
    segments = transView.split(LINEBREAK_SYMBOL)
    segments = [s.strip() for s in segments if s.strip()]
    
    if not segments:
        return

    new_lines = []
    current_line = segments[0]

    for current_segment in segments[1:]:
        if dialogue:
            if not new_lines:
                max_len = MAX_LEN
            else:
                max_len = MAX_LEN - 1
        else:
            max_len = MAX_LEN
        # 判断：如果 (当前行 + 下一段) 的长度 <= max_len
        if len(current_line) + len(current_segment) <= max_len:
            # 可以合并（直接拼接）
            current_line += current_segment
        else:
            # 不能合并了，把当前行存入结果列表
            new_lines.append(current_line)
            # 开启新的一行
            current_line = current_segment

    # 3. 循环结束后，别忘了把最后一行加进去
    new_lines.append(current_line)
    # 4. 将新分好的段落连接起来返回
    se.translated_preview = (LINEBREAK_SYMBOL + "　").join(new_lines) if dialogue else LINEBREAK_SYMBOL.join(new_lines)
    if se.original_text.startswith("　") and not se.translated_preview.startswith("　"):
        se.translated_preview = "　" + se.translated_preview


def dPostRun(se: gpp.Sentence):
    """
    处理每个句子的主函数。
    se 是一个 C++ Sentence 对象的代理。
    """
    try:
        if not gpp.utils.extractCJK(se.translated_preview) or se.original_text.startswith("　　"):
            return
        processSentence(se)
        #linkLine(se)
        #se.translated_preview = se.translated_preview.replace(LINEBREAK_SYMBOL, "\n")
    except Exception as e:
        logger.error(f"Error during LineLink run(): {e}")


def unload():
    with open(tokenizeCachePath, 'w', encoding='utf-8') as f:
        json.dump(tokenizeCache, f, ensure_ascii=False, indent=2)
    logger.info(f"LineLink tokenizeCache 已保存至 {tokenizeCachePath}")
    