# Copyright (c) 2022-2023 the Dviglo project
# Copyright (c) 2008-2023 the Urho3D project
# License: MIT

# В IDE сторонние библиотеки будут помещены в папку third-party libs
set (CMAKE_FOLDER "third-party libs")

# 基础第三方库：仅在目录存在时引入，避免裁剪后构建失败
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
