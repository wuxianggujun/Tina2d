// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "../Precompiled.h"

#include "../IO/Log.h"
#include "../Scene/Component.h"
#include "../Scene/SceneResolver.h"
#include "../Scene/Node.h"

#include "../DebugNew.h"

namespace Urho3D
{

SceneResolver::SceneResolver() = default;

SceneResolver::~SceneResolver() = default;

void SceneResolver::Reset()
{
    nodes_.clear();
    components_.clear();
}

void SceneResolver::AddNode(NodeId oldID, Node* node)
{
    if (node)
        nodes_[oldID] = node;
}

void SceneResolver::AddComponent(ComponentId oldID, Component* component)
{
    if (component)
        components_[oldID] = component;
}

void SceneResolver::Resolve()
{
    // Nodes do not have component or node ID attributes, so only have to go through components
    HashSet<StringHash> noIDAttributes;
    for (auto i = components_.begin(); i != components_.end(); ++i)
    {
        Component* component = i->second;
        if (!component || noIDAttributes.find(component->GetType()) != noIDAttributes.end())
            continue;

        bool hasIDAttributes = false;
        const Vector<AttributeInfo>* attributes = component->GetAttributes();
        if (!attributes)
        {
            noIDAttributes.insert(component->GetType());
            continue;
        }

        for (unsigned j = 0; j < attributes->Size(); ++j)
        {
            const AttributeInfo& info = attributes->At(j);
            if (info.mode_ & AM_NODEID)
            {
                hasIDAttributes = true;
                NodeId oldNodeID = component->GetAttribute(j).GetU32();

                if (oldNodeID)
                {
                    auto k = nodes_.find(oldNodeID);

                    if (k != nodes_.end() && k->second)
                    {
                        NodeId newNodeID = k->second->GetID();
                        component->SetAttribute(j, Variant(newNodeID));
                    }
                    else
                        URHO3D_LOGWARNING("Could not resolve node ID " + String(oldNodeID));
                }
            }
            else if (info.mode_ & AM_COMPONENTID)
            {
                hasIDAttributes = true;
                ComponentId oldComponentID = component->GetAttribute(j).GetU32();

                if (oldComponentID)
                {
                    auto k = components_.find(oldComponentID);

                    if (k != components_.end() && k->second)
                    {
                        ComponentId newComponentID = k->second->GetID();
                        component->SetAttribute(j, Variant(newComponentID));
                    }
                    else
                        URHO3D_LOGWARNING("Could not resolve component ID " + String(oldComponentID));
                }
            }
            else if (info.mode_ & AM_NODEIDVECTOR)
            {
                hasIDAttributes = true;
                Variant attrValue = component->GetAttribute(j);
                const VariantVector& oldNodeIDs = attrValue.GetVariantVector();

                if (oldNodeIDs.Size())
                {
                    // The first index stores the number of IDs redundantly. This is for editing
                    unsigned numIDs = oldNodeIDs[0].GetU32();
                    VariantVector newIDs;
                    newIDs.Push(numIDs);

                    for (unsigned k = 1; k < oldNodeIDs.Size(); ++k)
                    {
                        NodeId oldNodeID = oldNodeIDs[k].GetU32();
                        auto l = nodes_.find(oldNodeID);

                        if (l != nodes_.end() && l->second)
                            newIDs.Push(l->second->GetID());
                        else
                        {
                            // If node was not found, retain number of elements, just store ID 0
                            newIDs.Push(0);
                            URHO3D_LOGWARNING("Could not resolve node ID " + String(oldNodeID));
                        }
                    }

                    component->SetAttribute(j, newIDs);
                }
            }
        }

        // If component type had no ID attributes, cache this fact for optimization
        if (!hasIDAttributes)
            noIDAttributes.insert(component->GetType());
    }

    // Attributes have been resolved, so no need to remember the nodes after this
    Reset();
}

}
