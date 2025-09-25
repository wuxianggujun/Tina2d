// Copyright (c) 2024 Tina2D project
// SPDX-License-Identifier: MIT

#pragma once

#include "../Core/Object.h"
#include "../Input/InputConstants.h"
#include <RmlUi/Core/Input.h>

namespace Rml { class Context; }

namespace Urho3D
{

class Context;
class Input;

/// RmlUI input adapter for Urho3D input events.
class RmlUIInput : public Object
{
    URHO3D_OBJECT(RmlUIInput, Object);

public:
    /// Construct.
    explicit RmlUIInput(Context* context);
    /// Destruct.
    ~RmlUIInput() override;

    /// Set the active RmlUI context.
    void SetContext(Rml::Context* context) { rmlContext_ = context; }
    /// Get the active context.
    Rml::Context* GetContext() const { return rmlContext_; }
    
    /// Process mouse move event.
    bool ProcessMouseMove(int x, int y);
    /// Process mouse button event.
    bool ProcessMouseButton(MouseButton button, bool down);
    /// Process mouse wheel event.
    bool ProcessMouseWheel(int delta);
    
    /// Process key event.
    bool ProcessKeyEvent(Key key, bool down);
    /// Process text input event.
    bool ProcessTextInput(const String& text);
    
    /// Process touch event.
    bool ProcessTouch(unsigned id, int x, int y, bool down);
    
    /// Subscribe to input events.
    void SubscribeToEvents();
    /// Unsubscribe from input events.
    void UnsubscribeFromEvents();
    
    /// Handle mouse move event.
    void HandleMouseMove(StringHash eventType, VariantMap& eventData);
    /// Handle mouse button down event.
    void HandleMouseButtonDown(StringHash eventType, VariantMap& eventData);
    /// Handle mouse button up event.
    void HandleMouseButtonUp(StringHash eventType, VariantMap& eventData);
    /// Handle mouse wheel event.
    void HandleMouseWheel(StringHash eventType, VariantMap& eventData);
    /// Handle key down event.
    void HandleKeyDown(StringHash eventType, VariantMap& eventData);
    /// Handle key up event.
    void HandleKeyUp(StringHash eventType, VariantMap& eventData);
    /// Handle text input event.
    void HandleTextInput(StringHash eventType, VariantMap& eventData);
    /// Handle touch begin event.
    void HandleTouchBegin(StringHash eventType, VariantMap& eventData);
    /// Handle touch move event.
    void HandleTouchMove(StringHash eventType, VariantMap& eventData);
    /// Handle touch end event.
    void HandleTouchEnd(StringHash eventType, VariantMap& eventData);

private:
    /// Convert Urho3D key to RmlUI key.
    Rml::Input::KeyIdentifier ConvertKey(Key key);
    /// Get key modifiers state.
    int GetKeyModifiers();
    /// Convert mouse button.
    int ConvertMouseButton(MouseButton button);
    
    /// Input subsystem.
    WeakPtr<Input> input_;
    /// Active RmlUI context.
    Rml::Context* rmlContext_ = nullptr;
    /// Subscribed to events flag.
    bool subscribed_ = false;
};

} // namespace Urho3D