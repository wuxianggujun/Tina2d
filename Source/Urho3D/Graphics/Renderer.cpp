// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "../Precompiled.h"

#include "../Core/CoreEvents.h"
#include "../Core/Profiler.h"
#include "../Graphics/Camera.h"
#include "../Graphics/DebugRenderer.h"
#include "../Graphics/Geometry.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/GraphicsEvents.h"
#include "../Graphics/Material.h"
// 2D-only：不再使用遮挡缓冲，保留前向声明以满足成员声明
#include "../Graphics/OcclusionBuffer.h"
#include "../Graphics/Octree.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/RenderPath.h"
#include "../Graphics/Technique.h"
#include "../Graphics/View.h"
#include "../Graphics/Zone.h"
#include "../GraphicsAPI/GraphicsImpl.h"
#include "../GraphicsAPI/IndexBuffer.h"
#include "../GraphicsAPI/ShaderVariation.h"
#include "../GraphicsAPI/Texture2D.h"
#include "../GraphicsAPI/VertexBuffer.h"
#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/XMLFile.h"
#include "../Scene/Scene.h"

#include "../DebugNew.h"

namespace Urho3D
{

static const float dirLightVertexData[] =
{
    -1, 1, 0,
    1, 1, 0,
    1, -1, 0,
    -1, -1, 0,
};

static const unsigned short dirLightIndexData[] =
{
    0, 1, 2,
    2, 3, 0,
};

// 2D-only：移除点光/聚光体积网格数据

static const char* geometryVSVariations[] =
{
    "",
    "SKINNED ",
    "INSTANCED ",
    "BILLBOARD ",
    "DIRBILLBOARD ",
    "TRAILFACECAM ",
    "TRAILBONE "
};

static const char* lightVSVariations[] =
{
    "PERPIXEL DIRLIGHT ",
    "PERPIXEL SPOTLIGHT ",
    "PERPIXEL POINTLIGHT ",
    "PERPIXEL DIRLIGHT SHADOW ",
    "PERPIXEL SPOTLIGHT SHADOW ",
    "PERPIXEL POINTLIGHT SHADOW ",
    "PERPIXEL DIRLIGHT SHADOW NORMALOFFSET ",
    "PERPIXEL SPOTLIGHT SHADOW NORMALOFFSET ",
    "PERPIXEL POINTLIGHT SHADOW NORMALOFFSET "
};

static const char* vertexLightVSVariations[] =
{
    "",
    "NUMVERTEXLIGHTS=1 ",
    "NUMVERTEXLIGHTS=2 ",
    "NUMVERTEXLIGHTS=3 ",
    "NUMVERTEXLIGHTS=4 ",
};

static const char* deferredLightVSVariations[] =
{
    "",
    "DIRLIGHT ",
    "ORTHO ",
    "DIRLIGHT ORTHO "
};

static const char* lightPSVariations[] =
{
    "PERPIXEL DIRLIGHT ",
    "PERPIXEL SPOTLIGHT ",
    "PERPIXEL POINTLIGHT ",
    "PERPIXEL POINTLIGHT CUBEMASK ",
    "PERPIXEL DIRLIGHT SPECULAR ",
    "PERPIXEL SPOTLIGHT SPECULAR ",
    "PERPIXEL POINTLIGHT SPECULAR ",
    "PERPIXEL POINTLIGHT CUBEMASK SPECULAR ",
    "PERPIXEL DIRLIGHT SHADOW ",
    "PERPIXEL SPOTLIGHT SHADOW ",
    "PERPIXEL POINTLIGHT SHADOW ",
    "PERPIXEL POINTLIGHT CUBEMASK SHADOW ",
    "PERPIXEL DIRLIGHT SPECULAR SHADOW ",
    "PERPIXEL SPOTLIGHT SPECULAR SHADOW ",
    "PERPIXEL POINTLIGHT SPECULAR SHADOW ",
    "PERPIXEL POINTLIGHT CUBEMASK SPECULAR SHADOW ",
    "PERPIXEL DIRLIGHT SHADOW NORMALOFFSET ",
    "PERPIXEL SPOTLIGHT SHADOW NORMALOFFSET ",
    "PERPIXEL POINTLIGHT SHADOW NORMALOFFSET ",
    "PERPIXEL POINTLIGHT CUBEMASK SHADOW NORMALOFFSET ",
    "PERPIXEL DIRLIGHT SPECULAR SHADOW NORMALOFFSET ",
    "PERPIXEL SPOTLIGHT SPECULAR SHADOW NORMALOFFSET ",
    "PERPIXEL POINTLIGHT SPECULAR SHADOW NORMALOFFSET ",
    "PERPIXEL POINTLIGHT CUBEMASK SPECULAR SHADOW NORMALOFFSET "
};

static const char* heightFogVariations[] =
{
    "",
    "HEIGHTFOG "
};

static const unsigned MAX_BUFFER_AGE = 1000;

static const int MAX_EXTRA_INSTANCING_BUFFER_ELEMENTS = 4;

inline Vector<VertexElement> CreateInstancingBufferElements(unsigned numExtraElements)
{
    static const unsigned NUM_INSTANCEMATRIX_ELEMENTS = 3;
    static const unsigned FIRST_UNUSED_TEXCOORD = 4;

    Vector<VertexElement> elements;
    for (unsigned i = 0; i < NUM_INSTANCEMATRIX_ELEMENTS + numExtraElements; ++i)
        elements.Push(VertexElement(TYPE_VECTOR4, SEM_TEXCOORD, FIRST_UNUSED_TEXCOORD + i, true));
    return elements;
}

Renderer::Renderer(Context* context) :
    Object(context),
    defaultZone_(new Zone(context))
{
    SubscribeToEvent(E_SCREENMODE, URHO3D_HANDLER(Renderer, HandleScreenMode));

    // Try to initialize right now, but skip if screen mode is not yet set
    Initialize();
}

Renderer::~Renderer() = default;

void Renderer::SetNumViewports(i32 num)
{
    assert(num >= 0);
    viewports_.Resize(num);
}

void Renderer::SetViewport(i32 index, Viewport* viewport)
{
    assert(index >= 0);

    if (index >= viewports_.Size())
        viewports_.Resize(index + 1);

    viewports_[index] = viewport;
}

void Renderer::SetDefaultRenderPath(RenderPath* renderPath)
{
    if (renderPath)
        defaultRenderPath_ = renderPath;
}

void Renderer::SetDefaultRenderPath(XMLFile* xmlFile)
{
    SharedPtr<RenderPath> newRenderPath(new RenderPath());
    if (newRenderPath->Load(xmlFile))
        defaultRenderPath_ = newRenderPath;
}

void Renderer::SetDefaultTechnique(Technique* technique)
{
    defaultTechnique_ = technique;
}

void Renderer::SetHDRRendering(bool enable)
{
    hdrRendering_ = enable;
}

void Renderer::SetSpecularLighting(bool enable)
{
    specularLighting_ = enable;
}

void Renderer::SetTextureAnisotropy(int level)
{
    textureAnisotropy_ = Max(level, 1);
}

void Renderer::SetTextureFilterMode(TextureFilterMode mode)
{
    textureFilterMode_ = mode;
}

void Renderer::SetTextureQuality(MaterialQuality quality)
{
    quality = Clamp(quality, QUALITY_LOW, QUALITY_HIGH);

    if (quality != textureQuality_)
    {
        textureQuality_ = quality;
        ReloadTextures();
    }
}

void Renderer::SetMaterialQuality(MaterialQuality quality)
{
    quality = Clamp(quality, QUALITY_LOW, QUALITY_MAX);

    if (quality != materialQuality_)
    {
        materialQuality_ = quality;
        shadersDirty_ = true;
        // Reallocate views to not store eg. pass information that might be unnecessary on the new material quality level
        resetViews_ = true;
    }
}

void Renderer::SetDrawShadows(bool enable)
{
    if (!graphics_ || !graphics_->GetShadowMapFormat())
        return;

    drawShadows_ = enable;
    if (!drawShadows_)
        ResetShadowMaps();
}

void Renderer::SetShadowMapSize(int size)
{
    if (!graphics_)
        return;

    size = NextPowerOfTwo((unsigned)Max(size, SHADOW_MIN_PIXELS));
    if (size != shadowMapSize_)
    {
        shadowMapSize_ = size;
        ResetShadowMaps();
    }
}

void Renderer::SetShadowQuality(ShadowQuality quality)
{
    if (!graphics_)
        return;

    // If no hardware PCF, do not allow to select one-sample quality
    if (!graphics_->GetHardwareShadowSupport())
    {
        if (quality == SHADOWQUALITY_SIMPLE_16BIT)
            quality = SHADOWQUALITY_PCF_16BIT;

        if (quality == SHADOWQUALITY_SIMPLE_24BIT)
            quality = SHADOWQUALITY_PCF_24BIT;
    }
    // if high resolution is not allowed
    if (!graphics_->GetHiresShadowMapFormat())
    {
        if (quality == SHADOWQUALITY_SIMPLE_24BIT)
            quality = SHADOWQUALITY_SIMPLE_16BIT;

        if (quality == SHADOWQUALITY_PCF_24BIT)
            quality = SHADOWQUALITY_PCF_16BIT;
    }
    if (quality != shadowQuality_)
    {
        shadowQuality_ = quality;
        shadersDirty_ = true;
        if (quality == SHADOWQUALITY_BLUR_VSM)
            SetShadowMapFilter(this, static_cast<ShadowMapFilter>(&Renderer::BlurShadowMap));
        else
            SetShadowMapFilter(nullptr, nullptr);

        ResetShadowMaps();
    }
}

void Renderer::SetShadowSoftness(float shadowSoftness)
{
    shadowSoftness_ = Max(shadowSoftness, 0.0f);
}

void Renderer::SetVSMShadowParameters(float minVariance, float lightBleedingReduction)
{
    vsmShadowParams_.x_ = Max(minVariance, 0.0f);
    vsmShadowParams_.y_ = Clamp(lightBleedingReduction, 0.0f, 1.0f);
}

void Renderer::SetVSMMultiSample(int multiSample)
{
    multiSample = Clamp(multiSample, 1, 16);
    if (multiSample != vsmMultiSample_)
    {
        vsmMultiSample_ = multiSample;
        ResetShadowMaps();
    }
}

void Renderer::SetShadowMapFilter(Object* instance, ShadowMapFilter functionPtr)
{
    shadowMapFilterInstance_ = instance;
    shadowMapFilter_ = functionPtr;
}

void Renderer::SetReuseShadowMaps(bool enable)
{
    reuseShadowMaps_ = enable;
}

void Renderer::SetMaxShadowMaps(int shadowMaps)
{
    if (shadowMaps < 1)
        return;

    maxShadowMaps_ = shadowMaps;
    for (HashMap<int, Vector<SharedPtr<Texture2D>>>::Iterator i = shadowMaps_.Begin(); i != shadowMaps_.End(); ++i)
    {
        if ((int)i->second_.Size() > maxShadowMaps_)
            i->second_.Resize((unsigned)maxShadowMaps_);
    }
}

void Renderer::SetDynamicInstancing(bool enable)
{
    if (!instancingBuffer_)
        enable = false;

    dynamicInstancing_ = enable;
}

void Renderer::SetNumExtraInstancingBufferElements(int elements)
{
    if (numExtraInstancingBufferElements_ != elements)
    {
        numExtraInstancingBufferElements_ = Clamp(elements, 0, MAX_EXTRA_INSTANCING_BUFFER_ELEMENTS);
        CreateInstancingBuffer();
    }
}

void Renderer::SetMinInstances(int instances)
{
    minInstances_ = Max(instances, 1);
}

void Renderer::SetMaxSortedInstances(int instances)
{
    maxSortedInstances_ = Max(instances, 0);
}

void Renderer::SetMaxOccluderTriangles(int triangles)
{
    maxOccluderTriangles_ = Max(triangles, 0);
}

void Renderer::SetOcclusionBufferSize(int size)
{
    occlusionBufferSize_ = Max(size, 1);
    occlusionBuffers_.Clear();
}

void Renderer::SetMobileShadowBiasMul(float mul)
{
    mobileShadowBiasMul_ = mul;
}

void Renderer::SetMobileShadowBiasAdd(float add)
{
    mobileShadowBiasAdd_ = add;
}

void Renderer::SetMobileNormalOffsetMul(float mul)
{
    mobileNormalOffsetMul_ = mul;
}

void Renderer::SetOccluderSizeThreshold(float screenSize)
{
    occluderSizeThreshold_ = Max(screenSize, 0.0f);
}

void Renderer::SetThreadedOcclusion(bool enable)
{
    if (enable != threadedOcclusion_)
    {
        threadedOcclusion_ = enable;
        occlusionBuffers_.Clear();
    }
}

void Renderer::ReloadShaders()
{
    shadersDirty_ = true;
}

void Renderer::ApplyShadowMapFilter(View* view, Texture2D* shadowMap, float blurScale)
{
    // 2D-only：不应用阴影滤波
    (void)view; (void)shadowMap; (void)blurScale;
    return;
}


RenderPath* Renderer::GetDefaultRenderPath() const
{
    return defaultRenderPath_;
}

Technique* Renderer::GetDefaultTechnique() const
{
    // Assign default when first asked if not assigned yet
    if (!defaultTechnique_)
        const_cast<SharedPtr<Technique>& >(defaultTechnique_) = GetSubsystem<ResourceCache>()->GetResource<Technique>("Techniques/NoTextureUnlit.xml");

    return defaultTechnique_;
}

i32 Renderer::GetNumGeometries(bool allViews) const
{
    i32 numGeometries = 0;
    i32 lastView = allViews ? static_cast<i32>(views_.Size()) : 1;

    for (i32 i = 0; i < lastView; ++i)
    {
        // Use the source view's statistics if applicable
        View* view = GetActualView(views_[i]);
        if (!view)
            continue;

        numGeometries += view->GetGeometries().Size();
    }

    return numGeometries;
}

i32 Renderer::GetNumLights(bool allViews) const
{
    i32 numLights = 0;
    i32 lastView = allViews ? static_cast<i32>(views_.Size()) : 1;

    for (i32 i = 0; i < lastView; ++i)
    {
        View* view = GetActualView(views_[i]);
        if (!view)
            continue;

        numLights += view->GetLights().Size();
    }

    return numLights;
}

i32 Renderer::GetNumShadowMaps(bool allViews) const
{
    i32 numShadowMaps = 0;
    i32 lastView = allViews ? static_cast<i32>(views_.Size()) : 1;

    for (i32 i = 0; i < lastView; ++i)
    {
        View* view = GetActualView(views_[i]);
        if (!view)
            continue;

        const Vector<LightBatchQueue>& lightQueues = view->GetLightQueues();
        for (Vector<LightBatchQueue>::ConstIterator i = lightQueues.Begin(); i != lightQueues.End(); ++i)
        {
            if (i->shadowMap_)
                ++numShadowMaps;
        }
    }

    return numShadowMaps;
}

i32 Renderer::GetNumOccluders(bool allViews) const
{
    i32 numOccluders = 0;
    i32 lastView = allViews ? static_cast<i32>(views_.Size()) : 1;

    for (i32 i = 0; i < lastView; ++i)
    {
        View* view = GetActualView(views_[i]);
        if (!view)
            continue;

        numOccluders += view->GetNumActiveOccluders();
    }

    return numOccluders;
}

void Renderer::Update(float timeStep)
{
    URHO3D_PROFILE(UpdateViews);

    views_.Clear();
    preparedViews_.Clear();

    // If device lost, do not perform update. This is because any dynamic vertex/index buffer updates happen already here,
    // and if the device is lost, the updates queue up, causing memory use to rise constantly
    if (!graphics_ || !graphics_->IsInitialized() || graphics_->IsDeviceLost())
        return;

    // Set up the frameinfo structure for this frame
    frame_.frameNumber_ = GetSubsystem<Time>()->GetFrameNumber();
    frame_.timeStep_ = timeStep;
    frame_.camera_ = nullptr;
    numShadowCameras_ = 0;
    numOcclusionBuffers_ = 0;
    updatedOctrees_.Clear();

    // Reload shaders now if needed
    if (shadersDirty_)
        LoadShaders();

    // Queue update of the main viewports. Use reverse order, as rendering order is also reverse
    // to render auxiliary views before dependent main views
    for (i32 i = viewports_.Size() - 1; i >= 0; --i)
        QueueViewport(nullptr, viewports_[i]);

    // Update main viewports. This may queue further views
    unsigned numMainViewports = queuedViewports_.Size();
    for (unsigned i = 0; i < numMainViewports; ++i)
        UpdateQueuedViewport(i);

    // Gather queued & autoupdated render surfaces
    SendEvent(E_RENDERSURFACEUPDATE);

    // Update viewports that were added as result of the event above
    for (unsigned i = numMainViewports; i < queuedViewports_.Size(); ++i)
        UpdateQueuedViewport(i);

    queuedViewports_.Clear();
    resetViews_ = false;
}

void Renderer::Render()
{
    // 在启用 bgfx 集成时，不再走旧的渲染路径，避免混用导致崩溃。
#ifdef URHO3D_BGFX
    if (graphics_ && graphics_->IsBgfxActive())
    {
        // 最小闭环：由 Graphics::BeginFrame/EndFrame 驱动 bgfx 帧提交，这里直接返回。
        return;
    }
#endif

    // Engine does not render when window is closed or device is lost（旧后端路径）
    assert(graphics_ && graphics_->IsInitialized() && !graphics_->IsDeviceLost());

    URHO3D_PROFILE(RenderViews);

// 2D-only：移除点光阴影重定向纹理恢复逻辑


    graphics_->SetDefaultTextureFilterMode(textureFilterMode_);
    graphics_->SetDefaultTextureAnisotropy((unsigned)textureAnisotropy_);

    // If no views that render to the backbuffer, clear the screen so that e.g. the UI is not rendered on top of previous frame
    bool hasBackbufferViews = false;

    for (const WeakPtr<View>& view : views_)
    {
        if (!view->GetRenderTarget())
        {
            hasBackbufferViews = true;
            break;
        }
    }

    if (!hasBackbufferViews)
    {
        graphics_->SetBlendMode(BLEND_REPLACE);
        graphics_->SetColorWrite(true);
        graphics_->SetDepthWrite(true);
        graphics_->SetScissorTest(false);
        graphics_->SetStencilTest(false);
        graphics_->ResetRenderTargets();
        graphics_->Clear(CLEAR_COLOR | CLEAR_DEPTH | CLEAR_STENCIL, defaultZone_->GetFogColor());
    }

    // Render views from last to first. Each main (backbuffer) view is rendered after the auxiliary views it depends on
    for (i32 i = views_.Size() - 1; i >= 0; --i)
    {
        if (!views_[i])
            continue;

        // Screen buffers can be reused between views, as each is rendered completely
        PrepareViewRender();
        views_[i]->Render();
    }

    // Copy the number of batches & primitives from Graphics so that we can account for 3D geometry only
    numPrimitives_ = graphics_->GetNumPrimitives();
    numBatches_ = graphics_->GetNumBatches();

    // Remove unused occlusion buffers and renderbuffers
    RemoveUnusedBuffers();

    // All views done, custom rendering can now be done before UI
    SendEvent(E_ENDALLVIEWSRENDER);
}

void Renderer::DrawDebugGeometry(bool depthTest)
{

    URHO3D_PROFILE(RendererDrawDebug);

    /// \todo Because debug geometry is per-scene, if two cameras show views of the same area, occlusion is not shown correctly
    HashSet<Drawable*> processedGeometries;

    for (unsigned i = 0; i < views_.Size(); ++i)
    {
        View* view = views_[i];
        if (!view || !view->GetDrawDebug())
            continue;
        Octree* octree = view->GetOctree();
        if (!octree)
            continue;
        DebugRenderer* debug = octree->GetComponent<DebugRenderer>();
        if (!debug || !debug->IsEnabledEffective())
            continue;

        // Process geometries / lights only once
        const Vector<Drawable*>& geometries = view->GetGeometries();
        const Vector<Light*>& lights = view->GetLights();

        for (Drawable* geometry : geometries)
        {
            if (!processedGeometries.Contains(geometry))
            {
                geometry->DrawDebugGeometry(debug, depthTest);
                processedGeometries.Insert(geometry);
            }
        }

        // 2D-only 模式下跳过 Light 的调试绘制，避免触发任何 3D 调试渲染
// 2D-only：不绘制 Light 的调试几何

    }
}

void Renderer::QueueRenderSurface(RenderSurface* renderTarget)
{
    if (renderTarget)
    {
        unsigned numViewports = renderTarget->GetNumViewports();

        for (unsigned i = 0; i < numViewports; ++i)
            QueueViewport(renderTarget, renderTarget->GetViewport(i));
    }
}

void Renderer::QueueViewport(RenderSurface* renderTarget, Viewport* viewport)
{
    if (viewport)
    {
        Pair<WeakPtr<RenderSurface>, WeakPtr<Viewport>> newView =
            MakePair(WeakPtr<RenderSurface>(renderTarget), WeakPtr<Viewport>(viewport));

        // Prevent double add of the same rendertarget/viewport combination
        if (!queuedViewports_.Contains(newView))
            queuedViewports_.Push(newView);
    }
}

Geometry* Renderer::GetLightGeometry(Light* light)
{
    switch (light->GetLightType())
    {
    case LIGHT_DIRECTIONAL:
        return dirLightGeometry_;
    default:
        return nullptr;
    }
}

Geometry* Renderer::GetQuadGeometry()
{
    return dirLightGeometry_;
}

Texture2D* Renderer::GetShadowMap(Light* light, Camera* camera, i32 viewWidth, i32 viewHeight)
{
    // 2D-only：不支持阴影贴图
    (void)light; (void)camera; (void)viewWidth; (void)viewHeight;
    return nullptr;
}

RenderSurface* Renderer::GetDepthStencil(int width, int height, int multiSample, bool autoResolve)
{
    // Return the default depth-stencil surface if applicable
    // (when using OpenGL Graphics will allocate right size surfaces on demand to emulate Direct3D9)
    if (width == graphics_->GetWidth() && height == graphics_->GetHeight() && multiSample == 1 &&
        graphics_->GetMultiSample() == multiSample)
        return nullptr;
    else
    {
        return static_cast<Texture2D*>(GetScreenBuffer(width, height, Graphics::GetDepthStencilFormat(), multiSample, autoResolve,
            false, false, false))->GetRenderSurface();
    }
}

OcclusionBuffer* Renderer::GetOcclusionBuffer(Camera* camera)
{
    // 2D-only：禁用遮挡缓冲，直接返回空指针，避免链接到 OcclusionBuffer 实现
    (void)camera;
    return nullptr;
}

Camera* Renderer::GetShadowCamera(){    // 2D-only：移除点光阴影重定向纹理恢复逻辑
    return nullptr;}

void Renderer::StorePreparedView(View* view, Camera* camera)
{
    if (view && camera)
        preparedViews_[camera] = view;
}

View* Renderer::GetPreparedView(Camera* camera)
{
    HashMap<Camera*, WeakPtr<View>>::Iterator i = preparedViews_.Find(camera);
    return i != preparedViews_.End() ? i->second_ : nullptr;
}

View* Renderer::GetActualView(View* view)
{
    if (view && view->GetSourceView())
        return view->GetSourceView();
    else
        return view;
}

void Renderer::SetBatchShaders(Batch& batch, Technique* tech, bool allowShadows, const BatchQueue& queue)
{
    Pass* pass = batch.pass_;

    // Check if need to release/reload all shaders
    if (pass->GetShadersLoadedFrameNumber() != shadersChangedFrameNumber_)
        pass->ReleaseShaders();

    Vector<SharedPtr<ShaderVariation>>& vertexShaders = queue.hasExtraDefines_ ? pass->GetVertexShaders(queue.vsExtraDefinesHash_) : pass->GetVertexShaders();
    Vector<SharedPtr<ShaderVariation>>& pixelShaders = queue.hasExtraDefines_ ? pass->GetPixelShaders(queue.psExtraDefinesHash_) : pass->GetPixelShaders();

    // Load shaders now if necessary
    if (!vertexShaders.Size() || !pixelShaders.Size())
        LoadPassShaders(pass, vertexShaders, pixelShaders, queue);

    // Make sure shaders are loaded now
    if (vertexShaders.Size() && pixelShaders.Size())
    {
        bool heightFog = batch.zone_ && batch.zone_->GetHeightFog();

        // If instancing is not supported, but was requested, choose static geometry vertex shader instead
        if (batch.geometryType_ == GEOM_INSTANCED && !GetDynamicInstancing())
            batch.geometryType_ = GEOM_STATIC;

        if (batch.geometryType_ == GEOM_STATIC_NOINSTANCING)
            batch.geometryType_ = GEOM_STATIC;

        //  Check whether is a pixel lit forward pass. If not, there is only one pixel shader
        if (pass->GetLightingMode() == LIGHTING_PERPIXEL)
        {
            LightBatchQueue* lightQueue = batch.lightQueue_;
            if (!lightQueue)
            {
                // Do not log error, as it would result in a lot of spam
                batch.vertexShader_ = nullptr;
                batch.pixelShader_ = nullptr;
                return;
            }

            Light* light = lightQueue->light_;
            unsigned vsi = 0;
            unsigned psi = 0;
            vsi = batch.geometryType_ * MAX_LIGHT_VS_VARIATIONS;

            bool materialHasSpecular = batch.material_ ? batch.material_->GetSpecular() : true;
            if (specularLighting_ && light->GetSpecularIntensity() > 0.0f && materialHasSpecular)
                psi += LPS_SPEC;
            if (allowShadows && lightQueue->shadowMap_)
            {
                if (light->GetShadowBias().normalOffset_ > 0.0f)
                    vsi += LVS_SHADOWNORMALOFFSET;
                else
                    vsi += LVS_SHADOW;
                psi += LPS_SHADOW;
            }

            switch (light->GetLightType())
            {
            case LIGHT_DIRECTIONAL:
                vsi += LVS_DIR;
                break;
            default:
                // 2D-only：忽略非方向光
                break;
            }

            if (heightFog)
                psi += MAX_LIGHT_PS_VARIATIONS;

            batch.vertexShader_ = vertexShaders[vsi];
            batch.pixelShader_ = pixelShaders[psi];
        }
        else
        {
            // Check if pass has vertex lighting support
            if (pass->GetLightingMode() == LIGHTING_PERVERTEX)
            {
                unsigned numVertexLights = 0;
                if (batch.lightQueue_)
                    numVertexLights = batch.lightQueue_->vertexLights_.Size();

                unsigned vsi = batch.geometryType_ * MAX_VERTEXLIGHT_VS_VARIATIONS + numVertexLights;
                batch.vertexShader_ = vertexShaders[vsi];
            }
            else
            {
                unsigned vsi = batch.geometryType_;
                batch.vertexShader_ = vertexShaders[vsi];
            }

            batch.pixelShader_ = pixelShaders[heightFog ? 1 : 0];
        }
    }

    // Log error if shaders could not be assigned, but only once per technique
    if (!batch.vertexShader_ || !batch.pixelShader_)
    {
        if (!shaderErrorDisplayed_.Contains(tech))
        {
            shaderErrorDisplayed_.Insert(tech);
            URHO3D_LOGERROR("Technique " + tech->GetName() + " has missing shaders");
        }
    }
}

void Renderer::SetLightVolumeBatchShaders(Batch& batch, Camera* camera, const String& vsName, const String& psName, const String& vsDefines,
    const String& psDefines)
{
    (void)batch; (void)camera; (void)vsName; (void)psName; (void)vsDefines; (void)psDefines;
    // 2D-only：无延迟光照体积渲染，保持空实现
}

void Renderer::SetCullMode(CullMode mode, Camera* camera)
{
    // If a camera is specified, check whether it reverses culling due to vertical flipping or reflection
    if (camera && camera->GetReverseCulling())
    {
        if (mode == CULL_CW)
            mode = CULL_CCW;
        else if (mode == CULL_CCW)
            mode = CULL_CW;
    }

    graphics_->SetCullMode(mode);
}

bool Renderer::ResizeInstancingBuffer(i32 numInstances)
{
    assert(numInstances >= 0);

    if (!instancingBuffer_ || !dynamicInstancing_)
        return false;

    i32 oldSize = instancingBuffer_->GetVertexCount();
    if (numInstances <= oldSize)
        return true;

    i32 newSize = INSTANCING_BUFFER_DEFAULT_SIZE;
    while (newSize < numInstances)
        newSize <<= 1;

    const Vector<VertexElement> instancingBufferElements = CreateInstancingBufferElements(numExtraInstancingBufferElements_);
    if (!instancingBuffer_->SetSize(newSize, instancingBufferElements, true))
    {
        URHO3D_LOGERROR("Failed to resize instancing buffer to " + String(newSize));
        // If failed, try to restore the old size
        instancingBuffer_->SetSize(oldSize, instancingBufferElements, true);
        return false;
    }

    URHO3D_LOGDEBUG("Resized instancing buffer to " + String(newSize));
    return true;
}

void Renderer::OptimizeLightByScissor(Light* light, Camera* camera)
{
    // 2D-only：不使用基于光照体积的裁剪优化
    (void)light; (void)camera;
    graphics_->SetScissorTest(false);
}

void Renderer::OptimizeLightByStencil(Light* light, Camera* camera)
{
    // 2D-only：不使用光照体积模板优化
    (void)light; (void)camera;
    graphics_->SetStencilTest(false);
}

Viewport* Renderer::GetViewport(i32 index) const
{
    assert(index >= 0);
    return index < viewports_.Size() ? viewports_[index] : nullptr;
}

Viewport* Renderer::GetViewportForScene(Scene* scene, i32 index) const
{
    assert(index >= 0);

    for (unsigned i = 0; i < viewports_.Size(); ++i)
    {
        Viewport* viewport = viewports_[i];
        if (viewport && viewport->GetScene() == scene)
        {
            if (index == 0)
                return viewport;
            else
                --index;
        }
    }
    return nullptr;
}

Texture* Renderer::GetScreenBuffer(int width, int height, unsigned format, int multiSample, bool autoResolve, bool cubemap, bool filtered, bool srgb,
    hash32 persistentKey)
{
    bool depthStencil = (format == Graphics::GetDepthStencilFormat()) || (format == Graphics::GetReadableDepthFormat());
    if (depthStencil)
    {
        filtered = false;
        srgb = false;
    }

    if (cubemap)
        height = width;

    multiSample = Clamp(multiSample, 1, 16);
    if (multiSample == 1)
        autoResolve = false;

    hash64 searchKey = (hash64)format << 32u | multiSample << 24u | width << 12u | height;
    if (filtered)
        searchKey |= 0x8000000000000000ULL;
    if (srgb)
        searchKey |= 0x4000000000000000ULL;
    if (cubemap)
        searchKey |= 0x2000000000000000ULL;
    if (autoResolve)
        searchKey |= 0x1000000000000000ULL;

    // Add persistent key if defined
    if (persistentKey)
        searchKey += (hash64)persistentKey << 32u;

    // If new size or format, initialize the allocation stats
    if (screenBuffers_.Find(searchKey) == screenBuffers_.End())
        screenBufferAllocations_[searchKey] = 0;

    // Reuse depth-stencil buffers whenever the size matches, instead of allocating new
    // Unless persistency specified
    unsigned allocations = screenBufferAllocations_[searchKey];
    if (!depthStencil || persistentKey)
        ++screenBufferAllocations_[searchKey];

    if (allocations >= screenBuffers_[searchKey].Size())
    {
        SharedPtr<Texture> newBuffer;

        if (!cubemap)
        {
            SharedPtr<Texture2D> newTex2D(new Texture2D(context_));
            /// \todo Mipmaps disabled for now. Allow to request mipmapped buffer?
            newTex2D->SetNumLevels(1);
            newTex2D->SetSize(width, height, format, depthStencil ? TEXTURE_DEPTHSTENCIL : TEXTURE_RENDERTARGET, multiSample, autoResolve);

#ifdef URHO3D_OPENGL
            // OpenGL hack: clear persistent floating point screen buffers to ensure the initial contents aren't illegal (NaN)?
            // Otherwise eg. the AutoExposure post process will not work correctly
            if (Graphics::GetGAPI() == GAPI_OPENGL && persistentKey && Texture::GetDataType_OGL(format) == GL_FLOAT)
            {
                // Note: this loses current rendertarget assignment
                graphics_->ResetRenderTargets();
                graphics_->SetRenderTarget(0, newTex2D);
                graphics_->SetDepthStencil((RenderSurface*)nullptr);
                graphics_->SetViewport(IntRect(0, 0, width, height));
                graphics_->Clear(CLEAR_COLOR);
            }
#endif

            newBuffer = newTex2D;
        }
        else
        {
            // 2D-only：与非 cubemap 路径一致，分配 2D 纹理
            SharedPtr<Texture2D> newTex2D(new Texture2D(context_));
            newTex2D->SetNumLevels(1);
            newTex2D->SetSize(width, height, format, depthStencil ? TEXTURE_DEPTHSTENCIL : TEXTURE_RENDERTARGET, multiSample, autoResolve);
            newBuffer = newTex2D;
        }

        newBuffer->SetSRGB(srgb);
        newBuffer->SetFilterMode(filtered ? FILTER_BILINEAR : FILTER_NEAREST);
        newBuffer->ResetUseTimer();
        screenBuffers_[searchKey].Push(newBuffer);

        URHO3D_LOGDEBUG("Allocated new screen buffer size " + String(width) + "x" + String(height) + " format " + String(format));
        return newBuffer;
    }
    else
    {
        Texture* buffer = screenBuffers_[searchKey][allocations];
        buffer->ResetUseTimer();
        return buffer;
    }
}

const Rect& Renderer::GetLightScissor(Light* light, Camera* camera)
{
    (void)light; (void)camera;
    static Rect full(0.0f, 0.0f, 1.0f, 1.0f);
    return full;
}

void Renderer::UpdateQueuedViewport(i32 index)
{
    assert(index >= 0);

    WeakPtr<RenderSurface>& renderTarget = queuedViewports_[index].first_;
    WeakPtr<Viewport>& viewport = queuedViewports_[index].second_;

    // Null pointer means backbuffer view. Differentiate between that and an expired rendersurface
    if ((renderTarget.NotNull() && renderTarget.Expired()) || viewport.Expired())
        return;

    // (Re)allocate the view structure if necessary
    if (!viewport->GetView() || resetViews_)
        viewport->AllocateView();

    View* view = viewport->GetView();
    assert(view);
    // Check if view can be defined successfully (has either valid scene, camera and octree, or no scene passes)
    if (!view->Define(renderTarget, viewport))
        return;

    views_.Push(WeakPtr<View>(view));

    const IntRect& viewRect = viewport->GetRect();
    Scene* scene = viewport->GetScene();
    if (!scene)
        return;

    auto* octree = scene->GetComponent<Octree>();

    // Update octree (perform early update for drawables which need that, and reinsert moved drawables).
    // However, if the same scene is viewed from multiple cameras, update the octree only once
    if (!updatedOctrees_.Contains(octree))
    {
        frame_.camera_ = viewport->GetCamera();
        frame_.viewSize_ = viewRect.Size();
        if (frame_.viewSize_ == IntVector2::ZERO)
            frame_.viewSize_ = IntVector2(graphics_->GetWidth(), graphics_->GetHeight());
        octree->Update(frame_);
        updatedOctrees_.Insert(octree);

        // Set also the view for the debug renderer already here, so that it can use culling
        /// \todo May result in incorrect debug geometry culling if the same scene is drawn from multiple viewports
        auto* debug = scene->GetComponent<DebugRenderer>();
        if (debug && viewport->GetDrawDebug())
            debug->SetView(viewport->GetCamera());
    }

    // Update view. This may queue further views. View will send update begin/end events once its state is set
    ResetShadowMapAllocations(); // Each view can reuse the same shadow maps
    view->Update(frame_);
}

void Renderer::PrepareViewRender()
{
    ResetScreenBufferAllocations();
    lightScissorCache_.Clear();
    lightStencilValue_ = 1;
}

void Renderer::RemoveUnusedBuffers()
{
    // 2D-only：不维护遮挡缓冲，直接确保容器为空
    occlusionBuffers_.Clear();

    for (HashMap<hash64, Vector<SharedPtr<Texture>>>::Iterator i = screenBuffers_.Begin(); i != screenBuffers_.End();)
    {
        HashMap<hash64, Vector<SharedPtr<Texture>>>::Iterator current = i++;
        Vector<SharedPtr<Texture>>& buffers = current->second_;
        for (i32 j = buffers.Size() - 1; j >= 0; --j)
        {
            Texture* buffer = buffers[j];
            if (buffer->GetUseTimer() > MAX_BUFFER_AGE)
            {
                URHO3D_LOGDEBUG("Removed unused screen buffer size " + String(buffer->GetWidth()) + "x" + String(buffer->GetHeight()) +
                         " format " + String(buffer->GetFormat()));
                buffers.Erase(j);
            }
        }
        if (buffers.Empty())
        {
            screenBufferAllocations_.Erase(current->first_);
            screenBuffers_.Erase(current);
        }
    }
}

void Renderer::ResetShadowMapAllocations()
{
    for (HashMap<int, Vector<Light*>>::Iterator i = shadowMapAllocations_.Begin(); i != shadowMapAllocations_.End(); ++i)
        i->second_.Clear();
}

void Renderer::ResetScreenBufferAllocations()
{
    for (HashMap<hash64, i32>::Iterator i = screenBufferAllocations_.Begin(); i != screenBufferAllocations_.End(); ++i)
        i->second_ = 0;
}

void Renderer::Initialize()
{
    auto* graphics = GetSubsystem<Graphics>();
    auto* cache = GetSubsystem<ResourceCache>();

    if (!graphics || !graphics->IsInitialized() || !cache)
        return;

    URHO3D_PROFILE(InitRenderer);

    graphics_ = graphics;

    if (!graphics_->GetShadowMapFormat())
        drawShadows_ = false;
    // Validate the shadow quality level
    SetShadowQuality(shadowQuality_);

    // 2D-only：不再加载 3D 灯光相关默认纹理（Ramp/Spot）
    defaultLightRamp_.Reset();
    defaultLightSpot_.Reset();
    defaultMaterial_ = new Material(context_);

    defaultRenderPath_ = new RenderPath();
    defaultRenderPath_->Load(cache->GetResource<XMLFile>("RenderPaths/Forward.xml"));

    CreateGeometries();
    CreateInstancingBuffer();

    viewports_.Resize(1);
    ResetShadowMaps();
    ResetBuffers();

    initialized_ = true;

    SubscribeToEvent(E_RENDERUPDATE, URHO3D_HANDLER(Renderer, HandleRenderUpdate));

    URHO3D_LOGINFO("Initialized renderer");
}

void Renderer::LoadShaders()
{
    URHO3D_LOGDEBUG("Reloading shaders");

    // Release old material shaders, mark them for reload
    ReleaseMaterialShaders();
    shadersChangedFrameNumber_ = GetSubsystem<Time>()->GetFrameNumber();

    // Construct new names for deferred light volume pixel shaders based on rendering options
    deferredLightPSVariations_.Resize(MAX_DEFERRED_LIGHT_PS_VARIATIONS);

    for (unsigned i = 0; i < MAX_DEFERRED_LIGHT_PS_VARIATIONS; ++i)
    {
        deferredLightPSVariations_[i] = lightPSVariations[i % DLPS_ORTHO];
        if ((i % DLPS_ORTHO) >= DLPS_SHADOW)
            deferredLightPSVariations_[i] += GetShadowVariations();
        if (i >= DLPS_ORTHO)
            deferredLightPSVariations_[i] += "ORTHO ";
    }

    shadersDirty_ = false;
}

void Renderer::LoadPassShaders(Pass* pass, Vector<SharedPtr<ShaderVariation>>& vertexShaders, Vector<SharedPtr<ShaderVariation>>& pixelShaders, const BatchQueue& queue)
{
    URHO3D_PROFILE(LoadPassShaders);

    // Forget all the old shaders
    vertexShaders.Clear();
    pixelShaders.Clear();

    String vsDefines = pass->GetEffectiveVertexShaderDefines();
    String psDefines = pass->GetEffectivePixelShaderDefines();

    // Make sure to end defines with space to allow appending engine's defines
    if (vsDefines.Length() && !vsDefines.EndsWith(" "))
        vsDefines += ' ';
    if (psDefines.Length() && !psDefines.EndsWith(" "))
        psDefines += ' ';

    // Append defines from batch queue (renderpath command) if needed
    if (queue.vsExtraDefines_.Length())
    {
        vsDefines += queue.vsExtraDefines_;
        vsDefines += ' ';
    }
    if (queue.psExtraDefines_.Length())
    {
        psDefines += queue.psExtraDefines_;
        psDefines += ' ';
    }

    // Add defines for VSM in the shadow pass if necessary
    if (pass->GetName() == "shadow"
        && (shadowQuality_ == SHADOWQUALITY_VSM || shadowQuality_ == SHADOWQUALITY_BLUR_VSM))
    {
        vsDefines += "VSM_SHADOW ";
        psDefines += "VSM_SHADOW ";
    }

    if (pass->GetLightingMode() == LIGHTING_PERPIXEL)
    {
        // Load forward pixel lit variations
        vertexShaders.Resize(MAX_GEOMETRYTYPES * MAX_LIGHT_VS_VARIATIONS);
        pixelShaders.Resize(MAX_LIGHT_PS_VARIATIONS * 2);

        for (unsigned j = 0; j < MAX_GEOMETRYTYPES * MAX_LIGHT_VS_VARIATIONS; ++j)
        {
            unsigned g = j / MAX_LIGHT_VS_VARIATIONS;
            unsigned l = j % MAX_LIGHT_VS_VARIATIONS;

            vertexShaders[j] = graphics_->GetShader(VS, pass->GetVertexShader(),
                vsDefines + lightVSVariations[l] + geometryVSVariations[g]);
        }
        for (unsigned j = 0; j < MAX_LIGHT_PS_VARIATIONS * 2; ++j)
        {
            unsigned l = j % MAX_LIGHT_PS_VARIATIONS;
            unsigned h = j / MAX_LIGHT_PS_VARIATIONS;

            if (l & LPS_SHADOW)
            {
                pixelShaders[j] = graphics_->GetShader(PS, pass->GetPixelShader(),
                    psDefines + lightPSVariations[l] + GetShadowVariations() +
                    heightFogVariations[h]);
            }
            else
                pixelShaders[j] = graphics_->GetShader(PS, pass->GetPixelShader(),
                    psDefines + lightPSVariations[l] + heightFogVariations[h]);
        }
    }
    else
    {
        // Load vertex light variations
        if (pass->GetLightingMode() == LIGHTING_PERVERTEX)
        {
            vertexShaders.Resize(MAX_GEOMETRYTYPES * MAX_VERTEXLIGHT_VS_VARIATIONS);
            for (unsigned j = 0; j < MAX_GEOMETRYTYPES * MAX_VERTEXLIGHT_VS_VARIATIONS; ++j)
            {
                unsigned g = j / MAX_VERTEXLIGHT_VS_VARIATIONS;
                unsigned l = j % MAX_VERTEXLIGHT_VS_VARIATIONS;
                vertexShaders[j] = graphics_->GetShader(VS, pass->GetVertexShader(),
                    vsDefines + vertexLightVSVariations[l] + geometryVSVariations[g]);
            }
        }
        else
        {
            vertexShaders.Resize(MAX_GEOMETRYTYPES);
            for (unsigned j = 0; j < MAX_GEOMETRYTYPES; ++j)
            {
                vertexShaders[j] = graphics_->GetShader(VS, pass->GetVertexShader(),
                    vsDefines + geometryVSVariations[j]);
            }
        }

        pixelShaders.Resize(2);
        for (unsigned j = 0; j < 2; ++j)
        {
            pixelShaders[j] =
                graphics_->GetShader(PS, pass->GetPixelShader(), psDefines + heightFogVariations[j]);
        }
    }

    pass->MarkShadersLoaded(shadersChangedFrameNumber_);
}

void Renderer::ReleaseMaterialShaders()
{
    auto* cache = GetSubsystem<ResourceCache>();
    Vector<Material*> materials;

    cache->GetResources<Material>(materials);

    for (Material* material : materials)
        material->ReleaseShaders();
}

void Renderer::ReloadTextures()
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    Vector<Resource*> textures;

