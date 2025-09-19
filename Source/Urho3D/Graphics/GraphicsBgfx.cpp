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
#include <vector>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <cstdio>

#include "../Resource/ResourceCache.h"
#include "../IO/File.h"
#include "../GraphicsAPI/Texture2D.h"
#include "../Resource/Image.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Material.h"
#include "../Core/Variant.h"
#include "../Container/Vector.h"
#include "../IO/Log.h"
#include <bgfx/bgfx.h>

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
    // 确保默认视图与 UI 视图（31）在本帧有效，并设置 UI 视图到 backbuffer
    bgfx::touch(0);
    const uint16_t uiView = 31;
    bgfx::setViewRect(uiView, 0, 0, static_cast<uint16_t>(width_), static_cast<uint16_t>(height_));
    bgfx::setViewFrameBuffer(uiView, BGFX_INVALID_HANDLE);
    bgfx::touch(uiView);
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
    lastBlendMode_ = mode;
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
    if (initialized_)
    {
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
            bgfx::setScissor(UINT16_MAX);
        }
    }
#else
    (void)enable; (void)rect;
#endif
}

void GraphicsBgfx::SetStencilTest(bool enable, CompareMode mode, StencilOp pass, StencilOp fail, StencilOp zFail,
                        unsigned stencilRef, unsigned compareMask, unsigned writeMask)
{
#ifdef URHO3D_BGFX
    stencilEnabled_ = enable;
    stencilFunc_ = mode;
    stencilPass_ = pass;
    stencilFail_ = fail;
    stencilZFail_ = zFail;
    stencilRef_ = stencilRef & 0xFFu;
    stencilReadMask_ = compareMask & 0xFFu;
    stencilWriteMask_ = writeMask & 0xFFu;

    if (!initialized_)
        return;

    if (!stencilEnabled_)
    {
        bgfx::setStencil(BGFX_STENCIL_NONE);
        return;
    }

    auto mapCmp = [](CompareMode m)->uint32_t
    {
        switch (m)
        {
        case CMP_EQUAL:       return BGFX_STENCIL_TEST_EQUAL;
        case CMP_NOTEQUAL:    return BGFX_STENCIL_TEST_NOTEQUAL;
        case CMP_LESS:        return BGFX_STENCIL_TEST_LESS;
        case CMP_LESSEQUAL:   return BGFX_STENCIL_TEST_LEQUAL;
        case CMP_GREATER:     return BGFX_STENCIL_TEST_GREATER;
        case CMP_GREATEREQUAL:return BGFX_STENCIL_TEST_GEQUAL;
        case CMP_ALWAYS:
        default:              return BGFX_STENCIL_TEST_ALWAYS;
        }
    };
    auto mapOpFailS = [](StencilOp op)->uint32_t
    {
        switch (op)
        {
        case OP_ZERO: return BGFX_STENCIL_OP_FAIL_S_ZERO;
        case OP_REF:  return BGFX_STENCIL_OP_FAIL_S_REPLACE;
        case OP_INCR: return BGFX_STENCIL_OP_FAIL_S_INCR;
        case OP_DECR: return BGFX_STENCIL_OP_FAIL_S_DECR;
        case OP_KEEP:
        default:      return BGFX_STENCIL_OP_FAIL_S_KEEP;
        }
    };
    auto mapOpFailZ = [](StencilOp op)->uint32_t
    {
        switch (op)
        {
        case OP_ZERO: return BGFX_STENCIL_OP_FAIL_Z_ZERO;
        case OP_REF:  return BGFX_STENCIL_OP_FAIL_Z_REPLACE;
        case OP_INCR: return BGFX_STENCIL_OP_FAIL_Z_INCR;
        case OP_DECR: return BGFX_STENCIL_OP_FAIL_Z_DECR;
        case OP_KEEP:
        default:      return BGFX_STENCIL_OP_FAIL_Z_KEEP;
        }
    };
    auto mapOpPassZ = [](StencilOp op)->uint32_t
    {
        switch (op)
        {
        case OP_ZERO: return BGFX_STENCIL_OP_PASS_Z_ZERO;
        case OP_REF:  return BGFX_STENCIL_OP_PASS_Z_REPLACE;
        case OP_INCR: return BGFX_STENCIL_OP_PASS_Z_INCR;
        case OP_DECR: return BGFX_STENCIL_OP_PASS_Z_DECR;
        case OP_KEEP:
        default:      return BGFX_STENCIL_OP_PASS_Z_KEEP;
        }
    };

    uint32_t flags = 0;
    flags |= mapCmp(stencilFunc_);
    flags |= mapOpFailS(stencilFail_);
    flags |= mapOpFailZ(stencilZFail_);
    flags |= mapOpPassZ(stencilPass_);
    flags |= BGFX_STENCIL_FUNC_REF(stencilRef_);
    flags |= BGFX_STENCIL_FUNC_RMASK(stencilReadMask_);
    // 写掩码在当前 bgfx 版本未提供独立宏，忽略之以维持 2D-only 行为

    bgfx::setStencil(flags);
#else
    (void)enable; (void)mode; (void)pass; (void)fail; (void)zFail; (void)stencilRef; (void)compareMask; (void)writeMask;
#endif
}

void GraphicsBgfx::SetFillMode(FillMode mode)
{
#ifdef URHO3D_BGFX
    // bgfx 无全局多边形模式切换（线框/点渲染需使用特殊顶点/着色器或线段/点拓扑），
    // 2D-only 场景下保留记录并静默忽略非 FILL_SOLID。
    fillMode_ = mode;
    if (mode != FILL_SOLID)
        URHO3D_LOGDEBUG("BGFX: 非 FILL_SOLID 模式在 2D-only 路径下被忽略（如需线框请在上层使用调试/自定义着色器）。");
#else
    (void)mode;
#endif
}

void GraphicsBgfx::SetDepthBias(float constantBias, float slopeScaledBias)
{
#ifdef URHO3D_BGFX
    // 通用深度偏移不在 bgfx 通用状态集合中，通常通过着色器/渲染目标方案实现。
    // 2D-only 中保留记录，默认忽略。
    depthBiasConst_ = constantBias;
    depthBiasSlope_ = slopeScaledBias;
    {
        static bool once = false;
        if (!once)
        {
            URHO3D_LOGDEBUG("BGFX: DepthBias 在 2D-only 路径下被忽略（如需层次排序请使用 Z 或批次顺序）");
            once = true;
        }
    }
#else
    (void)constantBias; (void)slopeScaledBias;
#endif
}

void GraphicsBgfx::SetLineAntiAlias(bool enable)
{
#ifdef URHO3D_BGFX
    lineAA_ = enable;
    {
        static bool once = false;
        if (!once)
        {
            URHO3D_LOGDEBUG("BGFX: 线段抗锯齿依赖具体后端/着色器，2D-only 路径暂不提供全局切换");
            once = true;
        }
    }
#else
    (void)enable;
#endif
}

