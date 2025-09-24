// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

// 直接使用 EASTL::hash_set，无兼容层
#include <EASTL/hash_set.h>
#include <EASTL/functional.h>
#include "../Container/Hash.h"

namespace Urho3D
{

// 直接使用 EASTL 容器，无包装层
template <class T>
using HashSet = eastl::hash_set<T, Urho3D::HashAdapter<T>, eastl::equal_to<T>, eastl::allocator, false>;

} // namespace Urho3D
