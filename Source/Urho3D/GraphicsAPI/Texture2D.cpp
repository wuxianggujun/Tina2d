// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../Core/Profiler.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/GraphicsEvents.h"
#ifdef URHO3D_BGFX
#include "../Graphics/Graphics.h"
#endif
#include "../Graphics/Renderer.h"
#include "../GraphicsAPI/GraphicsImpl.h"
#include "../GraphicsAPI/Texture2D.h"
#include "../IO/FileSystem.h"
#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/XMLFile.h"

#include "../DebugNew.h"

namespace Urho3D
{

Texture2D::Texture2D(Context* context) :
    Texture(context)
{
#ifdef URHO3D_OPENGL
    if (Graphics::GetGAPI() == GAPI_OPENGL)
        target_ = GL_TEXTURE_2D;
#endif
}

Texture2D::~Texture2D()
{
    Release();
}

void Texture2D::RegisterObject(Context* context)
{
    context->RegisterFactory<Texture2D>();
}

bool Texture2D::BeginLoad(Deserializer& source)
{
    // In headless mode, do not actually load the texture, just return success
    if (!graphics_)
        return true;

    // If device is lost, retry later
    if (graphics_->IsDeviceLost())
    {
        URHO3D_LOGWARNING("Texture load while device is lost");
        dataPending_ = true;
        return true;
    }

    // Load the image data for EndLoad()
    loadImage_ = new Image(context_);
    if (!loadImage_->Load(source))
    {
        loadImage_.Reset();
        return false;
    }

    // Precalculate mip levels if async loading
    if (GetAsyncLoadState() == ASYNC_LOADING)
        loadImage_->PrecalculateLevels();

    // Load the optional parameters file
    auto* cache = GetSubsystem<ResourceCache>();
    String xmlName = ReplaceExtension(GetName(), ".xml");
    loadParameters_ = cache->GetTempResource<XMLFile>(xmlName, false);

    return true;
}

bool Texture2D::EndLoad()
{
    // In headless mode, do not actually load the texture, just return success
    if (!graphics_ || graphics_->IsDeviceLost())
        return true;

    // If over the texture budget, see if materials can be freed to allow textures to be freed
    CheckTextureBudget(GetTypeStatic());

    SetParameters(loadParameters_);
    bool success = true;

#ifdef URHO3D_BGFX
    // BGFX 后端：不在 Urho 的 GL/D3D 设备上创建纹理，记录元数据供 BGFX 路径使用。
    if (Graphics::GetGAPI() == GAPI_BGFX)
    {
        if (loadImage_)
        {
            const int w = (int)loadImage_->GetWidth();
            const int h = (int)loadImage_->GetHeight();
            unsigned fmt = 0;
            const unsigned comps = loadImage_->GetComponents();
            if (comps == 1)
                fmt = Graphics::GetAlphaFormat();
            else if (comps == 3)
                fmt = Graphics::GetRGBFormat();
            else
                fmt = Graphics::GetRGBAFormat();

            SetSizeForBgfx_NoCreate(w, h, fmt);
            success = true;
        }
        else
            success = false;
    }
    else
#endif
    {
        success = SetData(loadImage_);
    }

    loadImage_.Reset();
    loadParameters_.Reset();

    return success;
}

bool Texture2D::SetSize(int width, int height, unsigned format, TextureUsage usage, int multiSample, bool autoResolve)
{
    if (width <= 0 || height <= 0)
    {
        URHO3D_LOGERROR("Zero or negative texture dimensions");
        return false;
    }

    multiSample = Clamp(multiSample, 1, 16);
    if (multiSample == 1)
        autoResolve = false;
    else if (multiSample > 1 && usage < TEXTURE_RENDERTARGET)
    {
        URHO3D_LOGERROR("Multisampling is only supported for rendertarget or depth-stencil textures");
        return false;
    }

    // Disable mipmaps if multisample & custom resolve
    if (multiSample > 1 && autoResolve == false)
        requestedLevels_ = 1;

    // Delete the old rendersurface if any
    renderSurface_.Reset();

    usage_ = usage;

    if (usage >= TEXTURE_RENDERTARGET)
    {
        renderSurface_ = new RenderSurface(this);

        // Clamp mode addressing by default and nearest filtering
        addressModes_[COORD_U] = ADDRESS_CLAMP;
        addressModes_[COORD_V] = ADDRESS_CLAMP;
        filterMode_ = FILTER_NEAREST;
    }

    if (usage == TEXTURE_RENDERTARGET)
        SubscribeToEvent(E_RENDERSURFACEUPDATE, URHO3D_HANDLER(Texture2D, HandleRenderSurfaceUpdate));
    else
        UnsubscribeFromEvent(E_RENDERSURFACEUPDATE);

    width_ = width;
    height_ = height;
    format_ = format;
    depth_ = 1;
    multiSample_ = multiSample;
    autoResolve_ = autoResolve;

    return Create();
}

bool Texture2D::GetImage(Image& image) const
{
    if (format_ != Graphics::GetRGBAFormat() && format_ != Graphics::GetRGBFormat())
    {
        URHO3D_LOGERROR("Unsupported texture format, can not convert to Image");
        return false;
    }

    image.SetSize(width_, height_, GetComponents());
    GetData(0, image.GetData());
    return true;
}

SharedPtr<Image> Texture2D::GetImage() const
{
    auto rawImage = MakeShared<Image>(context_);
    if (!GetImage(*rawImage))
        return nullptr;
    return rawImage;
}

void Texture2D::HandleRenderSurfaceUpdate(StringHash eventType, VariantMap& eventData)
{
    if (renderSurface_ && (renderSurface_->GetUpdateMode() == SURFACE_UPDATEALWAYS || renderSurface_->IsUpdateQueued()))
    {
        auto* renderer = GetSubsystem<Renderer>();
        if (renderer)
            renderer->QueueRenderSurface(renderSurface_);
        renderSurface_->ResetUpdateQueued();
    }
}

void Texture2D::OnDeviceLost()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return OnDeviceLost_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return OnDeviceLost_D3D11();
#endif
}

