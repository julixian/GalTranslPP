import gpp_plugin_api as gpp

import threading
from pathlib import Path
from typing import cast


# C++ 会在 init/run 前把当前 Translator 注入到这里。
pythonTranslator = cast(gpp.NormalJsonTranslator, None)
logger = cast(gpp.spdlogLogger, None)


def init() -> None:
    logger.info("SampleNormalJsonFilePlugin 初始化")
    logger.info("当前项目目录: " + str(pythonTranslator.m_projectDir))


def run() -> None:
    # 默认演示 Python 侧手工线程管理。这样即使你把 m_onFileProcessed 等回调换成 Python 闭包，
    # 回调也会在 Python 创建的线程里执行，不会交给 C++ 线程池直接调用 Python 闭包。
    pythonTranslator.normalJsonInit()
    pythonTranslator.normalJsonBeforeRun()
    try:
        processCurrentFilesWithPythonThreads()
    finally:
        pythonTranslator.normalJsonAfterRun()


def runStandardLifecycle() -> None:
    # 不改 Python 闭包回调时，可以直接使用标准 NormalJson 生命周期。
    pythonTranslator.normalJsonInit()
    pythonTranslator.normalJsonBeforeRun()
    pythonTranslator.normalJsonProcess()
    pythonTranslator.normalJsonAfterRun()


def processCurrentFilesWithPythonThreads() -> None:
    controller = pythonTranslator.m_controller

    if pythonTranslator.m_transEngine == gpp.TransEngine.DumpName:
        controller.updateBar(controller.m_totalSentences)
        return

    if pythonTranslator.m_transEngine == gpp.TransEngine.NameTrans:
        nameTranslator = pythonTranslator.m_nameTranslator
        if nameTranslator is None:
            raise RuntimeError("NameTranslator 未创建")
        nameTranslator.run(pythonTranslator.m_nameTablePath)
        return

    if pythonTranslator.m_transEngine == gpp.TransEngine.GenDict:
        dictionaryGenerator = pythonTranslator.m_dictionaryGenerator
        if dictionaryGenerator is None:
            raise RuntimeError("DictionaryGenerator 未创建")
        dictionaryGenerator.generate(pythonTranslator.m_projectDir / "ProjGptDict-Gen.toml")
        return

    relFilePaths = pythonTranslator.m_currentRunRelFilePaths
    if relFilePaths is None:
        return

    maxWorkers = min(pythonTranslator.m_threadsNum, len(relFilePaths))

    # ThreadPoolExecutor 会收不回来锁不知道为什么，必须手工管理线程
    nextFileIndex = 0
    nextFileIndexLock = threading.Lock()
    stopEvent = threading.Event()
    errors: list[BaseException] = []
    errorsLock = threading.Lock()

    def worker(threadId: int) -> None:
        nonlocal nextFileIndex
        while not stopEvent.is_set() and not controller.shouldStop():
            with nextFileIndexLock:
                if nextFileIndex >= len(relFilePaths):
                    return
                relFilePath = Path(relFilePaths[nextFileIndex])
                nextFileIndex += 1

            try:
                processOneFile(relFilePath, threadId)
            except BaseException as e:
                with errorsLock:
                    errors.append(e)
                stopEvent.set()
                return

    threads = [
        threading.Thread(target=worker, args=(threadId,), name=f"gppPyFile{threadId}")
        for threadId in range(maxWorkers)
    ]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()

    if errors:
        raise errors[0]

    if (pythonTranslator.m_reuseRepeatedBlocks
            and pythonTranslator.m_transEngine != gpp.TransEngine.ShowNormal):
        pythonTranslator.resolveRepeatedBlockReferences()
    if pythonTranslator.m_agentEnabled and pythonTranslator.m_transAgent is not None:
        pythonTranslator.m_transAgent.applyAgentSuggestions()


def processOneFile(relFilePath: Path, threadId: int) -> None:
    controller = pythonTranslator.m_controller
    if controller.shouldStop():
        return

    controller.addThreadNum()
    try:
        pythonTranslator.processFile(relFilePath, threadId)
    finally:
        controller.reduceThreadNum()


def unload() -> None:
    logger.info("SampleNormalJsonFilePlugin 卸载")
