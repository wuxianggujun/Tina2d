// 版权声明同项目整体许可证
//
// 本文件为 bgfx 渲染后端的最小封装骨架。
// 目标：在不破坏现有 OpenGL/D3D 渲染路径的前提下，引入 bgfx 依赖并提供统一接口，
// 后续可逐步将 Graphics/Renderer 等调用切换到该封装。

#pragma once

#include "../GraphicsAPI/GraphicsDefs.h"
#include "../Math/Rect.h"
#include "../Math/Color.h"

namespace Urho3D
{

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

private:
    void ApplyState();

private:
    bool initialized_{};
    unsigned width_{};
    unsigned height_{};
    uint64_t state_{};     // bgfx 渲染状态位
    bool scissorEnabled_{};
    IntRect scissorRect_{};
};

} // namespace Urho3D
