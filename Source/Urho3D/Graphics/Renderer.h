// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

/// \file

#pragma once

#include "../Container/HashSet.h"
#include "../Core/Mutex.h"
#include "../Graphics/Batch.h"
#include "../Graphics/Drawable.h"
#include "../Graphics/Viewport.h"
#include "../Math/Color.h"

namespace Urho3D
{

class Geometry;
class Drawable;
class Light;
class Material;
class Pass;
class Technique;
class Octree;
class Graphics;
class RenderPath;
class RenderSurface;
class ResourceCache;
class Scene;
class Technique;
class Texture;
class Texture2D;
class View;
class Zone;
struct BatchQueue;

static const int SHADOW_MIN_PIXELS = 64;
static const int INSTANCING_BUFFER_DEFAULT_SIZE = 1024;

/// Light vertex shader variations. (2D-only: minimal set)
enum LightVSVariation
{
    LVS_DIR = 0,
    // 2D-only: 移除其他3D光照变体，但保留占位符避免编译错误
    LVS_SPOT = -1,         // 无效值
    LVS_POINT = -1,        // 无效值
    LVS_SHADOW = -1,       // 无效值
    LVS_SHADOWNORMALOFFSET = -1, // 无效值
    MAX_LIGHT_VS_VARIATIONS = 1
};

/// Per-vertex light vertex shader variations. (2D-only: minimal set)  
enum VertexLightVSVariation
{
    VLVS_NOLIGHTS = 0,
    // 2D-only: 移除多光源变体
    MAX_VERTEXLIGHT_VS_VARIATIONS = 1
};

/// Light pixel shader variations. (2D-only: minimal set)
enum LightPSVariation
{
    LPS_NONE = 0,
    // 2D-only: 移除其他3D光照变体，但保留占位符避免编译错误
    LPS_SPEC = -1,         // 无效值
    LPS_SHADOW = -1,       // 无效值
    LPS_SPOT = -1,         // 无效值
    LPS_POINT = -1,        // 无效值
    LPS_POINTMASK = -1,    // 无效值
    MAX_LIGHT_PS_VARIATIONS = 1
};

/// Deferred light volume vertex shader variations.
enum DeferredLightVSVariation
{
    DLVS_NONE = 0,
    DLVS_DIR,
    DLVS_ORTHO,
    DLVS_ORTHODIR,
    MAX_DEFERRED_LIGHT_VS_VARIATIONS
};

/// Deferred light volume pixels shader variations.
enum DeferredLightPSVariation
{
    DLPS_NONE = 0,
    DLPS_SPOT,
    DLPS_POINT,
    DLPS_POINTMASK,
    DLPS_SPEC,
    DLPS_SPOTSPEC,
    DLPS_POINTSPEC,
    DLPS_POINTMASKSPEC,
    DLPS_SHADOW,
    DLPS_SPOTSHADOW,
    DLPS_POINTSHADOW,
    DLPS_POINTMASKSHADOW,
    DLPS_SHADOWSPEC,
    DLPS_SPOTSHADOWSPEC,
    DLPS_POINTSHADOWSPEC,
    DLPS_POINTMASKSHADOWSPEC,
    DLPS_SHADOWNORMALOFFSET,
    DLPS_SPOTSHADOWNORMALOFFSET,
    DLPS_POINTSHADOWNORMALOFFSET,
    DLPS_POINTMASKSHADOWNORMALOFFSET,
    DLPS_SHADOWSPECNORMALOFFSET,
    DLPS_SPOTSHADOWSPECNORMALOFFSET,
    DLPS_POINTSHADOWSPECNORMALOFFSET,
    DLPS_POINTMASKSHADOWSPECNORMALOFFSET,
    DLPS_ORTHO,
    DLPS_ORTHOSPOT,
    DLPS_ORTHOPOINT,
    DLPS_ORTHOPOINTMASK,
    DLPS_ORTHOSPEC,
    DLPS_ORTHOSPOTSPEC,
    DLPS_ORTHOPOINTSPEC,
    DLPS_ORTHOPOINTMASKSPEC,
    DLPS_ORTHOSHADOW,
    DLPS_ORTHOSPOTSHADOW,
    DLPS_ORTHOPOINTSHADOW,
    DLPS_ORTHOPOINTMASKSHADOW,
    DLPS_ORTHOSHADOWSPEC,
    DLPS_ORTHOSPOTSHADOWSPEC,
    DLPS_ORTHOPOINTSHADOWSPEC,
    DLPS_ORTHOPOINTMASKSHADOWSPEC,
    DLPS_ORTHOSHADOWNORMALOFFSET,
    DLPS_ORTHOSPOTSHADOWNORMALOFFSET,
    DLPS_ORTHOPOINTSHADOWNORMALOFFSET,
    DLPS_ORTHOPOINTMASKSHADOWNORMALOFFSET,
    DLPS_ORTHOSHADOWSPECNORMALOFFSET,
    DLPS_ORTHOSPOTSHADOWSPECNORMALOFFSET,
    DLPS_ORTHOPOINTSHADOWSPECNORMALOFFSET,
    DLPS_ORTHOPOINTMASKSHADOWSPECNORMALOFFSET,
    MAX_DEFERRED_LIGHT_PS_VARIATIONS
};

/// High-level rendering subsystem. Manages drawing of 3D views.
class URHO3D_API Renderer : public Object
{
    URHO3D_OBJECT(Renderer, Object);

public:
    using ShadowMapFilter = void(Object::*)(View* view, Texture2D* shadowMap, float blurScale);

