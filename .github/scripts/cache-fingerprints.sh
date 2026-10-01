#!/usr/bin/env bash
# 每个缓存一行："<名称> <摘要>"。windows.yml 在构建前后各算一次，摘要变化的缓存才保存。
# 摘要只读目录结构与索引文件，不逐个读取大目录中的文件。
set -uo pipefail
digest() { sha256sum | cut -c1-16; }
echo "packages $( { ls -1d "$HOME"/.mcpp/registry/data/xpkgs/*/* 2>/dev/null; ls -1 "$HOME"/.mcpp/provisioned 2>/dev/null; } | digest)"
echo "vcpkg_installed $(cat vcpkg_installed/*/vcpkg/status 2>/dev/null | digest)"
echo "vcpkg_binaries $(find "$HOME/AppData/Local/vcpkg/archives" -type f -printf '%P %s\n' 2>/dev/null | sort | digest)"
echo "deps_cmake $(find "$HOME/AppData/Local/mcpp-plugins/deps-cmake" -mindepth 2 -maxdepth 2 -type d 2>/dev/null | sort | digest)"