void GraphicsBgfx::SetClipPlane(bool enable, const Vector4& clipPlane)
{
#ifdef URHO3D_BGFX
    clipPlaneEnabled_ = enable;
    clipPlane_ = clipPlane;
    {
        static bool once = false;
        if (!once)
        {
            URHO3D_LOGDEBUG("BGFX: 通用 ClipPlane 需着色器支持，2D-only 路径暂不提供全局剪裁平面");
            once = true;
        }
    }
#else
    (void)enable; (void)clipPlane;
#endif
}

bool GraphicsBgfx::LoadUIPrograms(ResourceCache* cache)
{
#ifdef URHO3D_BGFX
    if (!initialized_ || !cache)
        return false;
    if (ui_.ready)
        return true;

    // 按当前渲染后端推断 profile 子目录（与 ShaderUtils.cmake 的 _bgfx_get_profile_path_ext 对齐）
    auto getProfileDir = []() -> const char*
    {
        using RT = bgfx::RendererType::Enum;
        switch (bgfx::getRendererType())
        {
        case RT::Direct3D11: return "dx11"; // s_5_0
        case RT::Direct3D12: return "dx11"; // 复用 s_5_0
        case RT::OpenGL:     return "glsl"; // 120
        case RT::OpenGLES:   return "essl"; // 300_es
        case RT::Metal:      return "metal";
        case RT::Vulkan:     return "spirv";
        default:             return "glsl";  // 保守默认
        }
    };

    const String profile = getProfileDir();

    auto tryOpen = [&](const String& relPath) -> SharedPtr<File>
    {
        if (!cache->Exists(relPath))
            return SharedPtr<File>();
        SharedPtr<File> f(cache->GetFile(relPath));
        if (f && f->IsOpen())
            return f;
        return SharedPtr<File>();
    };

    auto findShader = [&](const char* base) -> SharedPtr<File>
    {
        // 尝试平铺输出：Shaders/BGFX/base.bin
        if (auto f = tryOpen(String("Shaders/BGFX/") + base + ".bin"))
            return f;
        // 尝试平铺但带 .sc 后缀：base.sc.bin
        if (auto f = tryOpen(String("Shaders/BGFX/") + base + ".sc.bin"))
            return f;
        // 尝试 profile 子目录：profile/base.bin
        if (auto f = tryOpen(String("Shaders/BGFX/") + profile + "/" + base + ".bin"))
            return f;
        // 尝试 profile 子目录 + .sc.bin
        if (auto f = tryOpen(String("Shaders/BGFX/") + profile + "/" + base + ".sc.bin"))
            return f;
        URHO3D_LOGERRORF("BGFX shader not found: %s (%s)", base, profile.CString());
        return SharedPtr<File>();
    };

    auto loadShader = [](File* file)->bgfx::ShaderHandle
    {
        if (!file)
            return bgfx::ShaderHandle{bgfx::kInvalidHandle};
        const uint32_t size = static_cast<uint32_t>(file->GetSize());
        if (!size)
            return bgfx::ShaderHandle{bgfx::kInvalidHandle};
        SharedArrayPtr<unsigned char> buf(new unsigned char[size]);
        file->Read(buf.Get(), size);
        const bgfx::Memory* mem = bgfx::copy(buf.Get(), size);
        return bgfx::createShader(mem);
    };

    // 遵循 add_shader_compile_dir 的命名：<Base>_vs / <Base>_fs
    auto findPair = [&](const char* base) -> std::pair<SharedPtr<File>, SharedPtr<File>>
    {
        String b(base);
        return { findShader((b + "_vs").CString()), findShader((b + "_fs").CString()) };
    };

    auto [sv_vc, sf_vc] = findPair("Basic_VC");
    auto [sv_diff, sf_diff] = findPair("Basic_Diff_VC");
    auto [sv_alpha, sf_alpha] = findPair("Basic_Alpha_VC");
    auto [sv_mask, sf_mask] = findPair("Basic_DiffAlphaMask_VC");
    // 至少需要 Diff 版本
    if (!sv_diff || !sf_diff)
        return false;

    // programDiff（各自持有自己的 shader，便于销毁）
    auto vsh_diff = loadShader(sv_diff);
    auto fsh_diff = loadShader(sf_diff);
    if (!bgfx::isValid(vsh_diff) || !bgfx::isValid(fsh_diff))
        return false;
    ui_.programDiff = bgfx::createProgram(vsh_diff, fsh_diff, true /* destroy shaders */).idx;

    // programAlpha（可选）
    if (sv_alpha && sf_alpha)
    {
        auto vsh_alpha = loadShader(sv_alpha);
        auto fsh_alpha = loadShader(sf_alpha);
        if (bgfx::isValid(vsh_alpha) && bgfx::isValid(fsh_alpha))
            ui_.programAlpha = bgfx::createProgram(vsh_alpha, fsh_alpha, true).idx;
    }

    if (sv_mask && sf_mask)
    {
        auto vsh_mask = loadShader(sv_mask);
        auto fsh_mask = loadShader(sf_mask);
        if (bgfx::isValid(vsh_mask) && bgfx::isValid(fsh_mask))
            ui_.programMask = bgfx::createProgram(vsh_mask, fsh_mask, true).idx;
    }

    // uniforms（通用）
    ui_.u_mvp = bgfx::createUniform("u_mvp", bgfx::UniformType::Mat4).idx;
    ui_.s_tex = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler).idx;
    ui_.s_texAlt = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler).idx;

    // 创建 1x1 白色纹理
    const uint32_t white = 0xFFFFFFFFu;
    const bgfx::Memory* tmem = bgfx::copy(&white, sizeof(white));
    ui_.whiteTex = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8,
        (BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_W_CLAMP | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT), tmem).idx;

    bgfx::ProgramHandle phDiff{ ui_.programDiff };
    bgfx::UniformHandle umvp{ ui_.u_mvp };
    bgfx::UniformHandle stex{ ui_.s_tex };
    bgfx::TextureHandle wtex{ ui_.whiteTex };
    ui_.ready = bgfx::isValid(phDiff) && bgfx::isValid(umvp)
        && bgfx::isValid(stex) && bgfx::isValid(wtex);
    if (!ui_.ready)
        URHO3D_LOGERROR("BGFX UI programs not ready (program/uniform creation failed)");
    else
        URHO3D_LOGINFOF("BGFX UI programs loaded for profile: %s", profile.CString());
    return ui_.ready;
#else
    (void)cache; return false;
#endif
}