    /// Construct.
    explicit Renderer(Context* context);
    /// Destruct.
    ~Renderer() override;

    /// Set number of backbuffer viewports to render.
    /// @property
    void SetNumViewports(i32 num);
    /// Set a backbuffer viewport.
    /// @property{set_viewports}
    void SetViewport(i32 index, Viewport* viewport);
    /// Set default renderpath.
    /// @property
    void SetDefaultRenderPath(RenderPath* renderPath);
    /// Set default renderpath from an XML file.
    void SetDefaultRenderPath(XMLFile* xmlFile);
    /// Set default non-textured material technique.
    /// @property
    void SetDefaultTechnique(Technique* technique);
    /// Set HDR rendering on/off.
    /// @property{set_hdrRendering}
    void SetHDRRendering(bool enable);
    /// Set specular lighting on/off.
    /// @property
    void SetSpecularLighting(bool enable);
    /// Set default texture max anisotropy level.
    /// @property
    void SetTextureAnisotropy(int level);
    /// Set default texture filtering.
    /// @property
    void SetTextureFilterMode(TextureFilterMode mode);
    /// Set texture quality level. See the QUALITY constants in GraphicsDefs.h.
    /// @property
    void SetTextureQuality(MaterialQuality quality);
    /// Set material quality level. See the QUALITY constants in GraphicsDefs.h.
    /// @property
    void SetMaterialQuality(MaterialQuality quality);
    /// Set shadows on/off.
    /// @property
    void SetDrawShadows(bool enable);
    /// Set shadow map resolution.
    /// @property
    void SetShadowMapSize(int size);
    /// Set shadow quality mode. See the SHADOWQUALITY enum in GraphicsDefs.h.
    /// @property
    void SetShadowQuality(ShadowQuality quality);
    /// Set shadow softness, only works when SHADOWQUALITY_BLUR_VSM is used.
    /// @property
    void SetShadowSoftness(float shadowSoftness);
    /// Set shadow parameters when VSM is used, they help to reduce light bleeding. LightBleeding must be in [0, 1].
    void SetVSMShadowParameters(float minVariance, float lightBleedingReduction);
    /// Set VSM shadow map multisampling level. Default 1 (no multisampling).
    /// @property{set_vsmMultiSample}
    void SetVSMMultiSample(int multiSample);
    /// Set post processing filter to the shadow map.
    /// @nobind
    void SetShadowMapFilter(Object* instance, ShadowMapFilter functionPtr);
    /// Set reuse of shadow maps. Default is true. If disabled, also transparent geometry can be shadowed.
    /// @property
    void SetReuseShadowMaps(bool enable);
    /// Set maximum number of shadow maps created for one resolution. Only has effect if reuse of shadow maps is disabled.
    /// @property
    void SetMaxShadowMaps(int shadowMaps);
    /// Set dynamic instancing on/off. When on (default), drawables using the same static-type geometry and material will be automatically combined to an instanced draw call.
    /// @property
    void SetDynamicInstancing(bool enable);
    /// Set number of extra instancing buffer elements. Default is 0. Extra 4-vectors are available through TEXCOORD7 and further.
    /// @property
    void SetNumExtraInstancingBufferElements(int elements);
    /// Set minimum number of instances required in a batch group to render as instanced.
    /// @property
    void SetMinInstances(int instances);
    /// Set maximum number of sorted instances per batch group. If exceeded, instances are rendered unsorted.
    /// @property
    void SetMaxSortedInstances(int instances);
    /// Set shadow depth bias multiplier for mobile platforms to counteract possible worse shadow map precision. Default 1.0 (no effect).
    /// @property
    void SetMobileShadowBiasMul(float mul);
    /// Set shadow depth bias addition for mobile platforms to counteract possible worse shadow map precision. Default 0.0 (no effect).
    /// @property
    void SetMobileShadowBiasAdd(float add);
    /// Set shadow normal offset multiplier for mobile platforms to counteract possible worse shadow map precision. Default 1.0 (no effect).
    /// @property
    void SetMobileNormalOffsetMul(float mul);
    /// Force reload of shaders.
    void ReloadShaders();

