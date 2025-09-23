// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

// 统一 EASTL / std 的适配层：
// - 若检测到 EASTL 头文件，则将 Urho3D::stl 指向 eastl 命名空间；
// - 否则回退到标准库 std，保证构建不因缺失 EASTL 而失败。

// 注意：本适配层仅提供最常用容器与字符串的包含与命名空间别名，
// 如需更多容器，请在此处按需补充引入。

#if defined(__has_include)
#  if __has_include(<EASTL/vector.h>)
#    define URHO3D_USE_EASTL 1
#  endif
#endif

#if URHO3D_USE_EASTL
  #include <EASTL/vector.h>
  #include <EASTL/array.h>
  #include <EASTL/string.h>
  #include <EASTL/unique_ptr.h>
  #include <EASTL/shared_ptr.h>
  #include <EASTL/unordered_map.h>
  #include <EASTL/unordered_set.h>
  #include <EASTL/map.h>
  #include <EASTL/set.h>
  #include <EASTL/functional.h>
  namespace Urho3D { namespace stl = eastl; }
#else
  #include <vector>
  #include <array>
  #include <string>
  #include <memory>
  #include <unordered_map>
  #include <unordered_set>
  #include <map>
  #include <set>
  #include <functional>
  namespace Urho3D { namespace stl = std; }
#endif

