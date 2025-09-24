// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include "../Core/Context.h"
#include <EASTL/allocator.h>
#include <mimalloc.h>

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
            return mi_malloc(n);
        }
        
        void* allocate(size_t n, size_t alignment, size_t /*offset*/, int /*flags*/ = 0)
        {
            return mi_malloc_aligned(n, alignment);
        }
        
        void deallocate(void* p, size_t /*n*/)
        {
            if (!p) return;
            mi_free(p);
        }
        
        const char* get_name() const { return name_; }
        void set_name(const char* pName) { name_ = pName; }
        
    private:
        const char* name_;
    };
}

// 先暂时禁用自定义EASTL分配器，避免符号冲突
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

// 不再提供额外包装函数，直接在需要处调用 eastl::GetDefaultAllocator()
