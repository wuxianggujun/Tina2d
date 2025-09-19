// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "../Precompiled.h"

#include "../Core/Profiler.h"
#include "../Graphics/Camera.h"
#include "../Graphics/DebugRenderer.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/GraphicsEvents.h"
#include "../Graphics/Material.h"
#include "../Graphics/Octree.h"
#include "../Graphics/Technique.h"
#include "../Graphics/Zone.h"
#include "../GraphicsAPI/GraphicsImpl.h"
#include "../GraphicsAPI/Shader.h"
#include "../GraphicsAPI/ShaderPrecache.h"
#include "../GraphicsAPI/Texture2D.h"
#include "../GraphicsAPI/Texture2DArray.h"
#include "../GraphicsAPI/Texture3D.h"
#include "../GraphicsAPI/TextureCube.h"
#include "../GraphicsAPI/RenderSurface.h"
#ifdef URHO3D_BGFX
#include "../Graphics/GraphicsBgfx.h"
#endif
#include "../IO/FileSystem.h"
#include "../Resource/ResourceCache.h"
#include "../IO/Log.h"

#include <SDL3/SDL.h>

#include "../DebugNew.h"

namespace Urho3D
{

void Graphics::SetExternalWindow(void* window)
{
    if (!window_)
        externalWindow_ = window;
    else
        URHO3D_LOGERROR("Window already opened, can not set external window");
}

void Graphics::SetWindowTitle(const String& windowTitle)
{
    windowTitle_ = windowTitle;
    if (window_)
        SDL_SetWindowTitle(window_, windowTitle_.CString());
}

void Graphics::SetWindowIcon(Image* windowIcon)
{
    windowIcon_ = windowIcon;
    if (window_)
        CreateWindowIcon();
}

void Graphics::SetWindowPosition(const IntVector2& position)
{
    if (window_)
        SDL_SetWindowPosition(window_, position.x_, position.y_);
    else
        position_ = position; // Sets as initial position for SDL_CreateWindow()
}

void Graphics::SetWindowPosition(int x, int y)
{
    SetWindowPosition(IntVector2(x, y));
}

void Graphics::SetOrientations(const String& orientations)
{
    orientations_ = orientations.Trimmed();
    SDL_SetHint(SDL_HINT_ORIENTATIONS, orientations_.CString());
}

bool Graphics::SetScreenMode(int width, int height)
{
    return SetScreenMode(width, height, screenParams_);
}

bool Graphics::SetWindowModes(const WindowModeParams& windowMode, const WindowModeParams& secondaryWindowMode, bool maximize)
{
    primaryWindowMode_ = windowMode;
    secondaryWindowMode_ = secondaryWindowMode;
    return SetScreenMode(primaryWindowMode_.width_, primaryWindowMode_.height_, primaryWindowMode_.screenParams_, maximize);
}

bool Graphics::SetDefaultWindowModes(int width, int height, const ScreenModeParams& params)
{
    // Fill window mode to be applied now
    WindowModeParams primaryWindowMode;
    primaryWindowMode.width_ = width;
    primaryWindowMode.height_ = height;
    primaryWindowMode.screenParams_ = params;

    // Fill window mode to be applied on Graphics::ToggleFullscreen
    WindowModeParams secondaryWindowMode = primaryWindowMode;

    // Pick resolution automatically
    secondaryWindowMode.width_ = 0;
    secondaryWindowMode.height_ = 0;

    if (params.fullscreen_ || params.borderless_)
    {
        secondaryWindowMode.screenParams_.fullscreen_ = false;
        secondaryWindowMode.screenParams_.borderless_ = false;
    }
    else
    {
        secondaryWindowMode.screenParams_.borderless_ = true;
    }

    const bool maximize = (!width || !height) && !params.fullscreen_ && !params.borderless_ && params.resizable_;
    return SetWindowModes(primaryWindowMode, secondaryWindowMode, maximize);
}

bool Graphics::SetMode(int width, int height, bool fullscreen, bool borderless, bool resizable,
    bool highDPI, bool vsync, bool tripleBuffer, int multiSample, int monitor, int refreshRate)
{
    ScreenModeParams params;
    params.fullscreen_ = fullscreen;
    params.borderless_ = borderless;
    params.resizable_ = resizable;
    params.highDPI_ = highDPI;
    params.vsync_ = vsync;
    params.tripleBuffer_ = tripleBuffer;
    params.multiSample_ = multiSample;
    params.monitor_ = monitor;
    params.refreshRate_ = refreshRate;

    return SetDefaultWindowModes(width, height, params);
}

bool Graphics::SetMode(int width, int height)
{
    return SetDefaultWindowModes(width, height, screenParams_);
}

bool Graphics::ToggleFullscreen()
{
    Swap(primaryWindowMode_, secondaryWindowMode_);
    return SetScreenMode(primaryWindowMode_.width_, primaryWindowMode_.height_, primaryWindowMode_.screenParams_);
}

void Graphics::SetShaderParameter(StringHash param, const Variant& value)
{
    switch (value.GetType())
    {
    case VAR_BOOL:
        SetShaderParameter(param, value.GetBool());
        break;

    case VAR_INT:
        SetShaderParameter(param, value.GetI32());
        break;

    case VAR_FLOAT:
    case VAR_DOUBLE:
        SetShaderParameter(param, value.GetFloat());
        break;

    case VAR_VECTOR2:
        SetShaderParameter(param, value.GetVector2());
        break;

    case VAR_VECTOR3:
        SetShaderParameter(param, value.GetVector3());
        break;

    case VAR_VECTOR4:
        SetShaderParameter(param, value.GetVector4());
        break;

    case VAR_COLOR:
        SetShaderParameter(param, value.GetColor());
        break;

    case VAR_MATRIX3:
        SetShaderParameter(param, value.GetMatrix3());
        break;

    case VAR_MATRIX3X4:
        SetShaderParameter(param, value.GetMatrix3x4());
        break;

    case VAR_MATRIX4:
        SetShaderParameter(param, value.GetMatrix4());
        break;

    case VAR_BUFFER:
        {
            const Vector<byte>& buffer = value.GetBuffer();
            if (buffer.Size() >= sizeof(float))
                SetShaderParameter(param, reinterpret_cast<const float*>(&buffer[0]), buffer.Size() / sizeof(float));
        }
        break;

    default:
        // Unsupported parameter type, do nothing
        break;
    }
}

IntVector2 Graphics::GetWindowPosition() const
{
    if (window_)
    {
        IntVector2 position;
        SDL_GetWindowPosition(window_, &position.x_, &position.y_);
        return position;
    }
    return position_;
}

Vector<IntVector3> Graphics::GetResolutions(int monitor) const
{
    Vector<IntVector3> ret;

    // Emscripten is not able to return a valid list
#ifndef __EMSCRIPTEN__
    int displayCount = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&displayCount);
    SDL_DisplayID displayID = 0;
    if (displays && monitor >= 0 && monitor < displayCount)
        displayID = displays[monitor];
    else
        displayID = SDL_GetPrimaryDisplay();
    if (displays)
        SDL_free(displays);

