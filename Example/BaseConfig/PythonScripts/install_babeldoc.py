"""安装 GPP 的 PDF 流程使用的 BabelDOC 及其裁剪依赖。"""

import subprocess
import sys
from pathlib import Path


def main():
    requirements = Path(__file__).with_name("requirements-babeldoc.txt")
    subprocess.run([sys.executable, "-m", "pip", "install", "-r", str(requirements)], check=True)
    # 完整元数据还声明了我们不使用的功能；单独安装本体，避免 pip 把裁掉的依赖装回来。
    subprocess.run([sys.executable, "-m", "pip", "install", "--no-deps", "babeldoc==0.6.4"], check=True)


if __name__ == "__main__":
    main()
