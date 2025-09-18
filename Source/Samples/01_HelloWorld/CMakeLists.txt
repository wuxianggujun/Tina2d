# Copyright (c) 2008-2023 the Urho3D project
# License: MIT

# Define target name
set (TARGET_NAME 01_HelloWorld)

# Define source files
define_source_files (EXTRA_H_FILES ${COMMON_SAMPLE_H_FILES})

# Setup target with resource copying
setup_main_executable ()

# 确保示例在构建前已生成 bgfx 着色器
if (TARGET tina2d_bgfx_shaders)
    add_dependencies (${TARGET_NAME} tina2d_bgfx_shaders)
endif ()

# Setup test cases
setup_test ()