    int modeCount = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(displayID, &modeCount);
    if (modes)
    {
        for (int i = 0; modes[i] != nullptr; ++i)
        {
            const SDL_DisplayMode* mode = modes[i];
            const int width = mode->w;
            const int height = mode->h;
            const int rate = (int)mode->refresh_rate;

            bool unique = true;
            for (const IntVector3& resolution : ret)
            {
                if (resolution.x_ == width && resolution.y_ == height && resolution.z_ == rate)
                {
                    unique = false;
                    break;
                }
            }
            if (unique)
                ret.Push(IntVector3(width, height, rate));
        }
        SDL_free(modes);
    }
#endif
    return ret;
}

i32 Graphics::FindBestResolutionIndex(int monitor, int width, int height, int refreshRate) const
{
    const Vector<IntVector3> resolutions = GetResolutions(monitor);
    if (resolutions.Empty())
        return NINDEX;

    i32 best = 0;
    i32 bestError = M_MAX_INT;

    for (i32 i = 0; i < resolutions.Size(); ++i)
    {
        i32 error = Abs(resolutions[i].x_ - width) + Abs(resolutions[i].y_ - height);
        if (refreshRate != 0)
            error += Abs(resolutions[i].z_ - refreshRate);
        if (error < bestError)
        {
            best = i;
            bestError = error;
        }
    }

    return best;
}

IntVector2 Graphics::GetDesktopResolution(int monitor) const
{
#if !defined(__ANDROID__) && !defined(IOS) && !defined(TVOS)
    int displayCount = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&displayCount);
    SDL_DisplayID displayID = 0;
    if (displays && monitor >= 0 && monitor < displayCount)
        displayID = displays[monitor];
    else
        displayID = SDL_GetPrimaryDisplay();
    if (displays)
        SDL_free(displays);

    const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(displayID);
    if (mode)
        return IntVector2(mode->w, mode->h);
    else
        return IntVector2(width_, height_);
#else
    // SDL_GetDesktopDisplayMode() may not work correctly on mobile platforms. Rather return the window size
    return IntVector2(width_, height_);
#endif
}

int Graphics::GetMonitorCount() const
{
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    if (displays)
        SDL_free(displays);
    return count;
}

int Graphics::GetCurrentMonitor() const
{
    if (!window_)
        return 0;
    SDL_DisplayID id = SDL_GetDisplayForWindow(window_);
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    int index = 0;
    if (displays)
    {
        for (int i = 0; i < count; ++i)
        {
            if (displays[i] == id)
            {
                index = i;
                break;
            }
        }
        SDL_free(displays);
    }
    return index;
}

bool Graphics::GetMaximized() const
{
    return window_? static_cast<bool>(SDL_GetWindowFlags(window_) & SDL_WINDOW_MAXIMIZED) : false;
}

Vector3 Graphics::GetDisplayDPI(int monitor) const
{
    // SDL3 不再提供 DPI 接口，使用内容缩放比（相对 96 DPI）近似
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    SDL_DisplayID id = 0;
    if (displays && monitor >= 0 && monitor < count)
        id = displays[monitor];
    else
        id = SDL_GetPrimaryDisplay();
    if (displays)
        SDL_free(displays);

    const float scale = SDL_GetDisplayContentScale(id);
    const float dpi = scale * 96.0f; // 以 96 DPI 为基准估算
    return Vector3(dpi, dpi, dpi);
}

void Graphics::Maximize()
{
    if (!window_)
        return;

    SDL_MaximizeWindow(window_);
}

void Graphics::Minimize()
{
    if (!window_)
        return;

    SDL_MinimizeWindow(window_);
}

void Graphics::Raise() const
{
    if (!window_)
        return;

    SDL_RaiseWindow(window_);
}

void Graphics::BeginDumpShaders(const String& fileName)
{
    shaderPrecache_ = new ShaderPrecache(context_, fileName);
}

void Graphics::EndDumpShaders()
{
    shaderPrecache_.Reset();
}

void Graphics::PrecacheShaders(Deserializer& source)
{
    URHO3D_PROFILE(PrecacheShaders);

    ShaderPrecache::LoadShaders(this, source);
}

void Graphics::SetShaderCacheDir(const String& path)
{
    String trimmedPath = path.Trimmed();
    if (trimmedPath.Length())
        shaderCacheDir_ = AddTrailingSlash(trimmedPath);
}

void Graphics::AddGPUObject(GPUObject* object)
{
    MutexLock lock(gpuObjectMutex_);

    gpuObjects_.Push(object);
}

void Graphics::RemoveGPUObject(GPUObject* object)
{
    MutexLock lock(gpuObjectMutex_);

    gpuObjects_.Remove(object);
}

void* Graphics::ReserveScratchBuffer(i32 size)
{
    assert(size >= 0);

    if (!size)
        return nullptr;

    if (size > maxScratchBufferRequest_)
        maxScratchBufferRequest_ = size;

    // First check for a free buffer that is large enough
    for (ScratchBuffer& scratchBuffer : scratchBuffers_)
    {
        if (!scratchBuffer.reserved_ && scratchBuffer.size_ >= size)
        {
            scratchBuffer.reserved_ = true;
            return scratchBuffer.data_.Get();
        }
    }

    // Then check if a free buffer can be resized
    for (ScratchBuffer& scratchBuffer : scratchBuffers_)
    {
        if (!scratchBuffer.reserved_)
        {
            scratchBuffer.data_ = new u8[size];
            scratchBuffer.size_ = size;
            scratchBuffer.reserved_ = true;

            URHO3D_LOGDEBUG("Resized scratch buffer to size " + String(size));

            return scratchBuffer.data_.Get();
        }
    }

    // Finally allocate a new buffer
    ScratchBuffer newBuffer;
    newBuffer.data_ = new u8[size];
    newBuffer.size_ = size;
    newBuffer.reserved_ = true;
    scratchBuffers_.Push(newBuffer);

    URHO3D_LOGDEBUG("Allocated scratch buffer with size " + String(size));

    return newBuffer.data_.Get();
}

void Graphics::FreeScratchBuffer(void* buffer)
{
    if (!buffer)
        return;

    for (ScratchBuffer& scratchBuffer : scratchBuffers_)
    {
        if (scratchBuffer.reserved_ && scratchBuffer.data_.Get() == buffer)
        {
            scratchBuffer.reserved_ = false;
            return;
        }
    }

    URHO3D_LOGWARNING("Reserved scratch buffer " + ToStringHex((unsigned)(size_t)buffer) + " not found");
}

