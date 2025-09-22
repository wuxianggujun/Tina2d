# Copyright (c) 2008-2023 the Urho3D project
# License: MIT

# Define target name
set (TARGET_NAME FreeType)

add_definitions (-DFT2_BUILD_LIBRARY)

# 某些平台上（旧 Clang/Assimp 组合）可能出现 zlib 符号冲突，旧工程通过重命名规避。
# 现代 FreeType 不再默认启用压缩子模块，通常不必强制改符号；若用户需要可在本地打开。
# 保持原逻辑但更保守，仅在明确 Debug 或 Xcode 下启用。
if (XCODE OR CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions (-Dz_verbose=ft2_z_verbose -Dz_error=ft2_z_error)
endif ()

# Define source files
# 源文件清单更新到上游 2.13+ 结构，保留原有模块，附加若干新增基础模块（如 ftcolor/psprops 等）。
set (SOURCE_FILES
    src/autofit/autofit.c
    src/base/ftadvanc.c
    src/base/ftbbox.c
    src/base/ftbdf.c
    src/base/ftbitmap.c
    src/base/ftcalc.c
    src/base/ftcid.c
    src/base/ftcolor.c
    src/base/ftdebug.c
    src/base/fterrors.c
    src/base/ftfntfmt.c
    src/base/ftfstype.c
    src/base/ftgasp.c
    src/base/ftgloadr.c
    src/base/ftglyph.c
    src/base/ftgxval.c
    src/base/fthash.c
    src/base/ftinit.c
    src/base/ftlcdfil.c
    src/base/ftmm.c
    src/base/ftobjs.c
    src/base/ftotval.c
    src/base/ftoutln.c
    src/base/ftpatent.c
    src/base/ftpfr.c
    src/base/ftpsprop.c
    src/base/ftrfork.c
    src/base/ftsnames.c
    src/base/ftstream.c
    src/base/ftstroke.c
    src/base/ftsynth.c
    src/base/ftsystem.c
    src/base/fttrigon.c
    src/base/fttype1.c
    src/base/ftutil.c
    src/base/ftwinfnt.c
    src/bdf/bdf.c
    src/bzip2/ftbzip2.c
    src/cache/ftcache.c
    src/cff/cff.c
    src/cid/type1cid.c
    src/gxvalid/gxvalid.c
    src/gzip/ftgzip.c
    src/lzw/ftlzw.c
    src/otvalid/otvalid.c
    src/pcf/pcf.c
    src/pfr/pfr.c
    src/psaux/psaux.c
    src/pshinter/pshinter.c
    src/psnames/psmodule.c
    src/raster/raster.c
    src/sfnt/sfnt.c
    src/smooth/smooth.c
    src/truetype/truetype.c
    src/type1/type1.c
    src/type42/type42.c
    src/winfonts/winfnt.c)

# 新增：SDF/SVG 渲染器（提供 ft_sdf_renderer_class / ft_bitmap_sdf_renderer_class / ft_svg_renderer_class）
list (APPEND SOURCE_FILES
    src/sdf/ftsdfrend.c
    src/sdf/ftbsdf.c
    src/sdf/ftsdf.c
    src/sdf/ftsdfcommon.c
    src/svg/ftsvg.c)

# Define dependency libs
set (INCLUDE_DIRS include)

# Setup target
setup_library ()

# Install headers for building the Urho3D library
install_header_files (DIRECTORY include/freetype DESTINATION ${DEST_INCLUDE_DIR}/ThirdParty FILES_MATCHING PATTERN *.h BUILD_TREE_ONLY)
install_header_files (FILES include/ft2build.h DESTINATION ${DEST_INCLUDE_DIR}/ThirdParty FILES_MATCHING PATTERN *.h BUILD_TREE_ONLY)