void GraphicsBgfx::DebugDrawHello()
{
#ifdef URHO3D_BGFX
    if (!initialized_ || !ui_.ready)
        return;

    // 顶点声明：pos(3f), color0(ub4n), texcoord0(2f)
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0,2, bgfx::AttribType::Float)
        .end();

    struct Vtx { float x,y,z; uint32_t abgr; float u,v; };
    const Vtx verts[4] = {
        {-0.5f,-0.5f,0.0f, 0xFF00FF00u, 0.0f,0.0f},
        { 0.5f,-0.5f,0.0f, 0xFF00FF00u, 1.0f,0.0f},
        { 0.5f, 0.5f,0.0f, 0xFF00FF00u, 1.0f,1.0f},
        {-0.5f, 0.5f,0.0f, 0xFF00FF00u, 0.0f,1.0f},
    };
    const uint16_t indices[6] = {0,1,2, 0,2,3};

    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer tib;
    if (!bgfx::allocTransientBuffers(&tvb, layout, 4, &tib, 6))
        return;
    memcpy(tvb.data, verts, sizeof(verts));
    memcpy(tib.data, indices, sizeof(indices));

    // 设定 uniform（单位矩阵），并绑定白色纹理
    float mvp[16] = {1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1};
    bgfx::UniformHandle umvp; umvp.idx = ui_.u_mvp;
    bgfx::UniformHandle stex1; stex1.idx = ui_.s_tex;
    bgfx::UniformHandle stex2; stex2.idx = ui_.s_texAlt;
    bgfx::TextureHandle wtex; wtex.idx = ui_.whiteTex;
    bgfx::setUniform(umvp, mvp);
    // 注意：不可在同一纹理单元(stage)上为两个不同的采样器uniform重复绑定，
    // 否则后一条会覆盖前一条，导致着色器实际使用的采样器未被绑定。
    // 约定：stage 0 绑定 s_texColor；stage 1 绑定 s_tex（兼容 Basic2D 等）。
    bgfx::setTexture(0, stex1, wtex);
    bgfx::setTexture(1, stex2, wtex);

    // 应用状态并提交 drawcall（最小：颜色可写）
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::ProgramHandle ph2; ph2.idx = ui_.programDiff;
    bgfx::submit(0, ph2);
#endif
}

static uint64_t GetBgfxSamplerFlagsFromTexture(const Texture2D* tex)
{
    uint64_t flags = 0;
    // Addressing modes
    auto au = tex->GetAddressMode(COORD_U);
    auto av = tex->GetAddressMode(COORD_V);
    auto aw = tex->GetAddressMode(COORD_W);
    auto toFlags = [](TextureAddressMode m, uint64_t mirror, uint64_t clamp) -> uint64_t
    {
        switch (m)
        {
        case ADDRESS_MIRROR: return mirror;
        case ADDRESS_CLAMP:  return clamp;
        case ADDRESS_WRAP:
        default: return 0; // wrap 为默认（0）
        }
    };
    flags |= toFlags(au, BGFX_SAMPLER_U_MIRROR, BGFX_SAMPLER_U_CLAMP);
    flags |= toFlags(av, BGFX_SAMPLER_V_MIRROR, BGFX_SAMPLER_V_CLAMP);
    flags |= toFlags(aw, BGFX_SAMPLER_W_MIRROR, BGFX_SAMPLER_W_CLAMP);

    // Filter mode（兼容较老版本的 bgfx：仅使用 *_POINT 标志，线性为默认）
    switch (tex->GetFilterMode())
    {
    case FILTER_NEAREST:
        // 最邻近：min/mag/mip 全部 point
        flags |= (BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT);
        break;
    case FILTER_BILINEAR:
        // 双线性：min/mag 使用默认线性，仅将 mip 固定为 point
        flags |= BGFX_SAMPLER_MIP_POINT;
        break;
    case FILTER_TRILINEAR:
        // 三线性：min/mag/mip 均使用默认线性（不设置 point 标志）
        break;
    default:
        // 默认：保守采用双线性
        flags |= BGFX_SAMPLER_MIP_POINT;
        break;
    }

    // 各向异性（某些 bgfx 版本不提供该标志，条件编译处理）
    if (tex->GetAnisotropy() > 1)
    {
    #ifdef BGFX_SAMPLER_ANISOTROPIC
        flags |= BGFX_SAMPLER_ANISOTROPIC;
    #endif
    }

    return flags;
}

