// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include <Urho3D/Core/Object.h>

namespace Urho3D
{

/// 选中变化事件
static const StringHash E_EDITOR_SELECTION_CHANGED("EditorSelectionChanged");
static const StringHash P_NODE("Node");

/// 简单选中服务：当前仅支持选中一个 Node
class SelectionService final : public Object
{
    URHO3D_OBJECT(SelectionService, Object);
public:
    explicit SelectionService(Context* context) : Object(context) {}

    void SelectNode(class Node* node)
    {
        if (selectedNode_ == node)
            return;
        selectedNode_ = node;
        VariantMap& ev = GetEventDataMap();
        ev[P_NODE] = selectedNode_;
        SendEvent(E_EDITOR_SELECTION_CHANGED, ev);
    }

    class Node* GetSelectedNode() const { return selectedNode_; }

private:
    class Node* selectedNode_{};
};

} // namespace Urho3D

