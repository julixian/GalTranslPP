from pathlib import Path
import threading
import shutil
import subprocess
import os
import re
import sys

def process_fullwidth_space_lines(folder_path: str, encoding: str, action: str, suffix: str = ".txt") -> None:
    """
    递归遍历文件夹，处理所有 .txt 文件中只有一个全角空格的行。

    :param folder_path: 指定的文件夹路径
    :param encoding: 文件的编码格式 (例如: 'utf-8', 'gbk')
    :param action: 操作类型。
                   'empty'  -> 将该行变成空文本（保留换行符，即变成空行）
                   'delete' -> 直接删掉这一行
    """
    # 校验操作参数
    if action not in ('empty', 'delete'):
        raise ValueError("action 参数必须是 'empty' 或 'delete'")

    folder = Path(folder_path)
    if not folder.is_dir():
        print(f"错误：指定的文件夹不存在 -> {folder_path}")
        return

    # 全角空格的 Unicode 字符
    FULLWIDTH_SPACE = '\u3000' 
    processed_count = 0

    # 递归遍历所有文件
    for file_path in folder.rglob('*'):
        # 检查是否为文件，且后缀为 suffix（不区分大小写）
        if file_path.is_file() and file_path.suffix.lower() == suffix:
            try:
                # 以指定编码读取文件
                with open(file_path, 'r', encoding=encoding) as f:
                    lines = f.readlines()

                new_lines = []
                modified = False

                for line in lines:
                    # 去除行尾的换行符(\n 或 \r\n)后，判断剩下的内容是否只有一个全角空格
                    if line.strip('\r\n') == FULLWIDTH_SPACE:
                        modified = True
                        if action == 'empty':
                            # 变成空文本：保留原有的换行符，去掉全角空格
                            # line[1:] 就是去掉第一个字符(全角空格)后的内容(即换行符)
                            new_lines.append(line[1:])
                        elif action == 'delete':
                            # 删掉这一行：直接不加入到新列表中
                            pass
                    else:
                        # 不符合条件的行原样保留
                        new_lines.append(line)

                # 只有当文件内容真正发生改变时，才重新写入文件（保护硬盘，提高效率）
                if modified:
                    with open(file_path, 'w', encoding=encoding) as f:
                        f.writelines(new_lines)
                    print(f"已修改: {file_path}")
                    processed_count += 1

            except UnicodeDecodeError:
                print(f"警告：编码错误跳过 (请确认该文件是否为 {encoding} 编码) -> {file_path}")
            except PermissionError:
                print(f"警告：权限不足跳过 -> {file_path}")
            except Exception as e:
                print(f"警告：处理文件时发生未知错误 {file_path} -> {e}")

    print(f"\n处理完成！共修改了 {processed_count} 个文件。")

def run_script_in_its_dir(script_path, to_input = "\n"):

    # 1. 获取目标脚本的绝对路径
    abs_script_path = os.path.abspath(script_path)
    # 2. 获取目标脚本所在的目录
    target_dir = os.path.dirname(abs_script_path)

    try:
        subprocess.run(
            [sys.executable, abs_script_path], 
            cwd=target_dir,
            check=True,              # 如果子脚本执行报错（返回码非0），则抛出异常
            text=True,               # 将输入输出作为字符串处理，而不是字节
            input=to_input
        )
        print("\n--- 子脚本执行成功 ---")
            
    except subprocess.CalledProcessError as e:
        print("\n--- 子脚本执行失败 ---")

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
    projectPath = Path(r"C:\Users\julixian\Desktop\Works\VS\JLXHP\Release\Komorebi")
    packPyScript = projectPath / "pack.py"
    newScPath = projectPath / "Komorebi_cn_script"
    basePath = projectPath / "Komorebi_cn_base"
    newCharMapPath = basePath / "base"

    shutil.copytree("text__", "data11/scenario", dirs_exist_ok=True)
    process_fullwidth_space_lines("text__", "cp932", "delete", ".s")
    shutil.copytree("data11", newScPath, dirs_exist_ok=True)

    result = subprocess.run(
                ["MergeJsonTransMap.exe", "textproc\\trans", "textproc\\orig", newCharMapPath / "transMap.json"],
                capture_output=True,  # 捕获输出
                text=True,            # 以文本形式返回
                encoding='utf-8'
            )
    print(f"返回输出: {result.stdout}")

    shutil.copy2("Komorebi_mjp.ttf", basePath / "Komorebi_cn" / "Font")
    run_script_in_its_dir(packPyScript)
    wait_for_exit(3)

except Exception as e:
    print(f"Error during unload(): {e}")
    input("Enter any key to exit.")
    sys.exit(1)
    