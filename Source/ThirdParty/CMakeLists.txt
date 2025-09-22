# Copyright (c) 2022-2023 the Dviglo project
# Copyright (c) 2008-2023 the Urho3D project
# License: MIT

# В IDE сторонние библиотеки будут помещены в папку third-party libs
set (CMAKE_FOLDER "third-party libs")

# 基础第三方库：仅在目录存在时引入，避免裁剪后构建失败
set (SDL_TEST_LIBRARY OFF CACHE BOOL "Disable building SDL3_test" FORCE)
set (SDL_TESTS        OFF CACHE BOOL "Disable SDL tests"      FORCE)
set (SDL_EXAMPLES     OFF CACHE BOOL "Disable SDL examples"   FORCE)
foreach (TARGET ETCPACK FreeType LZ4 PugiXml rapidjson SDL STB)
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}/CMakeLists.txt)
        add_subdirectory (${TARGET})
    endif ()
endforeach ()

# 可选：StanHull 在 3D 物理/导入场景中使用；2D-only 可裁剪
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/StanHull/CMakeLists.txt)
    add_subdirectory (StanHull)
endif ()

if (URHO3D_ANGELSCRIPT)
    add_subdirectory (AngelScript)
    if (WEB OR (ARM AND URHO3D_64BIT) OR URHO3D_FORCE_AS_MAX_PORTABILITY)
        add_subdirectory (boost)
    endif ()
endif ()

if (URHO3D_LUA)
    add_subdirectory (Lua${JIT})
    add_subdirectory (toluapp/src/lib)
endif ()

if (URHO3D_NETWORK)
    add_subdirectory (Civetweb)
    add_subdirectory (SLikeNet)
endif ()

if (URHO3D_DATABASE_ODBC)
    add_subdirectory (nanodbc)
endif ()
if (URHO3D_DATABASE_SQLITE)
    add_subdirectory (SQLite)
endif ()

if (URHO3D_NAVIGATION)
    add_subdirectory (Detour)
    add_subdirectory (DetourCrowd)
    add_subdirectory (DetourTileCache)
    add_subdirectory (Recast)
endif ()

if (URHO3D_URHO2D)
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/spine)
        add_subdirectory (spine)
        # Add definition for Spine
        add_definitions (-DURHO3D_SPINE)
    endif ()
endif ()

if (URHO3D_WEBP)
    add_subdirectory (WebP)
endif ()

if (URHO3D_PHYSICS)
    add_subdirectory (Bullet)
endif ()

if (URHO3D_PHYSICS2D)
    add_subdirectory (Box2D)
endif ()

if (URHO3D_TRACY_PROFILING)
    add_subdirectory (Tracy)
endif ()

# 高性能分配器：mimalloc 深度集成（仅使用本地 ThirdParty），默认尝试启用；
# 若本地不存在或当前工具链无法编译 C/Windows SDK 头，则静默跳过并退回 CRT。
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mimalloc/CMakeLists.txt)
    include(CheckCSourceCompiles)
    set(CMAKE_REQUIRED_QUIET TRUE)
    check_c_source_compiles("#include <stddef.h>\n#ifdef _WIN32\n#include <windows.h>\n#endif\nint main(void){return 0;}" CAN_BUILD_C_SDK)
    if (CAN_BUILD_C_SDK)
        set (MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory (mimalloc)
        if (TARGET mimalloc-static)
            set (MIMALLOC_TARGET mimalloc-static)
        elseif (TARGET mimalloc)
            set (MIMALLOC_TARGET mimalloc)
        endif ()
        set (HAVE_MIMALLOC ON)
    else ()
        message (STATUS "mimalloc: 检测到当前工具链环境缺少标准/SDK 头，已跳过本地 mimalloc 构建（回退 CRT）")
    endif ()
else ()
    message (STATUS "mimalloc: 未发现 Source/ThirdParty/mimalloc，已跳过（回退 CRT）")
endif ()

# EASTL / EABase：构建并整库链接（推荐）。
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/EASTL AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/EABase)
    # 优先使用本地 EABase 源，避免网络获取失败或版本漂移
    set(FETCHCONTENT_SOURCE_DIR_EABASE ${CMAKE_CURRENT_SOURCE_DIR}/EABase CACHE PATH "Local EABase path for EASTL FetchContent")
    set(FETCHCONTENT_UPDATES_DISCONNECTED_EABASE ON CACHE BOOL "Disable EABase FetchContent updates" FORCE)
    # 先添加 EABase，确保 INTERFACE 目标被创建
    add_subdirectory (EABase)
    add_subdirectory (EASTL)
    # EASTL 源文件直接包含 <EABase/eabase.h>，需要把 EABase 的 include 路径注入到 EASTL 目标
    if (TARGET EASTL AND TARGET EABase)
        get_target_property(_EABASE_INCS EABase INTERFACE_INCLUDE_DIRECTORIES)
        if (_EABASE_INCS)
            target_include_directories(EASTL PUBLIC ${_EABASE_INCS})
        endif ()
    endif ()
    # 兜底：直接注入本地 EABase 头文件路径，防止上游 CMake 未正确透传
    if (TARGET EASTL)
        target_include_directories(EASTL PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/EABase/include/Common)
    endif ()
