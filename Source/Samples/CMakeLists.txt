# Copyright (c) 2008-2023 the Urho3D project
# License: MIT

if (NOT URHO3D_SAMPLES)
    return ()
endif ()

# Set project name
project (Urho3D-Samples)

# В IDE примеры будут помещаться в папку samples
set (CMAKE_FOLDER samples)

# Find Urho3D library
find_package (Urho3D REQUIRED)
include_directories (${URHO3D_INCLUDE_DIRS})
include_directories (${CMAKE_SOURCE_DIR}/Source)

# Include common to all samples
set (COMMON_SAMPLE_H_FILES "${CMAKE_CURRENT_SOURCE_DIR}/Sample.h" "${CMAKE_CURRENT_SOURCE_DIR}/Sample.inl")

# Define dependency libs
set (INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

# Macro for adding sample subdirectory
macro (add_sample_subdirectory SOURCE_DIR)
    if (DEFINED ENV{included_sample})
        if (DEFINED ENV{INCLUDED_SAMPLE_${SOURCE_DIR}})
            add_subdirectory (${SOURCE_DIR} ${ARGN})
        endif ()
    elseif (NOT DEFINED ENV{EXCLUDED_SAMPLE_${SOURCE_DIR}})
        add_subdirectory (${SOURCE_DIR} ${ARGN})
    endif ()
endmacro ()

# Add samples
file (GLOB_RECURSE DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CMakeLists.txt)
list (SORT DIRS)

# 仅构建 2D/通用 UI 示例，减少构建时间与依赖
set (ALLOWED_2D_SAMPLES
    01_HelloWorld
    02_HelloGUI
    03_Sprites
    24_Urho2DSprite
    25_Urho2DParticle
    27_Physics2D
    28_Physics2DRope
    33_Urho2DSpriterAnimation
    36_Urho2DTileMap
    37_UIDrag
    47_Typography
    48_MSDFText
    49_Urho2DIsometricDemo
    50_Urho2DPlatformer
    51_Urho2DStretchableSprite
    52_2_5DCore)

foreach (DIR ${DIRS})
    get_filename_component (DIR ${DIR} PATH)
    if (DIR)
        list (FIND ALLOWED_2D_SAMPLES ${DIR} ALLOWED_INDEX)
        if (ALLOWED_INDEX GREATER -1)
            add_sample_subdirectory (${DIR})
        endif ()
    endif ()
endforeach ()
