import gpp_plugin_api as gpp
from pathlib import Path
import re
from typing import cast

logger = cast(gpp.spdlogLogger, None)
targetLangUseTokenizer = False
targetLangTokenizeFunc = None

def init(projectDir: Path):
    """
    插件初始化函数，由 C++ 调用一次。
    """
    logger.info(f"CheckFFFD 初始化成功，projectDir: {projectDir}")


def dPostRun(se: gpp.Sentence):
    if '\ufffd' in se.transview:
        se.problems += ["U+FFFD 残留"]
