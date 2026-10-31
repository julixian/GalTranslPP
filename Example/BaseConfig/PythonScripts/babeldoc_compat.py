"""为没有启用的 BabelDOC 功能提供导入兼容，不替代实际检测或术语匹配。"""

import importlib.util
import sys
from types import ModuleType


def _structural_similarity(*args, **kwargs):
    raise NotImplementedError("GPP 的 BabelDOC 裁剪版未启用扫描件相似度检测。")


def prepare_optional_imports():
    # 已安装的真实模块优先；真实模块本身损坏时仍正常报错，不用兼容模块掩盖问题。
    if "skimage" not in sys.modules and importlib.util.find_spec("skimage") is None:
        skimage = ModuleType("skimage")
        skimage.__path__ = []
        metrics = ModuleType("skimage.metrics")
        metrics.structural_similarity = _structural_similarity
        skimage.metrics = metrics
        sys.modules["skimage"] = skimage
        sys.modules["skimage.metrics"] = metrics
    if "hyperscan" not in sys.modules and importlib.util.find_spec("hyperscan") is None:
        sys.modules["hyperscan"] = ModuleType("hyperscan")
