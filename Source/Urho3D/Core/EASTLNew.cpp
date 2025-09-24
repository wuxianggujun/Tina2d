// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "../Precompiled.h"

// 注意：全局 new/delete 重载现在统一在 GlobalNewDelete.h 中以内联形式提供
// 本文件仅保留 EASTL 专用的调试版本和对齐版本重载，避免符号冲突

#include <cstddef>
#include <new>
#include <cstdlib>

#ifdef _MSC_VER
    #include <malloc.h>
#endif

#ifdef URHO3D_HAS_MIMALLOC
    #include <mimalloc.h>
#endif

// EASTL 自定义 new/delete 钩子实现。
// 目的：当 EASTL 以带调试信息的"placement new 风格"调用这些符号时，
// 将其路由到 mimalloc（若启用）或回退到标准分配器。

namespace
{
    inline void* AllocRaw(size_t size)
    {
#ifdef URHO3D_HAS_MIMALLOC
        return mi_malloc(size);
#else
        return std::malloc(size);
#endif
    }

    inline void  FreeRaw(void* p) noexcept
    {
#ifdef URHO3D_HAS_MIMALLOC
        mi_free(p);
#else
        std::free(p);
#endif
    }

    inline void* AllocAligned(size_t size, size_t alignment)
    {
#ifdef URHO3D_HAS_MIMALLOC
        return mi_malloc_aligned(size, alignment);
#else
    #ifdef _MSC_VER
        return _aligned_malloc(size, alignment);
    #else
        // C11 aligned_alloc 要求 size 为 alignment 的倍数
        #if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
            size_t padded = (size + alignment - 1) / alignment * alignment;
            return aligned_alloc(alignment, padded);
        #else
            void* p = nullptr;
            if (posix_memalign(&p, alignment, size) != 0)
                return nullptr;
            return p;
        #endif
    #endif
#endif
    }

    inline void  FreeAligned(void* p) noexcept
    {
#ifdef URHO3D_HAS_MIMALLOC
        mi_free(p);
#else
    #ifdef _MSC_VER
        _aligned_free(p);
    #else
        std::free(p);
    #endif
#endif
    }
}

// 移除全局 new/delete 重载，避免与 GlobalNewDelete.h 中的内联版本冲突
// 全局 new/delete 现在统一由 GlobalNewDelete.h 提供

// 仅保留 EASTL 专用的调试和对齐版本

// 全局 new/delete 覆盖，使 EASTL 与引擎统一走 mimalloc（若启用）
#if !URHO3D_INLINE_GLOBAL_NEWDELETE
// 若未使用头文件内联的全局 new/delete，则应在此提供对应定义。
// 当前工程默认启用内联版本，这里无需重复定义。
#endif

// 非对齐版本（new / delete）
void* operator new[](size_t size,
                     const char* /*pName*/, int /*flags*/, unsigned /*debugFlags*/,
                     const char* /*file*/, int /*line*/)
{
    return AllocRaw(size);
}

void* operator new(size_t size,
                   const char* /*pName*/, int /*flags*/, unsigned /*debugFlags*/,
                   const char* /*file*/, int /*line*/)
{
    return AllocRaw(size);
}

void operator delete[](void* p,
                       const char* /*pName*/, int /*flags*/, unsigned /*debugFlags*/,
                       const char* /*file*/, int /*line*/) noexcept
{
    FreeRaw(p);
}

void operator delete(void* p,
                     const char* /*pName*/, int /*flags*/, unsigned /*debugFlags*/,
                     const char* /*file*/, int /*line*/) noexcept
{
    FreeRaw(p);
}

// 对齐版本（new / delete），EASTL 在需要对齐时会调该组符号
void* operator new[](size_t size, size_t alignment, size_t /*alignmentOffset*/,
                     const char* /*pName*/, int /*flags*/, unsigned /*debugFlags*/,
                     const char* /*file*/, int /*line*/)
{
    return AllocAligned(size, alignment);
}

void* operator new(size_t size, size_t alignment, size_t /*alignmentOffset*/,
                   const char* /*pName*/, int /*flags*/, unsigned /*debugFlags*/,
                   const char* /*file*/, int /*line*/)
{
    return AllocAligned(size, alignment);
}

void operator delete[](void* p, size_t /*alignment*/, size_t /*alignmentOffset*/,
                       const char* /*pName*/, int /*flags*/, unsigned /*debugFlags*/,
                       const char* /*file*/, int /*line*/) noexcept
{
    FreeAligned(p);
}

void operator delete(void* p, size_t /*alignment*/, size_t /*alignmentOffset*/,
                     const char* /*pName*/, int /*flags*/, unsigned /*debugFlags*/,
                     const char* /*file*/, int /*line*/) noexcept
{
    FreeAligned(p);
}
