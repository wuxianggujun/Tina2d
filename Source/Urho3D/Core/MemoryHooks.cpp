// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "../Precompiled.h"

#include "MemoryHooks.h"

    #include <mimalloc.h>
    #ifndef MINI_URHO
        #include <SDL3/SDL.h>
    #endif

namespace Urho3D
{

void InstallMimallocAllocator()
{
    // 对于 Windows 动态覆盖，确保链接并触发 mimalloc 初始化：
    // 参见 mimalloc 文档：需在运行时确保对 mimalloc 的显式引用。
    (void)mi_version();

    // 若使用 SDL，则将其内存接口也切换到 mimalloc，避免跨边界错配
    #ifndef MINI_URHO
    #if defined(SDL_MEMORY_FUNCTIONS) || 1
        // SDL3 暴露 SDL_SetMemoryFunctions；SDL2 也有该 API
        auto s_malloc  = +[](size_t n)               -> void* { return mi_malloc(n); };
        auto s_calloc  = +[](size_t a, size_t b)     -> void* { return mi_calloc(a, b); };
        auto s_realloc = +[](void* p, size_t n)      -> void* { return mi_realloc(p, n); };
        auto s_free    = +[](void* p)                      { if (p) mi_free(p); };
        SDL_SetMemoryFunctions(s_malloc, s_calloc, s_realloc, s_free);
    #endif
    #endif
}

}