    /// Apply post processing filter to the shadow map. Called by View.
    void ApplyShadowMapFilter(View* view, Texture2D* shadowMap, float blurScale);

    /// Return number of backbuffer viewports.
    /// @property
    i32 GetNumViewports() const { return viewports_.Size(); }

    /// Return backbuffer viewport by index.
    /// @property{get_viewports}
    Viewport* GetViewport(i32 index) const;
    /// Return nth backbuffer viewport associated to a scene. Index 0 returns the first.
    Viewport* GetViewportForScene(Scene* scene, i32 index) const;
    /// Return default renderpath.
    /// @property
    RenderPath* GetDefaultRenderPath() const;
    /// Return default non-textured material technique.
    /// @property
    Technique* GetDefaultTechnique() const;

    /// Return whether HDR rendering is enabled.
    /// @property{get_hdrRendering}
    bool GetHDRRendering() const { return hdrRendering_; }

    /// Return whether specular lighting is enabled.
    /// @property
    bool GetSpecularLighting() const { return specularLighting_; }

    /// Return whether drawing shadows is enabled.
    /// @property
    bool GetDrawShadows() const { return drawShadows_; }

    /// Return default texture max. anisotropy level.
    /// @property
    int GetTextureAnisotropy() const { return textureAnisotropy_; }

    /// Return default texture filtering mode.
    /// @property
    TextureFilterMode GetTextureFilterMode() const { return textureFilterMode_; }

    /// Return texture quality level.
    /// @property
    MaterialQuality GetTextureQuality() const { return textureQuality_; }

    /// Return material quality level.
    /// @property
    MaterialQuality GetMaterialQuality() const { return materialQuality_; }

    /// Return shadow map resolution.
    /// @property
    int GetShadowMapSize() const { return shadowMapSize_; }

    /// Return shadow quality.
    /// @property
    ShadowQuality GetShadowQuality() const { return shadowQuality_; }

    /// Return shadow softness.
    /// @property
    float GetShadowSoftness() const { return shadowSoftness_; }

    /// Return VSM shadow parameters.
    /// @property{get_vsmShadowParameters}
    Vector2 GetVSMShadowParameters() const { return vsmShadowParams_; };

    /// Return VSM shadow multisample level.
    /// @property{get_vsmMultiSample}
    int GetVSMMultiSample() const { return vsmMultiSample_; }

    /// Return whether shadow maps are reused.
    /// @property
    bool GetReuseShadowMaps() const { return reuseShadowMaps_; }

    /// Return maximum number of shadow maps per resolution.
    /// @property
    int GetMaxShadowMaps() const { return maxShadowMaps_; }

    /// Return whether dynamic instancing is in use.
    /// @property
    bool GetDynamicInstancing() const { return dynamicInstancing_; }

    /// Return number of extra instancing buffer elements.
    /// @property
    int GetNumExtraInstancingBufferElements() const { return numExtraInstancingBufferElements_; };

    /// Return minimum number of instances required in a batch group to render as instanced.
    /// @property
    int GetMinInstances() const { return minInstances_; }

    /// Return maximum number of sorted instances per batch group.
    /// @property
    int GetMaxSortedInstances() const { return maxSortedInstances_; }

    /// Return maximum number of occluder triangles. (2D-only: no occlusion, return 0)
    /// @property
    int GetMaxOccluderTriangles() const { return 0; }

