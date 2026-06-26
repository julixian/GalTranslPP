# 必须: 导入 C++ 绑定的模块
import gpp_plugin_api as gpp
from pathlib import Path
import shutil
import subprocess
from typing import cast

# 所有已注册类型和函数详见 GalTranslPP/PythonManager.cpp

# 必须: 声明 pythonTranslator
# C++ 会在 init 前将此属性赋值为基类指针
pythonTranslator = cast(gpp.NormalJsonTranslator, None)

logger = cast(gpp.spdlogLogger, None)

def run():
    pythonTranslator.normalJsonBeforeRun()
    pythonTranslator.normalJsonProcess()
    return


def init():
    # init 后 toml 配置文件及字典文件等才会被加载
    pythonTranslator.normalJsonInit()
    logger.info("MyFilePluginFromPython starts")
    logger.info("Current inputDir: " + str(pythonTranslator.m_inputDir))

def unload():
    try:
        if (pythonTranslator.m_currentRunRelFilePaths is not None
            and pythonTranslator.m_transEngine != gpp.TransEngine.ShowNormal):

            gamePath = Path(r"D:\GALGAME\linshi\WillPlus\リビドー・アバンちゅ～る")
            targetTransPath = gamePath / "Rio1_cn"
            newFontFaceName = "LibidoAventure"

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
    logger.info("MyFilePluginFromPython unloads")
    
