# 必须: 导入 C++ 绑定的模块
import gpp_plugin_api as gpp
from pathlib import Path
import tomllib
import threading
import queue
import time
import shutil
import subprocess

# 所有已注册类型和函数详见 GalTranslPP/PythonManager.cpp

# 必须: 声明 pythonTranslator
# C++ 会在 init 前将此属性赋值为基类指针
pythonTranslator = None

logger = None

runUnloadFunc = True

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

def multi_threads_run(relFilePaths):
    # 多线程使用手动线程管理
    # 因为子解释器里用 多进程 或 ThreadPoolExecutor 会有一些妙妙小问题
    # 在不进行大模型翻译时行为趋近于单线程，因为我只在大模型翻译部分释放了 C++ 侧占有的 GIL
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

def run():
    relFilePaths = pythonTranslator.normalJsonTranslator_beforeRun()
    if relFilePaths is None:
        global runUnloadFunc
        runUnloadFunc = False
        logger.info("可能是 DumpName/NameTrans/GenDict 之类无需 processFile 的 TransEngine")
        return

    multi_threads_run(relFilePaths)
    return


def init():
    # init 后 toml 配置文件及字典文件等才会被加载
    pythonTranslator.normalJsonTranslator_init()
    logger.info("MyFilePluginFromPython starts")
    logger.info("Current inputDir: " + str(pythonTranslator.m_inputDir))

def unload():
    try:
        if runUnloadFunc and pythonTranslator.m_transEngine != gpp.ShowNormal:

            fontChangerPath = pythonTranslator.m_projectDir / "DynamicFontChanger.exe"
            gamePath = Path(r"D:\GALGAME\linshi\ボンボンカンパニー\貧は僕らの福の神")
            targetTransPath = gamePath / "textproc" / "trans"
            charMapPath = pythonTranslator.m_projectDir / "charMap.json"
            charsNotMapPath = pythonTranslator.m_projectDir / "charsNotMap.json"
            newFontPath = pythonTranslator.m_projectDir / "B  I  N  B  O_mjp.ttf"

            result = subprocess.run(
                [fontChangerPath, "-j", "gt_output", 
                 "-i", "julixiansimhei-Regular.ttf", 
                 "-f", "B  I  N  B  O",
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

    pythonTranslator.normalJsonTranslator_afterRun()
    logger.info("MyFilePluginFromPython unloads")
    
