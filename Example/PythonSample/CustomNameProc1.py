import gpp_plugin_api as gpp
from pathlib import Path
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
targetLangUseTokenizer = False
targetLangTokenizerBackend = "spaCy"
targetLangSpaCyModelName = "zh_core_web_trf"
targetLangTokenizeFunc = cast(tokenizeFuncType, None)

def init(projectDir: Path):
    """
    插件初始化函数，由 C++ 调用一次。
    """
    logger.info(f"CustomNameProc 初始化成功")


def dPreRun(se: gpp.Sentence):
    """
    处理每个句子的主函数。
    se 是一个 C++ Sentence 对象的代理。
    """
    pattern = r'([^ ]+?) [^ ]+? text="([^"]+?)"'
    matched = re.search(pattern, se.name)
    if not matched:
        return
    se.otherinfo |= {"orgname":se.name}
    se.name = f"{matched.group(1)}/{matched.group(2)}"

def postRun(se: gpp.Sentence):
    if "orgname" not in se.otherinfo:
        return
    se.nametrans = se.otherinfo["orgname"]
    se.otherinfo = {}