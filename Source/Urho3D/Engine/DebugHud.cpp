// Copyright (c) 2024 Tina2D project  
// Stub implementation of DebugHud for RmlUI mode

#include "../Precompiled.h"
#include "../Container/FlagSet.h"
#include "DebugHud.h"
#include "../Core/Context.h"
#include "../IO/Log.h"

namespace Urho3D
{

DebugHud::DebugHud(Context* context) : Object(context)
{
    URHO3D_LOGINFO("DebugHud stub created (RmlUI mode)");
}

DebugHud::~DebugHud() = default;

void DebugHud::Update() {}
void DebugHud::SetDefaultStyle(XMLFile* style) {}
void DebugHud::SetMode(DebugHudElements elements) {}
void DebugHud::SetProfilerMaxDepth(unsigned /*depth*/) {}
void DebugHud::SetProfilerInterval(float interval) {}
void DebugHud::SetUseRendererStats(bool /*enable*/) {}
void DebugHud::Toggle(DebugHudElements element) {}
void DebugHud::ToggleAll() {}
void DebugHud::SetAppStats(const String& /*label*/, const Variant& /*stats*/) {}
void DebugHud::SetAppStats(const String& /*label*/, const String& /*stats*/) {}
bool DebugHud::ResetAppStats(const String& /*label*/) { return false; }
void DebugHud::ClearAppStats() {}
XMLFile* DebugHud::GetDefaultStyle() const { return nullptr; }
float DebugHud::GetProfilerInterval() const { return 0.0f; }

}


