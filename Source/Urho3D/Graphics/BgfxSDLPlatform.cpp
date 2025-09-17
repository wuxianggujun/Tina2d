// 版权同工程 License

#include "Precompiled.h"
#include "BgfxSDLPlatform.h"

#ifdef URHO3D_BGFX
#   include <SDL3/SDL.h>
#endif

namespace Urho3D
{

void* GetNativeWindowHandleFromSDL(SDL_Window* window)
{
#ifdef URHO3D_BGFX
    if (!window)
        return nullptr;
#   if defined(_WIN32)
    // SDL3: 通过属性系统获取 Win32 HWND
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#   elif defined(__APPLE__)
    // macOS: 获取 NSWindow*
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#   elif defined(SDL_PLATFORM_LINUX)
    // X11: 返回 X11 Window（整型 ID 需要 reinterpret_cast），ndt 需另行提供 Display*
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    Uint64 x11win = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(x11win));
#   else
    // 其他平台按需扩展：Android/iOS/Wayland 等
    return nullptr;
#   endif
#else
    (void)window;
    return nullptr;
#endif
}

void* GetNativeDisplayHandleFromSDL(SDL_Window* window)
{
#ifdef URHO3D_BGFX
    if (!window)
        return nullptr;
#   if defined(SDL_PLATFORM_LINUX)
    // X11: 取 Display*（bgfx::PlatformData::ndt）
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
#   else
    return nullptr;
#   endif
#else
    (void)window;
    return nullptr;
#endif
}

} // namespace Urho3D

