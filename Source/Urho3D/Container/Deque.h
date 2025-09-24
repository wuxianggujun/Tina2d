// Copyright (c) 2025 Tina2D project
// License: MIT

#pragma once

#include "../Base/PrimitiveTypes.h"
#include <EASTL/deque.h>

namespace Urho3D
{

/// Double-ended queue template class
template <class T>
class Deque : public eastl::deque<T>
{
public:
    using Base = eastl::deque<T>;
    using Iterator = typename Base::iterator;
    using ConstIterator = typename Base::const_iterator;
    using ValueType = T;

    /// Construct empty
    Deque() = default;

    /// Construct with size
    explicit Deque(i32 size) : Base(size) {}

    /// Construct with size and initial value
    Deque(i32 size, const T& value) : Base(size, value) {}

    /// Copy constructor
    Deque(const Deque<T>& deque) : Base(deque) {}

    /// Move constructor
    Deque(Deque<T>&& deque) noexcept : Base(eastl::move(deque)) {}

    /// Initializer list constructor
    Deque(std::initializer_list<T> list) : Base(list) {}

    /// Assignment operator
    Deque<T>& operator=(const Deque<T>& rhs)
    {
        Base::operator=(rhs);
        return *this;
    }

    /// Move assignment operator
    Deque<T>& operator=(Deque<T>&& rhs) noexcept
    {
        Base::operator=(eastl::move(rhs));
        return *this;
    }

    /// Add element to end
    void Push(const T& value) { Base::push_back(value); }
    void Push(T&& value) { Base::push_back(eastl::move(value)); }

    /// Add element to front  
    void PushFront(const T& value) { Base::push_front(value); }
    void PushFront(T&& value) { Base::push_front(eastl::move(value)); }

    /// Remove last element
    void Pop() { if (!Base::empty()) Base::pop_back(); }

    /// Remove first element
    void PopFront() { if (!Base::empty()) Base::pop_front(); }

    /// Insert element at position
    Iterator Insert(const Iterator& pos, const T& value)
    {
        return Base::insert(pos, value);
    }

    /// Insert element at position (move)
    Iterator Insert(const Iterator& pos, T&& value)
    {
        return Base::insert(pos, eastl::move(value));
    }

    /// Erase element at position
    Iterator Erase(const Iterator& pos)
    {
        return Base::erase(pos);
    }

    /// Erase range of elements
    Iterator Erase(const Iterator& start, const Iterator& end)
    {
        return Base::erase(start, end);
    }

    /// Clear all elements
    void Clear() { Base::clear(); }

    /// Resize the deque
    void Resize(i32 newSize) { Base::resize(newSize); }
    void Resize(i32 newSize, const T& value) { Base::resize(newSize, value); }

    /// Return size
    i32 Size() const { return static_cast<i32>(Base::size()); }

    /// Return whether is empty
    bool Empty() const { return Base::empty(); }

    /// Return first element
    T& Front() { return Base::front(); }
    const T& Front() const { return Base::front(); }

    /// Return last element  
    T& Back() { return Base::back(); }
    const T& Back() const { return Base::back(); }

    /// Return iterator to beginning
    Iterator Begin() { return Base::begin(); }
    ConstIterator Begin() const { return Base::begin(); }

    /// Return iterator to end
    Iterator End() { return Base::end(); }
    ConstIterator End() const { return Base::end(); }

    /// Find element and return iterator
    Iterator Find(const T& value)
    {
        return eastl::find(Base::begin(), Base::end(), value);
    }

    /// Find element and return const iterator
    ConstIterator Find(const T& value) const
    {
        return eastl::find(Base::begin(), Base::end(), value);
    }

    /// Test whether contains a specific value
    bool Contains(const T& value) const
    {
        return eastl::find(Base::begin(), Base::end(), value) != Base::end();
    }
};

}