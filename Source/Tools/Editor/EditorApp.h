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
    ~EditorApp() override;

    void Setup() override;
    void Start() override;
    void Stop() override;

private:
    void CreateUI();
    void CreateScene();

private:
    SharedPtr<class Scene> scene_;
    SharedPtr<class HierarchyPanel> hierarchyPanel_;
    SharedPtr<class InspectorPanel> inspectorPanel_;
    SharedPtr<class ResourcePanel> resourcePanel_;
    SharedPtr<class LogPanel> logPanel_;
};

} // namespace Urho3D