    cache->GetResources(textures, Texture2D::GetTypeStatic());

    for (Resource* texture : textures)
        cache->ReloadResource(texture);

    // 2D-only：不再重载立方体纹理
}

void Renderer::CreateGeometries()
{
    SharedPtr<VertexBuffer> dlvb(new VertexBuffer(context_));
    dlvb->SetShadowed(true);
    dlvb->SetSize(4, VertexElements::Position);
    dlvb->SetData(dirLightVertexData);

    SharedPtr<IndexBuffer> dlib(new IndexBuffer(context_));
    dlib->SetShadowed(true);
    dlib->SetSize(6, false);
    dlib->SetData(dirLightIndexData);

    dirLightGeometry_ = new Geometry(context_);
    dirLightGeometry_->SetVertexBuffer(0, dlvb);
    dirLightGeometry_->SetIndexBuffer(dlib);
    dirLightGeometry_->SetDrawRange(TRIANGLE_LIST, 0, dlib->GetIndexCount());

    // 2D-only：不创建聚光/点光体积几何
}

// 2D-only：移除点光阴影的立方体重定向贴图生成逻辑

void Renderer::CreateInstancingBuffer()
{
    // Do not create buffer if instancing not supported
    if (!graphics_->GetInstancingSupport())
    {
        instancingBuffer_.Reset();
        dynamicInstancing_ = false;
        return;
    }

    instancingBuffer_ = new VertexBuffer(context_);
    const Vector<VertexElement> instancingBufferElements = CreateInstancingBufferElements(numExtraInstancingBufferElements_);
    if (!instancingBuffer_->SetSize(INSTANCING_BUFFER_DEFAULT_SIZE, instancingBufferElements, true))
    {
        instancingBuffer_.Reset();
        dynamicInstancing_ = false;
    }
}

