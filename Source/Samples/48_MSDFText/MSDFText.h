// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include "../Sample.h"

namespace Urho3D
{
class UIElement;
}

class MSDFText : public Sample
{
    URHO3D_OBJECT(MSDFText, Sample);
public:
    explicit MSDFText(Context* context);
    void Start() override;

private:
    void CreateTextDemo(bool msdfAvailable);

private:
    SharedPtr<UIElement> uielement_;
};

