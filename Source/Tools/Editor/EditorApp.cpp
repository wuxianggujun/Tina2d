// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include <Urho3D/Precompiled.h>

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Engine/Application.h>
#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/GraphicsEvents.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/UI/UIElement.h>
#include <Urho3D/UI/Window.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/Menu.h>
#include <Urho3D/UI/BorderImage.h>
#include <Urho3D/UI/Cursor.h>

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
    engineParameters_[EP_WINDOW_RESIZABLE] = true;
}

void EditorApp::Setup()
{
}

void EditorApp::Start()
{
    // 设置鼠标模式为绝对坐标，允许鼠标移出窗口
    GetSubsystem<Input>()->SetMouseMode(MM_ABSOLUTE);
    GetSubsystem<Input>()->SetMouseVisible(true);
    
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
    menubar->SetLayout(LM_HORIZONTAL);
    menubar->SetEnableAnchor(true);
    menubar->SetMinAnchor(0.0f, 0.0f);
    menubar->SetMaxAnchor(1.0f, 0.0f);
    menubar->SetMinOffset(IntVector2(0, 0));
    menubar->SetMaxOffset(IntVector2(0, 28));
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
    // 不使用自动样式，避免应用UI纹理
    workspace->SetColor(Color(0.10f, 0.10f, 0.12f, 1.0f));
    workspace->SetEnableAnchor(true);
    workspace->SetMinAnchor(0.0f, 0.0f);
    workspace->SetMaxAnchor(1.0f, 1.0f);
    workspace->SetMinOffset(IntVector2(0, 28));
    workspace->SetMaxOffset(IntVector2(0, 0));
    ui->GetRoot()->AddChild(workspace);

    // 在UI元素创建完成后设置光标
    auto* cursor = ui->GetRoot()->CreateChild<Cursor>();
    cursor->SetStyleAuto();
    ui->SetCursor(cursor);

    // 自适应窗口变化
    // SubscribeToEvent(E_SCREENMODE, [this, workspace](StringHash, VariantMap&){
        // auto* gfx = GetSubsystem<Graphics>();
        // workspace->SetSize(gfx->GetWidth(), gfx->GetHeight() - 28);
    // });
}

} // namespace Urho3D

// 将入口宏放在全局命名空间，确保在 Win32 子系统下导出全局 WinMain 符号
URHO3D_DEFINE_APPLICATION_MAIN(Urho3D::EditorApp)

