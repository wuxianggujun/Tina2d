// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "../Precompiled.h"

#include "BgfxMiAllocator.h"

#ifdef URHO3D_HAS_MIMALLOC
#   include <mimalloc.h>
#endif

namespace Urho3D
{

#ifdef URHO3D_HAS_MIMALLOC
class BgfxMiAllocator final : public bx::AllocatorI
{
public:
    ~BgfxMiAllocator() override = default;

    void* realloc(void* ptr, size_t size, size_t align, const char* /*file*/, uint32_t /*line*/) override
    {
        // 分配 / 释放 / 重新分配 三态接口
        if (ptr == nullptr)
        {
            if (size == 0)
                return nullptr;
            if (align > 0)
                return mi_malloc_aligned(size, align);
            return mi_malloc(size);
        }
        else
        {
            if (size == 0)
            {
                mi_free(ptr);
                return nullptr;
            }
            if (align > 0)
                return mi_realloc_aligned(ptr, size, align);
            return mi_realloc(ptr, size);
        }
    }
};
#endif

bx::AllocatorI* GetBgfxAllocator()
{
#ifdef URHO3D_HAS_MIMALLOC
    static BgfxMiAllocator gMiAllocator;
    return &gMiAllocator;
#else
    static bx::DefaultAllocator gDefault;
    return &gDefault;
#endif
}

} // namespace Urho3D

