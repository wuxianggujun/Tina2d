// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include "../Core/Context.h"
#ifdef URHO3D_HAS_MIMALLOC
#include <mimalloc.h>
#endif

// 自定义EASTL分配器，确保与项目内存管理统一
// 
// 使用方式：
// 1. 定义 EASTL_USER_DEFINED_ALLOCATOR 宏
// 2. 提供自定义 allocator 实现
// 3. 所有EASTL容器将使用我们的分配器

namespace Urho3D
{
    /// EASTL分配器实现，与引擎内存系统统一
    class EASTLAllocator
    {
    public:
        EASTLAllocator(const char* pName = "EASTLAllocator") : name_(pName) {}
        
        void* allocate(size_t n, int /*flags*/ = 0)
        {
#ifdef URHO3D_HAS_MIMALLOC
            return mi_malloc(n);
#else
            return std::malloc(n);
#endif
        }
        
        void* allocate(size_t n, size_t alignment, size_t /*offset*/, int /*flags*/ = 0)
        {
#ifdef URHO3D_HAS_MIMALLOC
            return mi_malloc_aligned(n, alignment);
#else
#ifdef _MSC_VER
            return _aligned_malloc(n, alignment);
#else
            void* p = nullptr;
            if (posix_memalign(&p, alignment, n) != 0)
                return nullptr;
            return p;
#endif
#endif
        }
        
        void deallocate(void* p, size_t /*n*/)
        {
            if (!p) return;
#ifdef URHO3D_HAS_MIMALLOC
            mi_free(p);
#else
            std::free(p);
#endif
        }
        
        const char* get_name() const { return name_; }
        void set_name(const char* pName) { name_ = pName; }
        
    private:
        const char* name_;
    };
}

// 如果要启用自定义EASTL分配器，取消下面的注释
// #define EASTL_USER_DEFINED_ALLOCATOR

#ifdef EASTL_USER_DEFINED_ALLOCATOR
// 替换EASTL默认分配器
namespace eastl
{
    using allocator = Urho3D::EASTLAllocator;
    
    extern allocator gDefaultAllocator;
    extern allocator* gpDefaultAllocator;
    
    inline allocator* GetDefaultAllocator() { return gpDefaultAllocator; }
    inline allocator* SetDefaultAllocator(allocator* pAllocator) 
    { 
        allocator* pPrev = gpDefaultAllocator;
        gpDefaultAllocator = pAllocator;
        return pPrev;
    }
}
#endif