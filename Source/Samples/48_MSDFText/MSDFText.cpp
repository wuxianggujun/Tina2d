// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/ProcessUtils.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/UIElement.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Resource/XMLFile.h>
#include <Urho3D/IO/Log.h>

#include "MSDFText.h"

#include <Urho3D/DebugNew.h>

URHO3D_DEFINE_APPLICATION_MAIN(MSDFText)

MSDFText::MSDFText(Context* context)
    : Sample(context)
{
}

void MSDFText::Start()
{
    Sample::Start();

    // 鼠标可见
    GetSubsystem<Input>()->SetMouseVisible(true);

    // 默认 UI 样式
    auto* cache = GetSubsystem<ResourceCache>();
    auto* style = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");
    auto* ui = GetSubsystem<UI>();
    ui->GetRoot()->SetDefaultStyle(style);

    // 容器
    uielement_ = new UIElement(context_);
    uielement_->SetAlignment(HA_CENTER, VA_CENTER);
    uielement_->SetLayout(LM_VERTICAL, 8, IntRect(20, 40, 20, 40));
    ui->GetRoot()->AddChild(uielement_);

    // 先尝试加载 MSDF 资源名（约定：文件名含 msdf 则走 MSDF 管线）
    const String msdfName = "Fonts/BlueHighway_msdf.sdf";   // 用户可自备 MSDF 字体图集与映射
    const String sdfName  = "Fonts/BlueHighway.sdf";        // 回退到 SDF 示例

    bool hasMsdf = cache->Exists(msdfName);
    CreateTextDemo(hasMsdf);

    Sample::InitMouseMode(MM_FREE);
}

void MSDFText::CreateTextDemo(bool msdfAvailable)
{
    auto* cache = GetSubsystem<ResourceCache>();
    auto* ui = GetSubsystem<UI>();

    // 标题行
    {
        SharedPtr<Text> title(new Text(context_));
        title->SetStyleAuto();
        title->SetHorizontalAlignment(HA_CENTER);
        title->SetText(msdfAvailable ?
            "MSDF 演示（检测到 Fonts/BlueHighway_msdf.sdf）" :
            "MSDF 演示（未检测到 MSDF 资源，回退到 SDF）");
        uielement_->AddChild(title);

        SharedPtr<Text> hint(new Text(context_));
        hint->SetStyleAuto();
        hint->SetHorizontalAlignment(HA_CENTER);
        hint->SetText("提示：放置 MSDF 字体（文件名包含 msdf）到 Fonts/ 以启用 MSDF 管线");
        uielement_->AddChild(hint);
    }

    // 文本列表：从小字号到大字号，观察清晰度
    const char* sampleStr = "The quick brown fox jumps over the lazy dog 0123456789";
    const String fontName = msdfAvailable ? String("Fonts/BlueHighway_msdf.sdf") : String("Fonts/BlueHighway.sdf");

    for (int pt = 8; pt <= 48; pt += 4)
    {
        SharedPtr<Text> t(new Text(context_));
        t->SetText(String(sampleStr) + "  (" + String(pt) + "pt) ");
        t->SetFont(GetSubsystem<ResourceCache>()->GetResource<Font>(fontName), (float)pt);
        t->SetStyleAuto();
        uielement_->AddChild(t);
    }

    // 说明
    {
        SharedPtr<Text> desc(new Text(context_));
        desc->SetStyleAuto();
        desc->SetText("说明：\n - MSDF：使用 RGB 距离场，放大/旋转下边缘更锐利\n - SDF：单通道距离场，小字号也清晰，但尖角可能略软\n - 本引擎会基于字体名是否包含 'msdf' 自动切换着色器");
        uielement_->AddChild(desc);
    }
}