unsigned short GraphicsBgfx::GetOrCreateTexture(Texture2D* tex, ResourceCache* cache)
{
#ifdef URHO3D_BGFX
    if (!tex)
    {
        URHO3D_LOGDEBUG("GetOrCreateTexture: tex is null, returning white texture");
        return ui_.whiteTex;
    }
    auto it = textureCache_.find(tex);
    if (it != textureCache_.end())
        return it->second;
    
    const String& resName = tex->GetName();
    
    URHO3D_LOGDEBUGF("GetOrCreateTexture: Creating bgfx texture for %s (format=%u, size=%dx%d)", 
        resName.CString(), tex->GetFormat(), tex->GetWidth(), tex->GetHeight());

    // 优先根据纹理格式决定获取像素的方式，避免对非 RGBA/RGB 纹理调用 GetImage() 触发错误日志
    const unsigned fmt = tex->GetFormat();
    const unsigned alphaFmt = Graphics::GetAlphaFormat();
    const unsigned rgbaFmt = Graphics::GetRGBAFormat();
    const unsigned rgbFmt  = Graphics::GetRGBFormat();

    uint32_t w = 0, h = 0;
    const bgfx::Memory* mem = nullptr;

    const uint64_t samplerFlags = GetBgfxSamplerFlagsFromTexture(tex);

    if (fmt == alphaFmt)
    {
        // 单通道 Alpha 纹理：读取 A8 数据并扩展为 RGBA8(FFFFFF,A)
        w = (uint32_t)tex->GetWidth();
        h = (uint32_t)tex->GetHeight();
        const uint32_t aSize = w * h; // A8
        std::vector<unsigned char> a8(aSize);
        if (!tex->GetData(0, a8.data()))
        {
            URHO3D_LOGERRORF("GetOrCreateTexture: Failed to get A8 data from texture %s", tex->GetName().CString());
            return ui_.whiteTex;
        }
        std::vector<unsigned char> bgra; bgra.resize(w * h * 4u);
        unsigned char* dst = bgra.data();
        for (uint32_t i = 0; i < aSize; ++i)
        {
            const unsigned char a = a8[i];
            // BGRA 排列：B,G,R = 255，A = alpha
            dst[i*4 + 0] = 0xFF;
            dst[i*4 + 1] = 0xFF;
            dst[i*4 + 2] = 0xFF;
            dst[i*4 + 3] = a;
        }
        mem = bgfx::copy(bgra.data(), (uint32_t)bgra.size());
    }
    else if (fmt == rgbaFmt || fmt == rgbFmt)
    {
        URHO3D_LOGDEBUGF("GetOrCreateTexture: Processing RGBA/RGB texture %s", resName.CString());
        SharedPtr<Image> rgba;
        // 优先从资源系统重新解码原始图片（避免依赖 GPU 回读）。仅当资源名非空才尝试。
        if (cache && !resName.Empty())
        {
            SharedPtr<Image> src(cache->GetResource<Image>(resName));
            if (src)
            {
                rgba = src->IsCompressed() ? src->GetDecompressedImage() : src;
                if (rgba && rgba->GetComponents() != 4)
                    rgba = rgba->ConvertToRGBA();
            }
        }
        // 回退：尝试通过 Texture2D::GetImage（BGFX 下通常不可用），失败则用白纹理替代。
        if (!rgba)
        {
            if (resName.Empty())
            {
                // 未命名的运行时纹理（例如默认材质用的占位纹理），无可靠的源图像；返回白纹理，避免错误日志刷屏。
                URHO3D_LOGDEBUG("GetOrCreateTexture: Unnamed runtime texture, fallback to whiteTex");
                return ui_.whiteTex;
            }
            else
            {
                SharedPtr<Image> img = tex->GetImage();
                if (img)
                {
                    rgba = img->IsCompressed() ? img->GetDecompressedImage() : img;
                    if (rgba && rgba->GetComponents() != 4)
                        rgba = rgba->ConvertToRGBA();
                }
            }
        }

        if (!rgba)
        {
            URHO3D_LOGERRORF("GetOrCreateTexture: Failed to acquire RGBA image for %s", resName.CString());
            return ui_.whiteTex;
        }

        w = (uint32_t)rgba->GetWidth();
        h = (uint32_t)rgba->GetHeight();
        const uint32_t size = w * h * 4u;
        mem = bgfx::copy(rgba->GetData(), size);
    }
    else
    {
        // 其它格式（压缩/容器等）：尝试用 bimg 解码（支持 DDS/KTX/PVR），失败则回退白纹理
        if (cache)
        {
            const String& resName = tex->GetName();
            SharedPtr<File> f(cache->GetFile(resName));
            if (f && f->IsOpen())
            {
                const unsigned fsize = (unsigned)f->GetSize();
                if (fsize > 0)
                {
                    SharedArrayPtr<unsigned char> fbuf(new unsigned char[fsize]);
                    f->Read(fbuf.Get(), fsize);

                    bx::DefaultAllocator alloc;
                    bimg::ImageContainer* ic = bimg::imageParse(&alloc, fbuf.Get(), fsize);
                    if (ic)
                    {
                        const bool hasMips = ic->m_numMips > 1;
                        const uint16_t numLayers = (uint16_t)ic->m_numLayers;
                        const bgfx::TextureFormat::Enum bfmt = (bgfx::TextureFormat::Enum)ic->m_format;
                        uint64_t tflags = 0;
#ifdef BGFX_TEXTURE_SRGB
                        if (tex->GetSRGB()) tflags |= BGFX_TEXTURE_SRGB;
#endif
                        const bgfx::Memory* tmem = bgfx::copy(ic->m_data, (uint32_t)ic->m_size);
                        bgfx::TextureHandle th = bgfx::createTexture2D((uint16_t)ic->m_width, (uint16_t)ic->m_height, hasMips, numLayers, bfmt, tflags, tmem);
                        bimg::imageFree(ic);
                        if (bgfx::isValid(th))
                        {
                            textureCache_[tex] = th.idx;
                            return th.idx;
                        }
                    }
                }
            }
        }
        return ui_.whiteTex;
    }

    uint64_t tflags = 0;
#ifdef BGFX_TEXTURE_SRGB
    if (tex->GetSRGB()) tflags |= BGFX_TEXTURE_SRGB;
#endif
    bgfx::TextureHandle th = bgfx::createTexture2D(
        (uint16_t)w, (uint16_t)h, false, 1,
        bgfx::TextureFormat::RGBA8,
        tflags,
        mem);
    if (!bgfx::isValid(th))
    {
        URHO3D_LOGERRORF("GetOrCreateTexture: Failed to create bgfx texture for %s", tex->GetName().CString());
        return ui_.whiteTex;
    }
    URHO3D_LOGDEBUGF("GetOrCreateTexture: Successfully created bgfx texture for %s (handle=%u)", tex->GetName().CString(), th.idx);
    textureCache_[tex] = th.idx;
    return th.idx;
#else
    (void)tex; return 0xFFFF;
#endif
}

unsigned short GraphicsBgfx::GetOrCreateSampler(const char* name)
{
#ifdef URHO3D_BGFX
    auto it = samplerCache_.find(name);
    if (it != samplerCache_.end())
        return it->second;
    bgfx::UniformHandle h = bgfx::createUniform(name, bgfx::UniformType::Sampler);
    if (!bgfx::isValid(h))
        return bgfx::kInvalidHandle;
    samplerCache_[name] = h.idx;
    return h.idx;
#else
    (void)name; return bgfx::kInvalidHandle;
#endif
}

unsigned short GraphicsBgfx::GetOrCreateVec4(const char* name)
{
#ifdef URHO3D_BGFX
    auto it = vec4Cache_.find(name);
    if (it != vec4Cache_.end())
        return it->second;
    bgfx::UniformHandle h = bgfx::createUniform(name, bgfx::UniformType::Vec4);
    if (!bgfx::isValid(h))
        return bgfx::kInvalidHandle;
    vec4Cache_[name] = h.idx;
    return h.idx;
#else
    (void)name; return bgfx::kInvalidHandle;
#endif
}

unsigned short GraphicsBgfx::GetOrCreateMat4(const char* name)
{
#ifdef URHO3D_BGFX
    auto it = mat4Cache_.find(name);
    if (it != mat4Cache_.end())
        return it->second;
    bgfx::UniformHandle h = bgfx::createUniform(name, bgfx::UniformType::Mat4);
    if (!bgfx::isValid(h))
        return bgfx::kInvalidHandle;
    mat4Cache_[name] = h.idx;
    return h.idx;
#else
    (void)name; return bgfx::kInvalidHandle;
#endif
}

