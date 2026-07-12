# 必须: 导入 C++ 绑定的模块
import gpp_plugin_api as gpp
from pathlib import Path
import re

# Sentence 的声明详见 Example/LuaSample/SampleTextPlugin.lua

# 全局变量，由 C++ 注入
# python 中的 logger 和 toknizeFunc 不在 utils 中而是在当前模块的全局变量中
logger = None

# sourceLang_useTokenizer = True
# sourceLang_tokenizerBackend = "MeCab"
# sourceLang_meCabDictDir = "..."
# sourceLang_spaCyModelName = "ja_core_news_sm"
# targetLang_spaCyModelName = "..."
# targetLang_stanzaLang = "..."
# sourceLang_tokenizeFunc = None
# ...

targetLang_useTokenizer = False
# 如果使用提供的分词器则必须先定义 tokenizeFunc
targetLang_tokenizeFunc = None

def init(project_dir: Path):
    """
    插件初始化函数，由 C++ 调用一次。
    """
    logger.info(f"LineError 初始化成功，projectDir: {project_dir}")


def run(se: gpp.Sentence):
    """
    处理每个句子的主函数。
    se 是一个 C++ Sentence 对象的代理。
    """
    patternFurigana = r"\[([^\[\]/]+?)/([^\[\]/]+?)\]"
    patternTipJump = r"\{([^\{\}]+?)\}"
    transView = se.transview
    segments = transView.split("<br>")
    for i, s in enumerate(segments):
        removed = re.sub(patternFurigana, "$2", s)
        removed = re.sub(patternTipJump, "$1", removed)
        if len(removed) > 23:
            se.problems += [f"第 {i + 1} 行超过 23 个字"]


def unload():
    pass
