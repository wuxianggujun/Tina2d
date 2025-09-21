// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include "../Core/Context.h"
#include "../Scene/Component.h"
#include "../Math/Color.h"

namespace Urho3D
{

/// 2D 轻量光源组件（仅用于 2.5D 效果的顶点色调制）。
class URHO3D_API Light2D : public Component
{
    URHO3D_OBJECT(Light2D, Component);

public:
    enum Type
    {
        DIRECTIONAL = 0,
        POINT
    };

    explicit Light2D(Context* context);
    ~Light2D() override = default;

    static void RegisterObject(Context* context);

    void SetLightType(Type t) { type_ = t; }
    void SetColor(const Color& c) { color_ = c; }
    void SetIntensity(float i) { intensity_ = i; }
    void SetRadius(float r) { radius_ = Max(0.0f, r); }

    Type GetLightType() const { return type_; }
    const Color& GetColor() const { return color_; }
    float GetIntensity() const { return intensity_; }
    float GetRadius() const { return radius_; }

    // 用于属性系统的 int 适配器
    void SetLightTypeAttr(int v) { type_ = (Type)Clamp(v, 0, 1); }
    int GetLightTypeAttr() const { return (int)type_; }

private:
    Type type_ { POINT };
    Color color_ { Color::WHITE };
    float intensity_ { 1.0f };
    float radius_ { 2.0f };
};

}