void GraphicsBgfx::SetUniformByVariant(const char* name, const Variant& v)
{
#ifdef URHO3D_BGFX
    switch (v.GetType())
    {
    case VAR_BOOL:
    case VAR_INT:
    case VAR_FLOAT:
    {
        float data[4] = { (v.GetType()==VAR_FLOAT ? v.GetFloat() : (float)v.GetI32()), 0,0,0 };
        bgfx::UniformHandle uh; uh.idx = GetOrCreateVec4(name);
        if (bgfx::isValid(uh)) bgfx::setUniform(uh, data);
        break;
    }
    case VAR_VECTOR2:
    {
        const Vector2 vv = v.GetVector2(); float data[4] = { vv.x_, vv.y_, 0, 0 };
        bgfx::UniformHandle uh; uh.idx = GetOrCreateVec4(name);
        if (bgfx::isValid(uh)) bgfx::setUniform(uh, data);
        break;
    }
    case VAR_VECTOR3:
    {
        const Vector3 vv = v.GetVector3(); float data[4] = { vv.x_, vv.y_, vv.z_, 0 };
        bgfx::UniformHandle uh; uh.idx = GetOrCreateVec4(name);
        if (bgfx::isValid(uh)) bgfx::setUniform(uh, data);
        break;
    }
    case VAR_VECTOR4:
    {
        const Vector4 vv = v.GetVector4(); float data[4] = { vv.x_, vv.y_, vv.z_, vv.w_ };
        bgfx::UniformHandle uh; uh.idx = GetOrCreateVec4(name);
        if (bgfx::isValid(uh)) bgfx::setUniform(uh, data);
        break;
    }
    case VAR_COLOR:
    {
        const Color c = v.GetColor(); float data[4] = { c.r_, c.g_, c.b_, c.a_ };
        bgfx::UniformHandle uh; uh.idx = GetOrCreateVec4(name);
        if (bgfx::isValid(uh)) bgfx::setUniform(uh, data);
        break;
    }
    case VAR_MATRIX4:
    {
        const Matrix4 m = v.GetMatrix4();
        float mvpArr[16] = {
            m.m00_, m.m10_, m.m20_, m.m30_,
            m.m01_, m.m11_, m.m21_, m.m31_,
            m.m02_, m.m12_, m.m22_, m.m32_,
            m.m03_, m.m13_, m.m23_, m.m33_,
        };
        bgfx::UniformHandle uh; uh.idx = GetOrCreateMat4(name);
        if (bgfx::isValid(uh)) bgfx::setUniform(uh, mvpArr);
        break;
    }
    default:
        break;
    }
#else
    (void)name; (void)v;
#endif
}

void GraphicsBgfx::ReleaseTexture(Texture2D* tex)
{
#ifdef URHO3D_BGFX
    if (!tex)
        return;
    auto it = textureCache_.find(tex);
    if (it != textureCache_.end())
    {
        const unsigned short idx = it->second;
        if (idx != bgfx::kInvalidHandle)
        {
            bgfx::TextureHandle h; h.idx = idx;
            if (bgfx::isValid(h))
                bgfx::destroy(h);
        }
        textureCache_.erase(it);
    }
    // 同步清理引用该纹理的帧缓冲缓存
    for (auto fbIt = fbCache_.begin(); fbIt != fbCache_.end(); )
    {
        const FBKey& k = fbIt->first;
        if (k.color == tex || k.depth == tex)
        {
            unsigned short fidx = fbIt->second;
            if (fidx != bgfx::kInvalidHandle)
            {
                bgfx::FrameBufferHandle fh; fh.idx = fidx;
                if (bgfx::isValid(fh))
                    bgfx::destroy(fh);
            }
            fbIt = fbCache_.erase(fbIt);
        }
        else
            ++fbIt;
    }
#else
    (void)tex;
#endif
}

bool GraphicsBgfx::DrawQuads(const void* qvertices, int numVertices, Texture2D* texture, ResourceCache* cache, const Matrix4& mvp)
{
#ifdef URHO3D_BGFX
    if (!initialized_)
        return false;
    if (!LoadUIPrograms(cache))
        return false;
    if (numVertices <= 0)
        return true;

    // 顶点布局
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0,2, bgfx::AttribType::Float)
        .end();

    struct Vtx { float x,y,z; uint32_t abgr; float u,v; };
    const int vcount = numVertices;
    const int qcount = vcount / 4;
    const int icount = qcount * 6;

    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer tib;
    if (!bgfx::allocTransientBuffers(&tvb, layout, (uint32_t)vcount, &tib, (uint32_t)icount))
        return false;

    // 转拷数据
    const auto* src = reinterpret_cast<const unsigned char*>(qvertices);
    // QVertex: Vector3 position_, u32 color_, Vector2 uv_
    struct QV { float px,py,pz; uint32_t color; float u,v; };
    auto* vdst = reinterpret_cast<Vtx*>(tvb.data);
    for (int i=0;i<vcount;i++)
    {
        const QV& s = *reinterpret_cast<const QV*>(src + i*sizeof(QV));
        vdst[i] = {s.px,s.py,s.pz, s.color, s.u,s.v};
    }
    // 索引
    auto* idst = reinterpret_cast<uint16_t*>(tib.data);
    for (int q=0;q<qcount;q++)
    {
        uint16_t base = (uint16_t)(q*4);
        idst[q*6+0]=base+0; idst[q*6+1]=base+1; idst[q*6+2]=base+2;
        idst[q*6+3]=base+0; idst[q*6+4]=base+2; idst[q*6+5]=base+3;
    }

    // 设定 MVP=I 与纹理
    float mvpArr[16] = {
        mvp.m00_, mvp.m10_, mvp.m20_, mvp.m30_,
        mvp.m01_, mvp.m11_, mvp.m21_, mvp.m31_,
        mvp.m02_, mvp.m12_, mvp.m22_, mvp.m32_,
        mvp.m03_, mvp.m13_, mvp.m23_, mvp.m33_,
    };
    bgfx::UniformHandle umvp; umvp.idx = ui_.u_mvp;
    bgfx::UniformHandle stex1; stex1.idx = ui_.s_tex;
    bgfx::UniformHandle stex2; stex2.idx = ui_.s_texAlt;
    bgfx::TextureHandle texh; texh.idx = GetOrCreateTexture(texture, cache);
    bgfx::setUniform(umvp, mvpArr);
    uint64_t sflags = texture ? GetBgfxSamplerFlagsFromTexture(texture) : 0;
    bgfx::setTexture(0, stex1, texh, (uint32_t)sflags);
    bgfx::setTexture(1, stex2, texh, (uint32_t)sflags);

    // 提交
    bgfx::ProgramHandle ph; ph.idx = ui_.programDiff;
    bgfx::setState((state_ | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A));
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::submit(0, ph);
    return true;
#else
    (void)qvertices; (void)numVertices; (void)texture; (void)cache; return false;
#endif
}

