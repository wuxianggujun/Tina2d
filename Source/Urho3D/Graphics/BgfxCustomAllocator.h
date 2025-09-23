// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#ifdef URHO3D_BGFX
#include <bx/allocator.h>

#ifdef URHO3D_HAS_MIMALLOC
#include <mimalloc.h>
#endif

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
#ifdef URHO3D_HAS_MIMALLOC
                    mi_free(_ptr);
#else
                    free(_ptr);
#endif
                }
                return nullptr;
            }
            
            if (!_ptr)
            {
#ifdef URHO3D_HAS_MIMALLOC
                return _align <= 8 ? mi_malloc(_size) : mi_malloc_aligned(_size, _align);
#else
                return _align <= 8 ? malloc(_size) : nullptr; // 简化处理
#endif
            }
            
#ifdef URHO3D_HAS_MIMALLOC
            return _align <= 8 ? mi_realloc(_ptr, _size) : nullptr; // mimalloc没有对齐realloc
#else
            return realloc(_ptr, _size);
#endif
        }
    };
}

#endif // URHO3D_BGFX