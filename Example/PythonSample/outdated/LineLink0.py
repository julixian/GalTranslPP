# 必须: 导入 C++ 绑定的模块
import gpp_plugin_api as gpp
from pathlib import Path
import re

# Sentence 的声明详见 Example/LuaSample/SampleTextPlugin.lua

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

def init(project_dir: Path):
    """
    插件初始化函数，由 C++ 调用一次。
    """
    logger.info(f"LineLink 初始化成功，projectDir: {project_dir}")

def reformat_text(text, max_len=33):
    dialogue = text.startswith("「")
    if dialogue:
        max_len = 32
    segments = text.split("<br>")
    segments = [s.strip() for s in segments if s.strip()]
    
    if not segments:
        return ""

    # 2. 开始重新组装
    new_lines = []
    current_line = segments[0]  # 初始化当前行

    for next_segment in segments[1:]:
        # 判断：如果 (当前行 + 下一段) 的长度 <= 33
        if len(current_line) + len(next_segment) <= max_len:
            # 可以合并（直接拼接）
            current_line += next_segment

        else:
            # 不能合并了，把当前行存入结果列表
            new_lines.append(current_line)
            # 开启新的一行
            current_line = next_segment

    # 3. 循环结束后，别忘了把最后一行加进去
    new_lines.append(current_line)

    # 4. 用 <br> 将新分好的段落连接起来返回
    return "<br>　".join(new_lines) if dialogue else "<br>".join(new_lines)


def run(se: gpp.Sentence):
    """
    处理每个句子的主函数。
    se 是一个 C++ Sentence 对象的代理。
    """
    transView = se.transview
    if len(transView) > 33 and "<br>" not in transView and "\\" not in transView:
        max_len = 33
        dialogue = transView.startswith("「")
        if dialogue:
            max_len = 32
        wordPosVec, entityVec = targetLang_tokenizeFunc(transView)
        tokens = gpp.utils.splitIntoTokens(wordPosVec, transView)
        if not tokens:
            return

        new_lines = []
        current_line = tokens[0]

        for next_token in tokens[1:]:
            if len(current_line) + len(next_token) <= max_len:
                current_line += next_token
            else:
                new_lines.append(current_line)
                current_line = next_token
        new_lines.append(current_line)
        se.transview = "<br>　".join(new_lines) if dialogue else "<br>".join(new_lines)
        return
    transView = reformat_text(transView)
    if not transView:
        return
    se.transview = transView


def unload():
    pass