bool GraphicsBgfx::DrawTriangles(const void* tvertices, int numVertices, ResourceCache* cache, const Matrix4& mvp)
{
#ifdef URHO3D_BGFX
    if (!initialized_)
        return false;
    if (!LoadUIPrograms(cache))
        return false;
    if (numVertices <= 0)
        return true;

    // 布局同上：补 0 UV
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0,2, bgfx::AttribType::Float)
        .end();

    struct Vtx { float x,y,z; uint32_t abgr; float u,v; };
    const int vcount = numVertices;
    bgfx::TransientVertexBuffer tvb;
    if (bgfx::getAvailTransientVertexBuffer((uint32_t)vcount, layout) < (uint32_t)vcount)
        return false;
    bgfx::allocTransientVertexBuffer(&tvb, (uint32_t)vcount, layout);
    auto* vdst = reinterpret_cast<Vtx*>(tvb.data);
    // TVertex: Vector3 position_, u32 color_
    struct TV { float px,py,pz; uint32_t color; };
    const auto* src = reinterpret_cast<const unsigned char*>(tvertices);
    for (int i=0;i<vcount;i++)
    {
        const TV& s = *reinterpret_cast<const TV*>(src + i*sizeof(TV));
        vdst[i] = {s.px,s.py,s.pz, s.color, 0.0f, 0.0f};
    }

    float mvpArr[16] = {
        mvp.m00_, mvp.m10_, mvp.m20_, mvp.m30_,
        mvp.m01_, mvp.m11_, mvp.m21_, mvp.m31_,
        mvp.m02_, mvp.m12_, mvp.m22_, mvp.m32_,
        mvp.m03_, mvp.m13_, mvp.m23_, mvp.m33_,
    };
    bgfx::UniformHandle umvp; umvp.idx = ui_.u_mvp;
    bgfx::UniformHandle stex1; stex1.idx = ui_.s_tex;
    bgfx::UniformHandle stex2; stex2.idx = ui_.s_texAlt;
    bgfx::TextureHandle texh; texh.idx = ui_.whiteTex;
    bgfx::setUniform(umvp, mvpArr);
    bgfx::setTexture(0, stex1, texh);
    bgfx::setTexture(1, stex2, texh);

    // 使用顺序索引而非 setVertexCount，避免因 API 约束导致的 fatal
    if (numVertices > 0xFFFF)
        return false; // 防御：UI 批次异常大时避免 16-bit 索引溢出
    bgfx::TransientIndexBuffer tib;
    if (bgfx::getAvailTransientIndexBuffer((uint32_t)numVertices) < (uint32_t)numVertices)
        return false;
    bgfx::allocTransientIndexBuffer(&tib, (uint32_t)numVertices);
    {
        auto* idst = reinterpret_cast<uint16_t*>(tib.data);
        for (int i = 0; i < numVertices; ++i)
            idst[i] = (uint16_t)i;
    }

    bgfx::ProgramHandle ph; ph.idx = ui_.programDiff;
    bgfx::setState((state_ | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A));
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::submit(0, ph);
    return true;
#else
    (void)tvertices; (void)numVertices; (void)cache; return false;
#endif
}

bool GraphicsBgfx::DrawUITriangles(const float* vertices, int numVertices, Texture2D* texture, ResourceCache* cache, const Matrix4& mvp)
{
#ifdef URHO3D_BGFX
    if (!initialized_)
        return false;
    if (!LoadUIPrograms(cache))
        return false;
    if (numVertices <= 0 || vertices == nullptr)
        return true;

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0,2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer tvb;
    if (bgfx::getAvailTransientVertexBuffer((uint32_t)numVertices, layout) < (uint32_t)numVertices)
        return false;
    bgfx::allocTransientVertexBuffer(&tvb, (uint32_t)numVertices, layout);
    struct Vtx { float x,y,z; uint32_t abgr; float u,v; };
    auto* vdst = reinterpret_cast<Vtx*>(tvb.data);
    for (int i = 0; i < numVertices; ++i)
    {
        const float* src = vertices + i * 6;
        uint32_t color; memcpy(&color, &src[3], sizeof(uint32_t));
        vdst[i] = {src[0], src[1], src[2], color, src[4], src[5]};
    }

    float mvpArr[16] = {
        mvp.m00_, mvp.m10_, mvp.m20_, mvp.m30_,
        mvp.m01_, mvp.m11_, mvp.m21_, mvp.m31_,
        mvp.m02_, mvp.m12_, mvp.m22_, mvp.m32_,
        mvp.m03_, mvp.m13_, mvp.m23_, mvp.m33_,
    };
    bgfx::UniformHandle umvp; umvp.idx = ui_.u_mvp;
    bgfx::UniformHandle stex1; stex1.idx = ui_.s_tex;
    bgfx::UniformHandle stex2; stex2.idx = ui_.s_texAlt;
    bgfx::TextureHandle texh; texh.idx = GetOrCreateTexture(texture, cache);
    bgfx::setUniform(umvp, mvpArr);
    uint64_t sflags2 = texture ? GetBgfxSamplerFlagsFromTexture(texture) : 0;
    bgfx::setTexture(0, stex1, texh, (uint32_t)sflags2);
    bgfx::setTexture(1, stex2, texh, (uint32_t)sflags2);

    // 按纹理格式与混合模式选择像素程序
    unsigned short programIdx = ui_.programDiff;
    if (texture)
    {
        unsigned alphaFormat = Graphics::GetAlphaFormat();
        const bool isAlphaTex = texture->GetFormat() == alphaFormat;
        if (isAlphaTex && ui_.programAlpha != bgfx::kInvalidHandle)
        {
            programIdx = ui_.programAlpha;
        }
        else
        {
            const bool useMask = (lastBlendMode_ != BLEND_ALPHA && lastBlendMode_ != BLEND_ADDALPHA && lastBlendMode_ != BLEND_PREMULALPHA);
            if (!isAlphaTex && useMask && ui_.programMask != bgfx::kInvalidHandle)
                programIdx = ui_.programMask;
            else
                programIdx = ui_.programDiff;
        }
    }

    // 使用顺序索引而非 setVertexCount
    if (numVertices > 0xFFFF)
        return false; // 防御：UI 批次异常大时避免 16-bit 索引溢出
    bgfx::TransientIndexBuffer tib;
    if (bgfx::getAvailTransientIndexBuffer((uint32_t)numVertices) < (uint32_t)numVertices)
        return false;
    bgfx::allocTransientIndexBuffer(&tib, (uint32_t)numVertices);
    {
        auto* idst = reinterpret_cast<uint16_t*>(tib.data);
        for (int i = 0; i < numVertices; ++i)
            idst[i] = (uint16_t)i;
    }

    bgfx::ProgramHandle ph; ph.idx = programIdx;
    bgfx::setState((state_ | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A));
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::submit(0, ph);
    return true;
#else
    (void)vertices; (void)numVertices; (void)texture; (void)cache; (void)mvp; return false;
#endif
}