    /// Return occlusion buffer width. (2D-only: no occlusion, return 0)
    /// @property
    int GetOcclusionBufferSize() const { return 0; }

    /// Return occluder screen size threshold. (2D-only: no occlusion, return 0)
    /// @property
    float GetOccluderSizeThreshold() const { return 0.0f; }

    /// Return whether occlusion rendering is threaded. (2D-only: no)
    /// @property
    bool GetThreadedOcclusion() const { return false; }

    /// Return shadow depth bias multiplier for mobile platforms.
    /// @property
    float GetMobileShadowBiasMul() const { return mobileShadowBiasMul_; }

    /// Return shadow depth bias addition for mobile platforms.
    /// @property
    float GetMobileShadowBiasAdd() const { return mobileShadowBiasAdd_; }

    /// Return shadow normal offset multiplier for mobile platforms.
    /// @property
    float GetMobileNormalOffsetMul() const { return mobileNormalOffsetMul_; }

    /// Return number of views rendered.
    /// @property
    i32 GetNumViews() const { return views_.Size(); }

    /// Return number of primitives rendered.
    /// @property
    i32 GetNumPrimitives() const { return numPrimitives_; }

    /// Return number of batches rendered.
    /// @property
    i32 GetNumBatches() const { return numBatches_; }

    /// Return number of geometries rendered.
    /// @property
    i32 GetNumGeometries(bool allViews = false) const;
    /// Return number of lights rendered.
    /// @property
    i32 GetNumLights(bool allViews = false) const;
    /// Return number of shadow maps rendered.
    /// @property
    i32 GetNumShadowMaps(bool allViews = false) const;
    /// Return number of occluders rendered.
    /// @property
    i32 GetNumOccluders(bool allViews = false) const;

    /// Return the default zone.
    /// @property
    Zone* GetDefaultZone() const { return defaultZone_; }

    /// Return the default material.
    /// @property
    Material* GetDefaultMaterial() const { return defaultMaterial_; }

    /// Return the default range attenuation texture.
    /// @property
    Texture2D* GetDefaultLightRamp() const { return defaultLightRamp_; }

    /// Return the default spotlight attenuation texture.
    /// @property
    Texture2D* GetDefaultLightSpot() const { return defaultLightSpot_; }


    /// Return the instancing vertex buffer.
    VertexBuffer* GetInstancingBuffer() const { return dynamicInstancing_ ? instancingBuffer_.Get() : nullptr; }

    /// Return the frame update parameters.
    const FrameInfo& GetFrameInfo() const { return frame_; }

    /// Update for rendering. Called by HandleRenderUpdate().
    void Update(float timeStep);
    /// Render. Called by Engine.
    void Render();
    /// Add debug geometry to the debug renderer.
    void DrawDebugGeometry(bool depthTest);
    /// Queue a render surface's viewports for rendering. Called by the surface, or by View.
    void QueueRenderSurface(RenderSurface* renderTarget);
    /// Queue a viewport for rendering. Null surface means backbuffer.
    void QueueViewport(RenderSurface* renderTarget, Viewport* viewport);

