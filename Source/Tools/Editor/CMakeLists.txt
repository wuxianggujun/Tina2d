# Copyright (c) 2008-2023 the Urho3D project
# License: MIT

set (TARGET_NAME Tina2D_Editor)

# 收集源码
define_source_files (RECURSE GROUP GLOB_CPP_PATTERNS *.cpp GLOB_H_PATTERNS *.h)

# 构建带资源复制的可执行文件
setup_main_executable ()

