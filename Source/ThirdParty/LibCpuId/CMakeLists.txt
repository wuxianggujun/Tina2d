# Copyright (c) 2008-2023 the Urho3D project
# License: MIT

# Define target name
set (TARGET_NAME LibCpuId)

# Define preprocessor macros
add_definitions (-DVERSION="0.4.0+")
if (HAVE_STDINT_H)
    add_definitions (-DHAVE_STDINT_H)
endif ()

# Define generated source files
if (MSVC AND URHO3D_64BIT)
    enable_language (ASM_MASM)
    # Commented out due to cmake bug
    #set (ASM_FILES src/masm-x64.asm)
    # As a temporary workaround
    # 说明：此前同时将編译产物 masm-x64.obj 与源文件 src/masm-x64.asm
    # 一并加入 ASM_FILES，最终会在静态库中产生两份等价实现（masm-x64.asm.obj 与 masm-x64.obj），
    # 导致链接阶段出现 LNK4006 重复定义（exec_cpuid/cpu_rdtsc/busy_sse_loop）。
    # 这里仅保留自定义命令生成的目标对象（.obj），避免重复收录同一实现。
    add_custom_command (
        OUTPUT masm-x64.obj
        COMMAND ${CMAKE_ASM_MASM_COMPILER} -Fo ${CMAKE_CURRENT_BINARY_DIR}/masm-x64.obj -c ${CMAKE_CURRENT_SOURCE_DIR}/src/masm-x64.asm
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/masm-x64.asm
        COMMENT "Temporary workaround for MASM (single .obj to avoid duplicates)"
    )
    set (ASM_FILES ${CMAKE_CURRENT_BINARY_DIR}/masm-x64.obj)
endif ()

# Define source files
define_source_files (GLOB_CPP_PATTERNS src/*.c GLOB_H_PATTERNS src/*.h)
list (APPEND SOURCE_FILES ${ASM_FILES})

# Define dependency libs
set (INCLUDE_DIRS src)

# Setup target
setup_library ()

# Install headers for building the Urho3D library
install_header_files (DIRECTORY src/ DESTINATION ${DEST_INCLUDE_DIR}/ThirdParty/LibCpuId FILES_MATCHING PATTERN *.h BUILD_TREE_ONLY)  # Note: the trailing slash is significant