void Graphics::CleanupScratchBuffers()
{
    for (ScratchBuffer& scratchBuffer : scratchBuffers_)
    {
        if (!scratchBuffer.reserved_ && scratchBuffer.size_ > maxScratchBufferRequest_ * 2 && scratchBuffer.size_ >= 1024 * 1024)
        {
            scratchBuffer.data_ = maxScratchBufferRequest_ > 0 ? (new u8[maxScratchBufferRequest_]) : nullptr;
            scratchBuffer.size_ = maxScratchBufferRequest_;

            URHO3D_LOGDEBUG("Resized scratch buffer to size " + String(maxScratchBufferRequest_));
        }
    }

    maxScratchBufferRequest_ = 0;
}

void Graphics::CreateWindowIcon()
{
    if (windowIcon_)
    {
        SDL_Surface* surface = windowIcon_->GetSDLSurface();
        if (surface)
        {
            SDL_SetWindowIcon(window_, surface);
            SDL_DestroySurface(surface);
        }
    }
}

void Graphics::AdjustScreenMode(int& newWidth, int& newHeight, ScreenModeParams& params, bool& maximize) const
{
    // High DPI is supported only for OpenGL backend
    if (Graphics::GetGAPI() != GAPI_OPENGL)
        params.highDPI_ = false;

#if defined(IOS) || defined(TVOS)
    // iOS and tvOS app always take the fullscreen (and with status bar hidden)
    params.fullscreen_ = true;
#endif

    // Make sure monitor index is not bigger than the currently detected monitors
    int __numDisplays = 0;
    SDL_DisplayID* __displays = SDL_GetDisplays(&__numDisplays);
    if (params.monitor_ >= __numDisplays || params.monitor_ < 0)
        params.monitor_ = 0; // this monitor is not present, use first monitor

    // Fullscreen or Borderless can not be resizable and cannot be maximized
    if (params.fullscreen_ || params.borderless_)
    {
        params.resizable_ = false;
        maximize = false;
    }

    // Borderless cannot be fullscreen, they are mutually exclusive
    if (params.borderless_)
        params.fullscreen_ = false;

    // On iOS window needs to be resizable to handle orientation changes properly
#ifdef IOS
    if (!externalWindow_)
        params.resizable_ = true;
#endif

    // Ensure that multisample factor is in valid range
    params.multiSample_ = NextPowerOfTwo(Clamp(params.multiSample_, 1, 16));

    // If zero dimensions in windowed mode, set windowed mode to maximize and set a predefined default restored window size.
    // If zero in fullscreen, use desktop mode
    if (!newWidth || !newHeight)
    {
        if (params.fullscreen_ || params.borderless_)
        {
            SDL_DisplayID __id = 0;
            if (__displays && params.monitor_ >= 0 && params.monitor_ < __numDisplays)
                __id = __displays[params.monitor_];
            else
                __id = SDL_GetPrimaryDisplay();
            const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(__id);
            if (mode)
            {
                newWidth = mode->w;
                newHeight = mode->h;
            }
        }
        else
        {
            newWidth = 1024;
            newHeight = 768;
        }
    }

    // Check fullscreen mode validity (desktop only). Use a closest match if not found
#ifdef DESKTOP_GRAPHICS
    if (params.fullscreen_)
    {
        const Vector<IntVector3> resolutions = GetResolutions(params.monitor_);
        if (!resolutions.Empty())
        {
            const i32 bestResolution = FindBestResolutionIndex(params.monitor_,
                newWidth, newHeight, params.refreshRate_);
            newWidth = resolutions[bestResolution].x_;
            newHeight = resolutions[bestResolution].y_;
            params.refreshRate_ = resolutions[bestResolution].z_;
        }
    }
    else
    {
        // If windowed, use the same refresh rate as desktop
        SDL_DisplayID __id = 0;
        if (__displays && params.monitor_ >= 0 && params.monitor_ < __numDisplays)
            __id = __displays[params.monitor_];
        else
            __id = SDL_GetPrimaryDisplay();
        const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(__id);
        if (mode)
            params.refreshRate_ = (int)mode->refresh_rate;
    }
#endif

    if (__displays)
        SDL_free(__displays);
}

void Graphics::OnScreenModeChanged()
{
#ifdef URHO3D_LOGGING
    String msg;
    msg.AppendWithFormat("Set screen mode %dx%d rate %d Hz %s monitor %d", width_, height_, screenParams_.refreshRate_,
        (screenParams_.fullscreen_ ? "fullscreen" : "windowed"), screenParams_.monitor_);
    if (screenParams_.borderless_)
        msg.Append(" borderless");
    if (screenParams_.resizable_)
        msg.Append(" resizable");
    if (screenParams_.highDPI_)
        msg.Append(" highDPI");
    if (screenParams_.multiSample_ > 1)
        msg.AppendWithFormat(" multisample %d", screenParams_.multiSample_);
    URHO3D_LOGINFO(msg);
#endif

    using namespace ScreenMode;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_WIDTH] = width_;
    eventData[P_HEIGHT] = height_;
    eventData[P_FULLSCREEN] = screenParams_.fullscreen_;
    eventData[P_BORDERLESS] = screenParams_.borderless_;
    eventData[P_RESIZABLE] = screenParams_.resizable_;
    eventData[P_HIGHDPI] = screenParams_.highDPI_;
    eventData[P_MONITOR] = screenParams_.monitor_;
    eventData[P_REFRESHRATE] = screenParams_.refreshRate_;
    SendEvent(E_SCREENMODE, eventData);

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
        bgfx_->Reset((unsigned)width_, (unsigned)height_);
#endif
}

Graphics::Graphics(Context* context, GAPI gapi)
    : Object(context)
{
    Graphics::gapi = gapi;

#ifdef URHO3D_BGFX
    bgfx_ = new GraphicsBgfx();
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
    {
        Constructor_OGL();
        return;
    }
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
    {
        Constructor_D3D11();
        return;
    }
#endif
}

Graphics::~Graphics()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    delete bgfx_;
    bgfx_ = nullptr;
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
    {
        Destructor_OGL();
        return;
    }
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
    {
        Destructor_D3D11();
        return;
    }
#endif
}

