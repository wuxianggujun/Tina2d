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
#include <algorithm>

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
    // 应用当前状态、裁剪等（供后续提交使用）。
    ApplyState();
    if (scissorEnabled_)
    {
    const uint16_t w = static_cast<uint16_t>(std::max(0, scissorRect_.Width()));
    const uint16_t h = static_cast<uint16_t>(std::max(0, scissorRect_.Height()));
    const uint16_t x = static_cast<uint16_t>(std::max(0, scissorRect_.left_));
    const uint16_t y = static_cast<uint16_t>(std::max(0, scissorRect_.top_));
        bgfx::setScissor(x, y, w, h);
    }
    else
    {
        // 关闭裁剪
        bgfx::setScissor(UINT16_MAX);
    }
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

void GraphicsBgfx::SetViewport(const IntRect& rect)
{
#ifdef URHO3D_BGFX
    if (!initialized_)
        return;
    const uint16_t w = static_cast<uint16_t>(std::max(0, rect.Width()));
    const uint16_t h = static_cast<uint16_t>(std::max(0, rect.Height()));
    const uint16_t x = static_cast<uint16_t>(std::max(0, rect.left_));
    const uint16_t y = static_cast<uint16_t>(std::max(0, rect.top_));
    bgfx::setViewRect(0, x, y, w, h);
#else
    (void)rect;
#endif
}

static inline uint32_t PackRGBA8(const Color& c)
{
    auto clamp = [](float v){ v = v < 0.f ? 0.f : (v > 1.f ? 1.f : v); return v; };
    const uint32_t r = static_cast<uint32_t>(clamp(c.r_) * 255.0f + 0.5f);
    const uint32_t g = static_cast<uint32_t>(clamp(c.g_) * 255.0f + 0.5f);
    const uint32_t b = static_cast<uint32_t>(clamp(c.b_) * 255.0f + 0.5f);
    const uint32_t a = static_cast<uint32_t>(clamp(c.a_) * 255.0f + 0.5f);
    // 按 RRGGBBAA 打包（与示例用法一致）
    return (r << 24) | (g << 16) | (b << 8) | a;
}

void GraphicsBgfx::Clear(ClearTargetFlags flags, const Color& color, float depth, u32 stencil)
{
#ifdef URHO3D_BGFX
    if (!initialized_)
        return;
    uint16_t mask = 0;
    if (flags & CLEAR_COLOR)
        mask |= BGFX_CLEAR_COLOR;
    if (flags & CLEAR_DEPTH)
        mask |= BGFX_CLEAR_DEPTH;
    if (flags & CLEAR_STENCIL)
        mask |= BGFX_CLEAR_STENCIL;
    const uint32_t rgba = PackRGBA8(color);
    bgfx::setViewClear(0, mask, rgba, depth, static_cast<uint8_t>(stencil & 0xFF));
    // 确保本帧至少提交一次 view 0（若调用方仅清屏）
    bgfx::touch(0);
#else
    (void)flags; (void)color; (void)depth; (void)stencil;
#endif
}

// --- 状态映射 ---

void GraphicsBgfx::ApplyState()
{
#ifdef URHO3D_BGFX
    bgfx::setState(state_);
#endif
}

void GraphicsBgfx::SetBlendMode(BlendMode mode, bool alphaToCoverage)
{
#ifdef URHO3D_BGFX
    // 清除所有混合位
    state_ &= ~BGFX_STATE_BLEND_MASK;
    state_ &= ~BGFX_STATE_BLEND_EQUATION_MASK;
    // Alpha-to-coverage
    if (alphaToCoverage)
        state_ |= BGFX_STATE_BLEND_ALPHA_TO_COVERAGE;
    else
        state_ &= ~BGFX_STATE_BLEND_ALPHA_TO_COVERAGE;

    switch (mode)
    {
    case BLEND_REPLACE: break; // 不设置混合位
    case BLEND_ALPHA: state_ |= BGFX_STATE_BLEND_ALPHA; break;
    case BLEND_ADD: state_ |= BGFX_STATE_BLEND_ADD; break;
    case BLEND_MULTIPLY: state_ |= BGFX_STATE_BLEND_MULTIPLY; break;
    case BLEND_PREMULALPHA:
        state_ |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
        break;
    case BLEND_ADDALPHA: state_ |= BGFX_STATE_BLEND_ADD; break; // 近似
    case BLEND_INVDESTALPHA: state_ |= BGFX_STATE_BLEND_ALPHA; break; // 近似
    case BLEND_SUBTRACT:
        state_ |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE);
        state_ |= BGFX_STATE_BLEND_EQUATION_SUB;
        break;
    case BLEND_SUBTRACTALPHA:
        state_ |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
        state_ |= BGFX_STATE_BLEND_EQUATION_SUB;
        break; // 近似：以 srcAlpha 权重相减
    default: break;
    }
#else
    (void)mode; (void)alphaToCoverage;
#endif
}

void GraphicsBgfx::SetColorWrite(bool enable)
{
#ifdef URHO3D_BGFX
    // 先清除颜色写入位
    state_ &= ~(BGFX_STATE_WRITE_R | BGFX_STATE_WRITE_G | BGFX_STATE_WRITE_B | BGFX_STATE_WRITE_A);
    if (enable)
        state_ |= (BGFX_STATE_WRITE_R | BGFX_STATE_WRITE_G | BGFX_STATE_WRITE_B | BGFX_STATE_WRITE_A);
#else
    (void)enable;
#endif
}

void GraphicsBgfx::SetCullMode(CullMode mode)
{
#ifdef URHO3D_BGFX
    state_ &= ~(BGFX_STATE_CULL_CW | BGFX_STATE_CULL_CCW);
    switch (mode)
    {
    case CULL_NONE: break;
    case CULL_CW: state_ |= BGFX_STATE_CULL_CW; break;
    case CULL_CCW: state_ |= BGFX_STATE_CULL_CCW; break;
    default: break;
    }
#else
    (void)mode;
#endif
}

void GraphicsBgfx::SetDepthTest(CompareMode mode)
{
#ifdef URHO3D_BGFX
    state_ &= ~BGFX_STATE_DEPTH_TEST_MASK;
    switch (mode)
    {
    case CMP_ALWAYS: state_ |= BGFX_STATE_DEPTH_TEST_ALWAYS; break;
    case CMP_EQUAL: state_ |= BGFX_STATE_DEPTH_TEST_EQUAL; break;
    case CMP_NOTEQUAL: state_ |= BGFX_STATE_DEPTH_TEST_NOTEQUAL; break;
    case CMP_LESS: state_ |= BGFX_STATE_DEPTH_TEST_LESS; break;
    case CMP_LESSEQUAL: state_ |= BGFX_STATE_DEPTH_TEST_LEQUAL; break;
    case CMP_GREATER: state_ |= BGFX_STATE_DEPTH_TEST_GREATER; break;
    case CMP_GREATEREQUAL: state_ |= BGFX_STATE_DEPTH_TEST_GEQUAL; break;
    default: break;
    }
#else
    (void)mode;
#endif
}

void GraphicsBgfx::SetDepthWrite(bool enable)
{
#ifdef URHO3D_BGFX
    if (enable)
        state_ |= BGFX_STATE_WRITE_Z;
    else
        state_ &= ~BGFX_STATE_WRITE_Z;
#else
    (void)enable;
#endif
}

void GraphicsBgfx::SetScissor(bool enable, const IntRect& rect)
{
#ifdef URHO3D_BGFX
    scissorEnabled_ = enable;
    scissorRect_ = rect;
#else
    (void)enable; (void)rect;
#endif
}

} // namespace Urho3D
