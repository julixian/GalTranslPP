set(VCPKG_BUILD_TYPE release) # header-only

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO julixian/sol2
    REF 481456db8ec6e6974ef6f63d8e002861ad2c435c
    SHA512 7a0ff80e9c063aa2c10e30b9dfc3eddb5bfa98d1e698bdd6f6dd411088100546568d16fc9911d4845ca4b4d402827a1bfd1575434e7037f46ab9cb4044b99da1
    HEAD_REF develop
    PATCHES
        header-only.patch
        lua-5.5.diff # variation of https://github.com/ThePhD/sol2/pull/1723
        pkgconfig.diff
)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH share/cmake/sol2)
vcpkg_fixup_pkgconfig()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