    /// Return volume geometry for a light.
    Geometry* GetLightGeometry(Light* light);
    /// Return quad geometry used in postprocessing.
    Geometry* GetQuadGeometry();
    /// Allocate a shadow map. If shadow map reuse is disabled, a different map is returned each time.
    Texture2D* GetShadowMap(Light* light, Camera* camera, i32 viewWidth, i32 viewHeight);
    /// Allocate a rendertarget or depth-stencil texture for deferred rendering or postprocessing. Should only be called during actual rendering, not before.
    Texture* GetScreenBuffer
        (int width, int height, unsigned format, int multiSample, bool autoResolve, bool cubemap, bool filtered, bool srgb, hash32 persistentKey = 0);
    /// Allocate a depth-stencil surface that does not need to be readable. Should only be called during actual rendering, not before.
    RenderSurface* GetDepthStencil(int width, int height, int multiSample, bool autoResolve);
    /// Allocate a temporary shadow camera and a scene node for it. Is thread-safe.
    Camera* GetShadowCamera();
    /// Mark a view as prepared by the specified culling camera.
    void StorePreparedView(View* view, Camera* camera);
    /// Return a prepared view if exists for the specified camera. Used to avoid duplicate view preparation CPU work.
    View* GetPreparedView(Camera* camera);
    /// Choose shaders for a forward rendering batch. The related batch queue is provided in case it has extra shader compilation defines.
    void SetBatchShaders(Batch& batch, Technique* tech, bool allowShadows, const BatchQueue& queue);
    /// Choose shaders for a deferred light volume batch.
    void SetLightVolumeBatchShaders
        (Batch& batch, Camera* camera, const String& vsName, const String& psName, const String& vsDefines, const String& psDefines);
    /// Set cull mode while taking possible projection flipping into account.
    void SetCullMode(CullMode mode, Camera* camera);
    /// Ensure sufficient size of the instancing vertex buffer. Return true if successful.
    bool ResizeInstancingBuffer(i32 numInstances);
    /// Optimize a light by scissor rectangle.
    void OptimizeLightByScissor(Light* light, Camera* camera);
    /// Optimize a light by marking it to the stencil buffer and setting a stencil test.
    void OptimizeLightByStencil(Light* light, Camera* camera);
    /// Return a scissor rectangle for a light.
    const Rect& GetLightScissor(Light* light, Camera* camera);

    /// Return a view or its source view if it uses one. Used internally for render statistics.
    static View* GetActualView(View* view);

private:
    /// Initialize when screen mode initially set.
    void Initialize();
    /// Reload shaders.
    void LoadShaders();
    /// Reload shaders for a material pass. The related batch queue is provided in case it has extra shader compilation defines.
    void LoadPassShaders(Pass* pass, Vector<SharedPtr<ShaderVariation>>& vertexShaders, Vector<SharedPtr<ShaderVariation>>& pixelShaders, const BatchQueue& queue);
    /// Release shaders used in materials.
    void ReleaseMaterialShaders();
    /// Reload textures.
    void ReloadTextures();
    /// Create light volume geometries.
    void CreateGeometries();
    /// Create instancing vertex buffer.
    void CreateInstancingBuffer();
    /// Update a queued viewport for rendering.
    void UpdateQueuedViewport(i32 index);
    /// Prepare for rendering of a new view.
    void PrepareViewRender();
    /// Remove unused occlusion and screen buffers.
    void RemoveUnusedBuffers();
    /// Reset shadow map allocation counts.
    void ResetShadowMapAllocations();
    /// Reset screem buffer allocation counts.
    void ResetScreenBufferAllocations();
    /// Remove all shadow maps. Called when global shadow map resolution or format is changed.
    void ResetShadowMaps();
    /// Remove all occlusion and screen buffers.
    void ResetBuffers();
    /// Find variations for shadow shaders.
    String GetShadowVariations() const;
    /// Handle screen mode event.
    void HandleScreenMode(StringHash eventType, VariantMap& eventData);
    /// Handle render update event.
    void HandleRenderUpdate(StringHash eventType, VariantMap& eventData);
    /// Blur the shadow map.
    void BlurShadowMap(View* view, Texture2D* shadowMap, float blurScale);

