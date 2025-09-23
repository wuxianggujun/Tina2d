// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/JSONValue.h"
#include "../Resource/JSONObject.h"
#include "../Resource/XMLElement.h"
#include "../Scene/Animatable.h"
#include "../Scene/ObjectAnimation.h"
#include "../Scene/SceneEvents.h"
#include "../Scene/ValueAnimation.h"

#include "../DebugNew.h"

namespace Urho3D
{

extern const char* wrapModeNames[];

AttributeAnimationInfo::AttributeAnimationInfo(Animatable* animatable, const AttributeInfo& attributeInfo,
    ValueAnimation* attributeAnimation, WrapMode wrapMode, float speed) :
    ValueAnimationInfo(animatable, attributeAnimation, wrapMode, speed),
    attributeInfo_(attributeInfo)
{
}

AttributeAnimationInfo::AttributeAnimationInfo(const AttributeAnimationInfo& other) = default;

AttributeAnimationInfo::~AttributeAnimationInfo() = default;

void AttributeAnimationInfo::ApplyValue(const Variant& newValue)
{
    auto* animatable = static_cast<Animatable*>(target_.Get());
    if (animatable)
    {
        animatable->OnSetAttribute(attributeInfo_, newValue);
        animatable->ApplyAttributes();
    }
}

Animatable::Animatable(Context* context) :
    Serializable(context),
    animationEnabled_(true)
{
}

Animatable::~Animatable() = default;

void Animatable::RegisterObject(Context* context)
{
    URHO3D_ACCESSOR_ATTRIBUTE("Object Animation", GetObjectAnimationAttr, SetObjectAnimationAttr,
        ResourceRef(ObjectAnimation::GetTypeStatic()), AM_DEFAULT);
}

bool Animatable::LoadXML(const XMLElement& source)
{
    if (!Serializable::LoadXML(source))
        return false;

    SetObjectAnimation(nullptr);
    attributeAnimationInfos_.Clear();

    XMLElement elem = source.GetChild("objectanimation");
    if (elem)
    {
        SharedPtr<ObjectAnimation> objectAnimation(new ObjectAnimation(context_));
        if (!objectAnimation->LoadXML(elem))
            return false;

        SetObjectAnimation(objectAnimation);
    }

    elem = source.GetChild("attributeanimation");
    while (elem)
    {
        String name = elem.GetAttribute("name");
        SharedPtr<ValueAnimation> attributeAnimation(new ValueAnimation(context_));
        if (!attributeAnimation->LoadXML(elem))
            return false;

        String wrapModeString = source.GetAttribute("wrapmode");
        WrapMode wrapMode = WM_LOOP;
        for (int i = 0; i <= WM_CLAMP; ++i)
        {
            if (wrapModeString == wrapModeNames[i])
            {
                wrapMode = (WrapMode)i;
                break;
            }
        }

        float speed = elem.GetFloat("speed");
        SetAttributeAnimation(name, attributeAnimation, wrapMode, speed);

        elem = elem.GetNext("attributeanimation");
    }

    return true;
}

bool Animatable::LoadJSON(const JSONValue& source)
{
    if (!Serializable::LoadJSON(source))
        return false;

    SetObjectAnimation(nullptr);
    attributeAnimationInfos_.Clear();

    JSONValue value = source.Get("objectanimation");
    if (!value.IsNull())
    {
        SharedPtr<ObjectAnimation> objectAnimation(new ObjectAnimation(context_));
        if (!objectAnimation->LoadJSON(value))
            return false;

        SetObjectAnimation(objectAnimation);
    }

    JSONValue attributeAnimationValue = source.Get("attributeanimation");

    if (attributeAnimationValue.IsNull())
        return true;

    if (!attributeAnimationValue.IsObject())
    {
        URHO3D_LOGWARNING("'attributeanimation' value is present in JSON data, but is not a JSON object; skipping it");
        return true;
    }

    const JSONObject& attributeAnimationObject = attributeAnimationValue.GetObject();
    for (const auto& kv : attributeAnimationObject)
    {
        const String& name = kv.first;
        JSONValue value = kv.second;
        SharedPtr<ValueAnimation> attributeAnimation(new ValueAnimation(context_));
        if (!attributeAnimation->LoadJSON(value))
            return false;

        String wrapModeString = value.Get("wrapmode").GetString();
        WrapMode wrapMode = WM_LOOP;
        for (int i = 0; i <= WM_CLAMP; ++i)
        {
            if (wrapModeString == wrapModeNames[i])
            {
                wrapMode = (WrapMode)i;
                break;
            }
        }

        float speed = value.Get("speed").GetFloat();
        SetAttributeAnimation(name, attributeAnimation, wrapMode, speed);
    }

    return true;
}

bool Animatable::SaveXML(XMLElement& dest) const
{
    if (!Serializable::SaveXML(dest))
        return false;

    // Object animation without name
    if (objectAnimation_ && objectAnimation_->GetName().Empty())
    {
        XMLElement elem = dest.CreateChild("objectanimation");
        if (!objectAnimation_->SaveXML(elem))
            return false;
    }


    for (auto it = attributeAnimationInfos_.begin(); it != attributeAnimationInfos_.end(); ++it)
    {
        ValueAnimation* attributeAnimation = it->second->GetAnimation();
        if (attributeAnimation->GetOwner())
            continue;

        const AttributeInfo& attr = it->second->GetAttributeInfo();
        XMLElement elem = dest.CreateChild("attributeanimation");
        elem.SetAttribute("name", attr.name_);
        if (!attributeAnimation->SaveXML(elem))
            return false;

        elem.SetAttribute("wrapmode", wrapModeNames[it->second->GetWrapMode()]);
        elem.SetFloat("speed", it->second->GetSpeed());
    }

    return true;
}

bool Animatable::SaveJSON(JSONValue& dest) const
{
    if (!Serializable::SaveJSON(dest))
        return false;

    // Object animation without name
    if (objectAnimation_ && objectAnimation_->GetName().Empty())
    {
        JSONValue objectAnimationValue;
        if (!objectAnimation_->SaveJSON(objectAnimationValue))
            return false;
        dest.Set("objectanimation", objectAnimationValue);
    }

    JSONValue attributeAnimationValue;

    for (auto it = attributeAnimationInfos_.begin(); it != attributeAnimationInfos_.end(); ++it)
    {
        ValueAnimation* attributeAnimation = it->second->GetAnimation();
        if (attributeAnimation->GetOwner())
            continue;

        const AttributeInfo& attr = it->second->GetAttributeInfo();
        JSONValue attributeValue;
        attributeValue.Set("name", attr.name_);
        if (!attributeAnimation->SaveJSON(attributeValue))
            return false;

        attributeValue.Set("wrapmode", wrapModeNames[it->second->GetWrapMode()]);
        attributeValue.Set("speed", (float) it->second->GetSpeed());

        attributeAnimationValue.Set(attr.name_, attributeValue);
    }

    if (!attributeAnimationValue.IsNull())
        dest.Set("attributeanimation", attributeAnimationValue);

    return true;
}

void Animatable::SetAnimationEnabled(bool enable)
{
    if (objectAnimation_)
    {
        // In object animation there may be targets in hierarchy. Set same enable/disable state in all
        HashSet<Animatable*> targets;
        const HashMap<String, SharedPtr<ValueAnimationInfo>>& infos = objectAnimation_->GetAttributeAnimationInfos();
        for (auto it = infos.begin(); it != infos.end(); ++it)
        {
            String outName;
            Animatable* target = FindAttributeAnimationTarget(it->first, outName);
            if (target && target != this)
                targets.insert(target);
        }

        for (auto it = targets.begin(); it != targets.end(); ++it)
            (*it)->animationEnabled_ = enable;
    }

    animationEnabled_ = enable;
}

void Animatable::SetAnimationTime(float time)
{
    if (objectAnimation_)
    {
        // In object animation there may be targets in hierarchy. Set same time in all
        const HashMap<String, SharedPtr<ValueAnimationInfo>>& infos = objectAnimation_->GetAttributeAnimationInfos();
        for (auto it = infos.begin(); it != infos.end(); ++it)
        {
            String outName;
            Animatable* target = FindAttributeAnimationTarget(it->first, outName);
            if (target)
                target->SetAttributeAnimationTime(outName, time);
        }
    }
    else
    {
        for (HashMap<String, SharedPtr<AttributeAnimationInfo>>::ConstIterator i = attributeAnimationInfos_.Begin();
            i != attributeAnimationInfos_.End(); ++i)
            i->second->SetTime(time);
    }
}

void Animatable::SetObjectAnimation(ObjectAnimation* objectAnimation)
{
    if (objectAnimation == objectAnimation_)
        return;

    if (objectAnimation_)
    {
        OnObjectAnimationRemoved(objectAnimation_);
        UnsubscribeFromEvent(objectAnimation_, E_ATTRIBUTEANIMATIONADDED);
        UnsubscribeFromEvent(objectAnimation_, E_ATTRIBUTEANIMATIONREMOVED);
    }

    objectAnimation_ = objectAnimation;

    if (objectAnimation_)
    {
        OnObjectAnimationAdded(objectAnimation_);
        SubscribeToEvent(objectAnimation_, E_ATTRIBUTEANIMATIONADDED, URHO3D_HANDLER(Animatable, HandleAttributeAnimationAdded));
        SubscribeToEvent(objectAnimation_, E_ATTRIBUTEANIMATIONREMOVED, URHO3D_HANDLER(Animatable, HandleAttributeAnimationRemoved));
    }
}

void Animatable::SetAttributeAnimation(const String& name, ValueAnimation* attributeAnimation, WrapMode wrapMode, float speed)
{
    AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);