endif ()

if (NOT ANDROID AND NOT ARM AND NOT WEB)
    # Tina2D: 不再使用传统 OpenGL 渲染路径，彻底移除 GLEW 依赖
    # 如果未来需要恢复 OpenGL 支持，可在此处重新引入 GLEW。

    if (NOT CMAKE_SYSTEM_NAME STREQUAL Linux)
        add_subdirectory (LibCpuId)
    endif ()
endif ()

# В CMake нельзя менять переменные, объявленные выше родительской области видимости
# https://levelup.gitconnected.com/cmake-variable-scope-f062833581b7
# Вызовы макроса setup_library() пополняют список статических таргетов STATIC_LIBRARY_TARGETS,
# используя PARENT_SCOPE (т.е. в scope папки ThirdParty).
# Пробрасываем сформированный список ещё на уровень выше, чтобы список был доступен
# в scope папки Source.
# Возможно вместо этого стоит использовать GLOBAL property в качестве глобальной переменной
set (STATIC_LIBRARY_TARGETS ${STATIC_LIBRARY_TARGETS} PARENT_SCOPE)

# 可选引入 bgfx（本地优先且仅本地）：要求存在目录 Source/ThirdParty/bgfx.cmake
if (URHO3D_BGFX)
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/bgfx.cmake/CMakeLists.txt)
        message (STATUS "Using local bgfx.cmake at ThirdParty/bgfx.cmake (no FetchContent)")
        # 关闭 bgfx 的示例构建，保留工具（shaderc 等）以用于着色器编译
        set (BGFX_BUILD_EXAMPLES OFF CACHE BOOL "Disable bgfx examples" FORCE)
        set (BGFX_BUILD_TESTS    OFF CACHE BOOL "Disable bgfx tests"    FORCE)
        set (BGFX_BUILD_TOOLS    ON  CACHE BOOL "Build bgfx tools"       FORCE)
        # 仅需 shaderc，禁用纹理工具链，避免 iqa 等 C 依赖在非开发者命令行下找不到 SDK 头
        set (BGFX_BUILD_TOOLS_TEXTURE OFF CACHE BOOL "Disable bgfx texture tools" FORCE)
        set (BGFX_BUILD_TOOLS_GEOMETRY OFF CACHE BOOL "Disable bgfx geometry tools" FORCE)
        set (BGFX_BUILD_TOOLS_BIN2C OFF CACHE BOOL "Disable bgfx bin2c tool" FORCE)
        add_subdirectory (bgfx.cmake)
        # 将 bgfx 工具 (shaderc/texturec 等) 拷贝到引擎惯用的 tool 目录
        # 注意：add_custom_command(TARGET ...) 只能作用于当前目录创建的目标，因此
        # 这里采用独立的自定义目标 copy_bgfx_tools，并为其添加依赖与拷贝命令。
        set (BGFX_TOOLS_DIR ${CMAKE_BINARY_DIR}/bin/tool)
        file (MAKE_DIRECTORY ${BGFX_TOOLS_DIR})

        # 收集可用的工具目标（不同平台/配置可能并非全部可用）
        set (_BGFX_TOOL_CANDIDATES shaderc texturec geometryc texturev shaderbin2c)
        set (_BGFX_TOOL_TARGETS)
        foreach (TGT ${_BGFX_TOOL_CANDIDATES})
            if (TARGET ${TGT})
                list (APPEND _BGFX_TOOL_TARGETS ${TGT})
            endif ()
        endforeach ()

        if (_BGFX_TOOL_TARGETS)
            add_custom_target (copy_bgfx_tools ALL
                COMMAND ${CMAKE_COMMAND} -E make_directory ${BGFX_TOOLS_DIR}
                COMMENT "Copying bgfx tools to ${BGFX_TOOLS_DIR}")
            foreach (TGT ${_BGFX_TOOL_TARGETS})
                add_dependencies (copy_bgfx_tools ${TGT})
                add_custom_command (TARGET copy_bgfx_tools POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${TGT}> ${BGFX_TOOLS_DIR}
                    COMMENT "Copy bgfx tool: ${TGT}")
            endforeach ()
        endif ()

        # 兼容 ShaderUtils.cmake：该模块期望存在可执行目标 bgfx::shaderc
        # 标准 bgfx.cmake 定义的工具目标名为 shaderc，这里创建一个别名以便统一调用
        if (TARGET shaderc AND NOT TARGET bgfx::shaderc)
            add_executable(bgfx::shaderc ALIAS shaderc)
        endif ()
    else ()
        message (FATAL_ERROR "URHO3D_BGFX=ON 但未找到本地 ThirdParty/bgfx.cmake。请先将 bgfx.cmake 克隆到 Source/ThirdParty/bgfx.cmake 再配置构建。")
    endif ()
endif ()