bool GraphicsBgfx::CreateTextureFromImage(Texture2D* tex, Image* image, bool useAlpha)
{
#ifdef URHO3D_BGFX
    if (!tex || !image)
        return false;

    const uint64_t samplerFlags = GetBgfxSamplerFlagsFromTexture(tex);

    // 处理组件数：1(A8)/3(RGB)/4(RGBA)
    SharedPtr<Image> rgba(image);
    if (image->IsCompressed())
        rgba = image->GetDecompressedImage();
    if (!rgba)
        return false;

    const unsigned comps = rgba->GetComponents();
    std::vector<unsigned char> data;
    const unsigned w = rgba->GetWidth();
    const unsigned h = rgba->GetHeight();
    const unsigned pixels = w * h;

    if (comps == 1)
    {
        const unsigned char* a8 = rgba->GetData();
        data.resize(pixels * 4u);
        for (unsigned i = 0; i < pixels; ++i)
        {
            unsigned char a = a8[i];
            data[i*4+0] = 0xFF; // R
            data[i*4+1] = 0xFF; // G
            data[i*4+2] = 0xFF; // B
            data[i*4+3] = a;    // A
        }
    }
    else if (comps == 3)
    {
        const unsigned char* src = rgba->GetData();
        data.resize(pixels * 4u);
        for (unsigned i = 0; i < pixels; ++i)
        {
            data[i*4+0] = src[i*3+0];
            data[i*4+1] = src[i*3+1];
            data[i*4+2] = src[i*3+2];
            data[i*4+3] = 0xFF;
        }
    }
    else if (comps == 4)
    {
        // 直接复制 RGBA8
        const unsigned char* src = rgba->GetData();
        data.assign(src, src + pixels*4u);
    }
    else
    {
        // 转成 RGBA 再试
        rgba = rgba->ConvertToRGBA();
        if (!rgba)
            return false;
        const unsigned char* src = rgba->GetData();
        data.assign(src, src + rgba->GetWidth()*rgba->GetHeight()*4u);
    }

    const bgfx::Memory* mem = bgfx::copy(data.data(), (uint32_t)data.size());
    uint64_t tflags = 0;
#ifdef BGFX_TEXTURE_SRGB
    if (tex->GetSRGB()) tflags |= BGFX_TEXTURE_SRGB;
#endif
    bgfx::TextureHandle th = bgfx::createTexture2D((uint16_t)w, (uint16_t)h, false, 1,
        bgfx::TextureFormat::RGBA8, tflags, mem);
    if (!bgfx::isValid(th))
        return false;

    textureCache_[tex] = th.idx;
    return true;
#else
    (void)tex; (void)image; (void)useAlpha; return false;
#endif
}

bool GraphicsBgfx::DrawUIWithMaterial(const float* vertices, int numVertices, Material* material, ResourceCache* cache, const Matrix4& mvp)
{
#ifdef URHO3D_BGFX
    if (!initialized_)
        return false;
    if (!LoadUIPrograms(cache))
        return false;
    if (numVertices <= 0 || vertices == nullptr)
        return true;

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0,2, bgfx::AttribType::Float)
        .end();

    bgfx::TransientVertexBuffer tvb;
    if (bgfx::getAvailTransientVertexBuffer((uint32_t)numVertices, layout) < (uint32_t)numVertices)
        return false;
    bgfx::allocTransientVertexBuffer(&tvb, (uint32_t)numVertices, layout);
    struct Vtx { float x,y,z; uint32_t abgr; float u,v; };
    auto* vdst = reinterpret_cast<Vtx*>(tvb.data);
    for (int i = 0; i < numVertices; ++i)
    {
        const float* src = vertices + i * 6;
        uint32_t color; memcpy(&color, &src[3], sizeof(uint32_t));
        vdst[i] = {src[0], src[1], src[2], color, src[4], src[5]};
    }

    float mvpArr[16] = {
        mvp.m00_, mvp.m10_, mvp.m20_, mvp.m30_,
        mvp.m01_, mvp.m11_, mvp.m21_, mvp.m31_,
        mvp.m02_, mvp.m12_, mvp.m22_, mvp.m32_,
        mvp.m03_, mvp.m13_, mvp.m23_, mvp.m33_,
    };
    bgfx::UniformHandle umvp; umvp.idx = ui_.u_mvp;
    if (!bgfx::isValid(umvp))
        umvp.idx = GetOrCreateMat4("u_mvp");
    if (bgfx::isValid(umvp))
        bgfx::setUniform(umvp, mvpArr);

    // 贴图绑定：按 unit 升序映射到连续 stage，stage0/1 使用 s_texColor/s_tex
    Texture2D* primaryTex = nullptr;
    if (material)
    {
        const auto textures = material->GetTextures();
        if (!textures.Empty())
        {
            Vector<unsigned> units; units.Reserve(textures.Size());
            for (auto it = textures.Begin(); it != textures.End(); ++it)
                units.Push(it->first_);
            Sort(units.Begin(), units.End());

            unsigned stage = 0;
            for (unsigned u : units)
            {
                auto it2 = textures.Find(static_cast<TextureUnit>(u));
                if (it2 == textures.End())
                    continue;
                Texture* t = it2->second_.Get();
                if (!t) continue;
                auto* t2d = dynamic_cast<Texture2D*>(t);
                if (!t2d) continue;
                if (!primaryTex) primaryTex = t2d;

                const uint64_t sflags = GetBgfxSamplerFlagsFromTexture(t2d);
                bgfx::TextureHandle th; th.idx = GetOrCreateTexture(t2d, cache);
                if (!bgfx::isValid(th)) continue;

                const char* uname = nullptr;
                if (stage == 0) uname = "s_texColor";
                else if (stage == 1) uname = "s_tex";
                else
                {
                    static char buf[32];
                    snprintf(buf, sizeof(buf), "s_tex%u", stage);
                    uname = buf;
                }
                bgfx::UniformHandle stex;
                if (stage == 0) { stex.idx = ui_.s_tex; }
                else if (stage == 1) { stex.idx = ui_.s_texAlt; }
                else { stex.idx = GetOrCreateSampler(uname); }
                if (!bgfx::isValid(stex))
                    stex.idx = GetOrCreateSampler(uname);
                if (bgfx::isValid(stex))
                    bgfx::setTexture((uint8_t)stage, stex, th, (uint32_t)sflags);
                ++stage;
            }
        }
    }

    // 自定义 uniform：Variant -> Vec4/Mat4
    if (material)
    {
        const auto shaderParams = material->GetShaderParameters();
        for (auto it = shaderParams.Begin(); it != shaderParams.End(); ++it)
            SetUniformByVariant(it->second_.name_.CString(), it->second_.value_);
    }

    // 程序选择（与 UI 路径一致）
    unsigned short programIdx = ui_.programDiff;
    if (primaryTex)
    {
        const bool isAlphaTex = primaryTex->GetFormat() == Graphics::GetAlphaFormat();
        if (isAlphaTex && ui_.programAlpha != bgfx::kInvalidHandle)
            programIdx = ui_.programAlpha;
        else
        {
            const bool useMask = (lastBlendMode_ != BLEND_ALPHA && lastBlendMode_ != BLEND_ADDALPHA && lastBlendMode_ != BLEND_PREMULALPHA);
            if (!isAlphaTex && useMask && ui_.programMask != bgfx::kInvalidHandle)
                programIdx = ui_.programMask;
        }
    }

    if (numVertices > 0xFFFF)
        return false;
    bgfx::TransientIndexBuffer tib;
    if (bgfx::getAvailTransientIndexBuffer((uint32_t)numVertices) < (uint32_t)numVertices)
        return false;
    bgfx::allocTransientIndexBuffer(&tib, (uint32_t)numVertices);
    {
        auto* idst = reinterpret_cast<uint16_t*>(tib.data);
        for (int i = 0; i < numVertices; ++i)
            idst[i] = (uint16_t)i;
    }

    bgfx::ProgramHandle ph; ph.idx = programIdx;
    bgfx::setState((state_ | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A));
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::submit(0, ph);
    return true;
