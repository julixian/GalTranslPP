# 必须: 导入 C++ 绑定的模块
import gpp_plugin_api as gpp
from pathlib import Path
import tomllib
import threading
import queue
import time
import traceback

# 所有已注册类型和函数详见 GalTranslPP/PythonManager.cpp

# 必须: 声明 pythonTranslator
# C++ 会在 init 前将此属性赋值为基类指针
pythonTranslator = None

logger = None

lock = threading.Lock()

def isApiTranslationEngine(transEngine):
    if (transEngine == gpp.TransEngine.ForGalJson or
        transEngine == gpp.TransEngine.ForGalTsv or
        transEngine == gpp.TransEngine.ForNovelTsv or
        transEngine == gpp.TransEngine.DeepseekJson or
        transEngine == gpp.TransEngine.Sakura
    ):
        return True
    return False

def process_file(rel_file_path, worker_id):
    """包装函数，worker_id是线程编号"""
    try:
        pythonTranslator.m_controller.addThreadNum()
        pythonTranslator.processFile(rel_file_path, worker_id)
        pythonTranslator.m_controller.reduceThreadNum()
    except Exception as e:
        error_msg = f"[线程{worker_id}] 处理文件 {rel_file_path} 时出错: {str(e)}\n{traceback.format_exc()}"
        logger.error(error_msg)

def worker(worker_id, task_queue):
    """工作线程函数，worker_id是该线程的固定编号"""
    while True:
        task = task_queue.get()
        if task is None:  # 收到退出信号
            task_queue.task_done()
            break
        
        try:
            rel_file_path = task  # 任务只包含文件路径
            process_file(rel_file_path, worker_id)
        finally:
            task_queue.task_done()

def multi_threads_run():
    # 多线程使用手动线程管理
    # 因为子解释器里用 多进程 或 ThreadPoolExecutor 会有一些妙妙小问题
    # 在不进行大模型翻译时行为趋近于单线程，因为我只在大模型翻译部分释放了 C++ 侧占有的 GIL，否则又会有一些妙妙小问题
    relFilePaths = pythonTranslator.m_currentRunRelFilePaths
    MAX_WORKERS = pythonTranslator.m_threadsNum
    pythonTranslator.m_controller.makeBar(pythonTranslator.m_controller.m_totalSentences, MAX_WORKERS)
    task_queue = queue.Queue()
    threads = []
    
    # 创建并启动工作线程，每个线程有固定的worker_id
    for worker_id in range(MAX_WORKERS):
        t = threading.Thread(
            target=worker, 
            args=(worker_id, task_queue),  # 传递worker_id
            name=f"Worker-{worker_id}"
        )
        t.daemon = False
        t.start()
        threads.append(t)
        logger.debug(f"启动工作线程 {worker_id}")
    
    # 将所有任务放入队列
    for relFilePath in relFilePaths:
        task_queue.put(relFilePath)
    logger.info(f"已添加 {len(relFilePaths)} 个任务到队列")
    
    # 等待所有任务完成
    task_queue.join()
    
    # 发送退出信号给所有工作线程
    for _ in range(MAX_WORKERS):
        task_queue.put(None)
    
    # 等待所有线程结束
    for t in threads:
        t.join(timeout=30)
        if t.is_alive():
            logger.warn(f"线程 {t.name} 未能正常结束")

    logger.info("所有 Python 线程执行完毕")

    if not pythonTranslator.m_controller.shouldStop():
        if pythonTranslator.m_useRepeatedBlockInputCache and isApiTranslationEngine(pythonTranslator.m_transEngine):
            pythonTranslator.resolveRepeatedBlockReferences()
        if pythonTranslator.m_agentEnabled:
            pythonTranslator.applyAgentRetranslateSuggestions()