bool Graphics::SetScreenMode(int width, int height, const ScreenModeParams& params, bool maximize)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (gapi == GAPI_BGFX)
    {
        // 简化版的窗口创建/调整，仅为 bgfx 后端提供 SDL 窗口
        int newWidth = width > 0 ? width : 1280;
        int newHeight = height > 0 ? height : 720;

        const bool wantBorderless = params.borderless_;
        const bool wantResizable = params.resizable_;
        const bool wantHighDPI = params.highDPI_;

        Uint32 flags = 0;
        if (wantResizable)
            flags |= SDL_WINDOW_RESIZABLE;
#ifdef SDL_WINDOW_HIGH_PIXEL_DENSITY
        if (wantHighDPI)
            flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
#endif
        if (wantBorderless)
            flags |= SDL_WINDOW_BORDERLESS;

        if (!window_)
        {
            // 如果设置了初始位置，则使用之
            const int posX = position_.x_ ? position_.x_ : SDL_WINDOWPOS_CENTERED;
            const int posY = position_.y_ ? position_.y_ : SDL_WINDOWPOS_CENTERED;
            window_ = SDL_CreateWindow(windowTitle_.Empty() ? "Urho3D" : windowTitle_.CString(), newWidth, newHeight, flags);
            if (!window_)
            {
                URHO3D_LOGERRORF("Failed to create window for BGFX: %s", SDL_GetError());
                return false;
            }
            if (params.fullscreen_)
                SDL_SetWindowFullscreen(window_, true);
            SDL_SetWindowPosition(window_, posX, posY);
            if (windowIcon_)
                CreateWindowIcon();
        }
        else
        {
            SDL_SetWindowSize(window_, newWidth, newHeight);
            if (params.fullscreen_)
                SDL_SetWindowFullscreen(window_, true);
            else
                SDL_SetWindowFullscreen(window_, false);
            SDL_SetWindowBordered(window_, !wantBorderless);
            SDL_SetWindowResizable(window_, wantResizable);
        }

        width_ = newWidth;
        height_ = newHeight;
        screenParams_ = params;

#ifdef URHO3D_BGFX
        // 确保在广播屏幕模式事件前完成 bgfx 初始化，使 Input::Initialize() 能顺利通过 IsInitialized 判定
        if (bgfx_ && !bgfx_->IsInitialized())
            bgfx_->InitializeFromSDL(window_, (unsigned)width_, (unsigned)height_);
#endif

        OnScreenModeChanged();
        return true;
    }
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetScreenMode_OGL(width, height, params, maximize);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetScreenMode_D3D11(width, height, params, maximize);;
#endif

    return {}; // Prevent warning
}

void Graphics::SetSRGB(bool enable)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && gapi == GAPI_BGFX)
    {
        bgfx_->SetSRGBBackbuffer(enable);
        // 需要 reset 才能生效
        if (IsInitialized())
            bgfx_->Reset((unsigned)width_, (unsigned)height_);
        sRGB_ = enable;
        return;
    }
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetSRGB_OGL(enable);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetSRGB_D3D11(enable);
#endif
}

void Graphics::SetDither(bool enable)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetDither_OGL(enable);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetDither_D3D11(enable);
#endif
}

void Graphics::SetFlushGPU(bool enable)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetFlushGPU_OGL(enable);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetFlushGPU_D3D11(enable);;
#endif
}

void Graphics::SetForceGL2(bool enable)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetForceGL2_OGL(enable);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetForceGL2_D3D11(enable);
#endif
}

void Graphics::Close()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return Close_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return Close_D3D11();
#endif
}

bool Graphics::TakeScreenShot(Image& destImage)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && gapi == GAPI_BGFX)
    {
        // 默认 2D 路径：使用内置离屏 RT 作为截图源
        if (useOffscreen_ && offscreenColor_.NotNull())
            return bgfx_->ReadRenderTargetToImage(offscreenColor_.Get(), destImage);
        // 回退：若外部显式绑定了离屏 RT，也支持读回
        RenderSurface* surface = GetRenderTarget(0);
        if (surface && surface->GetParentTexture())
            return bgfx_->ReadRenderTargetToImage(static_cast<Texture2D*>(surface->GetParentTexture()), destImage);
        URHO3D_LOGWARNING("BGFX TakeScreenShot: 未启用内置离屏且无外部 RT，无法截图");
        return false;
    }
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return TakeScreenShot_OGL(destImage);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return TakeScreenShot_D3D11(destImage);
#endif

    return {}; // Prevent warning
}

bool Graphics::BeginFrame()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    // 宏开关：当启用 bgfx 时，以 bgfx 为主进行帧提交，便于与原管线对比。
    if (bgfx_)
    {
        // 若启用内置离屏 RT，确保尺寸匹配
        if (useOffscreen_)
            EnsureOffscreenRT();
        if (!bgfx_->IsInitialized() && window_)
        {
            // 使用 SDL 窗口进行初始化（提取原生句柄）。
            bgfx_->InitializeFromSDL(window_, (unsigned)width_, (unsigned)height_);
        }
        if (bgfx_->IsInitialized())
        {
            bgfx_->BeginFrame();
            return true;
        }
    }
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return BeginFrame_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return BeginFrame_D3D11();
#endif

    return {}; // Prevent warning
}

void Graphics::EndFrame()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
    {
        // 若启用内置离屏渲染，将其呈现到 backbuffer
        if (useOffscreen_ && offscreenColor_.NotNull())
        {
            // 切换到 backbuffer
            bgfx_->ResetFrameBuffer();
            // 将 UI 程序绘制一个全屏四边形到 backbuffer
            // 注意：GraphicsBgfx::PresentOffscreen 使用 ui_ 程序，并在内部切换到 backbuffer
            // 这里不需要更改当前 RT 状态
            // 为防止之前视口影响，Present 将覆盖 view0 的 viewport
            bgfx_->SetViewport(IntRect(0, 0, width_, height_));
            // 使用 UI 提交一组全屏三角形：复用 BgfxDrawUITriangles（提供2个三角的6顶点）
            float verts[6 * 6] = {
                // x, y, z, color(abgr), u, v
                -1.f,-1.f,0.f, 1, 0.f,1.f,
                 1.f,-1.f,0.f, 1, 1.f,1.f,
                 1.f, 1.f,0.f, 1, 1.f,0.f,
                -1.f,-1.f,0.f, 1, 0.f,1.f,
                 1.f, 1.f,0.f, 1, 1.f,0.f,
                -1.f, 1.f,0.f, 1, 0.f,0.f,
            };
            Matrix4 id(Matrix4::IDENTITY);
            BgfxDrawUITriangles(verts, 6, offscreenColor_.Get(), id);
        }
        bgfx_->EndFrame();
        return;
    }
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return EndFrame_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return EndFrame_D3D11();
#endif
}

void Graphics::Clear(ClearTargetFlags flags, const Color& color, float depth, unsigned stencil)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
        return bgfx_->Clear(flags, color, depth, stencil);
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return Clear_OGL(flags, color, depth, stencil);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return Clear_D3D11(flags, color, depth, stencil);
#endif
}

bool Graphics::ResolveToTexture(Texture2D* destination, const IntRect& viewport)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && gapi == GAPI_BGFX)
    {
        // 简化：仅当当前绑定了离屏 RT 时，支持从当前颜色附件 blit 到目标纹理。
        if (!destination)
            return false;
        if (bgfxColorRT_)
        {
            IntRect vp = viewport;
            return bgfx_->Blit(destination, bgfxColorRT_, &vp);
        }
        URHO3D_LOGWARNING("BGFX ResolveToTexture: 当前未绑定离屏颜色目标，无法从 backbuffer 解析");
        return false;
    }
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return ResolveToTexture_OGL(destination, viewport);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return ResolveToTexture_D3D11(destination, viewport);
#endif

    return {}; // Prevent warning
}

