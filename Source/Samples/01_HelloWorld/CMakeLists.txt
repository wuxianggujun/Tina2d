# Copyright (c) 2008-2023 the Urho3D project
# License: MIT

# Define target name
set (TARGET_NAME 01_HelloWorld)

# Define source files
define_source_files (EXTRA_H_FILES ${COMMON_SAMPLE_H_FILES})

# Setup target with resource copying
setup_main_executable ()

# 注意：着色器依赖已通过 Urho3D 库传递，无需在示例中重复声明

# Setup test cases
setup_test ()