    if (attributeAnimation)
    {
        if (info && attributeAnimation == info->GetAnimation())
        {
            info->SetWrapMode(wrapMode);
            info->SetSpeed(speed);
            return;
        }

        // Get attribute info
        const AttributeInfo* attributeInfo = nullptr;
        if (info)
            attributeInfo = &info->GetAttributeInfo();
        else
        {
            const Vector<AttributeInfo>* attributes = GetAttributes();
            if (!attributes)
            {
                URHO3D_LOGERROR(GetTypeName() + " has no attributes");
                return;
            }

            for (const auto& a : *attributes)
            {
                if (name == a.name_)
                {
                    attributeInfo = &a;
                    break;
                }
            }
        }

        if (!attributeInfo)
        {
            URHO3D_LOGERROR("Invalid name: " + name);
            return;
        }

        // Check value type is same with attribute type
        if (attributeAnimation->GetValueType() != attributeInfo->type_)
        {
            URHO3D_LOGERROR("Invalid value type");
            return;
        }

        // Add network attribute to set
        if (attributeInfo->mode_ & AM_NET)
            animatedNetworkAttributes_.Insert(attributeInfo);

        attributeAnimationInfos_[name] = new AttributeAnimationInfo(this, *attributeInfo, attributeAnimation, wrapMode, speed);

        if (!info)
            OnAttributeAnimationAdded();
    }
    else
    {
        if (!info)
            return;

        // Remove network attribute from set
        if (info->GetAttributeInfo().mode_ & AM_NET)
            animatedNetworkAttributes_.Erase(&info->GetAttributeInfo());

        attributeAnimationInfos_.Erase(name);
        OnAttributeAnimationRemoved();
    }
}