bool Graphics::ResolveToTexture(Texture2D* texture)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return ResolveToTexture_OGL(texture);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return ResolveToTexture_D3D11(texture);
#endif

    return {}; // Prevent warning
}

bool Graphics::ResolveToTexture(TextureCube* texture)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return ResolveToTexture_OGL(texture);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return ResolveToTexture_D3D11(texture);
#endif

    return {}; // Prevent warning
}

void Graphics::Draw(PrimitiveType type, unsigned vertexStart, unsigned vertexCount)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return Draw_OGL(type, vertexStart, vertexCount);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return Draw_D3D11(type, vertexStart, vertexCount);
#endif
#ifdef URHO3D_BGFX
    if (gapi == GAPI_BGFX)
    {
        static bool warned1 = false;
        if (!warned1)
        {
            URHO3D_LOGWARNING("BGFX: Graphics::Draw (non-indexed) 尚未实现，调用被忽略。请使用 SpriteBatch/UI 的 bgfx 路径或实现通用网格提交。");
            warned1 = true;
        }
        return;
    }
#endif
}

void Graphics::Draw(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned minVertex, unsigned vertexCount)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return Draw_OGL(type, indexStart, indexCount, minVertex, vertexCount);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return Draw_D3D11(type, indexStart, indexCount, minVertex, vertexCount);
#endif
#ifdef URHO3D_BGFX
    if (gapi == GAPI_BGFX)
    {
        static bool warned2 = false;
        if (!warned2)
        {
            URHO3D_LOGWARNING("BGFX: Graphics::Draw (indexed) 尚未实现，调用被忽略。请使用 SpriteBatch/UI 的 bgfx 路径或实现通用网格提交。");
            warned2 = true;
        }
        return;
    }
#endif
}

void Graphics::Draw(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned baseVertexIndex, unsigned minVertex, unsigned vertexCount)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return Draw_OGL(type, indexStart, indexCount, baseVertexIndex, minVertex, vertexCount);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return Draw_D3D11(type, indexStart, indexCount, baseVertexIndex, minVertex, vertexCount);
#endif
#ifdef URHO3D_BGFX
    if (gapi == GAPI_BGFX)
    {
        static bool warned3 = false;
        if (!warned3)
        {
            URHO3D_LOGWARNING("BGFX: Graphics::Draw (indexed+baseVertex) 尚未实现，调用被忽略。");
            warned3 = true;
        }
        return;
    }
#endif
}

void Graphics::DrawInstanced(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned minVertex, unsigned vertexCount, unsigned instanceCount)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return DrawInstanced_OGL(type, indexStart, indexCount, minVertex, vertexCount, instanceCount);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return DrawInstanced_D3D11(type, indexStart, indexCount, minVertex, vertexCount, instanceCount);
#endif
#ifdef URHO3D_BGFX
    if (gapi == GAPI_BGFX)
    {
        static bool warned4 = false;
        if (!warned4)
        {
            URHO3D_LOGWARNING("BGFX: Graphics::DrawInstanced 尚未实现，调用被忽略。");
            warned4 = true;
        }
        return;
    }
#endif
}

void Graphics::DrawInstanced(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned baseVertexIndex, unsigned minVertex,
    unsigned vertexCount, unsigned instanceCount)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return DrawInstanced_OGL(type, indexStart, indexCount, baseVertexIndex, minVertex, vertexCount, instanceCount);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return DrawInstanced_D3D11(type, indexStart, indexCount, baseVertexIndex, minVertex, vertexCount, instanceCount);
#endif
#ifdef URHO3D_BGFX
    if (gapi == GAPI_BGFX)
    {
        static bool warned5 = false;
        if (!warned5)
        {
            URHO3D_LOGWARNING("BGFX: Graphics::DrawInstanced(+baseVertex) 尚未实现，调用被忽略。");
            warned5 = true;
        }
        return;
    }
#endif
}

void Graphics::SetVertexBuffer(VertexBuffer* buffer)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetVertexBuffer_OGL(buffer);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetVertexBuffer_D3D11(buffer);
#endif
}

bool Graphics::SetVertexBuffers(const Vector<VertexBuffer*>& buffers, unsigned instanceOffset)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetVertexBuffers_OGL(buffers, instanceOffset);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetVertexBuffers_D3D11(buffers, instanceOffset);
#endif

    return {}; // Prevent warning
}

bool Graphics::SetVertexBuffers(const Vector<SharedPtr<VertexBuffer>>& buffers, unsigned instanceOffset)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetVertexBuffers_OGL(buffers, instanceOffset);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetVertexBuffers_D3D11(buffers, instanceOffset);
#endif

    return {}; // Prevent warning
}

void Graphics::SetIndexBuffer(IndexBuffer* buffer)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetIndexBuffer_OGL(buffer);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetIndexBuffer_D3D11(buffer);
#endif
}

void Graphics::SetShaders(ShaderVariation* vs, ShaderVariation* ps)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetShaders_OGL(vs, ps);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetShaders_D3D11(vs, ps);
#endif
}

void Graphics::SetShaderParameter(StringHash param, const float* data, unsigned count)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetShaderParameter_OGL(param, data, count);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetShaderParameter_D3D11(param, data, count);
#endif
}

void Graphics::SetShaderParameter(StringHash param, float value)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetShaderParameter_OGL(param, value);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetShaderParameter_D3D11(param, value);
#endif
}

void Graphics::SetShaderParameter(StringHash param, int value)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetShaderParameter_OGL(param, value);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetShaderParameter_D3D11(param, value);
#endif
}

void Graphics::SetShaderParameter(StringHash param, bool value)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetShaderParameter_OGL(param, value);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetShaderParameter_D3D11(param, value);
#endif
}

void Graphics::SetShaderParameter(StringHash param, const Color& color)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetShaderParameter_OGL(param, color);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetShaderParameter_D3D11(param, color);
#endif
}

void Graphics::SetShaderParameter(StringHash param, const Vector2& vector)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetShaderParameter_OGL(param, vector);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetShaderParameter_D3D11(param, vector);
#endif
}

void Graphics::SetShaderParameter(StringHash param, const Matrix3& matrix)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetShaderParameter_OGL(param, matrix);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetShaderParameter_D3D11(param, matrix);
#endif
}

void Graphics::SetShaderParameter(StringHash param, const Vector3& vector)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetShaderParameter_OGL(param, vector);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetShaderParameter_D3D11(param, vector);
#endif
}

void Graphics::SetShaderParameter(StringHash param, const Matrix4& matrix)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetShaderParameter_OGL(param, matrix);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetShaderParameter_D3D11(param, matrix);
#endif
}

void Graphics::SetShaderParameter(StringHash param, const Vector4& vector)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetShaderParameter_OGL(param, vector);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetShaderParameter_D3D11(param, vector);
#endif
}

