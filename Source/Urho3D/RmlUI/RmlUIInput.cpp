// Copyright (c) 2024 Tina2D project
// SPDX-License-Identifier: MIT

#include "../Precompiled.h"
#include "RmlUIInput.h"
#include "../Input/Input.h"
#include "../Input/InputEvents.h"
#include "../IO/Log.h"
#include <RmlUi/Core/Context.h>

namespace Urho3D
{

RmlUIInput::RmlUIInput(Context* context)
    : Object(context)
    , rmlContext_(nullptr)
    , subscribed_(false)
{
    input_ = GetSubsystem<Input>();
}

RmlUIInput::~RmlUIInput()
{
    UnsubscribeFromEvents();
}

bool RmlUIInput::ProcessMouseMove(int x, int y)
{
    if (!rmlContext_)
        return false;
    
    return rmlContext_->ProcessMouseMove(x, y, GetKeyModifiers());
}

bool RmlUIInput::ProcessMouseButton(MouseButton button, bool down)
{
    if (!rmlContext_)
        return false;
    
    int rmlButton = ConvertMouseButton(button);
    
    if (down)
        return rmlContext_->ProcessMouseButtonDown(rmlButton, GetKeyModifiers());
    else
        return rmlContext_->ProcessMouseButtonUp(rmlButton, GetKeyModifiers());
}

bool RmlUIInput::ProcessMouseWheel(int delta)
{
    if (!rmlContext_)
        return false;
    
    float wheelDelta = delta > 0 ? 1.0f : -1.0f;
    return rmlContext_->ProcessMouseWheel(wheelDelta, GetKeyModifiers());
}

bool RmlUIInput::ProcessKeyEvent(Key key, bool down)
{
    if (!rmlContext_)
        return false;
    
    Rml::Input::KeyIdentifier rmlKey = ConvertKey(key);
    
    if (down)
        return rmlContext_->ProcessKeyDown(rmlKey, GetKeyModifiers());
    else
        return rmlContext_->ProcessKeyUp(rmlKey, GetKeyModifiers());
}

bool RmlUIInput::ProcessTextInput(const String& text)
{
    if (!rmlContext_)
        return false;
    
    bool result = false;
    for (unsigned i = 0; i < text.Length(); ++i)
    {
        result |= rmlContext_->ProcessTextInput(text[i]);
    }
    return result;
}

bool RmlUIInput::ProcessTouch(unsigned id, int x, int y, bool down)
{
    if (!rmlContext_)
        return false;
    
    // 将触摸事件转换为鼠标事件
    // TODO: 实现多点触控支持
    if (id == 0) // 只处理第一个触摸点
    {
        ProcessMouseMove(x, y);
        return ProcessMouseButton(MOUSEB_LEFT, down);
    }
    
    return false;
}

void RmlUIInput::SubscribeToEvents()
{
    if (subscribed_)
        return;
    
    SubscribeToEvent(E_MOUSEMOVE, URHO3D_HANDLER(RmlUIInput, HandleMouseMove));
    SubscribeToEvent(E_MOUSEBUTTONDOWN, URHO3D_HANDLER(RmlUIInput, HandleMouseButtonDown));
    SubscribeToEvent(E_MOUSEBUTTONUP, URHO3D_HANDLER(RmlUIInput, HandleMouseButtonUp));
    SubscribeToEvent(E_MOUSEWHEEL, URHO3D_HANDLER(RmlUIInput, HandleMouseWheel));
    SubscribeToEvent(E_KEYDOWN, URHO3D_HANDLER(RmlUIInput, HandleKeyDown));
    SubscribeToEvent(E_KEYUP, URHO3D_HANDLER(RmlUIInput, HandleKeyUp));
    SubscribeToEvent(E_TEXTINPUT, URHO3D_HANDLER(RmlUIInput, HandleTextInput));
    SubscribeToEvent(E_TOUCHBEGIN, URHO3D_HANDLER(RmlUIInput, HandleTouchBegin));
    SubscribeToEvent(E_TOUCHMOVE, URHO3D_HANDLER(RmlUIInput, HandleTouchMove));
    SubscribeToEvent(E_TOUCHEND, URHO3D_HANDLER(RmlUIInput, HandleTouchEnd));
    
    subscribed_ = true;
}

void RmlUIInput::UnsubscribeFromEvents()
{
    if (!subscribed_)
        return;
    
    UnsubscribeFromAllEvents();
    subscribed_ = false;
}

void RmlUIInput::HandleMouseMove(StringHash eventType, VariantMap& eventData)
{
    using namespace MouseMove;
    int x = eventData[P_X].GetInt();
    int y = eventData[P_Y].GetInt();
    
    if (ProcessMouseMove(x, y))
        eventData[P_CONSUMED] = true;
}

void RmlUIInput::HandleMouseButtonDown(StringHash eventType, VariantMap& eventData)
{
    using namespace MouseButtonDown;
    MouseButton button = (MouseButton)eventData[P_BUTTON].GetInt();
    
    if (ProcessMouseButton(button, true))
        eventData[P_CONSUMED] = true;
}

void RmlUIInput::HandleMouseButtonUp(StringHash eventType, VariantMap& eventData)
{
    using namespace MouseButtonUp;
    MouseButton button = (MouseButton)eventData[P_BUTTON].GetInt();
    
    if (ProcessMouseButton(button, false))
        eventData[P_CONSUMED] = true;
}

void RmlUIInput::HandleMouseWheel(StringHash eventType, VariantMap& eventData)
{
    using namespace MouseWheel;
    int wheel = eventData[P_WHEEL].GetInt();
    
    if (ProcessMouseWheel(wheel))
        eventData[P_CONSUMED] = true;
}

void RmlUIInput::HandleKeyDown(StringHash eventType, VariantMap& eventData)
{
    using namespace KeyDown;
    Key key = (Key)eventData[P_KEY].GetInt();
    
    if (ProcessKeyEvent(key, true))
        eventData[P_CONSUMED] = true;
}

void RmlUIInput::HandleKeyUp(StringHash eventType, VariantMap& eventData)
{
    using namespace KeyUp;
    Key key = (Key)eventData[P_KEY].GetInt();
    
    if (ProcessKeyEvent(key, false))
        eventData[P_CONSUMED] = true;
}

void RmlUIInput::HandleTextInput(StringHash eventType, VariantMap& eventData)
{
    using namespace TextInput;
    String text = eventData[P_TEXT].GetString();
    
    if (ProcessTextInput(text))
        eventData[P_CONSUMED] = true;
}

void RmlUIInput::HandleTouchBegin(StringHash eventType, VariantMap& eventData)
{
    using namespace TouchBegin;
    int id = eventData[P_TOUCHID].GetInt();
    int x = eventData[P_X].GetInt();
    int y = eventData[P_Y].GetInt();
    
    if (ProcessTouch(id, x, y, true))
        eventData[P_CONSUMED] = true;
}

void RmlUIInput::HandleTouchMove(StringHash eventType, VariantMap& eventData)
{
    using namespace TouchMove;
    int id = eventData[P_TOUCHID].GetInt();
    int x = eventData[P_X].GetInt();
    int y = eventData[P_Y].GetInt();
    
    if (id == 0) // 只处理第一个触摸点
    {
        if (ProcessMouseMove(x, y))
            eventData[P_CONSUMED] = true;
    }
}

void RmlUIInput::HandleTouchEnd(StringHash eventType, VariantMap& eventData)
{
    using namespace TouchEnd;
    int id = eventData[P_TOUCHID].GetInt();
    int x = eventData[P_X].GetInt();
    int y = eventData[P_Y].GetInt();
    
    if (ProcessTouch(id, x, y, false))
        eventData[P_CONSUMED] = true;
}

Rml::Input::KeyIdentifier RmlUIInput::ConvertKey(Key key)
{
    // 转换常用按键
    switch (key)
    {
    case KEY_SPACE: return Rml::Input::KI_SPACE;
    case KEY_0: return Rml::Input::KI_0;
    case KEY_1: return Rml::Input::KI_1;
    case KEY_2: return Rml::Input::KI_2;
    case KEY_3: return Rml::Input::KI_3;
    case KEY_4: return Rml::Input::KI_4;
    case KEY_5: return Rml::Input::KI_5;
    case KEY_6: return Rml::Input::KI_6;
    case KEY_7: return Rml::Input::KI_7;
    case KEY_8: return Rml::Input::KI_8;
    case KEY_9: return Rml::Input::KI_9;
    case KEY_A: return Rml::Input::KI_A;
    case KEY_B: return Rml::Input::KI_B;
    case KEY_C: return Rml::Input::KI_C;
    case KEY_D: return Rml::Input::KI_D;
    case KEY_E: return Rml::Input::KI_E;
    case KEY_F: return Rml::Input::KI_F;
    case KEY_G: return Rml::Input::KI_G;
    case KEY_H: return Rml::Input::KI_H;
    case KEY_I: return Rml::Input::KI_I;
    case KEY_J: return Rml::Input::KI_J;
    case KEY_K: return Rml::Input::KI_K;
    case KEY_L: return Rml::Input::KI_L;
    case KEY_M: return Rml::Input::KI_M;
    case KEY_N: return Rml::Input::KI_N;
    case KEY_O: return Rml::Input::KI_O;
    case KEY_P: return Rml::Input::KI_P;
    case KEY_Q: return Rml::Input::KI_Q;
    case KEY_R: return Rml::Input::KI_R;
    case KEY_S: return Rml::Input::KI_S;
    case KEY_T: return Rml::Input::KI_T;
    case KEY_U: return Rml::Input::KI_U;
    case KEY_V: return Rml::Input::KI_V;
    case KEY_W: return Rml::Input::KI_W;
    case KEY_X: return Rml::Input::KI_X;
    case KEY_Y: return Rml::Input::KI_Y;
    case KEY_Z: return Rml::Input::KI_Z;
    case KEY_BACKSPACE: return Rml::Input::KI_BACK;
    case KEY_TAB: return Rml::Input::KI_TAB;
    case KEY_RETURN: return Rml::Input::KI_RETURN;
    case KEY_ESCAPE: return Rml::Input::KI_ESCAPE;
    case KEY_DELETE: return Rml::Input::KI_DELETE;
    case KEY_INSERT: return Rml::Input::KI_INSERT;
    case KEY_LEFT: return Rml::Input::KI_LEFT;
    case KEY_RIGHT: return Rml::Input::KI_RIGHT;
    case KEY_UP: return Rml::Input::KI_UP;
    case KEY_DOWN: return Rml::Input::KI_DOWN;
    case KEY_PAGEUP: return Rml::Input::KI_PRIOR;
    case KEY_PAGEDOWN: return Rml::Input::KI_NEXT;
    case KEY_HOME: return Rml::Input::KI_HOME;
    case KEY_END: return Rml::Input::KI_END;
    case KEY_F1: return Rml::Input::KI_F1;
    case KEY_F2: return Rml::Input::KI_F2;
    case KEY_F3: return Rml::Input::KI_F3;
    case KEY_F4: return Rml::Input::KI_F4;
    case KEY_F5: return Rml::Input::KI_F5;
    case KEY_F6: return Rml::Input::KI_F6;
    case KEY_F7: return Rml::Input::KI_F7;
    case KEY_F8: return Rml::Input::KI_F8;
    case KEY_F9: return Rml::Input::KI_F9;
    case KEY_F10: return Rml::Input::KI_F10;
    case KEY_F11: return Rml::Input::KI_F11;
    case KEY_F12: return Rml::Input::KI_F12;
    case KEY_LSHIFT:
    case KEY_RSHIFT: return Rml::Input::KI_LSHIFT;
    case KEY_LCTRL:
    case KEY_RCTRL: return Rml::Input::KI_LCONTROL;
    case KEY_LALT:
    case KEY_RALT: return Rml::Input::KI_LMENU;
    default: return Rml::Input::KI_UNKNOWN;
    }
}

int RmlUIInput::GetKeyModifiers()
{
    if (!input_)
        return 0;
    
    int modifiers = 0;
    
    if (input_->GetKeyDown(KEY_LCTRL) || input_->GetKeyDown(KEY_RCTRL))
        modifiers |= Rml::Input::KM_CTRL;
    
    if (input_->GetKeyDown(KEY_LSHIFT) || input_->GetKeyDown(KEY_RSHIFT))
        modifiers |= Rml::Input::KM_SHIFT;
    
    if (input_->GetKeyDown(KEY_LALT) || input_->GetKeyDown(KEY_RALT))
        modifiers |= Rml::Input::KM_ALT;
    
    // TODO: 添加 Meta/Super 键支持
    
    return modifiers;
}

int RmlUIInput::ConvertMouseButton(MouseButton button)
{
    switch (button)
    {
    case MOUSEB_LEFT: return 0;
    case MOUSEB_RIGHT: return 1;
    case MOUSEB_MIDDLE: return 2;
    case MOUSEB_X1: return 3;
    case MOUSEB_X2: return 4;
    default: return 0;
    }
}

} // namespace Urho3D