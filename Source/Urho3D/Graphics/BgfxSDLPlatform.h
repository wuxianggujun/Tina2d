// 版权同工程 License
//
// SDL3 原生窗口句柄适配：为 bgfx 提供平台相关的 nwh/ndt 等句柄。
// 仅在启用 URHO3D_BGFX 时使用；未启用时不产生额外依赖。

#pragma once

struct SDL_Window; // 前置声明，避免头部污染

namespace Urho3D
{

// 从 SDL3 窗口获取 bgfx 所需的原生窗口句柄。
// 返回值意义：
// - Windows: HWND（void*）
// - macOS: NSWindow*（void*）
// - X11: Window（整数 ID 需 reinterpret_cast<void*>，并进一步通过 bgfx::PlatformData::ndt 传 Display*）
// 备注：此函数仅返回 nwh，若平台需要 ndt/上下文等，请另行在 Initialize 时补充。
void* GetNativeWindowHandleFromSDL(SDL_Window* window);

// 如需要也可扩展获取“显示连接句柄（ndt）”等接口。
void* GetNativeDisplayHandleFromSDL(SDL_Window* window);

} // namespace Urho3D
