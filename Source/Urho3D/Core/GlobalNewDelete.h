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
#   ifdef _DEBUG
#       include <crtdbg.h>
#   endif
#endif

#include <mimalloc.h>

// 确保 mimalloc 在任何全局对象构造之前就被初始化
namespace {
    struct MimallocEarlyInit {
        MimallocEarlyInit() {
            mi_process_init();
            // 启用额外的调试信息来跟踪内存问题
            mi_option_set(mi_option_show_errors, 1);
            // 默认不在进程退出时打印统计，避免噪声；可用环境变量 MIMALLOC_SHOW_STATS=1 打开
            mi_option_set(mi_option_show_stats, 0);

#if defined(_MSC_VER) && defined(_DEBUG)
            // 可选：启用 MSVC CRT 泄漏检查与在退出时自动转储
#   if defined(URHO3D_ENABLE_CRT_LEAK_CHECK)
            int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
            flags |= _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF;
            _CrtSetDbgFlag(flags);
            // 输出到调试器窗口
            _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
            _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
            _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
            // 如需输出到控制台：
            // _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
            // _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
#   endif
#endif
        }
    };
    // 使用 [[maybe_unused]] 避免编译器警告，并确保变量被实例化
    [[maybe_unused]] static MimallocEarlyInit early_init;
}

// 将全局 new/delete 覆盖为 mimalloc（若启用），以确保数组/对齐/有无大小参数的所有路径一致。
// 注意：使用 inline 使其可在多个 TU 中安全定义，且确保在包含本头的 TU 内联生效，避免库链接顺序导致的覆盖失败。

inline void* operator new(std::size_t size)
{
    void* p = mi_malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

inline void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    return mi_malloc(size);
}

inline void* operator new[](std::size_t size)
{
    void* p = mi_malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

inline void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    return mi_malloc(size);
}

inline void operator delete(void* p) noexcept
{
    if (!p) return;
    mi_free(p);
}

inline void operator delete[](void* p) noexcept
{
    if (!p) return;
    mi_free(p);
}

// sized delete
inline void operator delete(void* p, std::size_t) noexcept { operator delete(p); }
inline void operator delete[](void* p, std::size_t) noexcept { operator delete[](p); }

#if defined(__cpp_aligned_new) || (defined(_MSC_VER) && _MSC_VER >= 1912)
inline void* operator new(std::size_t size, std::align_val_t al)
{
    void* p = mi_malloc_aligned(size, (size_t)al);
    if (!p) throw std::bad_alloc();
    return p;
}

inline void* operator new[](std::size_t size, std::align_val_t al)
{
    void* p = mi_malloc_aligned(size, (size_t)al);
    if (!p) throw std::bad_alloc();
    return p;
}

inline void operator delete(void* p, std::align_val_t) noexcept
{
    if (!p) return;
    mi_free(p);
}

inline void operator delete[](void* p, std::align_val_t) noexcept
{
    if (!p) return;
    mi_free(p);
}

inline void operator delete(void* p, std::size_t, std::align_val_t) noexcept { operator delete(p, std::align_val_t(alignof(std::max_align_t))); }
inline void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { operator delete[](p, std::align_val_t(alignof(std::max_align_t))); }
#endif

#if defined(_MSC_VER) && defined(_DEBUG)
// MSVC CRT 调试堆：为 new(_NORMAL_BLOCK, __FILE__, __LINE__) 等提供匹配重载，仍路由到 mimalloc。
// 这样既能保留文件/行号的泄漏定位，又不会与 mimalloc 发生“跨分配器”错配。

// 非对齐（标量/数组）
inline void* operator new(std::size_t size, int /*blockUse*/, const char* /*file*/, int /*line*/)
{
    void* p = mi_malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

inline void* operator new[](std::size_t size, int /*blockUse*/, const char* /*file*/, int /*line*/)
{
    void* p = mi_malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

inline void operator delete(void* p, int /*blockUse*/, const char* /*file*/, int /*line*/) noexcept
{
    if (p) mi_free(p);
}

inline void operator delete[](void* p, int /*blockUse*/, const char* /*file*/, int /*line*/) noexcept
{
    if (p) mi_free(p);
}

#if defined(__cpp_aligned_new) || (defined(_MSC_VER) && _MSC_VER >= 1912)
// 对齐（标量/数组）
inline void* operator new(std::size_t size, std::align_val_t al, int /*blockUse*/, const char* /*file*/, int /*line*/)
{
    void* p = mi_malloc_aligned(size, (size_t)al);
    if (!p) throw std::bad_alloc();
    return p;
}

inline void* operator new[](std::size_t size, std::align_val_t al, int /*blockUse*/, const char* /*file*/, int /*line*/)
{
    void* p = mi_malloc_aligned(size, (size_t)al);
    if (!p) throw std::bad_alloc();
    return p;
}

inline void operator delete(void* p, std::align_val_t /*al*/, int /*blockUse*/, const char* /*file*/, int /*line*/) noexcept
{
    if (p) mi_free(p);
}

inline void operator delete[](void* p, std::align_val_t /*al*/, int /*blockUse*/, const char* /*file*/, int /*line*/) noexcept
{
    if (p) mi_free(p);
}
#endif // aligned debug new/delete
#endif // MSVC _DEBUG

#ifdef _MSC_VER
#pragma warning(pop)
#endif
