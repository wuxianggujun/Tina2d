// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

// 基于 EASTL::hash_map 的轻量包装，直接暴露 EASTL 迭代器与 pair(first/second)
#include <EASTL/hash_map.h>
#include <EASTL/functional.h>
#include <type_traits>
#include "../Container/Pair.h"
#include "../Container/Vector.h"
#include "../Container/Hash.h"

namespace Urho3D
{

// 仅使用 EASTL 接口，不提供任何兼容方法/迭代器别名
template <class K, class V>
using HashMap = eastl::hash_map<K, V, Urho3D::HashAdapter<K>, eastl::equal_to<K>, eastl::allocator, false>;

} // namespace Urho3D


