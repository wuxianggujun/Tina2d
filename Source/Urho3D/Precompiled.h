// 该文件不再作为“预编译头（PCH）”使用，仅作为普通聚合头存在，
// 以保持各源码文件中对 "Precompiled.h" 的包含语句不需要修改。
// 如需最小化依赖，可按需精简下面的包含列表。

#ifdef __cplusplus

#ifndef URHO3D_PCH_INCLUDED
#define URHO3D_PCH_INCLUDED

#include "Container/HashMap.h"
#include "Container/HashSet.h"
#include "Container/Pair.h"
#include "Container/Sort.h"
#include "Container/Str.h"

// 在 PCH 中开启“内联全局 new/delete”开关，避免链接顺序导致的覆盖失败
#ifndef URHO3D_INLINE_GLOBAL_NEWDELETE
#define URHO3D_INLINE_GLOBAL_NEWDELETE 1
#endif

// 统一全局 new/delete 到 mimalloc（若启用），避免库链接顺序导致的覆盖失败
#include "Core/GlobalNewDelete.h"

// SDL 相关：各翻译单元按需直接包含 <SDL3/SDL.h>

#endif

#endif
