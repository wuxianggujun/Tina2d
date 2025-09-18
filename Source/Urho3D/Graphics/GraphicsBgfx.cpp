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

#include "../Resource/ResourceCache.h"
#include "../IO/File.h"
#include "../GraphicsAPI/Texture2D.h"
#include "../Resource/Image.h"
#include "../Graphics/Graphics.h"
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
    if (color)
    {
        unsigned short ti = GetOrCreateTexture(color, nullptr);
        if (ti != bgfx::kInvalidHandle)
        {
            atts[num].init(bgfx::TextureHandle{ti});
            ++num;
        }
    }
    if (depth)
    {
        unsigned short ti = GetOrCreateTexture(depth, nullptr);
        if (ti != bgfx::kInvalidHandle)
        {
            atts[num].init(bgfx::TextureHandle{ti});
            ++num;
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
    return true;
#else
    (void)color; (void)depth; return false;
#endif
}

bool GraphicsBgfx::ResetFrameBuffer()
{
#ifdef URHO3D_BGFX
    bgfx::setViewFrameBuffer(0, BGFX_INVALID_HANDLE);
    return true;
#else
    return false;
#endif
}
} // namespace Urho3D
