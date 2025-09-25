// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include <Urho3D/Precompiled.h>

#include <Urho3D/UI/UIElement.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/Scene/Node.h>

#include "InspectorPanel.h"
#include "../Services/SelectionService.h"

namespace Urho3D
{

InspectorPanel::InspectorPanel(Context* context, UIElement* parent)
    : Object(context)
{
    title_ = parent->CreateChild<Text>("InspectorTitle");
    title_->SetStyleAuto();
    title_->SetText("未选中对象");

    SubscribeToEvent(E_EDITOR_SELECTION_CHANGED, URHO3D_HANDLER(InspectorPanel, HandleSelectionChanged));
}

void InspectorPanel::HandleSelectionChanged(StringHash, VariantMap& data)
{
    Node* node = static_cast<Node*>(data[P_NODE].GetPtr());
    if (!node)
    {
        title_->SetText("未选中对象");
        return;
    }
    String label = "选中：";
    label += node->GetName().Empty() ? String("Node ") + String(node->GetID()) : node->GetName();
    title_->SetText(label);
}

} // namespace Urho3D

