import gpp_plugin_api as gpp

import threading
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import cast


# C++ 会在 init/run 前把当前 Translator 注入到这里。
pythonTranslator = cast(gpp.NormalJsonTranslator, None)
logger = cast(gpp.spdlogLogger, None)


def init() -> None:
    logger.info("SampleNormalJsonFilePlugin 初始化")
    logger.info("当前项目目录: " + str(pythonTranslator.m_projectDir))


def run() -> None:
    # 默认演示 Python 侧线程池。这样即使你把 m_onFileProcessed 等回调换成 Python 闭包，
    # 回调也会在 Python 创建的线程里执行，不会交给 C++ 线程池直接调用 Python 闭包。
    pythonTranslator.normalJsonInit()
    pythonTranslator.normalJsonBeforeRun()
    try:
        processCurrentFilesWithThreadPool()
    finally:
        pythonTranslator.normalJsonAfterRun()


def runStandardLifecycle() -> None:
    # 不改 Python 闭包回调时，可以直接使用标准 NormalJson 生命周期。
    pythonTranslator.normalJsonInit()
    pythonTranslator.normalJsonBeforeRun()
    pythonTranslator.normalJsonProcess()
    pythonTranslator.normalJsonAfterRun()


def processCurrentFilesWithThreadPool() -> None:
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
    if not relFilePaths:
        return

    maxWorkers = min(pythonTranslator.m_threadsNum, len(relFilePaths))

    workerState = threading.local()
    nextThreadId = 1
    threadIdLock = threading.Lock()
    stopEvent = threading.Event()

    def initWorker() -> None:
        nonlocal nextThreadId
        # 每个工作线程固定使用一个从 1 开始的编号，换文件时保留对应 Agent 的上下文。
        with threadIdLock:
            workerState.threadId = nextThreadId
            nextThreadId += 1

    def worker(relFilePath: Path) -> None:
        if stopEvent.is_set():
            return
        try:
            processOneFile(relFilePath, workerState.threadId)
        except BaseException:
            stopEvent.set()
            raise

    with ThreadPoolExecutor(max_workers=maxWorkers, thread_name_prefix="gppPyFile",
                            initializer=initWorker) as executor:
        futures = [executor.submit(worker, Path(relFilePath)) for relFilePath in relFilePaths]
        for future in as_completed(futures):
            # 传播文件处理异常；退出作用域时等待正在执行的任务结束并关闭线程池。
            future.result()

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
