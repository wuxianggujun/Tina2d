// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include <Urho3D/Engine/Application.h>

namespace Urho3D
{

class EditorApp final : public Application
{
    URHO3D_OBJECT(EditorApp, Application);
public:
    explicit EditorApp(Context* context);

    void Setup() override;
    void Start() override;
    void Stop() override;

private:
    void CreateUI();
};

} // namespace Urho3D

