// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include <bx/allocator.h>

namespace Urho3D
{

// 返回一个全局可用的 bgfx/bx 分配器：
// - 开启 mimalloc 时，使用 mimalloc 作为底层（mi_malloc/mi_realloc/mi_free，含对齐）；
// - 否则回退到 bx::DefaultAllocator。
bx::AllocatorI* GetBgfxAllocator();

} // namespace Urho3D

