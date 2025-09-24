// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

namespace Urho3D
{
    // 安装高性能分配器（若可用）。
    // - 当启用 mimalloc 并成功链接时，将触发对 mimalloc 的引用以确保覆盖生效；
    // - 否则为空操作，回退到 CRT。
    void InstallMimallocAllocator();
}

