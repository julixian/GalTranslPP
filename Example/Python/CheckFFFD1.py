# 必须: 导入 C++ 绑定的模块
import gpp_plugin_api as gpp
from pathlib import Path
import re
from typing import cast

logger = cast(gpp.spdlogLogger, None)
targetLang_useTokenizer = False
targetLang_tokenizeFunc = None

def init(project_dir: Path):
    """
    插件初始化函数，由 C++ 调用一次。
    """
    logger.info(f"CheckFFFD 初始化成功，projectDir: {project_dir}")


def dPostRun(se: gpp.Sentence):
    if '\ufffd' in se.translated_preview:
        se.problems += ["U+FFFD 残留"]