#else
    (void)vertices; (void)numVertices; (void)material; (void)cache; (void)mvp; return false;
#endif
}

bool GraphicsBgfx::UpdateTextureRegion(Texture2D* tex, int x, int y, int width, int height, const void* data, unsigned level)
{
#ifdef URHO3D_BGFX
    if (!initialized_ || !tex || !data || width <= 0 || height <= 0)
        return false;

    // 查找或创建纹理句柄
    unsigned short ti = bgfx::kInvalidHandle;
    auto it = textureCache_.find(tex);
    if (it != textureCache_.end())
        ti = it->second;
    if (ti == bgfx::kInvalidHandle)
    {
        // 分配空纹理存储（无初始数据），便于后续 updateTexture2D 上传
        uint64_t tflags = 0;
#ifdef BGFX_TEXTURE_SRGB
        if (tex->GetSRGB()) tflags |= BGFX_TEXTURE_SRGB;
#endif
        const bgfx::TextureFormat::Enum fmt = bgfx::TextureFormat::RGBA8; // 统一使用 RGBA8 存储
        bgfx::TextureHandle th = bgfx::createTexture2D((uint16_t)tex->GetWidth(), (uint16_t)tex->GetHeight(), false, 1, fmt, tflags, nullptr);
        if (!bgfx::isValid(th))
            return false;
        textureCache_[tex] = th.idx;
        ti = th.idx;
    }

    bgfx::TextureHandle th; th.idx = ti;
    if (!bgfx::isValid(th))
        return false;

    // 若原纹理是 A8，需要将 A8 扩展为 RGBA8 再上传
    const bool isAlphaOnly = (tex->GetFormat() == Graphics::GetAlphaFormat());
    uint32_t pitch = (uint32_t)width * 4u;
    std::vector<unsigned char> rgbaBuf;
    const void* src = data;
    if (isAlphaOnly)
    {
        const unsigned char* a8 = reinterpret_cast<const unsigned char*>(data);
        rgbaBuf.resize((size_t)width * (size_t)height * 4u);
        for (int i = 0; i < height; ++i)
        {
            for (int j = 0; j < width; ++j)
            {
                const int idx = i * width + j;
                const unsigned char a = a8[idx];
                rgbaBuf[idx*4 + 0] = 0xFF;
                rgbaBuf[idx*4 + 1] = 0xFF;
                rgbaBuf[idx*4 + 2] = 0xFF;
                rgbaBuf[idx*4 + 3] = a;
            }
        }
        src = rgbaBuf.data();
    }

    const bgfx::Memory* mem = bgfx::copy(src, (uint32_t)height * pitch);
    bgfx::updateTexture2D(th, 0 /*layer*/, (uint8_t)level, (uint16_t)x, (uint16_t)y, (uint16_t)width, (uint16_t)height, mem, pitch);
    return true;
#else
    (void)tex; (void)x; (void)y; (void)width; (void)height; (void)data; (void)level; return false;
#endif
}

bool GraphicsBgfx::SetFrameBuffer(Texture2D* color, Texture2D* depth)
{
#ifdef URHO3D_BGFX
    // 构造 key 并查询缓存
    FBKey key{color, depth};
    auto it = fbCache_.find(key);
    if (it != fbCache_.end())
    {
        bgfx::FrameBufferHandle fh; fh.idx = it->second;
        if (bgfx::isValid(fh))
        {
            bgfx::setViewFrameBuffer(0, fh);
            return true;
        }
    }

    // 构建附件
    bgfx::Attachment atts[2];
    uint8_t num=0;
    int texW = 0, texH = 0;
    if (color)
    {
        unsigned short ti = GetOrCreateTexture(color, nullptr);
        if (ti != bgfx::kInvalidHandle)
        {
            atts[num].init(bgfx::TextureHandle{ti});
            ++num;
            texW = color->GetWidth();
            texH = color->GetHeight();
        }
    }
    if (depth)
    {
        unsigned short ti = GetOrCreateTexture(depth, nullptr);
        if (ti != bgfx::kInvalidHandle)
        {
            // 检查尺寸一致性；若不一致则忽略 depth 并给出日志
            if (texW != 0 && (depth->GetWidth() != (unsigned)texW || depth->GetHeight() != (unsigned)texH))
            {
                URHO3D_LOGWARNINGF("BGFX SetFrameBuffer: depth attachment size %ux%u mismatches color %dx%d, ignoring depth",
                    depth->GetWidth(), depth->GetHeight(), texW, texH);
            }
            else
            {
                if (texW == 0) { texW = depth->GetWidth(); texH = depth->GetHeight(); }
                atts[num].init(bgfx::TextureHandle{ti});
                ++num;
            }
        }
    }
    if (num==0)
    {
        URHO3D_LOGERROR("SetFrameBuffer called with no valid attachments");
        return false;
    }

    bgfx::FrameBufferHandle fh = bgfx::createFrameBuffer(num, atts, false);
    if (!bgfx::isValid(fh))
        return false;
    fbCache_[key] = fh.idx;
    bgfx::setViewFrameBuffer(0, fh);
    // 若未显式设置过视口，这里以目标尺寸更新 view 0 的 rect（UI 调用通常随后会覆盖）
    if (texW > 0 && texH > 0)
        bgfx::setViewRect(0, 0, 0, (uint16_t)texW, (uint16_t)texH);
    return true;
#else
    (void)color; (void)depth; return false;
#endif
}

bool GraphicsBgfx::ResetFrameBuffer()
{
#ifdef URHO3D_BGFX
    bgfx::setViewFrameBuffer(0, BGFX_INVALID_HANDLE);
    // 回到默认 backbuffer 尺寸
    bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(width_), static_cast<uint16_t>(height_));
    return true;
#else
    return false;
#endif
}
} // namespace Urho3D