void Animatable::SetAttributeAnimationWrapMode(const String& name, WrapMode wrapMode)
{
    AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    if (info)
        info->SetWrapMode(wrapMode);
}

void Animatable::SetAttributeAnimationSpeed(const String& name, float speed)
{
    AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    if (info)
        info->SetSpeed(speed);
}

void Animatable::SetAttributeAnimationTime(const String& name, float time)
{
    AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    if (info)
        info->SetTime(time);
}

void Animatable::RemoveObjectAnimation()
{
    SetObjectAnimation(nullptr);
}

void Animatable::RemoveAttributeAnimation(const String& name)
{
    SetAttributeAnimation(name, nullptr);
}

ObjectAnimation* Animatable::GetObjectAnimation() const
{
    return objectAnimation_;
}

ValueAnimation* Animatable::GetAttributeAnimation(const String& name) const
{
    const AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info ? info->GetAnimation() : nullptr;
}

WrapMode Animatable::GetAttributeAnimationWrapMode(const String& name) const
{
    const AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info ? info->GetWrapMode() : WM_LOOP;
}

float Animatable::GetAttributeAnimationSpeed(const String& name) const
{
    const AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info ? info->GetSpeed() : 1.0f;
}

float Animatable::GetAttributeAnimationTime(const String& name) const
{
    const AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info ? info->GetTime() : 0.0f;
}

