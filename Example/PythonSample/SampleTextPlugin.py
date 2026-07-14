import gpp_plugin_api as gpp

from pathlib import Path
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

sampleOrig = "\n  「――――きて」\n "

def init(projectDir: Path) -> None:
    logger.info(f"SampleTextPlugin 初始化，projectDir: {projectDir}")


def checkConditionForRetranslKeysFunc(se: gpp.Sentence) -> bool:
    # retranslKey 条件函数只负责判断是否重翻，返回 True 表示命中。
    if se.index == 2 and se.orig == sampleOrig:
        logger.info("Python retranslKey 示例命中检查")
    return False


def checkConditionForSkipProblemsFunc(se: gpp.Sentence, problem: str) -> bool:
    # skipProblems 条件函数拿到的是即将输出的 Sentence 和当前命中的问题文本。
    # 返回 True 表示跳过这个 problem，返回 False 表示保留。
    if se.index != 2 or se.orig != sampleOrig:
        return False

    se.otherinfo |= {"pythonSkippedProblem": problem}
    logger.info("Python skipProblems 示例命中检查: " + problem)
    if problem == "测试问题1":
        return True
    return False


def dPostRun(se: gpp.Sentence) -> None:
    # dPostRun 在后处理后执行。这里演示调用 tokenizer 并把结果写入 otherinfo。
    if targetLangTokenizeFunc is None:
        return
    if se.orig != sampleOrig:
        return

    wordPosVec, entityVec = targetLangTokenizeFunc("测试目标语言分词器")
    tokens = gpp.utils.splitIntoTokens(wordPosVec, "测试目标语言分词器")
    se.otherinfo |= {"tokensPython": "|".join(tokens)}

    se.problems += ["测试问题1"]
    se.problems += ["测试问题2"]
    se.transview = se.transview + "❤️🧡❤️"


def unload() -> None:
    logger.info("SampleTextPlugin 卸载")


# 可选入口还有 dPreRun、preRun、postRun。