void Graphics::SetShaderParameter(StringHash param, const Matrix3x4& matrix)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetShaderParameter_OGL(param, matrix);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetShaderParameter_D3D11(param, matrix);
#endif
}

bool Graphics::NeedParameterUpdate(ShaderParameterGroup group, const void* source)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return NeedParameterUpdate_OGL(group, source);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return NeedParameterUpdate_D3D11(group, source);
#endif

    return {}; // Prevent warning
}

bool Graphics::HasShaderParameter(StringHash param)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return HasShaderParameter_OGL(param);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return HasShaderParameter_D3D11(param);
#endif

    return {}; // Prevent warning
}

bool Graphics::HasTextureUnit(TextureUnit unit)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return HasTextureUnit_OGL(unit);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return HasTextureUnit_D3D11(unit);
#endif

    return {}; // Prevent warning
}

void Graphics::ClearParameterSource(ShaderParameterGroup group)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return ClearParameterSource_OGL(group);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return ClearParameterSource_D3D11(group);
#endif
}

void Graphics::ClearParameterSources()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return ClearParameterSources_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return ClearParameterSources_D3D11();
#endif
}

void Graphics::ClearTransformSources()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return ClearTransformSources_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return ClearTransformSources_D3D11();
#endif
}

void Graphics::SetTexture(unsigned index, Texture* texture)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetTexture_OGL(index, texture);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetTexture_D3D11(index, texture);
#endif
}

void Graphics::SetDefaultTextureFilterMode(TextureFilterMode mode)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && gapi == GAPI_BGFX)
    {
        // 与各向异性一起统一下发
        bgfx_->SetDefaultSampler(mode, GetDefaultTextureAnisotropy());
        defaultTextureFilterMode_ = mode;
        return;
    }
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetDefaultTextureFilterMode_OGL(mode);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetDefaultTextureFilterMode_D3D11(mode);
#endif
}

void Graphics::SetDefaultTextureAnisotropy(unsigned level)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && gapi == GAPI_BGFX)
    {
        bgfx_->SetDefaultSampler(GetDefaultTextureFilterMode(), level);
        defaultTextureAnisotropy_ = level;
        return;
    }
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetDefaultTextureAnisotropy_OGL(level);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetDefaultTextureAnisotropy_D3D11(level);
#endif
}

void Graphics::ResetRenderTargets()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
    {
        if (useOffscreen_ && offscreenColor_.NotNull())
        {
            bgfxColorRT_ = offscreenColor_.Get();
            // 默认无需深度
            bgfxDepthRT_ = nullptr;
            if (!bgfx_->SetFrameBuffer(bgfxColorRT_, bgfxDepthRT_))
            {
                URHO3D_LOGWARNING("BGFX ResetRenderTargets: SetFrameBuffer failed, fallback to backbuffer");
                bgfxColorRT_ = nullptr;
                bgfxDepthRT_ = nullptr;
                bgfx_->ResetFrameBuffer();
            }
        }
        else
        {
            bgfxColorRT_ = nullptr;
            bgfxDepthRT_ = nullptr;
            bgfx_->ResetFrameBuffer();
        }
        return;
    }
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return ResetRenderTargets_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return ResetRenderTargets_D3D11();
#endif
}

void Graphics::ResetRenderTarget(unsigned index)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
    {
        if (index == 0)
            bgfxColorRT_ = nullptr;
        bgfx_->SetFrameBuffer(bgfxColorRT_, bgfxDepthRT_);
        return;
    }
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return ResetRenderTarget_OGL(index);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return ResetRenderTarget_D3D11(index);
#endif
}

void Graphics::ResetDepthStencil()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
    {
        bgfxDepthRT_ = nullptr;
        bgfx_->SetFrameBuffer(bgfxColorRT_, bgfxDepthRT_);
        return;
    }
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return ResetDepthStencil_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return ResetDepthStencil_D3D11();
#endif
}

void Graphics::SetRenderTarget(unsigned index, RenderSurface* renderTarget)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
    {
        // 简化：Urho3D 的 RenderSurface 基于 Texture2D，转换指向的父纹理
        Texture2D* tex = renderTarget ? static_cast<Texture2D*>(renderTarget->GetParentTexture()) : nullptr;
        if (index == 0)
            bgfxColorRT_ = tex;
        bgfx_->SetFrameBuffer(bgfxColorRT_, bgfxDepthRT_);
        return;
    }
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetRenderTarget_OGL(index, renderTarget);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetRenderTarget_D3D11(index, renderTarget);
#endif
}

void Graphics::SetRenderTarget(unsigned index, Texture2D* texture)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
    {
        if (index == 0)
            bgfxColorRT_ = texture;
        bgfx_->SetFrameBuffer(bgfxColorRT_, bgfxDepthRT_);
        return;
    }
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetRenderTarget_OGL(index, texture);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetRenderTarget_D3D11(index, texture);
#endif
}

void Graphics::SetDepthStencil(RenderSurface* depthStencil)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
    {
        Texture2D* tex = depthStencil ? static_cast<Texture2D*>(depthStencil->GetParentTexture()) : nullptr;
        bgfxDepthRT_ = tex;
        bgfx_->SetFrameBuffer(bgfxColorRT_, bgfxDepthRT_);
        return;
    }
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetDepthStencil_OGL(depthStencil);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetDepthStencil_D3D11(depthStencil);
#endif
}

void Graphics::SetDepthStencil(Texture2D* texture)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
    {
        bgfxDepthRT_ = texture;
        bgfx_->SetFrameBuffer(bgfxColorRT_, bgfxDepthRT_);
        return;
    }
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetDepthStencil_OGL(texture);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetDepthStencil_D3D11(texture);
#endif
}

void Graphics::SetViewport(const IntRect& rect)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
        return bgfx_->SetViewport(rect);
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetViewport_OGL(rect);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetViewport_D3D11(rect);
#endif
}

void Graphics::SetBlendMode(BlendMode mode, bool alphaToCoverage)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
        return bgfx_->SetBlendMode(mode, alphaToCoverage);
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetBlendMode_OGL(mode, alphaToCoverage);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetBlendMode_D3D11(mode, alphaToCoverage);
#endif
}

void Graphics::SetColorWrite(bool enable)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
        return bgfx_->SetColorWrite(enable);
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetColorWrite_OGL(enable);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetColorWrite_D3D11(enable);
#endif
}

void Graphics::SetCullMode(CullMode mode)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
        return bgfx_->SetCullMode(mode);
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetCullMode_OGL(mode);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetCullMode_D3D11(mode);
#endif
}

void Graphics::SetDepthBias(float constantBias, float slopeScaledBias)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
        return bgfx_->SetDepthBias(constantBias, slopeScaledBias);
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetDepthBias_OGL(constantBias, slopeScaledBias);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetDepthBias_D3D11(constantBias, slopeScaledBias);
#endif
}

