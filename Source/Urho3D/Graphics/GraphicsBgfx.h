// 版权声明同项目整体许可证
//
// 本文件为 bgfx 渲染后端的最小封装骨架。
// 目标：在不破坏现有 OpenGL/D3D 渲染路径的前提下，引入 bgfx 依赖并提供统一接口，
// 后续可逐步将 Graphics/Renderer 等调用切换到该封装。

#pragma once

#include "../GraphicsAPI/GraphicsDefs.h"
#include "../Math/Rect.h"
#include "../Math/Color.h"
#include "../Math/Matrix4.h"
#include <unordered_map>

namespace Urho3D
{

class Texture2D;
class ResourceCache;

/// bgfx 渲染器薄封装（最小骨架）。
/// 说明：仅提供初始化/帧提交等基础能力，具体渲染管线、资源管理与 Urho3D 类型映射将在后续阶段逐步完善。
class GraphicsBgfx
{
public:
    GraphicsBgfx();
    ~GraphicsBgfx();

    /// 使用原生窗口句柄进行初始化（SDL/Win32/… 均可传入底层原生句柄）。
    /// width/height 初始分辨率，非严格要求，后续可 Reset。
    bool Initialize(void* nativeWindowHandle, unsigned width, unsigned height, void* nativeDisplayHandle = nullptr);

    /// 通过 SDL_Window* 初始化，内部会提取 nwh/ndt（仅在部分平台可用）。
    bool InitializeFromSDL(void* sdlWindow, unsigned width, unsigned height);

    /// 关闭并释放 bgfx 资源。
    void Shutdown();

    /// 视口重置（窗口尺寸变化时调用）。
    void Reset(unsigned width, unsigned height);

    /// 帧开始：可做清屏等。
    void BeginFrame();

    /// 帧结束：提交并翻转。
    void EndFrame();

    /// 设置视口矩形（映射到 view 0）。
    void SetViewport(const IntRect& rect);

    /// 清屏：flags 使用 Urho3D 的 CLEAR_* 标志位。
    void Clear(ClearTargetFlags flags, const Color& color, float depth, u32 stencil);

    // 渲染状态设置（2D 需要的最小子集）
    void SetBlendMode(BlendMode mode, bool alphaToCoverage);
    void SetColorWrite(bool enable);
    void SetCullMode(CullMode mode);
    void SetDepthTest(CompareMode mode);
    void SetDepthWrite(bool enable);
    void SetScissor(bool enable, const IntRect& rect);

    /// 是否已完成初始化。
    bool IsInitialized() const { return initialized_; }

    /// 载入 UI 所需着色器程序（vs_ui + fs_ui_diff/fs_ui_alpha），从资源系统读取 BGFX 编译产物。
    /// 需要 ResourceCache 可用（CoreData/Shaders/BGFX）。
    bool LoadUIPrograms(class ResourceCache* cache);
    /// 使用最小示例程序绘制一个测试四边形（验证渲染/着色器/管线）。
    void DebugDrawHello();

    // 批量绘制（供 SpriteBatch 调用）
    bool DrawQuads(const void* qvertices /*SpriteBatchBase::QVertex[]*/, int numVertices, Texture2D* texture, ResourceCache* cache, const Matrix4& mvp);
    bool DrawTriangles(const void* tvertices /*SpriteBatchBase::TVertex[]*/, int numVertices, ResourceCache* cache, const Matrix4& mvp);
    // UI: 直接从 UI 顶点浮点数组绘制三角形（pos, color, uv，按 UI_VERTEX_SIZE=6 排列）
    bool DrawUITriangles(const float* vertices, int numVertices, Texture2D* texture, ResourceCache* cache, const Matrix4& mvp);

    // 从 Image 创建 BGFX 纹理并保存到映射（供 UI 字体等路径使用）
    bool CreateTextureFromImage(Texture2D* tex, class Image* image, bool useAlpha);

    // 渲染目标与帧缓冲
    bool SetFrameBuffer(Texture2D* color, Texture2D* depth);
    bool ResetFrameBuffer();

private:
    void ApplyState();

private:
    bool initialized_{};
    unsigned width_{};
    unsigned height_{};
    uint64_t state_{};     // bgfx 渲染状态位
    bool scissorEnabled_{};
    IntRect scissorRect_{};
    BlendMode lastBlendMode_{BLEND_REPLACE};

    // 简单示例程序与资源
    struct UIHandles
    {
        unsigned short programDiff{0xFFFF};   // Basic+DIFFMAP+VERTEXCOLOR
        unsigned short programAlpha{0xFFFF};  // Basic+ALPHAMAP+VERTEXCOLOR（字体）
        unsigned short programMask{0xFFFF};   // Basic+DIFFMAP+ALPHAMASK+VERTEXCOLOR
        unsigned short u_mvp{0xFFFF};
        // s_tex 约定为 s_texColor（主采样器），s_texAlt 兼容 Basic2D 等使用 s_tex 的着色器
        unsigned short s_tex{0xFFFF};      // "s_texColor"
        unsigned short s_texAlt{0xFFFF};   // "s_tex"
        unsigned short whiteTex{0xFFFF};
        bool ready{};
    } ui_;

    // 纹理缓存：Urho3D Texture2D* -> bgfx::TextureHandle.idx
    std::unordered_map<const Texture2D*, unsigned short> textureCache_;
    unsigned short GetOrCreateTexture(Texture2D* tex, class ResourceCache* cache);

    struct FBKey
    {
        const Texture2D* color{};
        const Texture2D* depth{};
        bool operator==(const FBKey& rhs) const { return color==rhs.color && depth==rhs.depth; }
    };
    struct FBKeyHash { size_t operator()(const FBKey& k) const { return (reinterpret_cast<size_t>(k.color)>>4) ^ (reinterpret_cast<size_t>(k.depth)<<1); } };
    std::unordered_map<FBKey, unsigned short, FBKeyHash> fbCache_;
};

} // namespace Urho3D
