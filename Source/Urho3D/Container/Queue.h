// Copyright (c) 2025 Tina2D project
// License: MIT

#pragma once

#include "../Base/PrimitiveTypes.h"
#include <EASTL/queue.h>
#include <EASTL/deque.h>

namespace Urho3D
{

/// Queue adapter template class (FIFO container)
template <class T, class Container = eastl::deque<T>>
class Queue : public eastl::queue<T, Container>
{
public:
    using Base = eastl::queue<T, Container>;
    using ValueType = T;
    using ContainerType = Container;

    /// Construct empty
    Queue() = default;

    /// Copy constructor
    Queue(const Queue<T, Container>& queue) : Base(queue) {}

    /// Move constructor  
    Queue(Queue<T, Container>&& queue) noexcept : Base(eastl::move(queue)) {}

    /// Assignment operator
    Queue<T, Container>& operator=(const Queue<T, Container>& rhs)
    {
        Base::operator=(rhs);
        return *this;
    }

    /// Move assignment operator
    Queue<T, Container>& operator=(Queue<T, Container>&& rhs) noexcept
    {
        Base::operator=(eastl::move(rhs));
        return *this;
    }

    /// Add element to end
    void Push(const T& value) { Base::push(value); }
    void Push(T&& value) { Base::push(eastl::move(value)); }

    /// Remove first element
    void Pop() { if (!Base::empty()) Base::pop(); }

    /// Return first element
    T& Front() { return Base::front(); }
    const T& Front() const { return Base::front(); }

    /// Return last element
    T& Back() { return Base::back(); }
    const T& Back() const { return Base::back(); }

    /// Return size  
    i32 Size() const { return static_cast<i32>(Base::size()); }

    /// Return whether is empty
    bool Empty() const { return Base::empty(); }

    /// Clear all elements
    void Clear() { Base::c.clear(); }
};

}