void Graphics::SetDepthTest(CompareMode mode)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
        return bgfx_->SetDepthTest(mode);
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetDepthTest_OGL(mode);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetDepthTest_D3D11(mode);
#endif
}

void Graphics::SetDepthWrite(bool enable)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
        return bgfx_->SetDepthWrite(enable);
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetDepthWrite_OGL(enable);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetDepthWrite_D3D11(enable);
#endif
}

void Graphics::SetFillMode(FillMode mode)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
        return bgfx_->SetFillMode(mode);
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetFillMode_OGL(mode);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetFillMode_D3D11(mode);
#endif
}

void Graphics::SetLineAntiAlias(bool enable)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
        return bgfx_->SetLineAntiAlias(enable);
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetLineAntiAlias_OGL(enable);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetLineAntiAlias_D3D11(enable);
#endif
}

void Graphics::SetScissorTest(bool enable, const Rect& rect, bool borderInclusive)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
    {
        IntRect ir(IntVector2((int)rect.min_.x_, (int)rect.min_.y_), IntVector2((int)rect.max_.x_, (int)rect.max_.y_));
        bgfx_->SetScissor(enable, ir);
        return;
    }
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetScissorTest_OGL(enable, rect, borderInclusive);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetScissorTest_D3D11(enable, rect, borderInclusive);
#endif
}

void Graphics::SetScissorTest(bool enable, const IntRect& rect)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
        return bgfx_->SetScissor(enable, rect);
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetScissorTest_OGL(enable, rect);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetScissorTest_D3D11(enable, rect);
#endif
}

void Graphics::SetClipPlane(bool enable, const Plane& clipPlane, const Matrix3x4& view, const Matrix4& projection)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
    {
        // 将平面从世界/视空间转换到后续使用空间由上层处理；2D-only 暂记录为无效果。
        (void)view; (void)projection;
        Vector4 p(clipPlane.normal_.x_, clipPlane.normal_.y_, clipPlane.normal_.z_, clipPlane.d_);
        bgfx_->SetClipPlane(enable, p);
        return;
    }
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetClipPlane_OGL(enable, clipPlane, view, projection);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetClipPlane_D3D11(enable, clipPlane, view, projection);
#endif
}

void Graphics::SetStencilTest(bool enable, CompareMode mode, StencilOp pass, StencilOp fail, StencilOp zFail, u32 stencilRef,
    u32 compareMask, u32 writeMask)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (bgfx_ && bgfx_->IsInitialized())
        return bgfx_->SetStencilTest(enable, mode, pass, fail, zFail, stencilRef, compareMask, writeMask);
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetStencilTest_OGL(enable, mode, pass, fail, zFail, stencilRef, compareMask, writeMask);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetStencilTest_D3D11(enable, mode, pass, fail, zFail, stencilRef, compareMask, writeMask);
#endif
}

bool Graphics::IsInitialized() const
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    // 若 bgfx 已初始化，则认为图形系统可用（以便 Renderer 不再依赖旧后端的状态）。
    if (bgfx_ && bgfx_->IsInitialized())
        return true;
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return IsInitialized_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return IsInitialized_D3D11();
#endif

    return {}; // Prevent warning
}

#ifdef URHO3D_BGFX
bool Graphics::IsBgfxActive() const
{
    return bgfx_ && bgfx_->IsInitialized();
}

void Graphics::DebugDrawBgfxHello()
{
    if (!bgfx_)
        return;
    auto* cache = GetSubsystem<ResourceCache>();
    if (cache && bgfx_->LoadUIPrograms(cache))
        bgfx_->DebugDrawHello();
}

bool Graphics::BgfxDrawQuads(const void* qvertices, int numVertices, Texture2D* texture, const Matrix4& mvp)
{
    if (!bgfx_)
        return false;
    auto* cache = GetSubsystem<ResourceCache>();
    return bgfx_->DrawQuads(qvertices, numVertices, texture, cache, mvp);
}

bool Graphics::BgfxDrawTriangles(const void* tvertices, int numVertices, const Matrix4& mvp)
{
    if (!bgfx_)
        return false;
    auto* cache = GetSubsystem<ResourceCache>();
    return bgfx_->DrawTriangles(tvertices, numVertices, cache, mvp);
}

bool Graphics::BgfxDrawUITriangles(const float* vertices, int numVertices, Texture2D* texture, const Matrix4& mvp)
{
    if (!bgfx_)
        return false;
    auto* cache = GetSubsystem<ResourceCache>();
    return bgfx_->DrawUITriangles(vertices, numVertices, texture, cache, mvp);
}

bool Graphics::BgfxDrawUIWithMaterial(const float* vertices, int numVertices, Material* material, const Matrix4& mvp)
{
#ifdef URHO3D_BGFX
    if (!bgfx_)
        return false;
    auto* cache = GetSubsystem<ResourceCache>();
    return bgfx_->DrawUIWithMaterial(vertices, numVertices, material, cache, mvp);
#else
    (void)vertices; (void)numVertices; (void)material; (void)mvp; return false;
#endif
}

#ifdef URHO3D_BGFX
void Graphics::EnsureOffscreenRT()
{
    if (!useOffscreen_)
        return;
    const unsigned w = (unsigned)(width_ > 0 ? width_ : 1);
    const unsigned h = (unsigned)(height_ > 0 ? height_ : 1);
    if (!offscreenColor_ || offscreenColor_->GetWidth() != (int)w || offscreenColor_->GetHeight() != (int)h)
    {
        offscreenColor_ = new Texture2D(context_);
        offscreenColor_->SetSize((int)w, (int)h, GetRGBAFormat(), TEXTURE_RENDERTARGET);
        offscreenColor_->SetFilterMode(defaultTextureFilterMode_);
        offscreenColor_->SetAnisotropy(defaultTextureAnisotropy_);
    }
}
#endif

bool Graphics::BgfxCreateTextureFromImage(Texture2D* texture, Image* image, bool useAlpha)
{
    if (!bgfx_)
        return false;
    // 复用 GraphicsBgfx 实现：直接从 Image 创建并缓存 BGFX 纹理句柄
    // 注意：Texture2D 的采样参数将被用于 BGFX sampler flags（寻址/过滤/各向异性/sRGB）
    return bgfx_->CreateTextureFromImage(texture, image, useAlpha);
}
#endif

bool Graphics::BeginUIDraw(RenderSurface* surface, int targetWidth, int targetHeight)
{
#ifdef URHO3D_BGFX
    if (IsBgfxActive())
    {
        if (surface)
            SetRenderTarget(0, surface);
        else
            ResetRenderTargets();
        SetViewport(IntRect(0, 0, targetWidth, targetHeight));
        return true;
    }
#else
    (void)surface; (void)targetWidth; (void)targetHeight;
#endif
    return false;
}