def run():
    # 每个 translator 的每个 filePlugin 和 textPlugin 均使用一个专门的子解释器(NLP tokenize系列除外)
    # 所以此函数可以阻塞，并不影响其它 translator 的工作
    pythonTranslator.normalJsonBeforeRun()
    if pythonTranslator.m_currentRunRelFilePaths is None:
        logger.info("可能是 DumpName/NameTrans/GenDict 之类无需 processFile 的 TransEngine")
        pythonTranslator.normalJsonAfterRun()
        return
    relFilePaths = pythonTranslator.m_currentRunRelFilePaths

    # normalJsonProcess() 是完整标准流程：处理 m_currentRunRelFilePaths，并执行重复块引用复用、Agent 建议等后续统一阶段
    # normalJsonProcessFiles(relFilePaths) 是文件级线程池辅助：只把传入文件按 m_threadsNum 投递给 processFile 并等待完成
    # Python 除了调用 normalJsonProcessFiles(relFilePaths)，也可以自由配置文件列表、排序、线程模型、锁和异常处理，然后直接调用 processFile
    # pythonTranslator.normalJsonProcess()
    # pythonTranslator.normalJsonAfterRun()
    # return

    multi_threads_run()
    pythonTranslator.normalJsonAfterRun()
    return

    ################################################################################
    ################################################################################
    ################################################################################

    # 下面是个继承 Epub 的小小示例，同样建议先看完 Lua 部分的说明
    pythonTranslator.epubBeforeRun()
    # std::function<void(fs::path)> m_onFileProcessed;
    # 在 Epub 下这个 function 本来的作用是判断是否一本书的所有 json 都翻译完毕，如果是则创建新 Epub
    orgOnFileProcessedInEpubTranslator = pythonTranslator.m_onFileProcessed
    # 同 Lua，这里也可以是闭包
    # 但如果覆盖了这个，那么则禁止使用 normalJson/epub/...Process() 等会在 C++ 侧创建线程的函数
    # 否则会导致死锁，因为 C++ 侧开辟的线程不具有 GIL，无法调用 python 闭包
    # 并且 pythonTranslator 的 m_onFileProcessed 会取消对线程安全的保证
    # 如果继承的不是 NormalJsonTranslator，强烈建议覆写 m_onFileProcessed 并在 python 侧重新做加锁操作
    def new_call_back(rel_file_path_processed):
        logger.info("此处应当加锁")
        lock.acquire()
        try:
            orgOnFileProcessedInEpubTranslator(rel_file_path_processed)
        finally:
            lock.release()
            
    pythonTranslator.m_onFileProcessed = new_call_back
    pythonTranslator.normalJsonBeforeRun()
    if pythonTranslator.m_currentRunRelFilePaths is None:
        logger.info("可能是 DumpName/NameTrans/GenDict 之类无需 processFile 的 TransEngine")
        pythonTranslator.normalJsonAfterRun()
        return
    multi_threads_run()
    pythonTranslator.normalJsonAfterRun()
    return

def init():
    # init 后 toml 配置文件及字典文件等才会被加载
    pythonTranslator.normalJsonInit()
    # 建议先去看完 Lua 部分的插件说明
    # pythonTranslator.epubInit()

    logger.info("MySampleFilePluginFromPython starts")
    logger.info("Current inputDir: " + str(pythonTranslator.m_inputDir))
    tomlPath = pythonTranslator.m_projectDir / "config.toml"

    with open(tomlPath, "rb") as f:
        tomlConfig = tomllib.load(f)
        epubPreReg1 = tomlConfig['plugins']['Epub']['preprocRegex'][0]
        logger.info("{epubPreReg1} org: " + epubPreReg1['org'] + ", rep: " + epubPreReg1['rep'])

def unload():
    logger.info("MySampleFilePluginFromPython unloads")
    # absl::flat_hash_map<fs::path, std::map<fs::path, bool>> m_jsonToSplitFileParts;
    partsTable = pythonTranslator.m_jsonToSplitFileParts
    strs = []
    for jsonPath, filePartsMap in partsTable.items():
        for splitFilePart, completed in filePartsMap.items():
            if completed:
                strs.append(str(splitFilePart))
            else:
                logger.error(f"文件 {splitFilePart} 尚未翻译完成")
    result = "\n".join(strs)
    # logger.info("map keys: \n" + result)
