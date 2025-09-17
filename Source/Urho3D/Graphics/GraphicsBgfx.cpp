// 版权声明同项目整体许可证
//
// bgfx 渲染后端最小封装实现：
// - 仅在定义 URHO3D_BGFX 时启用真实实现；
// - 未启用时提供空实现，确保不影响现有构建与链路。

#include "Precompiled.h"
#include "GraphicsBgfx.h"
#include "BgfxSDLPlatform.h"

#ifdef URHO3D_BGFX
    #include <bgfx/bgfx.h>
    #include <bgfx/platform.h>
#endif

namespace Urho3D
{

GraphicsBgfx::GraphicsBgfx() = default;

GraphicsBgfx::~GraphicsBgfx()
{
    Shutdown();
}

bool GraphicsBgfx::Initialize(void* nativeWindowHandle, unsigned width, unsigned height, void* nativeDisplayHandle)
{
#ifdef URHO3D_BGFX
    if (initialized_)
        return true;

    width_ = width;
    height_ = height;

    bgfx::Init init{};
    // 让 bgfx 自行选择最优后端（D3D/GL/Metal/Vulkan），满足跨平台目标。
    init.type = bgfx::RendererType::Count;
    init.resolution.width  = width_;
    init.resolution.height = height_;
    init.resolution.reset  = BGFX_RESET_VSYNC;

    // 提供原生窗口句柄（由上层获取，例如 SDL_GetProperty 获取 HWND/NSWindow/X11 Window）。
    init.platformData.nwh = nativeWindowHandle; // Win32: HWND, macOS: NSWindow*, X11: Window (cast)
    init.platformData.ndt = nativeDisplayHandle; // X11: Display*

    if (!bgfx::init(init))
        return false;

    // 设定默认视图（id=0），清屏并设置视口。
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ffu, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(width_), static_cast<uint16_t>(height_));

    initialized_ = true;
    return true;
#else
    (void)nativeWindowHandle; (void)width; (void)height;
    return false; // 未启用 bgfx 时返回失败，供上层判定是否走旧后端。
#endif
}

void GraphicsBgfx::Shutdown()
{
#ifdef URHO3D_BGFX
    if (!initialized_)
        return;
    bgfx::shutdown();
    initialized_ = false;
    width_ = height_ = 0;
#endif
}

void GraphicsBgfx::Reset(unsigned width, unsigned height)
{
#ifdef URHO3D_BGFX
    width_ = width;
    height_ = height;
    if (initialized_)
    {
        bgfx::reset(static_cast<uint16_t>(width_), static_cast<uint16_t>(height_), BGFX_RESET_VSYNC);
        bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(width_), static_cast<uint16_t>(height_));
    }
#else
    (void)width; (void)height;
#endif
}

void GraphicsBgfx::BeginFrame()
{
#ifdef URHO3D_BGFX
    if (!initialized_)
        return;
    // touch 可确保视图 0 在本帧有一次提交，即便没有绘制命令也会清屏并提交。
    bgfx::touch(0);
#endif
}

void GraphicsBgfx::EndFrame()
{
#ifdef URHO3D_BGFX
    if (!initialized_)
        return;
    bgfx::frame();
#endif
}
 
bool GraphicsBgfx::InitializeFromSDL(void* sdlWindow, unsigned width, unsigned height)
{
#ifdef URHO3D_BGFX
    // SDL_Window* 是前置声明，为避免直接包含 SDL 头，这里采用 void* 并在实现中转换。
    auto* window = reinterpret_cast<SDL_Window*>(sdlWindow);
    void* nwh = GetNativeWindowHandleFromSDL(window);
    void* ndt = GetNativeDisplayHandleFromSDL(window);
    return Initialize(nwh, width, height, ndt);
#else
    (void)sdlWindow; (void)width; (void)height;
    return false;
#endif
}

} // namespace Urho3D
