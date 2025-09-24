// Copyright (c) 2025 Tina2D project
// License: MIT

#pragma once

#include "../Base/PrimitiveTypes.h"
#include <EASTL/priority_queue.h>
#include <EASTL/vector.h>
#include <EASTL/functional.h>

namespace Urho3D
{

/// Priority queue adapter template class
template <class T, class Container = eastl::vector<T>, class Compare = eastl::less<T>>
class PriorityQueue : public eastl::priority_queue<T, Container, Compare>
{
public:
    using Base = eastl::priority_queue<T, Container, Compare>;
    using ValueType = T;
    using ContainerType = Container;
    using CompareType = Compare;

    /// Construct empty
    PriorityQueue() = default;

    /// Construct with compare function
    explicit PriorityQueue(const Compare& comp) : Base(comp) {}

    /// Copy constructor
    PriorityQueue(const PriorityQueue<T, Container, Compare>& queue) : Base(queue) {}

    /// Move constructor
    PriorityQueue(PriorityQueue<T, Container, Compare>&& queue) noexcept : Base(eastl::move(queue)) {}

    /// Assignment operator
    PriorityQueue<T, Container, Compare>& operator=(const PriorityQueue<T, Container, Compare>& rhs)
    {
        Base::operator=(rhs);
        return *this;
    }

    /// Move assignment operator
    PriorityQueue<T, Container, Compare>& operator=(PriorityQueue<T, Container, Compare>&& rhs) noexcept
    {
        Base::operator=(eastl::move(rhs));
        return *this;
    }

    /// Add element
    void Push(const T& value) { Base::push(value); }
    void Push(T&& value) { Base::push(eastl::move(value)); }

    /// Remove top element  
    void Pop() { if (!Base::empty()) Base::pop(); }

    /// Return top element
    const T& Top() const { return Base::top(); }

    /// Return size
    i32 Size() const { return static_cast<i32>(Base::size()); }

    /// Return whether is empty
    bool Empty() const { return Base::empty(); }

    /// Clear all elements
    void Clear() { Base::c.clear(); }
};

}