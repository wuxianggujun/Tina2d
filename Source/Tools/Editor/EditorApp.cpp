// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include <Urho3D/Precompiled.h>

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Engine/Application.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/UI/UIElement.h>
#include <Urho3D/UI/Window.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/Menu.h>
#include <Urho3D/UI/BorderImage.h>

#include "EditorApp.h"

namespace Urho3D
{

EditorApp::EditorApp(Context* context)
    : Application(context)
{
    // 缺省参数：窗口标题与尺寸
    engineParameters_[EP_FULL_SCREEN] = false;
    engineParameters_[EP_HEADLESS] = false;
    engineParameters_[EP_RESOURCE_PATHS] = String("Data;CoreData");
    engineParameters_[EP_WINDOW_TITLE] = String("Tina2D Editor");
}

void EditorApp::Setup()
{
}

void EditorApp::Start()
{
    CreateUI();
}

void EditorApp::Stop()
{
}

void EditorApp::CreateUI()
{
    auto* cache = GetSubsystem<ResourceCache>();
    auto* ui = GetSubsystem<UI>();

    // 加载默认样式
    if (auto* style = cache->GetResource<XMLFile>("UI/DefaultStyle.xml"))
        ui->GetRoot()->SetDefaultStyle(style);

    // 顶部菜单条占位
    SharedPtr<UIElement> menubar(new UIElement(context_));
    menubar->SetMinSize(IntVector2(0, 28));
    menubar->SetLayout(LM_HORIZONTAL);
    ui->GetRoot()->AddChild(menubar);

    // 简单标题
    SharedPtr<Text> title(new Text(context_));
    title->SetName("Title");
    title->SetText("Tina2D Editor (WIP)");
    title->SetStyleAuto();
    menubar->AddChild(title);

    // 中央工作区占位
    SharedPtr<BorderImage> workspace(new BorderImage(context_));
    workspace->SetName("Workspace");
    workspace->SetStyleAuto();
    workspace->SetColor(Color(0.10f, 0.10f, 0.12f));
    workspace->SetPosition(0, 28);
    workspace->SetSize(GetSubsystem<Graphics>()->GetWidth(), GetSubsystem<Graphics>()->GetHeight() - 28);
    ui->GetRoot()->AddChild(workspace);

    // 自适应窗口变化
    SubscribeToEvent(E_SCREENMODE, [this, workspace](StringHash, VariantMap&){
        auto* gfx = GetSubsystem<Graphics>();
        workspace->SetSize(gfx->GetWidth(), gfx->GetHeight() - 28);
    });
}

URHO3D_DEFINE_APPLICATION_MAIN(EditorApp)

} // namespace Urho3D