void Texture2D::OnDeviceReset()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return OnDeviceReset_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return OnDeviceReset_D3D11();
#endif
}

void Texture2D::Release()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return Release_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return Release_D3D11();
#endif

#ifdef URHO3D_BGFX
    if (gapi == GAPI_BGFX)
    {
        if (auto* graphics = GetSubsystem<Graphics>())
            graphics->BgfxReleaseTexture(this);
        // 清空逻辑尺寸与格式（仅作标记，不影响 BGFX 端释放）
        width_ = height_ = depth_ = 0;
        format_ = 0;
        return;
    }
#endif
}

bool Texture2D::SetData(unsigned level, int x, int y, int width, int height, const void* data)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    if (gapi == GAPI_BGFX)
    {
        auto* graphics = GetSubsystem<Graphics>();
        if (!graphics || !graphics->IsBgfxActive())
            return false;

        if (!data || width <= 0 || height <= 0)
            return false;

        // 支持整幅与子矩形更新。若尚未创建 bgfx 纹理，会先分配空纹理存储再更新。
        return graphics->BgfxUpdateTextureRegion(this, x, y, width, height, data, level);
    }
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetData_OGL(level, x, y, width, height, data);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetData_D3D11(level, x, y, width, height, data);
#endif

    return {}; // Prevent warning
}

bool Texture2D::SetData(Image* image, bool useAlpha)
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_BGFX
    // BGFX-only 路径：当旧后端被禁用时，从 Image 直接创建 BGFX 纹理
    if (gapi == GAPI_BGFX)
    {
        auto* graphics = GetSubsystem<Graphics>();
        if (graphics && graphics->IsBgfxActive())
        {
            // 记录逻辑尺寸与格式，供布局/判断使用
            unsigned fmt = 0;
            if (image)
            {
                const unsigned comps = image->GetComponents();
                if (comps == 1 || useAlpha)
                    fmt = Graphics::GetAlphaFormat();
                else
                    fmt = Graphics::GetRGBAFormat();
                SetSizeForBgfx_NoCreate(image->GetWidth(), image->GetHeight(), fmt);
            }
            return graphics->BgfxCreateTextureFromImage(this, image, useAlpha);
        }
        // 若 BGFX 尚未初始化，先返回 false 交由调用方稍后重试
        return false;
    }
#endif

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return SetData_OGL(image, useAlpha);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return SetData_D3D11(image, useAlpha);
#endif

    return {}; // Prevent warning
}

bool Texture2D::GetData(unsigned level, void* dest) const
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return GetData_OGL(level, dest);
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return GetData_D3D11(level, dest);
#endif

    return {}; // Prevent warning
}

bool Texture2D::Create()
{
    GAPI gapi = Graphics::GetGAPI();

#ifdef URHO3D_OPENGL
    if (gapi == GAPI_OPENGL)
        return Create_OGL();
#endif

#ifdef URHO3D_D3D11
    if (gapi == GAPI_D3D11)
        return Create_D3D11();
#endif

    return {}; // Prevent warning
}

}
