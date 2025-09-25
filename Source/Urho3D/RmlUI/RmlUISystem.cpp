// Copyright (c) 2024 Tina2D project
// SPDX-License-Identifier: MIT

#include "../Precompiled.h"
#include "RmlUISystem.h"
#include "RmlUIRenderer.h"
#include "RmlUIFile.h"
#include "RmlUISystemInterface.h"
#include "RmlUIInput.h"
#include "../Core/CoreEvents.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/GraphicsEvents.h"
#include "../Resource/ResourceCache.h"
#include "../IO/Log.h"
#include <RmlUi/Core.h>

namespace Urho3D
{

RmlUISystem::RmlUISystem(Context* context)
    : Object(context)
    , defaultContext_(nullptr)
    , initialized_(false)
{
    graphics_ = GetSubsystem<Graphics>();
    cache_ = GetSubsystem<ResourceCache>();
    
    // 初始化 RmlUI
    if (!Initialize())
    {
        URHO3D_LOGERROR("Failed to initialize RmlUI system");
        return;
    }
    
    // 订阅事件
    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(RmlUISystem, Update));
    SubscribeToEvent(E_RENDERUPDATE, URHO3D_HANDLER(RmlUISystem, HandleRenderUpdate));
    SubscribeToEvent(E_POSTRENDERUPDATE, URHO3D_HANDLER(RmlUISystem, HandlePostRenderUpdate));
    SubscribeToEvent(E_SCREENMODE, URHO3D_HANDLER(RmlUISystem, HandleScreenModeChanged));
}

RmlUISystem::~RmlUISystem()
{
    Shutdown();
}

bool RmlUISystem::Initialize()
{
    if (initialized_)
        return true;
    
    // 创建渲染接口
    renderInterface_ = new RmlUIRenderer(context_);
    if (!renderInterface_->Initialize())
    {
        URHO3D_LOGERROR("Failed to initialize RmlUI renderer");
        return false;
    }
    Rml::SetRenderInterface(renderInterface_);
    
    // 创建文件接口
    fileInterface_ = new RmlUIFile(context_);
    Rml::SetFileInterface(fileInterface_);
    
    // 创建系统接口
    systemInterface_ = new RmlUISystemInterface(context_);
    Rml::SetSystemInterface(systemInterface_);
    
    // 初始化 RmlUI 核心
    if (!Rml::Initialise())
    {
        URHO3D_LOGERROR("Failed to initialize RmlUI core");
        return false;
    }
    
    // 创建输入处理器
    input_ = new RmlUIInput(context_);
    
    // 创建默认上下文
    if (graphics_)
    {
        Vector2 dimensions((float)graphics_->GetWidth(), (float)graphics_->GetHeight());
        defaultContext_ = CreateContext("default", dimensions);
        if (defaultContext_)
        {
            input_->SetContext(defaultContext_);
            input_->SubscribeToEvents();
        }
    }
    
    // 加载默认字体
    if (!Rml::LoadFontFace("Data/Fonts/NotoSans-Regular.ttf"))
    {
        URHO3D_LOGWARNING("Failed to load default font");
    }
    
    initialized_ = true;
    URHO3D_LOGINFO("RmlUI system initialized successfully");
    return true;
}

void RmlUISystem::Shutdown()
{
    if (!initialized_)
        return;
    
    // 取消订阅输入事件
    if (input_)
        input_->UnsubscribeFromEvents();
    
    // 关闭所有文档
    for (auto& pair : contexts_)
    {
        if (pair.second)
        {
            pair.second->RemoveReference();
        }
    }
    contexts_.Clear();
    defaultContext_ = nullptr;
    
    // 关闭 RmlUI
    Rml::Shutdown();
    
    // 清理接口
    renderInterface_.Reset();
    fileInterface_.Reset();
    systemInterface_.Reset();
    input_.Reset();
    
    initialized_ = false;
    URHO3D_LOGINFO("RmlUI system shut down");
}

