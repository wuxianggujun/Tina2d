// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include <Urho3D/Precompiled.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/ProcessUtils.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/UI.h>

#include "HelloWorld.h"

#include <Urho3D/DebugNew.h>

#ifdef _MSC_VER
#include <iostream>
#include <mimalloc.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif
#endif

// 内存调试辅助函数
namespace MemoryDebugHelper {
    void PrintMemoryStats() {
#ifdef _MSC_VER
#ifdef _DEBUG
        // 获取当前分配计数
        _CrtMemState memState;
        _CrtMemCheckpoint(&memState);
        std::cout << "=== Memory Debug Info ===" << std::endl;
        std::cout << "Current allocations: " << memState.lCounts[_NORMAL_BLOCK] << std::endl;
        std::cout << "Current bytes: " << memState.lSizes[_NORMAL_BLOCK] << std::endl;
        std::cout << "=========================" << std::endl;
#endif
        
        // 打印 mimalloc 统计信息
        std::cout << "=== Mimalloc Stats ===" << std::endl;
        mi_stats_print(nullptr);
        std::cout << "======================" << std::endl;
#endif
    }
    
    void SetBreakOnAlloc(long allocNumber) {
#if defined(_MSC_VER) && defined(_DEBUG)
        if (allocNumber > 0) {
            _CrtSetBreakAlloc(allocNumber);
            std::cout << "Set breakpoint on allocation #" << allocNumber << std::endl;
        }
#endif
    }
}

// Expands to this example's entry-point
URHO3D_DEFINE_APPLICATION_MAIN(HelloWorld)

HelloWorld::HelloWorld(Context* context) :
    Sample(context)
{
}

HelloWorld::~HelloWorld()
{
    // 析构时打印最终的内存统计
#ifdef _DEBUG
    std::cout << "\n=== HelloWorld Destructor - Final Memory Stats ===" << std::endl;
    MemoryDebugHelper::PrintMemoryStats();
    std::cout << "================================================\n" << std::endl;
#endif
}

void HelloWorld::Start()
{
    // 启动时的内存调试
#ifdef _DEBUG
    std::cout << "\n=== HelloWorld Start - Memory Debug ===" << std::endl;
    
    // 可选：设置在特定分配编号处中断（用于定位泄漏）
    // 使用方法：运行一次，查看泄漏报告中的分配编号，然后取消注释并设置
    // MemoryDebugHelper::SetBreakOnAlloc(123);  // 替换 123 为实际的分配编号
    
    // 打印启动前的内存状态
    std::cout << "Before Sample::Start():" << std::endl;
    MemoryDebugHelper::PrintMemoryStats();
#endif

    // Execute base class startup
    Sample::Start();

#ifdef _DEBUG
    std::cout << "After Sample::Start():" << std::endl;
    MemoryDebugHelper::PrintMemoryStats();
#endif

    // Create "Hello World" Text
    CreateText();

#ifdef _DEBUG
    std::cout << "After CreateText():" << std::endl;
    MemoryDebugHelper::PrintMemoryStats();
    std::cout << "======================================\n" << std::endl;
#endif

    // Finally subscribe to the update event. Note that by subscribing events at this point we have already missed some events
    // like the ScreenMode event sent by the Graphics subsystem when opening the application window. To catch those as well we
    // could subscribe in the constructor instead.
    SubscribeToEvents();

    // Set the mouse mode to use in the sample
    Sample::InitMouseMode(MM_FREE);
}

void HelloWorld::CreateText()
{
    auto* cache = GetSubsystem<ResourceCache>();

    // Construct new Text object
    SharedPtr<Text> helloText(new Text(context_));

    // Set String to display
    helloText->SetText("Hello World from Urho3D!");

    // Set font and text color
    helloText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 30);
    helloText->SetColor(Color(0.0f, 1.0f, 0.0f));

    // Align Text center-screen
    helloText->SetHorizontalAlignment(HA_CENTER);
    helloText->SetVerticalAlignment(VA_CENTER);

    // Add Text instance to the UI root element
    GetSubsystem<UI>()->GetRoot()->AddChild(helloText);
}

void HelloWorld::SubscribeToEvents()
{
    // HelloWorld 示例不需要处理 Update 事件，专注于静态文本显示
}
