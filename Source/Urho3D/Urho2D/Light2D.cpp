// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../Urho2D/Light2D.h"
#include "../Urho2D/Urho2D.h"

#include "../DebugNew.h"

namespace Urho3D
{

extern const char* URHO2D_CATEGORY;

Light2D::Light2D(Context* context) : Component(context)
{
}

void Light2D::RegisterObject(Context* context)
{
    context->RegisterFactory<Light2D>(URHO2D_CATEGORY);

    URHO3D_ACCESSOR_ATTRIBUTE("Type", GetLightTypeAttr, SetLightTypeAttr, 1, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Color", color_, Color::WHITE, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Intensity", intensity_, 1.0f, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Radius", radius_, 2.0f, AM_DEFAULT);
}

}
