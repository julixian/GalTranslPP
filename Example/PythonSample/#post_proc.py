from pathlib import Path
import threading
import shutil
import subprocess
import os
import re
import sys

def wait_for_exit(sec):
    input_thread = threading.Thread(
        target=lambda: input(f"{sec} 秒钟后自动退出，或按回车直接退出"),
        # 将线程设置为守护线程 (Daemon)
        # 这样当主程序结束时，即使 input() 还在等待，整个程序也会强制退出
        daemon=True 
    )
    input_thread.start()
    input_thread.join(timeout=sec)

try:
    projectPath = Path(r"D:\VSProj\JLXHP\x64\Release\LibidoAventure")
    projectBaseName = str(projectPath.name)
    packPyScript = projectPath / "pack.py"
    basePath = projectPath / (projectBaseName + "_cn_base")
    newCharMapPath = basePath / "base"
    newScriptPath = projectPath / (projectBaseName + "_cn_script")

    subprocess.run(
                [sys.executable, r"AdvHD_ws2_Toolkit\src\ws2_json_handler.py",
                 "import", "--output-encrypt", "encrypted",
                 "Rio1__bak", "Rio1__cn", (newScriptPath / "Rio1")]
            )

    subprocess.run(
                ["MergeJsonTransMap.exe",
                 "Rio1__cn", "Rio1__jp", newCharMapPath / "transMap.json"]
            )

    shutil.copy2(projectBaseName + "_mjp.ttf",
                 basePath / (projectBaseName + "_cn") / "Font")
    shutil.copy2("charMap.json", newCharMapPath)
    shutil.copy2("charsNotMap.json", newCharMapPath)

    subprocess.run(
                [sys.executable, packPyScript],
                cwd=packPyScript.parent,
                text=True,               # 将输入输出作为字符串处理，而不是字节
                input="\n"
            )

    wait_for_exit(3)

except Exception as e:
    print(f"Error: {e}")
    input("Enter any key to exit.")
    sys.exit(1)
    