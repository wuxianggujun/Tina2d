// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include <Urho3D/Precompiled.h>

#include <Urho3D/Core/Context.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/UI/UIElement.h>
#include <Urho3D/UI/ListView.h>
#include <Urho3D/UI/UIEvents.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/IO/Log.h>

#include "HierarchyPanel.h"
#include "../Services/SelectionService.h"

namespace Urho3D
{

HierarchyPanel::HierarchyPanel(Context* context, UIElement* parent, Scene* scene)
    : Object(context)
    , scene_(scene)
{
    selection_ = context_->GetSubsystem<SelectionService>();

    list_ = parent->CreateChild<ListView>("HierarchyList");
    list_->SetStyleAuto();
    // 让列表填充父容器大小
    list_->SetEnableAnchor(true);
    list_->SetMinAnchor(0.0f, 0.0f);
    list_->SetMaxAnchor(1.0f, 1.0f);
    list_->SetMinOffset(IntVector2(0, 0));
    list_->SetMaxOffset(IntVector2(0, 0));
    list_->SetHierarchyMode(true);
    list_->SetSelectOnClickEnd(true);
    SubscribeToEvent(list_, E_ITEMCLICKED, URHO3D_HANDLER(HierarchyPanel, HandleItemClick));

    Rebuild();
}

void HierarchyPanel::Rebuild()
{
    list_->RemoveAllItems();
    if (!scene_)
        return;
    // 根节点显示
    auto* rootItem = new UIElement(context_);
    rootItem->SetMinHeight(20);
    auto* t = rootItem->CreateChild<Text>();
    t->SetStyleAuto();
    t->SetText("Scene");
    list_->AddItem(rootItem);
    // 子节点
    for (const auto& child : scene_->GetChildren())
        AddNodeItem(child, rootItem);
    // ListView 不提供一次性 ExpandAll，这里不处理
}

void HierarchyPanel::AddNodeItem(Node* node, UIElement* parentItem)
{
    auto* item = new UIElement(context_);
    item->SetMinHeight(20);
    auto* t = item->CreateChild<Text>();
    t->SetStyleAuto();
    t->SetText(node->GetName().Empty() ? String("Node ") + String(node->GetID()) : node->GetName());
    item->SetVar(StringHash("NodePtr"), node);
    // 插入为 parentItem 的子项
    list_->InsertItem(list_->GetNumItems(), item, parentItem);
    for (const auto& child : node->GetChildren())
        AddNodeItem(child, item);
}

void HierarchyPanel::HandleItemClick(StringHash, VariantMap&)
{
    UIElement* sel = list_->GetSelectedItem();
    if (!sel)
        return;
    Node* node = static_cast<Node*>(sel->GetVar(StringHash("NodePtr")).GetPtr());
    if (node && selection_)
        selection_->SelectNode(node);
}

void HierarchyPanel::HandleSelectionChanged(StringHash, VariantMap&)
{
    // TODO: 根据选择联动高亮，当前占位
}

} // namespace Urho3D
