import gpp_plugin_api as gpp
from pathlib import Path
import shutil
import subprocess
from typing import cast

# C++ 会在 init/run 前把当前 Translator 注入到这里。
pythonTranslator = cast(gpp.NormalJsonTranslator, None)
logger = cast(gpp.spdlogLogger, None)

def init() -> None:
    logger.info("MyFilePlugin7.py 初始化")

def run():
    runStandardLifecycle()
    return

def runStandardLifecycle() -> None:
    pythonTranslator.normalJsonInit()
    pythonTranslator.normalJsonBeforeRun()
    pythonTranslator.normalJsonProcess()
    # pythonTranslator.normalJsonAfterRun()

def unload():
    try:
        if (pythonTranslator.m_currentRunRelFilePaths is not None
            and pythonTranslator.m_transEngine != gpp.TransEngine.ShowNormal):

            gamePath = Path(r"D:\GALGAME\linshi\Cdrive\恋愛催眠　～ツンな彼女がデレる催眠～")
            targetTransPath = gamePath / "textproc" / "trans"
            newFontFaceName = "RenaiSaimin"

            fontChangerPath = pythonTranslator.m_projectDir / "DynamicFontChanger.exe"
            charMapPath = pythonTranslator.m_projectDir / "charMap.json"
            charsNotMapPath = pythonTranslator.m_projectDir / "charsNotMap.json"
            newFontPath = pythonTranslator.m_projectDir / (newFontFaceName + "_mjp.ttf")

            result = subprocess.run(
                [fontChangerPath, "-j", "gt_output",
                 "-i", "super_gagaga.ttf",
                 "-f", newFontFaceName,
                 "-e", "excludeList.json",
                 "-s"],
                cwd=pythonTranslator.m_projectDir,
                capture_output=True,  # 捕获输出
                text=True,            # 以文本形式返回
                encoding='utf-8'
            )
            logger.info(f"返回输出: {result.stdout}")
            
            shutil.copy2(charMapPath, gamePath)
            shutil.copy2(charsNotMapPath, gamePath)
            shutil.copy2(newFontPath, gamePath)
            shutil.copytree(pythonTranslator.m_projectDir / "gt_output_sjis_output", targetTransPath, dirs_exist_ok=True)

    except Exception as e:
        logger.error(f"Error during unload(): {e}")

    pythonTranslator.normalJsonAfterRun()
    logger.info("MyFilePlugin7.py unloads")
    
