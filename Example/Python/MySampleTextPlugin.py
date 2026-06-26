# 必须: 导入 C++ 绑定的模块
import gpp_plugin_api as gpp

from pathlib import Path
from typing import Callable, cast

# Sentence 的声明详见 Example/Lua/MySampleTextPlugin.lua

# 全局变量，由 C++ 注入
# python 中的 logger 和 toknizeFunc 不在 utils 中而是在当前模块的全局变量中
logger = cast(gpp.spdlogLogger, None)

# sourceLang_useTokenizer = True
# sourceLang_tokenizerBackend = "MeCab"
# sourceLang_meCabDictDir = "..."
# sourceLang_spaCyModelName = "ja_core_news_sm"
# targetLang_spaCyModelName = "..."
# targetLang_stanzaLang = "..."
# sourceLang_tokenizeFunc = None
# ...

targetLang_useTokenizer = True
targetLang_tokenizerBackend = "pkuseg"
# 如果使用提供的分词器则必须先定义 tokenizeFunc
targetLang_tokenizeFunc = cast(Callable[[str], tuple[list[list[str]], list[list[str]]]], None)


def checkConditionForRetranslKeysFunc(se: gpp.Sentence) -> bool:
    if se.index == 278:
        logger.info("RetransFromPython")
    return False

def checkConditionForSkipProblemsFunc(se: gpp.Sentence) -> bool:
    if se.index == 278:
        # retranslKey 中检查的 se 是一个副本，只负责检查，怎么设置都不会影响到要翻译和输出的部分
        # 但 skipProblems 的检查提供的是要输出的 se 的引用
        current_problem = ""
        for i, s in enumerate(se.problems):
            # 正则检查通过后，执行 Condition 验证时，当前正在检查的 problem 会拥有前缀 'Current problem:'
            # PS: 不要在此检查函数中的任何地方以任何方式修改 problems 对象本身，可能导致 C++侧 出现异常
            # 但可以通过 problems_set_by_index 修改元素内容曲线救国
            if s.startswith("Current problem:"):
                current_problem = s[16:]
            else:
                success = se.problems_set_by_index(i, "My new problem")
                if success:
                    se.other_info |= { f"pySuccess{i}": "New problem suc" }
                else:
                    se.other_info |= { f"pyFail{i}": "New problem fail" }

        se.other_info |= { "pythonCheck": "The 278th" }
        se.other_info |= { "pyCp": current_problem }
        logger.info("SkipProblemsFromPython")
    return False

def init(project_dir: Path):
    """
    插件初始化函数，由 C++ 调用一次。
    """
    logger.info(f"MySampleTextPluginFromLua 初始化成功，projectDir: {project_dir}")


def dPostRun(se: gpp.Sentence):
    """
    se 是一个 C++ Sentence 对象的代理。
    """
    if se.translated_preview == "久远紧锁眉头，脸上写着「骗人的吧，喂」。":
        wordpos_vec, entity_vec = targetLang_tokenizeFunc(se.translated_preview)
        # 其它(虽然没几个)工具函数依然在 utils 里
        tokens = gpp.utils.splitIntoTokens(wordpos_vec, se.translated_preview)
        parts = [f"[{value[0]}]" for value in wordpos_vec]
        result = "#".join(parts)
        # 所有容器均为 copy，直接对其进行 insert，append 等是无效的
        se.other_info |= { "tokens_python": result }

# def unload():
#     pass

# dPreRun, preRun, postRun, dPostRun, unload 均可选择性定义，如果定义则分别在对应阶段调用