void Graphics::BgfxReleaseTexture(Texture2D* texture)
{
#ifdef URHO3D_BGFX
    if (!bgfx_)
        return;
    bgfx_->ReleaseTexture(texture);
#else
    (void)texture;
#endif
}

bool Graphics::BgfxUpdateTextureRegion(Texture2D* texture, int x, int y, int width, int height, const void* data, unsigned level)
{
#ifdef URHO3D_BGFX
    if (!bgfx_)
        return false;
    return bgfx_->UpdateTextureRegion(texture, x, y, width, height, data, level);
#else
    (void)texture; (void)x; (void)y; (void)width; (void)height; (void)data; (void)level; return false;
#endif
}
void Graphics::EndUIDraw(RenderSurface* surface)
{
#ifdef URHO3D_BGFX
    if (IsBgfxActive())
    {
        if (surface)
            ResetRenderTargets();
    }
#else
    (void)surface;
#endif
}

bool Graphics::SubmitUIBatch(const float* vertices, int numVertices, Texture2D* texture,
                             const IntRect& scissor, BlendMode blend, const Matrix4& projection)
{
#ifdef URHO3D_BGFX
    if (IsBgfxActive())
    {
        SetBlendMode(blend);
        SetScissorTest(true, scissor);
        return BgfxDrawUITriangles(vertices, numVertices, texture, projection);
    }
#else
    (void)vertices; (void)numVertices; (void)texture; (void)scissor; (void)blend; (void)projection;
#endif
    return false;
}

bool Graphics::GetDither() const
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetDither_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetDither_D3D11();
#endif

    return {}; // Prevent warning
}

bool Graphics::IsDeviceLost() const
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return IsDeviceLost_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return IsDeviceLost_D3D11();
#endif

    return {}; // Prevent warning
}

Vector<int> Graphics::GetMultiSampleLevels() const
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetMultiSampleLevels_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetMultiSampleLevels_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetFormat(CompressedFormat format) const
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetFormat_OGL(format);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetFormat_D3D11(format);
#endif

    return {}; // Prevent warning
}

ShaderVariation* Graphics::GetShader(ShaderType type, const String& name, const String& defines) const
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetShader_OGL(type, name, defines);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetShader_D3D11(type, name, defines);
#endif

    return {}; // Prevent warning
}

ShaderVariation* Graphics::GetShader(ShaderType type, const char* name, const char* defines) const
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetShader_OGL(type, name, defines);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetShader_D3D11(type, name, defines);
#endif

    return {}; // Prevent warning
}

VertexBuffer* Graphics::GetVertexBuffer(unsigned index) const
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetVertexBuffer_OGL(index);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetVertexBuffer_D3D11(index);
#endif

    return {}; // Prevent warning
}

TextureUnit Graphics::GetTextureUnit(const String& name)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetTextureUnit_OGL(name);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetTextureUnit_D3D11(name);
#endif

    return {}; // Prevent warning
}

const String& Graphics::GetTextureUnitName(TextureUnit unit)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetTextureUnitName_OGL(unit);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetTextureUnitName_D3D11(unit);
#endif

    return String::EMPTY; // Prevent warning
}

Texture* Graphics::GetTexture(unsigned index) const
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetTexture_OGL(index);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetTexture_D3D11(index);
#endif

    return {}; // Prevent warning
}

RenderSurface* Graphics::GetRenderTarget(unsigned index) const
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetRenderTarget_OGL(index);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetRenderTarget_D3D11(index);
#endif

    return {}; // Prevent warning
}

IntVector2 Graphics::GetRenderTargetDimensions() const
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetRenderTargetDimensions_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetRenderTargetDimensions_D3D11();
#endif

    return {}; // Prevent warning
}

void Graphics::OnWindowResized()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return OnWindowResized_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return OnWindowResized_D3D11();
#endif
}

void Graphics::OnWindowMoved()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return OnWindowMoved_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return OnWindowMoved_D3D11();
#endif
}

ConstantBuffer* Graphics::GetOrCreateConstantBuffer(ShaderType type, unsigned index, unsigned size)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetOrCreateConstantBuffer_OGL(type, index, size);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetOrCreateConstantBuffer_D3D11(type, index, size);
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetMaxBones()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetMaxBones_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetMaxBones_D3D11();
#endif

    return {}; // Prevent warning
}

bool Graphics::GetGL3Support()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetGL3Support_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetGL3Support_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetAlphaFormat()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (gapi == GAPI_BGFX)
        return BGFX_FMT_ALPHA8;
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetAlphaFormat_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetAlphaFormat_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetLuminanceFormat()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (gapi == GAPI_BGFX)
        return BGFX_FMT_ALPHA8; // 作为占位，2D-only 不使用此格式
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetLuminanceFormat_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetLuminanceFormat_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetLuminanceAlphaFormat()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (gapi == GAPI_BGFX)
        return BGFX_FMT_RGB8; // 占位
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetLuminanceAlphaFormat_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetLuminanceAlphaFormat_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetRGBFormat()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (gapi == GAPI_BGFX)
        return BGFX_FMT_RGB8;
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetRGBFormat_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetRGBFormat_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetRGBAFormat()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (gapi == GAPI_BGFX)
        return BGFX_FMT_RGBA8;
#endif
#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetRGBAFormat_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetRGBAFormat_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetRGBA16Format()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetRGBA16Format_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetRGBA16Format_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetRGBAFloat16Format()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetRGBAFloat16Format_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetRGBAFloat16Format_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetRGBAFloat32Format()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetRGBAFloat32Format_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetRGBAFloat32Format_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetRG16Format()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetRG16Format_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetRG16Format_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetRGFloat16Format()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetRGFloat16Format_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetRGFloat16Format_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetRGFloat32Format()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetRGFloat32Format_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetRGFloat32Format_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetFloat16Format()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetFloat16Format_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetFloat16Format_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetFloat32Format()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetFloat32Format_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetFloat32Format_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetLinearDepthFormat()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetLinearDepthFormat_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetLinearDepthFormat_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetDepthStencilFormat()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetDepthStencilFormat_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetDepthStencilFormat_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetReadableDepthFormat()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetReadableDepthFormat_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetReadableDepthFormat_D3D11();
#endif

    return {}; // Prevent warning
}

unsigned Graphics::GetFormat(const String& formatName)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetFormat_OGL(formatName);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetFormat_D3D11(formatName);
#endif

    return {}; // Prevent warning
}

void RegisterGraphicsLibrary(Context* context)
{
    Material::RegisterObject(context);
    Shader::RegisterObject(context);
    Technique::RegisterObject(context);
    Texture2D::RegisterObject(context);
    Texture2DArray::RegisterObject(context);
    Texture3D::RegisterObject(context);
    TextureCube::RegisterObject(context);
    Camera::RegisterObject(context);
    Drawable::RegisterObject(context);
    DebugRenderer::RegisterObject(context);
    Octree::RegisterObject(context);
    Zone::RegisterObject(context);
}

}

