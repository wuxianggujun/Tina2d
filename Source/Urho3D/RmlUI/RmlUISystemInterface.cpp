// Copyright (c) 2024 Tina2D project
// SPDX-License-Identifier: MIT

#include "../Precompiled.h"
#include "RmlUISystemInterface.h"
#include "../Core/Timer.h"
#include "../IO/Log.h"
#include "../Input/Input.h"
#include <SDL3/SDL.h>

namespace Urho3D
{

RmlUISystemInterface::RmlUISystemInterface(Context* context)
    : Object(context)
    , startTime_(0)
{
    time_ = GetSubsystem<Time>();
    if (time_)
        startTime_ = time_->GetSystemTime();
}

RmlUISystemInterface::~RmlUISystemInterface()
{
}

double RmlUISystemInterface::GetElapsedTime()
{
    if (!time_)
        return 0.0;
    
    unsigned currentTime = time_->GetSystemTime();
    return (currentTime - startTime_) / 1000.0; // 转换为秒
}

void RmlUISystemInterface::SetClipboardText(const Rml::String& text)
{
    SDL_SetClipboardText(text.c_str());
}

void RmlUISystemInterface::GetClipboardText(Rml::String& text)
{
    char* clipboardText = SDL_GetClipboardText();
    if (clipboardText)
    {
        text = clipboardText;
        SDL_free(clipboardText);
    }
    else
    {
        text.clear();
    }
}

void RmlUISystemInterface::SetMouseCursor(const Rml::String& cursor_name)
{
    // TODO: 实现鼠标光标设置
    // 可以根据 cursor_name 设置不同的光标样式
    auto* input = GetSubsystem<Input>();
    if (input)
    {
        if (cursor_name == "none")
            input->SetMouseVisible(false);
        else
            input->SetMouseVisible(true);
    }
}

int RmlUISystemInterface::TranslateString(Rml::String& translated, const Rml::String& input)
{
    // 暂时不实现翻译功能，直接返回原文
    translated = input;
    return 0;
}

bool RmlUISystemInterface::LogMessage(Rml::Log::Type type, const Rml::String& message)
{
    switch (type)
    {
    case Rml::Log::LT_ERROR:
        URHO3D_LOGERRORF("[RmlUI] %s", message.c_str());
        break;
    case Rml::Log::LT_WARNING:
        URHO3D_LOGWARNINGF("[RmlUI] %s", message.c_str());
        break;
    case Rml::Log::LT_INFO:
        URHO3D_LOGINFOF("[RmlUI] %s", message.c_str());
        break;
    case Rml::Log::LT_DEBUG:
        URHO3D_LOGDEBUGF("[RmlUI] %s", message.c_str());
        break;
    default:
        URHO3D_LOGINFOF("[RmlUI] %s", message.c_str());
        break;
    }
    return true;
}

void RmlUISystemInterface::ActivateKeyboard(Rml::Vector2f caret_position, float line_height)
{
    // TODO: 在移动设备上激活软键盘
    // 这里需要根据平台实现
#ifdef __ANDROID__
    // Android 平台的软键盘激活
#elif defined(__IOS__)
    // iOS 平台的软键盘激活
#endif
}

void RmlUISystemInterface::DeactivateKeyboard()
{
    // TODO: 在移动设备上关闭软键盘
#ifdef __ANDROID__
    // Android 平台的软键盘关闭
#elif defined(__IOS__)
    // iOS 平台的软键盘关闭
#endif
}

} // namespace Urho3D