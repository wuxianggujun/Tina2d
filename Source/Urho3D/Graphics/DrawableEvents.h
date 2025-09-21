// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include "../Core/Object.h"

namespace Urho3D
{

/// 动画完成或循环结束（用于 2D 动画/Spriter 等）。
URHO3D_EVENT(E_ANIMATIONFINISHED, AnimationFinished)
{
    URHO3D_PARAM(P_NODE, Node);                    // Node pointer
    URHO3D_PARAM(P_ANIMATION, Animation);          // Animation pointer
    URHO3D_PARAM(P_NAME, Name);                    // String
    URHO3D_PARAM(P_LOOPED, Looped);                // Bool
}

}