void Renderer::ResetShadowMaps()
{
    shadowMaps_.Clear();
    shadowMapAllocations_.Clear();
    colorShadowMaps_.Clear();
}

void Renderer::ResetBuffers()
{
    occlusionBuffers_.Clear();
    screenBuffers_.Clear();
    screenBufferAllocations_.Clear();
}

String Renderer::GetShadowVariations() const
{
    // 2D-only：不使用阴影变体
    return "";
}

void Renderer::HandleScreenMode(StringHash eventType, VariantMap& eventData)
{
    if (!initialized_)
        Initialize();
    else
        resetViews_ = true;
}

void Renderer::HandleRenderUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace RenderUpdate;

    Update(eventData[P_TIMESTEP].GetFloat());
}


void Renderer::BlurShadowMap(View* view, Texture2D* shadowMap, float blurScale)
{
    graphics_->SetBlendMode(BLEND_REPLACE);
    graphics_->SetDepthTest(CMP_ALWAYS);
    graphics_->SetClipPlane(false);
    graphics_->SetScissorTest(false);

    // Get a temporary render buffer
    auto* tmpBuffer = static_cast<Texture2D*>(GetScreenBuffer(shadowMap->GetWidth(), shadowMap->GetHeight(),
        shadowMap->GetFormat(), 1, false, false, false, false));
    graphics_->SetRenderTarget(0, tmpBuffer->GetRenderSurface());
    graphics_->SetDepthStencil(GetDepthStencil(shadowMap->GetWidth(), shadowMap->GetHeight(), shadowMap->GetMultiSample(),
        shadowMap->GetAutoResolve()));
    graphics_->SetViewport(IntRect(0, 0, shadowMap->GetWidth(), shadowMap->GetHeight()));

    // Get shaders
    static const char* shaderName = "ShadowBlur";
    ShaderVariation* vs = graphics_->GetShader(VS, shaderName);
    ShaderVariation* ps = graphics_->GetShader(PS, shaderName);
    graphics_->SetShaders(vs, ps);

    view->SetGBufferShaderParameters(IntVector2(shadowMap->GetWidth(), shadowMap->GetHeight()), IntRect(0, 0, shadowMap->GetWidth(), shadowMap->GetHeight()));

    // Horizontal blur of the shadow map
    static const StringHash blurOffsetParam("BlurOffsets");

    graphics_->SetShaderParameter(blurOffsetParam, Vector2(shadowSoftness_ * blurScale / shadowMap->GetWidth(), 0.0f));
    graphics_->SetTexture(TU_DIFFUSE, shadowMap);
    view->DrawFullscreenQuad(true);

    // Vertical blur
    graphics_->SetRenderTarget(0, shadowMap);
    graphics_->SetViewport(IntRect(0, 0, shadowMap->GetWidth(), shadowMap->GetHeight()));
    graphics_->SetShaderParameter(blurOffsetParam, Vector2(0.0f, shadowSoftness_ * blurScale / shadowMap->GetHeight()));

    graphics_->SetTexture(TU_DIFFUSE, tmpBuffer);
    view->DrawFullscreenQuad(true);
}
}












