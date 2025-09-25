// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include <Urho3D/Precompiled.h>

#include <Urho3D/UI/UIElement.h>
#include <Urho3D/UI/ListView.h>
#include <Urho3D/UI/Text.h>

#include "ResourcePanel.h"

namespace Urho3D
{

ResourcePanel::ResourcePanel(Context* context, UIElement* parent)
    : Object(context)
{
    list_ = parent->CreateChild<ListView>("ResourceList");
    list_->SetStyleAuto();
    const char* cats[] = {"Textures/", "UI/", "Fonts/", "Techniques/", "Materials/"};
    for (auto* c : cats)
    {
        auto* t = new Text(context_);
        t->SetStyleAuto();
        t->SetText(c);
        list_->AddItem(t);
    }
}

} // namespace Urho3D

