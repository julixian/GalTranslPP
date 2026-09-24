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
    patchPackPath = Path("#g31pp补丁")
    projectPackPath = Path(r"D:\VSProj\JLXHP\x86\Release\clacra")
    projectName = projectPackPath.name
    packScriptPath = projectPackPath / "pack.py"
    basePackagePath = projectPackPath / (projectName + "_cn_base")
    scriptPackagePath = projectPackPath / (projectName + "_cn_script")
    baseDir = basePackagePath / "base"

    patchPackPath.mkdir(parents=True, exist_ok=True)


    subprocess.run(
        ["work/text_tool.exe", "inject",
         "clacra_rep.asm.txt", "rep/clacra_rep.json",
         "-o", "clacra_rep.injected.asm.txt"],
        cwd="work"
    )

    subprocess.run(
        ["work/script_tool.exe", "assemble",
         "clacra_rep.injected.asm.txt", "-o", "clacra_rep.injected.rebuild"],
        cwd="work"
    )

    subprocess.run(
        ["MergeJsonTransMap.exe", "work/rep", "work/bak", baseDir / "transMap.json"]
    )

    shutil.copy2("work/clacra_rep.injected.rebuild", scriptPackagePath / "clacra.hcb")
    shutil.copy2(projectName + "_mjp.ttf",
                 basePackagePath / (projectName + "_cn") / "Font")
    shutil.copy2("charMap.json", baseDir)
    shutil.copy2("charsNotMap.json", baseDir)
    shutil.copy2("charMap.json", patchPackPath)
    shutil.copy2("charsNotMap.json", patchPackPath)

    subprocess.run(
        [sys.executable, packScriptPath],
        cwd=packScriptPath.parent,
        text=True,               # 将输入输出作为字符串处理，而不是字节
        input="\n"
    )

    wait_for_exit(3)

except Exception as e:
    print(f"Error: {e}")
    input("Enter any key to exit.")
    sys.exit(1)
    