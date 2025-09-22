# Copyright (c) 2008-2023 the Urho3D project
# License: MIT

# 定义目标名
set (TARGET_NAME Box2D)

# 定义源文件（使用本地最新已同步的 include/src）
define_source_files (RECURSE GLOB_CPP_PATTERNS src/*.cpp src/*.h GLOB_H_PATTERNS include/box2d/*.h GROUP)

set (SOURCE_FILES ${CPP_FILES} ${H_FILES})

# 头文件包含目录
set (INCLUDE_DIRS include)

# 构建静态库目标
setup_library ()

# 安装头文件到 SDK 与构建树
install_header_files (DIRECTORY include/box2d DESTINATION ${DEST_INCLUDE_DIR}/ThirdParty)