    /// Graphics subsystem.
    WeakPtr<Graphics> graphics_;
    /// Default renderpath.
    SharedPtr<RenderPath> defaultRenderPath_;
    /// Default non-textured material technique.
    SharedPtr<Technique> defaultTechnique_;
    /// Default zone.
    SharedPtr<Zone> defaultZone_;
    /// Directional light quad geometry.
    SharedPtr<Geometry> dirLightGeometry_;
    /// Instance stream vertex buffer.
    SharedPtr<VertexBuffer> instancingBuffer_;
    /// Default material.
    SharedPtr<Material> defaultMaterial_;
    /// Default range attenuation texture.
    SharedPtr<Texture2D> defaultLightRamp_;
    /// Default spotlight attenuation texture.
    SharedPtr<Texture2D> defaultLightSpot_;
    /// Reusable scene nodes with shadow camera components.
    Vector<SharedPtr<Node>> shadowCameraNodes_;
    /// (2D-only) 不再维护遮挡缓冲。
    /// Shadow maps by resolution.
    HashMap<int, Vector<SharedPtr<Texture2D>>> shadowMaps_;
    /// Shadow map dummy color buffers by resolution.
    HashMap<int, SharedPtr<Texture2D>> colorShadowMaps_;
    /// Shadow map allocations by resolution.
    HashMap<int, Vector<Light*>> shadowMapAllocations_;
    /// Instance of shadow map filter.
    Object* shadowMapFilterInstance_{};
    /// Function pointer of shadow map filter.
    ShadowMapFilter shadowMapFilter_{};
    /// Screen buffers by resolution and format.
    HashMap<hash64, Vector<SharedPtr<Texture>>> screenBuffers_;
    /// Current screen buffer allocations by resolution and format.
    HashMap<hash64, i32> screenBufferAllocations_;
    /// Cache for light scissor queries.
    HashMap<Pair<Light*, Camera*>, Rect> lightScissorCache_;
    /// Backbuffer viewports.
    Vector<SharedPtr<Viewport>> viewports_;
    /// Render surface viewports queued for update.
    Vector<Pair<WeakPtr<RenderSurface>, WeakPtr<Viewport>>> queuedViewports_;
    /// Views that have been processed this frame.
    Vector<WeakPtr<View>> views_;
    /// Prepared views by culling camera.
    HashMap<Camera*, WeakPtr<View>> preparedViews_;
    /// Octrees that have been updated during the frame.
    HashSet<Octree*> updatedOctrees_;
    /// Techniques for which missing shader error has been displayed.
    HashSet<Technique*> shaderErrorDisplayed_;
    /// Mutex for shadow camera allocation.
    Mutex rendererMutex_;
    /// Current variation names for deferred light volume shaders.
    Vector<String> deferredLightPSVariations_;
    /// Frame info for rendering.
    FrameInfo frame_;
    /// Texture anisotropy level.
    int textureAnisotropy_{4};
    /// Texture filtering mode.
    TextureFilterMode textureFilterMode_{FILTER_TRILINEAR};
    /// Texture quality level.
    MaterialQuality textureQuality_{QUALITY_HIGH};
    /// Material quality level.
    MaterialQuality materialQuality_{QUALITY_HIGH};
    /// Shadow map resolution.
    int shadowMapSize_{1024};
    /// Shadow quality.
    ShadowQuality shadowQuality_{SHADOWQUALITY_PCF_16BIT};
    /// Shadow softness, only works when SHADOWQUALITY_BLUR_VSM is used.
    float shadowSoftness_{1.0f};
    /// Shadow parameters when VSM is used, they help to reduce light bleeding.
    Vector2 vsmShadowParams_{0.0000001f, 0.9f};
    /// Multisample level for VSM shadows.
    int vsmMultiSample_{1};
    /// Maximum number of shadow maps per resolution.
    int maxShadowMaps_{1};
    /// Minimum number of instances required in a batch group to render as instanced.
    int minInstances_{2};
    /// Maximum sorted instances per batch group.
    int maxSortedInstances_{1000};
    /// (2D-only) 移除遮挡缓冲相关设置。
    /// Mobile platform shadow depth bias multiplier.
    float mobileShadowBiasMul_{1.0f};
    /// Mobile platform shadow depth bias addition.
    float mobileShadowBiasAdd_{};
    /// Mobile platform shadow normal offset multiplier.
    float mobileNormalOffsetMul_{1.0f};
    /// (2D-only) 无遮挡缓冲计数。
    /// Number of temporary shadow cameras in use.
    i32 numShadowCameras_{};
    /// Number of primitives (3D geometry only).
    i32 numPrimitives_{};
    /// Number of batches (3D geometry only).
    i32 numBatches_{};
    /// Frame number on which shaders last changed.
    i32 shadersChangedFrameNumber_{NINDEX};
    /// Current stencil value for light optimization.
    unsigned char lightStencilValue_{};
    /// HDR rendering flag.
    bool hdrRendering_{};
    /// Specular lighting flag.
    bool specularLighting_{true};
    /// Draw shadows flag.
    bool drawShadows_{true};
    /// Shadow map reuse flag.
    bool reuseShadowMaps_{true};
    /// Dynamic instancing flag.
    bool dynamicInstancing_{true};
    /// Number of extra instancing data elements.
    int numExtraInstancingBufferElements_{};
    /// (2D-only) 无遮挡缓冲线程标志。
    /// Shaders need reloading flag.
    bool shadersDirty_{true};
    /// Initialized flag.
    bool initialized_{};
    /// Flag for views needing reset.
    bool resetViews_{};
};

}
