// Copyright (c) 2024 Tina2D project
// SPDX-License-Identifier: MIT

#pragma once

#include "../Core/Object.h"
#include <RmlUi/Core.h>

namespace Urho3D
{

class Context;
class ResourceCache;
class Graphics;
class RmlUIRenderer;
class RmlUIFile;
class RmlUISystemInterface;
class RmlUIInput;

/// RmlUI subsystem. Manages the UI rendering and input.
class URHO3D_API RmlUISystem : public Object
{
    URHO3D_OBJECT(RmlUISystem, Object);

public:
    /// Construct.
    explicit RmlUISystem(Context* context);
    /// Destruct.
    ~RmlUISystem() override;

    /// Create a RmlUI context with given dimensions.
    Rml::Context* CreateContext(const String& name, const Vector2& dimensions);
    /// Get a RmlUI context by name.
    Rml::Context* GetContext(const String& name) const;
    /// Load a document from file.
    Rml::ElementDocument* LoadDocument(const String& path, Rml::Context* context = nullptr);
    /// Reload all style sheets.
    void ReloadStyleSheets();
    
    /// Update UI logic.
    void Update(float timeStep);
    /// Render UI.
    void Render();
    
    /// Get the default context.
    Rml::Context* GetDefaultContext() const { return defaultContext_; }
    /// Get input handler.
    RmlUIInput* GetInput() const { return input_.Get(); }
    
    /// Handle screen mode change.
    void HandleScreenModeChanged(StringHash eventType, VariantMap& eventData);
    /// Handle update.
    void HandleUpdate(StringHash eventType, VariantMap& eventData);
    /// Handle render update.
    void HandleRenderUpdate(StringHash eventType, VariantMap& eventData);
    /// Handle post render update.
    void HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData);
    
private:
    /// Initialize RmlUI interfaces.
    bool Initialize();
    /// Shutdown RmlUI.
    void Shutdown();
    
    /// RmlUI renderer interface.
    SharedPtr<RmlUIRenderer> renderer_;
    /// RmlUI file interface.
    SharedPtr<RmlUIFile> fileInterface_;
    /// RmlUI system interface.
    SharedPtr<RmlUISystemInterface> systemInterface_;
    /// Input handler.
    SharedPtr<RmlUIInput> input_;
    
    /// RmlUI contexts.
    HashMap<String, Rml::Context*> contexts_;
    /// Default context.
    Rml::Context* defaultContext_ = nullptr;
    
    /// Resource cache.
    WeakPtr<ResourceCache> cache_;
    /// Graphics subsystem.
    WeakPtr<Graphics> graphics_;
    
    /// RmlUI initialized flag.
    bool initialized_ = false;
};

} // namespace Urho3D