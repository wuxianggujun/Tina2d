// Copyright (c) 2024 Tina2D project
// SPDX-License-Identifier: MIT

#pragma once

#include "../Core/Object.h"
#include <RmlUi/Core/SystemInterface.h>

namespace Urho3D
{

class Context;
class Time;

/// RmlUI system interface implementation.
class RmlUISystemInterface : public Object, public Rml::SystemInterface
{
    URHO3D_OBJECT(RmlUISystemInterface, Object);

public:
    /// Construct.
    explicit RmlUISystemInterface(Context* context);
    /// Destruct.
    ~RmlUISystemInterface() override;

    // Rml::SystemInterface implementation
    /// Get the number of seconds elapsed since the start of the application.
    double GetElapsedTime() override;
    
    /// Get/set clipboard text.
    void SetClipboardText(const Rml::String& text) override;
    void GetClipboardText(Rml::String& text) override;
    
    /// Set mouse cursor.
    void SetMouseCursor(const Rml::String& cursor_name) override;
    
    /// Translate the given string into the specified language.
    int TranslateString(Rml::String& translated, const Rml::String& input) override;
    
    /// Log a message to the system log.
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
    
    /// Activate keyboard (for mobile devices).
    void ActivateKeyboard(Rml::Vector2f caret_position, float line_height) override;
    void DeactivateKeyboard() override;

private:
    /// Time subsystem.
    WeakPtr<Time> time_;
    /// Start time for elapsed time calculation.
    unsigned startTime_ = 0;
};

} // namespace Urho3D