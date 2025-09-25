// Copyright (c) 2024 Tina2D project
// Stub implementation of Console for RmlUI mode

#include "../Precompiled.h"
#include "Console.h"
#include "../Core/Context.h"
#include "../IO/Log.h"

// 为了在 RmlUI 模式下避免链接原生 UI，定义最小占位类，
// 仅用于满足 SharedPtr<BorderImage>/SharedPtr<Button> 的完整类型要求。
// 不会被实例化，也不会参与实际逻辑。
namespace Urho3D { class BorderImage : public RefCounted {}; class Button : public RefCounted {}; }

namespace Urho3D
{

Console::Console(Context* context) : Object(context)
{
    URHO3D_LOGINFO("Console stub created (RmlUI mode)");
}

Console::~Console() = default;

void Console::SetDefaultStyle(XMLFile* /*style*/) {}
void Console::SetVisible(bool /*enable*/) {}
void Console::Toggle() {}
void Console::SetNumBufferedRows(i32 /*rows*/) {}
void Console::SetNumHistoryRows(i32 /*rows*/) {}
void Console::SetNumRows(i32 /*rows*/) {}
void Console::SetFocusOnShow(bool /*enable*/) {}
void Console::AddAutoComplete(const String& /*option*/) {}
void Console::RemoveAutoComplete(const String& /*option*/) {}
void Console::UpdateElements() {}
XMLFile* Console::GetDefaultStyle() const { return nullptr; }
void Console::CopySelectedRows() const {}
bool Console::IsVisible() const { return false; }
i32 Console::GetNumBufferedRows() const { return 0; }
const String& Console::GetHistoryRow(i32 /*index*/) const { static String empty; return empty; }

}