Rml::Context* RmlUISystem::CreateContext(const String& name, const Vector2& dimensions)
{
    if (!initialized_)
    {
        URHO3D_LOGERROR("RmlUI not initialized");
        return nullptr;
    }
    
    // 检查是否已存在
    auto it = contexts_.Find(name);
    if (it != contexts_.End())
    {
        URHO3D_LOGWARNINGF("Context '%s' already exists", name.CString());
        return it->second;
    }
    
    // 创建新上下文
    Rml::Context* context = Rml::CreateContext(
        name.CString(),
        Rml::Vector2i((int)dimensions.x_, (int)dimensions.y_)
    );
    
    if (!context)
    {
        URHO3D_LOGERRORF("Failed to create RmlUI context '%s'", name.CString());
        return nullptr;
    }
    
    contexts_[name] = context;
    
    // 如果是第一个上下文，设为默认
    if (!defaultContext_)
    {
        defaultContext_ = context;
        if (input_)
        {
            input_->SetContext(defaultContext_);
        }
    }
    
    URHO3D_LOGINFOF("Created RmlUI context '%s' (%dx%d)", 
                    name.CString(), (int)dimensions.x_, (int)dimensions.y_);
    return context;
}

Rml::Context* RmlUISystem::GetContext(const String& name) const
{
    auto it = contexts_.Find(name);
    return it != contexts_.End() ? it->second : nullptr;
}

Rml::ElementDocument* RmlUISystem::LoadDocument(const String& path, Rml::Context* context)
{
    if (!initialized_)
    {
        URHO3D_LOGERROR("RmlUI not initialized");
        return nullptr;
    }
    
    if (!context)
        context = defaultContext_;
    
    if (!context)
    {
        URHO3D_LOGERROR("No context available for loading document");
        return nullptr;
    }
    
    Rml::ElementDocument* document = context->LoadDocument(path.CString());
    if (!document)
    {
        URHO3D_LOGERRORF("Failed to load document: %s", path.CString());
        return nullptr;
    }
    
    URHO3D_LOGINFOF("Loaded RmlUI document: %s", path.CString());
    return document;
}

void RmlUISystem::ReloadStyleSheets()
{
    if (!initialized_)
        return;
    
    for (auto& pair : contexts_)
    {
        if (pair.second)
        {
            // 重新加载所有文档的样式表
            int numDocuments = pair.second->GetNumDocuments();
            for (int i = 0; i < numDocuments; ++i)
            {
                Rml::ElementDocument* doc = pair.second->GetDocument(i);
                if (doc)
                {
                    // TODO: 实现样式表重载
                    // doc->ReloadStyleSheet();
                }
            }
        }
    }
    
    URHO3D_LOGINFO("Reloaded RmlUI style sheets");
}

void RmlUISystem::Update(float timeStep)
{
    if (!initialized_)
        return;
    
    // 更新所有上下文
    for (auto& pair : contexts_)
    {
        if (pair.second)
        {
            pair.second->Update();
        }
    }
}

void RmlUISystem::Render()
{
    if (!initialized_)
        return;
    
    // 渲染所有上下文
    for (auto& pair : contexts_)
    {
        if (pair.second)
        {
            pair.second->Render();
        }
    }
}

void RmlUISystem::HandleScreenModeChanged(StringHash eventType, VariantMap& eventData)
{
    if (!initialized_ || !graphics_)
        return;
    
    // 更新所有上下文的尺寸
    int width = graphics_->GetWidth();
    int height = graphics_->GetHeight();
    
    for (auto& pair : contexts_)
    {
        if (pair.second)
        {
            pair.second->SetDimensions(Rml::Vector2i(width, height));
        }
    }
    
    // 更新渲染器投影矩阵
    if (renderInterface_)
    {
        renderInterface_->UpdateProjection();
    }
    
    URHO3D_LOGINFOF("Updated RmlUI contexts for new screen size: %dx%d", width, height);
}

void RmlUISystem::HandleRenderUpdate(StringHash eventType, VariantMap& eventData)
{
    // 可以在这里处理渲染前的更新
}

void RmlUISystem::HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData)
{
    // 在所有 3D 渲染完成后渲染 UI
    Render();
}

void RmlUISystem::Update(StringHash eventType, VariantMap& eventData)
{
    using namespace Update;
    float timeStep = eventData[P_TIMESTEP].GetFloat();
    Update(timeStep);
}

} // namespace Urho3D