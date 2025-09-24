// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include <cstddef>
#include <new>
#include <cstdlib>

#ifdef _MSC_VER
#pragma warning(push)
// 非成员 operator new/delete 内联定义：我们刻意在头内联覆盖，MSVC 发出 4595 警告，这里屏蔽之
#pragma warning(disable:4595)
#endif

#ifdef _MSC_VER
#   include <malloc.h>
#endif

#ifdef URHO3D_HAS_MIMALLOC
#   include <mimalloc.h>
#endif

// 将全局 new/delete 覆盖为 mimalloc（若启用），以确保数组/对齐/有无大小参数的所有路径一致。
// 注意：使用 inline 使其可在多个 TU 中安全定义，且确保在包含本头的 TU 内联生效，避免库链接顺序导致的覆盖失败。

inline void* operator new(std::size_t size)
{
#ifdef URHO3D_HAS_MIMALLOC
    return mi_malloc(size);
#else
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
#endif
}

inline void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
#ifdef URHO3D_HAS_MIMALLOC
    return mi_malloc(size);
#else
    return std::malloc(size);
#endif
}

inline void* operator new[](std::size_t size)
{
#ifdef URHO3D_HAS_MIMALLOC
    return mi_malloc(size);
#else
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
#endif
}

inline void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
#ifdef URHO3D_HAS_MIMALLOC
    return mi_malloc(size);
#else
    return std::malloc(size);
#endif
}

inline void operator delete(void* p) noexcept
{
    if (!p) return;
#ifdef URHO3D_HAS_MIMALLOC
    if (mi_is_in_heap_region(p)) { mi_free(p); return; }
#endif
    std::free(p);
}

inline void operator delete[](void* p) noexcept
{
    if (!p) return;
#ifdef URHO3D_HAS_MIMALLOC
    if (mi_is_in_heap_region(p)) { mi_free(p); return; }
#endif
    std::free(p);
}

// sized delete
inline void operator delete(void* p, std::size_t) noexcept { operator delete(p); }
inline void operator delete[](void* p, std::size_t) noexcept { operator delete[](p); }

#if defined(__cpp_aligned_new) || (defined(_MSC_VER) && _MSC_VER >= 1912)
inline void* operator new(std::size_t size, std::align_val_t al)
{
#ifdef URHO3D_HAS_MIMALLOC
    return mi_malloc_aligned(size, (size_t)al);
#else
#   ifdef _MSC_VER
    return _aligned_malloc(size, (size_t)al);
#   else
    size_t alignment = (size_t)al;
    void* p = nullptr;
    if (posix_memalign(&p, alignment, size) != 0) p = nullptr;
    if (!p) throw std::bad_alloc();
    return p;
#   endif
#endif
}

inline void* operator new[](std::size_t size, std::align_val_t al)
{
#ifdef URHO3D_HAS_MIMALLOC
    return mi_malloc_aligned(size, (size_t)al);
#else
#   ifdef _MSC_VER
    return _aligned_malloc(size, (size_t)al);
#   else
    size_t alignment = (size_t)al;
    void* p = nullptr;
    if (posix_memalign(&p, alignment, size) != 0) p = nullptr;
    if (!p) throw std::bad_alloc();
    return p;
#   endif
#endif
}

inline void operator delete(void* p, std::align_val_t) noexcept
{
    if (!p) return;
#ifdef URHO3D_HAS_MIMALLOC
    if (mi_is_in_heap_region(p)) { mi_free(p); return; }
#endif
#ifdef _MSC_VER
    _aligned_free(p);
#else
    std::free(p);
#endif
}

inline void operator delete[](void* p, std::align_val_t) noexcept
{
    if (!p) return;
#ifdef URHO3D_HAS_MIMALLOC
    if (mi_is_in_heap_region(p)) { mi_free(p); return; }
#endif
#ifdef _MSC_VER
    _aligned_free(p);
#else
    std::free(p);
#endif
}

inline void operator delete(void* p, std::size_t, std::align_val_t) noexcept { operator delete(p, std::align_val_t(alignof(std::max_align_t))); }
inline void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { operator delete[](p, std::align_val_t(alignof(std::max_align_t))); }
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif
