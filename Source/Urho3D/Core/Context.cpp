// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "MemoryHooks.h"
#include "../Core/EventProfiler.h"
#include "../IO/Log.h"

#include <EASTL/algorithm.h>

#ifndef MINI_URHO
#include <SDL3/SDL.h>
#endif

#include "../DebugNew.h"

namespace Urho3D
{

#ifndef MINI_URHO
// Keeps track of how many times SDL was initialised so we know when to call SDL_Quit().
static int sdlInitCounter = 0;

#endif


void EventReceiverGroup::BeginSendEvent()
{
    ++inSend_;
}

void EventReceiverGroup::EndSendEvent()
{
    assert(inSend_ > 0);
    --inSend_;

    if (inSend_ == 0 && dirty_)
    {
        /// \todo Could be optimized by erase-swap, but this keeps the receiver order
        for (i32 i = (i32)receivers_.size() - 1; i >= 0; --i)
        {
            if (!receivers_[(size_t)i])
                receivers_.erase(receivers_.begin() + i);
        }

        dirty_ = false;
    }
}

void EventReceiverGroup::Add(Object* object)
{
    if (object)
        receivers_.push_back(object);
}

void EventReceiverGroup::Remove(Object* object)
{
    if (inSend_ > 0)
    {
        auto i = eastl::find(receivers_.begin(), receivers_.end(), object);
        if (i != receivers_.end())
        {
            (*i) = nullptr;
            dirty_ = true;
        }
    }
    else
    {
        auto newEnd = eastl::remove(receivers_.begin(), receivers_.end(), object);
        receivers_.erase(newEnd, receivers_.end());
    }
}

void RemoveNamedAttribute(HashMap<StringHash, Vector<AttributeInfo>>& attributes, StringHash objectType, const char* name)
{
    auto i = attributes.find(objectType);
    if (i == attributes.end())
        return;

    Vector<AttributeInfo>& infos = i->second;

    for (Vector<AttributeInfo>::Iterator j = infos.Begin(); j != infos.End(); ++j)
    {
        if (!j->name_.Compare(name, true))
        {
            infos.Erase(j);
            break;
        }
    }

    // If the vector became empty, erase the object type from the map
    if (infos.Empty())
        attributes.erase(i);
}

Context::Context() :
    eventHandler_(nullptr)
{
    // 深度集成：尽早尝试安装 mimalloc（若可用），失败则回退 CRT
    InstallMimallocAllocator();
#ifdef __ANDROID__
    // Always reset the random seed on Android, as the Urho3D library might not be unloaded between runs
    SetRandomSeed(1);
#endif

    // Set the main thread ID (assuming the Context is created in it)
    Thread::SetMainThread();
}

Context::~Context()
{
    // Remove subsystems that use SDL in reverse order of construction, so that Graphics can shut down SDL last
    /// \todo Context should not need to know about subsystems
    RemoveSubsystem("Audio");
    RemoveSubsystem("UI");
    RemoveSubsystem("Input");
    RemoveSubsystem("Renderer");
    RemoveSubsystem("Graphics");

    subsystems_.clear();
    factories_.clear();

    // Delete allocated event data maps
    for (auto* p : eventDataMaps_)
        delete p;
    eventDataMaps_.clear();
}

SharedPtr<Object> Context::CreateObject(StringHash objectType)
{
    auto i = factories_.find(objectType);
    if (i != factories_.end())
        return i->second->CreateObject();
    else
        return SharedPtr<Object>();
}

void Context::RegisterFactory(ObjectFactory* factory)
{
    if (!factory)
        return;

    factories_[factory->GetType()] = factory;
}

void Context::RegisterFactory(ObjectFactory* factory, const char* category)
{
    if (!factory)
        return;

    RegisterFactory(factory);
    if (String::CStringLength(category))
        objectCategories_[category].push_back(factory->GetType());
}

void Context::RegisterSubsystem(Object* object)
{
    if (!object)
        return;

    subsystems_[object->GetType()] = object;
}

void Context::RemoveSubsystem(StringHash objectType)
{
    auto i = subsystems_.find(objectType);
    if (i != subsystems_.end())
        subsystems_.erase(i);
}

AttributeHandle Context::RegisterAttribute(StringHash objectType, const AttributeInfo& attr)
{
    // None or pointer types can not be supported
    if (attr.type_ == VAR_NONE || attr.type_ == VAR_VOIDPTR || attr.type_ == VAR_PTR
        || attr.type_ == VAR_CUSTOM_HEAP || attr.type_ == VAR_CUSTOM_STACK)
    {
        URHO3D_LOGWARNING("Attempt to register unsupported attribute type " + Variant::GetTypeName(attr.type_) + " to class " +
            GetTypeName(objectType));
        return AttributeHandle();
    }

    AttributeHandle handle;

    Vector<AttributeInfo>& objectAttributes = attributes_[objectType];
    objectAttributes.push_back(attr);
    handle.attributeInfo_ = &objectAttributes.back();

    if (attr.mode_ & AM_NET)
    {
        Vector<AttributeInfo>& objectNetworkAttributes = networkAttributes_[objectType];
        objectNetworkAttributes.push_back(attr);
        handle.networkAttributeInfo_ = &objectNetworkAttributes.back();
    }
    return handle;
}

void Context::RemoveAttribute(StringHash objectType, const char* name)
{
    RemoveNamedAttribute(attributes_, objectType, name);
    RemoveNamedAttribute(networkAttributes_, objectType, name);
}

void Context::RemoveAllAttributes(StringHash objectType)
{
    attributes_.erase(objectType);
    networkAttributes_.erase(objectType);
}

void Context::UpdateAttributeDefaultValue(StringHash objectType, const char* name, const Variant& defaultValue)
{
    AttributeInfo* info = GetAttribute(objectType, name);
    if (info)
        info->defaultValue_ = defaultValue;
}

VariantMap& Context::GetEventDataMap()
{
    unsigned nestingLevel = (unsigned)eventSenders_.size();
    while (eventDataMaps_.size() < nestingLevel + 1)
        eventDataMaps_.push_back(new VariantMap());

    VariantMap& ret = *eventDataMaps_[nestingLevel];
    ret.clear();
    return ret;
}

#ifndef MINI_URHO
bool Context::RequireSDL(unsigned int sdlFlags)
{
    // Always increment, the caller must match with ReleaseSDL(), regardless of
    // what happens.
    ++sdlInitCounter;

    // SDL3：SDL_Init/SDL_InitSubSystem 返回布尔值，true 表示成功。
    // 需要至少调用一次 SDL_Init() 以保证后续子系统初始化正常。
    if (sdlInitCounter == 1)
    {
        URHO3D_LOGDEBUG("Initialising SDL");
        if (!SDL_Init(0))
        {
            URHO3D_LOGERRORF("Failed to initialise SDL: %s", SDL_GetError());
            return false;
        }
    }

    Uint32 remainingFlags = sdlFlags & ~SDL_WasInit(0);
    if (remainingFlags != 0)
    {
        if (!SDL_InitSubSystem(remainingFlags))
        {
            URHO3D_LOGERRORF("Failed to initialise SDL subsystem: %s", SDL_GetError());
            return false;
        }
    }

    return true;
}

void Context::ReleaseSDL()
{
    --sdlInitCounter;

    if (sdlInitCounter == 0)
    {
        URHO3D_LOGDEBUG("Quitting SDL");
        // SDL3 无 SDL_INIT_EVERYTHING，按子系统位掩码统一退出
        const Uint32 kAll = SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK |
            SDL_INIT_HAPTIC | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS |
            SDL_INIT_SENSOR | SDL_INIT_CAMERA;
        SDL_QuitSubSystem(kAll);
        SDL_Quit();
    }

    if (sdlInitCounter < 0)
        URHO3D_LOGERROR("Too many calls to Context::ReleaseSDL()!");
}

#endif // ifndef MINI_URHO

void Context::CopyBaseAttributes(StringHash baseType, StringHash derivedType)
{
    // Prevent endless loop if mistakenly copying attributes from same class as derived
    if (baseType == derivedType)
    {
        URHO3D_LOGWARNING("Attempt to copy base attributes to itself for class " + GetTypeName(baseType));
        return;
    }

    const Vector<AttributeInfo>* baseAttributes = GetAttributes(baseType);
    if (baseAttributes)
    {
        for (unsigned i = 0; i < baseAttributes->Size(); ++i)
        {
            const AttributeInfo& attr = baseAttributes->At(i);
            attributes_[derivedType].Push(attr);
            if (attr.mode_ & AM_NET)
                networkAttributes_[derivedType].Push(attr);
        }
    }
}

Object* Context::GetSubsystem(StringHash type) const
{
    auto i = subsystems_.find(type);
    if (i != subsystems_.end())
        return i->second;
    else
        return nullptr;
}

const Variant& Context::GetGlobalVar(StringHash key) const
{
    auto i = globalVars_.find(key);
    return i != globalVars_.end() ? i->second : Variant::EMPTY;
}

void Context::SetGlobalVar(StringHash key, const Variant& value)
{
    globalVars_[key] = value;
}

Object* Context::GetEventSender() const
{
    if (!eventSenders_.empty())
        return eventSenders_.back();
    else
        return nullptr;
}

const String& Context::GetTypeName(StringHash objectType) const
{
    // Search factories to find the hash-to-name mapping
    auto i = factories_.find(objectType);
    return i != factories_.end() ? i->second->GetTypeName() : String::EMPTY;
}

AttributeInfo* Context::GetAttribute(StringHash objectType, const char* name)
{
    auto i = attributes_.find(objectType);
    if (i == attributes_.end())
        return nullptr;

    Vector<AttributeInfo>& infos = i->second;

    for (Vector<AttributeInfo>::Iterator j = infos.Begin(); j != infos.End(); ++j)
    {
        if (!j->name_.Compare(name, true))
            return &(*j);
    }

    return nullptr;
}

void Context::AddEventReceiver(Object* receiver, StringHash eventType)
{
    SharedPtr<EventReceiverGroup>& group = eventReceivers_[eventType];
    if (!group)
        group = new EventReceiverGroup();
    group->Add(receiver);
}

void Context::AddEventReceiver(Object* receiver, Object* sender, StringHash eventType)
{
    SharedPtr<EventReceiverGroup>& group = specificEventReceivers_[sender][eventType];
    if (!group)
        group = new EventReceiverGroup();
    group->Add(receiver);
}

void Context::RemoveEventSender(Object* sender)
{
    auto i = specificEventReceivers_.find(sender);
    if (i != specificEventReceivers_.end())
    {
        for (auto j = i->second.begin(); j != i->second.end(); ++j)
        {
            for (auto k = j->second->receivers_.begin(); k != j->second->receivers_.end(); ++k)
            {
                Object* receiver = *k;
                if (receiver)
                    receiver->RemoveEventSender(sender);
            }
        }
        specificEventReceivers_.erase(i);
    }
}

void Context::RemoveEventReceiver(Object* receiver, StringHash eventType)
{
    EventReceiverGroup* group = GetEventReceivers(eventType);
    if (group)
        group->Remove(receiver);
}

void Context::RemoveEventReceiver(Object* receiver, Object* sender, StringHash eventType)
{
    EventReceiverGroup* group = GetEventReceivers(sender, eventType);
    if (group)
        group->Remove(receiver);
}

void Context::BeginSendEvent(Object* sender, StringHash eventType)
{
#ifdef URHO3D_PROFILING
    if (EventProfiler::IsActive())
    {
        auto* eventProfiler = GetSubsystem<EventProfiler>();
        if (eventProfiler)
            eventProfiler->BeginBlock(eventType);
    }
#endif

    eventSenders_.push_back(sender);
}

void Context::EndSendEvent()
{
    eventSenders_.pop_back();

#ifdef URHO3D_PROFILING
    if (EventProfiler::IsActive())
    {
        auto* eventProfiler = GetSubsystem<EventProfiler>();
        if (eventProfiler)
            eventProfiler->EndBlock();
    }
#endif
}

}
