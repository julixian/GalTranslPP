"""Copy release data into the layouts used by the CLI and GUI."""

import shutil
from pathlib import Path


kScriptRoot = Path(__file__).resolve().parent
kReleaseDir = kScriptRoot / "Release"
kBaseConfigDir = kScriptRoot / "Example" / "BaseConfig"
kPythonArchive = "Python-3.12.10-embed-amd64.zip"


def copyTree(source: Path, destination: Path, *excluded: str) -> None:
    if not source.is_dir():
        raise FileNotFoundError(f"Missing release input: {source}")
    shutil.copytree(
        source, destination, dirs_exist_ok=True,
        ignore=shutil.ignore_patterns(*excluded) if excluded else None,
    )


def main() -> None:
    for member in ("GPPCLI", "GPPGUI"):
        copyTree(kBaseConfigDir, kReleaseDir / member / "BaseConfig", kPythonArchive, "opencc")
    copyTree(
        kBaseConfigDir, kReleaseDir / "GUICORE" / "BaseConfig",
        kPythonArchive, "opencc", "GlobalConfig.toml", "mecab", "Python-3.12.10-embed-amd64",
    )

    archiveDll = kScriptRoot / "3rdParty" / "7z.dll"
    if not archiveDll.is_file():
        raise FileNotFoundError(f"Missing release input: {archiveDll}")
    for member in ("GPPCLI", "GPPGUI", "GUICORE"):
        destination = kReleaseDir / member
        destination.mkdir(parents=True, exist_ok=True)
        shutil.copy2(archiveDll, destination / archiveDll.name)

    copyTree(kScriptRoot / "Example" / "SampleProject",
              kReleaseDir / "GPPCLI" / "SampleProject")


if __name__ == "__main__":
    main()
