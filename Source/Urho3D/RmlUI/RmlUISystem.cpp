// Copyright (c) 2024 Tina2D project
// SPDX-License-Identifier: MIT

#include "../Precompiled.h"
#include "RmlUISystem.h"
#include "RmlUIRenderer.h"
#include "RmlUIFile.h"
#include "RmlUISystemInterface.h"
#include "RmlUIInput.h"
#include "../Core/Context.h"
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
}

RmlUISystem::~RmlUISystem()
{
    Shutdown();
}

bool RmlUISystem::Initialize()
{
    if (initialized_)
        return true;
    
    // Subscribe to update events  
    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(RmlUISystem, HandleUpdate));
    SubscribeToEvent(E_POSTRENDERUPDATE, URHO3D_HANDLER(RmlUISystem, HandlePostRenderUpdate));
    SubscribeToEvent(E_SCREENMODE, URHO3D_HANDLER(RmlUISystem, HandleScreenModeChanged));
    
    // 设置渲染接口
    renderer_ = new RmlUIRenderer(context_);
    if (!renderer_->Initialize())
    {
        URHO3D_LOGERROR("Failed to initialize RmlUI renderer");
        return false;
    }
    Rml::SetRenderInterface(renderer_);
    
    // 设置文件接口
    fileInterface_ = new RmlUIFile(context_);
    Rml::SetFileInterface(fileInterface_);
    
    // 设置系统接口
    systemInterface_ = new RmlUISystemInterface(context_);
    Rml::SetSystemInterface(systemInterface_);
    
    // 设置输入处理
    input_ = new RmlUIInput(context_);
    
    // 初始化RmlUI
    if (!Rml::Initialise())
    {
        URHO3D_LOGERROR("Failed to initialise RmlUI");
        return false;
    }
    
    // 创建默认上下文
    Graphics* graphics = GetSubsystem<Graphics>();
    if (graphics)
    {
        Vector2 dimensions(graphics->GetWidth(), graphics->GetHeight());
        defaultContext_ = CreateContext("default", dimensions);
        if (defaultContext_)
        {
            input_->SetContext(defaultContext_);
            input_->SubscribeToEvents();
        }
    }
    
    initialized_ = true;
    URHO3D_LOGINFO("RmlUI initialized successfully");
    
    return true;
}

void RmlUISystem::Shutdown()
{
    if (!initialized_)
        return;
    
    // 清理所有上下文
    for (auto& pair : contexts_)
    {
        if (pair.second)
        {
            // 正确的释放方式是直接删除
            delete pair.second;
        }
    }
    contexts_.Clear();
    defaultContext_ = nullptr;
    
    // 关闭RmlUI
    Rml::Shutdown();
    
    // 清理接口
    renderer_.Reset();
    fileInterface_.Reset();
    systemInterface_.Reset();
    input_.Reset();
    
    UnsubscribeFromAllEvents();
    
    initialized_ = false;
    URHO3D_LOGINFO("RmlUI shut down");
}

Rml::Context* RmlUISystem::CreateContext(const String& name, const Vector2& dimensions)
{
    // 检查是否已存在
    auto it = contexts_.Find(name);
    if (it != contexts_.End())
    {
        URHO3D_LOGWARNING("RmlUI context '" + name + "' already exists");
        return it->second;
    }
    
    // 创建新上下文
    Rml::Context* context = Rml::CreateContext(name.CString(), 
        Rml::Vector2i((int)dimensions.x_, (int)dimensions.y_));
    
    if (!context)
    {
        URHO3D_LOGERROR("Failed to create RmlUI context: " + name);
        return nullptr;
    }
    
    contexts_[name] = context;
    
    // 如果没有默认上下文，设置为默认
    if (!defaultContext_)
    {
        defaultContext_ = context;
        if (input_)
        {
            input_->SetContext(defaultContext_);
            input_->SubscribeToEvents();
        }
    }
    
    URHO3D_LOGINFO("Created RmlUI context: " + name);
    return context;
}

Rml::Context* RmlUISystem::GetContext(const String& name) const
{
    auto it = contexts_.Find(name);
    if (it != contexts_.End())
        return it->second;
    return nullptr;
}

Rml::ElementDocument* RmlUISystem::LoadDocument(const String& path, Rml::Context* context)
{
    if (!context)
        context = defaultContext_;
    
    if (!context)
    {
        URHO3D_LOGERROR("No RmlUI context available to load document");
        return nullptr;
    }
    
    Rml::ElementDocument* document = context->LoadDocument(path.CString());
    if (!document)
    {
        URHO3D_LOGERROR("Failed to load RmlUI document: " + path);
        return nullptr;
    }
    
    return document;
}

void RmlUISystem::ReloadStyleSheets()
{
    // Reload style sheets for all contexts
    for (auto& pair : contexts_)
    {
        if (pair.second)
        {
            // RmlUI doesn't have a direct reload method
            // You'd need to reload documents to refresh styles
        }
    }
}

void RmlUISystem::Update(float timeStep)
{
    // Update all contexts
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
    // Render all contexts
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
    // 获取新的屏幕尺寸
    Graphics* graphics = GetSubsystem<Graphics>();
    if (!graphics)
        return;
    
    int width = graphics->GetWidth();
    int height = graphics->GetHeight();
    
    // 更新渲染器投影矩阵
    if (renderer_)
        renderer_->UpdateProjection(width, height);
    
    // 更新所有上下文的尺寸
    for (auto& pair : contexts_)
    {
        if (pair.second)
        {
            pair.second->SetDimensions(Rml::Vector2i(width, height));
        }
    }
}

void RmlUISystem::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace Update;
    float timeStep = eventData[P_TIMESTEP].GetFloat();
    Update(timeStep);
}

void RmlUISystem::HandleRenderUpdate(StringHash eventType, VariantMap& eventData)
{
    // 可以在这里处理渲染前的准备工作
}

void RmlUISystem::HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData)
{
    Render();
}

} // namespace Urho3D