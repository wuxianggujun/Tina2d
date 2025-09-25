# Copyright (c) 2008-2023 the Urho3D project
# License: MIT

set (TARGET_NAME Tina2D_Editor)

# 查找并引入 Urho3D 头文件与链接
find_package (Urho3D REQUIRED)
include_directories (${URHO3D_INCLUDE_DIRS})
# 直接加入 Source 根，便于包含 <Urho3D/Precompiled.h>
include_directories (${CMAKE_SOURCE_DIR}/Source)

# 收集源码
define_source_files (RECURSE GROUP GLOB_CPP_PATTERNS *.cpp GLOB_H_PATTERNS *.h)

# 构建带资源复制的可执行文件
setup_main_executable ()

