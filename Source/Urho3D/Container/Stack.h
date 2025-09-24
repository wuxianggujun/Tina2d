// Copyright (c) 2025 Tina2D project
// License: MIT

#pragma once

#include "../Base/PrimitiveTypes.h"
#include <EASTL/stack.h>
#include <EASTL/deque.h>

namespace Urho3D
{

/// Stack adapter template class (LIFO container)
template <class T, class Container = eastl::deque<T>>
class Stack : public eastl::stack<T, Container>
{
public:
    using Base = eastl::stack<T, Container>;
    using ValueType = T;
    using ContainerType = Container;

    /// Construct empty
    Stack() = default;

    /// Copy constructor  
    Stack(const Stack<T, Container>& stack) : Base(stack) {}

    /// Move constructor
    Stack(Stack<T, Container>&& stack) noexcept : Base(eastl::move(stack)) {}

    /// Assignment operator
    Stack<T, Container>& operator=(const Stack<T, Container>& rhs)
    {
        Base::operator=(rhs);
        return *this;
    }

    /// Move assignment operator
    Stack<T, Container>& operator=(Stack<T, Container>&& rhs) noexcept
    {
        Base::operator=(eastl::move(rhs));
        return *this;
    }

    /// Add element to top
    void Push(const T& value) { Base::push(value); }
    void Push(T&& value) { Base::push(eastl::move(value)); }

    /// Remove top element
    void Pop() { if (!Base::empty()) Base::pop(); }

    /// Return top element
    T& Top() { return Base::top(); }
    const T& Top() const { return Base::top(); }

    /// Return size
    i32 Size() const { return static_cast<i32>(Base::size()); }

    /// Return whether is empty
    bool Empty() const { return Base::empty(); }

    /// Clear all elements
    void Clear() { Base::c.clear(); }
};

}