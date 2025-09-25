// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include <Urho3D/Core/Object.h>

namespace Urho3D
{

class UIElement;
class ListView;
class Scene;
class SelectionService;

/// 层级面板：显示 Scene 树，支持点击选中 Node
class HierarchyPanel final : public Object
{
    URHO3D_OBJECT(HierarchyPanel, Object);
public:
    HierarchyPanel(Context* context, UIElement* parent, Scene* scene);

private:
    void Rebuild();
    void AddNodeItem(class Node* node, UIElement* parentItem);
    void HandleSelectionChanged(StringHash, VariantMap&);
    void HandleItemClick(StringHash, VariantMap&);

    ListView* list_{};
    Scene* scene_{};
    SelectionService* selection_{};
};

} // namespace Urho3D
