# Copyright (c) 2008-2023 the Urho3D project
# License: MIT

# Define target name
set (TARGET_NAME WebP)

# 定义源码：包含 upstream 的 src/ 与 sharpyuv/
# 说明：webp 近版本将 YUV 转换拆分为独立的 sharpyuv 库，需一并编译
define_source_files (
    RECURSE
    GLOB_CPP_PATTERNS
        src/*.c
        sharpyuv/*.c
    GLOB_H_PATTERNS
        src/*.h
        sharpyuv/*.h
)

# 头文件包含路径
# - 将当前目录与 src/、sharpyuv/ 纳入包含，满足诸如 "sharpyuv/sharpyuv.h" 的包含方式
set (INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sharpyuv
)
if (ANDROID)
    list (APPEND INCLUDE_DIRS ${ANDROID_NDK}/sources/android/cpufeatures)
endif ()

# Setup target
setup_library ()

# Install headers for building the Urho3D library
install_header_files (DIRECTORY src/webp/ DESTINATION ${DEST_INCLUDE_DIR}/ThirdParty/webp FILES_MATCHING PATTERN *.h BUILD_TREE_ONLY)  # Note: the trailing slash is significant
