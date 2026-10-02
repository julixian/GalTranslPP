import gpp_plugin_api as gpp

import threading
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Callable, cast


pythonTranslator = cast(gpp.EpubTranslator, None)
logger = cast(gpp.spdlogLogger, None)


def init() -> None:
    logger.info("SampleEpubFilePlugin 初始化")
    logger.info("当前项目目录: " + str(pythonTranslator.m_projectDir))


def run() -> None:
    pythonTranslator.normalJsonInit()
    pythonTranslator.epubInit()
    pythonTranslator.epubBeforeRun()

    originOnFileProcessed = cast(Callable[[object], None], pythonTranslator.m_onFileProcessed)

    def onFileProcessed(relFilePath: object) -> None:
        logger.info("EPUB 片段处理完成: " + str(relFilePath))
        originOnFileProcessed(relFilePath)

    # m_onFileProcessed 是 EPUB 判断所有 json 是否完成并回打包的关键回调。
    # 覆盖成 Python 闭包后，需用 Python 线程逐文件调用 processFile，避免 C++ 线程池直接调用 Python 闭包。
    # 另，调用可能同时发生，如有需要请在 onFileProcessed 内上锁 (originOnFileProcessed 内部是线程安全的)
    pythonTranslator.m_onFileProcessed = onFileProcessed

    pythonTranslator.normalJsonBeforeRun()
    try:
        processCurrentFilesWithThreadPool()
    finally:
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

    with ThreadPoolExecutor(max_workers=maxWorkers, thread_name_prefix="gppPyEpub",
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
    logger.info("SampleEpubFilePlugin 卸载")
