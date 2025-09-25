// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include <Urho3D/Core/Object.h>

namespace Urho3D
{

class UIElement;
class Text;

/// 检视面板：显示选中对象的基础信息
class InspectorPanel final : public Object
{
    URHO3D_OBJECT(InspectorPanel, Object);
public:
    InspectorPanel(Context* context, UIElement* parent);

private:
    void HandleSelectionChanged(StringHash, VariantMap&);
    Text* title_{};
};

} // namespace Urho3D

