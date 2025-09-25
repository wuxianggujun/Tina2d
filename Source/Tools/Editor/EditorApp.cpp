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
// 编辑器服务与面板
#include "Services/SelectionService.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ResourcePanel.h"
#include "Panels/LogPanel.h"
// 场景
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/Node.h>

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
    URHO3D_LOGINFO("EditorApp::Start");
    // 设置鼠标模式为绝对坐标，允许鼠标移出窗口
    GetSubsystem<Input>()->SetMouseMode(MM_ABSOLUTE);
    GetSubsystem<Input>()->SetMouseVisible(true);
    
    // 注册选择服务并创建演示 Scene
    context_->RegisterSubsystem(new SelectionService(context_));
    CreateScene();
    CreateUI();
}

EditorApp::~EditorApp() = default;

void EditorApp::Stop()
{
}

void EditorApp::CreateUI()
{
    URHO3D_LOGINFO("EditorApp::CreateUI: begin");
    auto* cache = GetSubsystem<ResourceCache>();
    auto* ui = GetSubsystem<UI>();
    // 根使用垂直布局：上-菜单栏，中-工作区（占剩余空间）
    ui->GetRoot()->SetLayout(LM_VERTICAL, 2);

    // 加载默认样式
    if (auto* style = cache->GetResource<XMLFile>("UI/DefaultStyle.xml"))
        ui->GetRoot()->SetDefaultStyle(style);

    // 顶部菜单条占位
    SharedPtr<BorderImage> menubar(new BorderImage(context_));
    menubar->SetLayout(LM_HORIZONTAL);
    menubar->SetStyleAuto();
    menubar->SetColor(Color(0.20f, 0.20f, 0.25f));
    menubar->SetMinHeight(28);
    menubar->SetMaxHeight(28);
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
    // 不使用自动样式，避免应用UI纹理
    workspace->SetColor(Color(0.10f, 0.10f, 0.12f, 1.0f));
    workspace->SetLayoutFlexScale(Vector2(1.0f, 1.0f));
    ui->GetRoot()->AddChild(workspace);

    // 在工作区内建立布局：上（内容行），下（日志）
    workspace->SetLayout(LM_VERTICAL, 2);
    auto* contentRow = workspace->CreateChild<UIElement>("ContentRow");
    contentRow->SetLayout(LM_HORIZONTAL, 2);
    contentRow->SetLayoutFlexScale(Vector2(1.0f, 1.0f));

    auto* logContainer = workspace->CreateChild<UIElement>("LogContainer");
    logContainer->SetLayout(LM_VERTICAL);
    logContainer->SetMinHeight(160);
    logContainer->SetMaxHeight(160);

    // 左列：层级 + 资源
    auto* leftColumn = contentRow->CreateChild<UIElement>("LeftColumn");
    leftColumn->SetLayout(LM_VERTICAL, 2);
    leftColumn->SetMinWidth(280);
    leftColumn->SetMaxWidth(280);

    // 中列：视口占位
    auto* centerColumn = contentRow->CreateChild<BorderImage>("CenterColumn");
    centerColumn->SetStyleAuto();
    centerColumn->SetColor(Color(0.12f, 0.12f, 0.14f));
    centerColumn->SetLayout(LM_VERTICAL);
    centerColumn->SetLayoutFlexScale(Vector2(1.0f, 1.0f));
    {
        auto* vpTitle = centerColumn->CreateChild<Text>();
        vpTitle->SetStyleAuto();
        vpTitle->SetText("Viewport (WIP)");
    }

    // 右列：检视
    auto* rightColumn = contentRow->CreateChild<UIElement>("RightColumn");
    rightColumn->SetLayout(LM_VERTICAL, 2);
    rightColumn->SetMinWidth(300);
    rightColumn->SetMaxWidth(300);

    // 在UI元素创建完成后设置光标
    auto* cursor = ui->GetRoot()->CreateChild<Cursor>();
    cursor->SetStyleAuto();
    ui->SetCursor(cursor);

    // 实例化并挂载各面板
    {
        auto* hierarchyContainer = leftColumn->CreateChild<UIElement>("HierarchyContainer");
        hierarchyContainer->SetLayout(LM_VERTICAL);
        hierarchyContainer->SetLayoutFlexScale(Vector2(1.0f, 1.0f));

        auto* resourceContainer = leftColumn->CreateChild<UIElement>("ResourceContainer");
        resourceContainer->SetLayout(LM_VERTICAL);
        resourceContainer->SetLayoutFlexScale(Vector2(1.0f, 1.0f));

        auto* inspectorContainer = rightColumn->CreateChild<UIElement>("InspectorContainer");
        inspectorContainer->SetLayout(LM_VERTICAL);
        inspectorContainer->SetLayoutFlexScale(Vector2(1.0f, 1.0f));

        hierarchyPanel_ = MakeShared<HierarchyPanel>(context_, hierarchyContainer, scene_);
        resourcePanel_ = MakeShared<ResourcePanel>(context_, resourceContainer);
        inspectorPanel_ = MakeShared<InspectorPanel>(context_, inspectorContainer);
        logPanel_ = MakeShared<LogPanel>(context_, logContainer);
    }
    // 强制一次布局计算，确保锚点与分栏立即生效
    ui->GetRoot()->UpdateLayout();
    // 进一步触发布局：让嵌套容器计算子元素尺寸
    workspace->UpdateLayout();
    contentRow->UpdateLayout();
    leftColumn->UpdateLayout();
    centerColumn->UpdateLayout();
    rightColumn->UpdateLayout();
    URHO3D_LOGINFOF("EditorApp::CreateUI: root size %dx%d", ui->GetRoot()->GetWidth(), ui->GetRoot()->GetHeight());
    // ���ӵ�����־�ı���������ȷ�� UI �Ƿ�ͼ��
    auto* dbg = ui->GetRoot()->CreateChild<Text>("DbgText");
    dbg->SetText("UI OK - Debug Text");
    dbg->SetStyleAuto();
    dbg->SetPosition(10, 40);

    // 自适应窗口变化
    // SubscribeToEvent(E_SCREENMODE, [this, workspace](StringHash, VariantMap&){
        // auto* gfx = GetSubsystem<Graphics>();
        // workspace->SetSize(gfx->GetWidth(), gfx->GetHeight() - 28);
    // });
}

void EditorApp::CreateScene()
{
    // 创建简单演示场景
    scene_ = new Scene(context_);
    scene_->CreateComponent<Octree>();

    // 添加一些示例节点
    for (int i = 0; i < 5; ++i)
    {
        Node* n = scene_->CreateChild("Node_" + String(i));
        for (int j = 0; j < 2; ++j)
            n->CreateChild("Child_" + String(i) + "_" + String(j));
    }
}

} // namespace Urho3D

// 将入口宏放在全局命名空间，确保在 Win32 子系统下导出全局 WinMain 符号
URHO3D_DEFINE_APPLICATION_MAIN(Urho3D::EditorApp)

