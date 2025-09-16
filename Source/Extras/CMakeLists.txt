# Copyright (c) 2008-2023 the Urho3D project
# License: MIT

# Set project name
project (Urho3D-Extras)

# Find Urho3D library
find_package (Urho3D REQUIRED)
include_directories (${URHO3D_INCLUDE_DIRS})

# 2D-only 清理：移除 3D 相关额外工具（OgreBatchConverter 已删除）
