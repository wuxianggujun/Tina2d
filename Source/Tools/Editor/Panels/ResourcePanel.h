// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include <Urho3D/Core/Object.h>

namespace Urho3D
{

class UIElement;
class ListView;

/// 简易资源面板：示例列出常用资源类别
class ResourcePanel final : public Object
{
    URHO3D_OBJECT(ResourcePanel, Object);
public:
    ResourcePanel(Context* context, UIElement* parent);

private:
    ListView* list_{};
};

} // namespace Urho3D