void Animatable::SetObjectAnimationAttr(const ResourceRef& value)
{
    if (!value.name_.Empty())
    {
        auto* cache = GetSubsystem<ResourceCache>();
        SetObjectAnimation(cache->GetResource<ObjectAnimation>(value.name_));
    }
}

ResourceRef Animatable::GetObjectAnimationAttr() const
{
    return GetResourceRef(objectAnimation_, ObjectAnimation::GetTypeStatic());
}

Animatable* Animatable::FindAttributeAnimationTarget(const String& name, String& outName)
{
    // Base implementation only handles self
    outName = name;
    return this;
}

void Animatable::SetObjectAttributeAnimation(const String& name, ValueAnimation* attributeAnimation, WrapMode wrapMode, float speed)
{
    String outName;
    Animatable* target = FindAttributeAnimationTarget(name, outName);
    if (target)
        target->SetAttributeAnimation(outName, attributeAnimation, wrapMode, speed);
}

void Animatable::OnObjectAnimationAdded(ObjectAnimation* objectAnimation)
{
    if (!objectAnimation)
        return;

    // Set all attribute animations from the object animation
    const HashMap<String, SharedPtr<ValueAnimationInfo>>& attributeAnimationInfos = objectAnimation->GetAttributeAnimationInfos();
    for (auto it = attributeAnimationInfos.begin(); it != attributeAnimationInfos.end(); ++it)
    {
        const String& name = it->first;
        ValueAnimationInfo* info = it->second;
        SetObjectAttributeAnimation(name, info->GetAnimation(), info->GetWrapMode(), info->GetSpeed());
    }
}

void Animatable::OnObjectAnimationRemoved(ObjectAnimation* objectAnimation)
{
    if (!objectAnimation)
        return;

    // Just remove all attribute animations listed by the object animation
    const HashMap<String, SharedPtr<ValueAnimationInfo>>& infos = objectAnimation->GetAttributeAnimationInfos();
    for (auto it = infos.begin(); it != infos.end(); ++it)
        SetObjectAttributeAnimation(it->first, nullptr, WM_LOOP, 1.0f);
}

void Animatable::UpdateAttributeAnimations(float timeStep)
{
    if (!animationEnabled_)
        return;

    // Keep weak pointer to self to check for destruction caused by event handling
    WeakPtr<Animatable> self(this);

    Vector<String> finishedNames;
    for (auto it = attributeAnimationInfos_.begin(); it != attributeAnimationInfos_.end(); ++it)
    {
        bool finished = it->second->Update(timeStep);
        // If self deleted as a result of an event sent during animation playback, nothing more to do
        if (self.Expired())
            return;

        if (finished)
            finishedNames.push_back(it->second->GetAttributeInfo().name_);
    }

    for (const String& finishedName : finishedNames)
        SetAttributeAnimation(finishedName, nullptr);
}

bool Animatable::IsAnimatedNetworkAttribute(const AttributeInfo& attrInfo) const
{
    return animatedNetworkAttributes_.find(&attrInfo) != animatedNetworkAttributes_.end();
}

AttributeAnimationInfo* Animatable::GetAttributeAnimationInfo(const String& name) const
{
    auto it = attributeAnimationInfos_.find(name);
    if (it != attributeAnimationInfos_.end())
        return it->second;

    return nullptr;
}

void Animatable::HandleAttributeAnimationAdded(StringHash eventType, VariantMap& eventData)
{
    if (!objectAnimation_)
        return;

    using namespace AttributeAnimationAdded;
    const String& name = eventData[P_ATTRIBUTEANIMATIONNAME].GetString();

    ValueAnimationInfo* info = objectAnimation_->GetAttributeAnimationInfo(name);
    if (!info)
        return;

    SetObjectAttributeAnimation(name, info->GetAnimation(), info->GetWrapMode(), info->GetSpeed());
}

void Animatable::HandleAttributeAnimationRemoved(StringHash eventType, VariantMap& eventData)
{
    if (!objectAnimation_)
        return;

    using namespace AttributeAnimationRemoved;
    const String& name = eventData[P_ATTRIBUTEANIMATIONNAME].GetString();

    SetObjectAttributeAnimation(name, nullptr, WM_LOOP, 1.0f);
}

}
