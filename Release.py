"""Copy release data into the layouts used by the CLI and GUI."""

import shutil
from pathlib import Path


ROOT = Path(__file__).resolve().parent
RELEASE = ROOT / "Release"
BASE_CONFIG = ROOT / "Example" / "BaseConfig"
PYTHON_ARCHIVE = "Python-3.12.10-embed-amd64.zip"


def copy_tree(source: Path, destination: Path, *excluded: str) -> None:
    if not source.is_dir():
        raise FileNotFoundError(f"Missing release input: {source}")
    shutil.copytree(
        source, destination, dirs_exist_ok=True,
        ignore=shutil.ignore_patterns(*excluded) if excluded else None,
    )


def main() -> None:
    opencc = (ROOT / "vcpkg_installed" / "gpp-x64-windows-release"
              / "share" / "opencc")
    copy_tree(opencc, BASE_CONFIG / "opencc")

    for member in ("GPPCLI", "GPPGUI"):
        copy_tree(BASE_CONFIG, RELEASE / member / "BaseConfig", PYTHON_ARCHIVE)
    copy_tree(
        BASE_CONFIG, RELEASE / "GUICORE" / "BaseConfig",
        PYTHON_ARCHIVE, "GlobalConfig.toml", "mecab", "Python-3.12.10-embed-amd64",
    )

    archive_dll = ROOT / "3rdParty" / "7z.dll"
    if not archive_dll.is_file():
        raise FileNotFoundError(f"Missing release input: {archive_dll}")
    for member in ("GPPCLI", "GPPGUI", "GUICORE"):
        destination = RELEASE / member
        destination.mkdir(parents=True, exist_ok=True)
        shutil.copy2(archive_dll, destination / archive_dll.name)

    copy_tree(ROOT / "Example" / "SampleProject",
              RELEASE / "GPPCLI" / "SampleProject")


if __name__ == "__main__":
    main()
