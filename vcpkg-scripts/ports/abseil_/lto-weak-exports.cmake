# CMake 的自动导出忽略 llvm-nm 的 W 类型；LTO 模板实例需要额外保留这些函数。
if(DEFINED ABSL_LTO_OBJECT_DIR)
    file(GLOB_RECURSE objects "${ABSL_LTO_OBJECT_DIR}/*.obj")
    set(symbols "")
    foreach(object IN LISTS objects)
        execute_process(COMMAND "${ABSL_LTO_NM}" --defined-only "${object}"
            OUTPUT_VARIABLE output COMMAND_ERROR_IS_FATAL ANY)
        string(REGEX MATCHALL "(^|\n)[^\n]* W [^\n\r]+" matches "${output}")
        foreach(match IN LISTS matches)
            string(REGEX REPLACE "^.* W " "" symbol "${match}")
            list(APPEND symbols "${symbol}")
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES symbols)
    list(SORT symbols)
    list(JOIN symbols "\n" exports)
    file(WRITE "${ABSL_LTO_DEF}" "EXPORTS\n${exports}\n")
    return()
endif()

if(ABSL_BUILD_DLL AND WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
        AND "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}" MATCHES "-flto")
    set(weak_def "${CMAKE_CURRENT_BINARY_DIR}/abseil-lto-weak.def")
    if(NOT EXISTS "${weak_def}")
        file(WRITE "${weak_def}" "EXPORTS\n")
    endif()
    # PRE_LINK 先生成补充列表，CMake 再将它与普通自动导出合并。
    target_sources(abseil_dll PRIVATE "${weak_def}")
    add_custom_command(TARGET abseil_dll PRE_LINK
        COMMAND "${CMAKE_COMMAND}"
            "-DABSL_LTO_OBJECT_DIR=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/abseil_dll.dir"
            "-DABSL_LTO_NM=${CMAKE_NM}"
            "-DABSL_LTO_DEF=${weak_def}"
            -P "${CMAKE_CURRENT_LIST_FILE}"
        VERBATIM)
endif()
