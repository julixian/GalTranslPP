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
    logger.info(f"BetterRetainJp 初始化成功，projectDir: {project_dir}")


def run(se: gpp.Sentence):
    """
    处理每个句子的主函数。
    se 是一个 C++ Sentence 对象的代理。
    """
    pattern = r"\[([^\[\]/]+?)/([^\[\]/]+?)\]"
    # 这个世界上我唯一爱过的[ひと/女人]。
    orgTransView = se.transview
    if orgTransView.startswith("(Failed to translate)"):
        return
    # 这个世界上我唯一爱过的。
    removeFurigana = re.sub(pattern, "", orgTransView)
    # ""
    kanas = gpp.utils.extractKana(removeFurigana)

    # [ひと/(女人)]
    matches = re.finditer(pattern, orgTransView)
    for match in matches:
        # 女人
        baseText = match.group(2)
        # ""
        kanas += gpp.utils.extractKana(baseText)

        cjkInBaseText = gpp.utils.extractCJK(baseText)
        if not cjkInBaseText:
            se.problems += ["注音顺序错误(基本文本中无汉字): " + match.group(0)]

    if kanas:
        logger.trace("BetterRetainJp 残留日文检测: " + kanas)
        se.problems += ["残留日文: " + kanas]


def unload():
    pass
