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
    projectPath = Path(r"D:\VSProj\JLXHP\Release\DimensionToTsuLovers")
    packPyScript = projectPath / "pack.py"
    newScPath = projectPath / "DimensionToTsuLovers_cn_script"
    basePath = projectPath / "DimensionToTsuLovers_cn_base"
    newCharMapPath = basePath / "base"

    shutil.copy2(r"#Script\textproc\new\text.txt", r"#Script")

    subprocess.run(
                [sys.executable, r"pac_text_ju.py", "-i"],
                cwd=r"#Script"
            )
    shutil.copy2(r"#Script\TEXT.DAT_NEW", r"data_patched\TEXT_GEMINI.DAT")
    shutil.copy2(r"#Script\SCRIPT.SRC_NEW", r"data_patched\SCRIPT_GEMINI.SRC")
    shutil.copytree("data_patched", newScPath / "patch", dirs_exist_ok=True)

    subprocess.run(
                ["MergeJsonTransMap.exe", "#Script\\textproc\\trans", "#Script\\textproc\\orig", newCharMapPath / "transMap_gemini.json"]
            )

    shutil.copy2("DimensionToTsuLovers_mjp.ttf", basePath / "DimensionToTsuLovers_cn" / "Font")

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
    