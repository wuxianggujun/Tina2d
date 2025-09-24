// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include <bx/allocator.h>
#include <cstdlib>
#include <mimalloc.h>

namespace Urho3D 
{
    /// 自定义bgfx分配器，与Urho3D内存系统统一
    class BgfxCustomAllocator : public bx::AllocatorI
    {
    public:
        BgfxCustomAllocator() = default;
        ~BgfxCustomAllocator() override = default;

        void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) override
        {
            (void)_file; (void)_line; // 忽略调试信息
            
            if (_size == 0)
            {
                if (_ptr)
                {
                    mi_free(_ptr);
                }
                return nullptr;
            }
            
            if (!_ptr)
            {
                return _align <= 8 ? mi_malloc(_size) : mi_malloc_aligned(_size, _align);
            }
            
            // 重新分配：按对齐选择 API
            return _align <= 8 ? mi_realloc(_ptr, _size) : mi_realloc_aligned(_ptr, _size, _align);
        }
    };
}
