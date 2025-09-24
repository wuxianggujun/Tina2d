// Copyright (c) 2025 Tina2D project
// License: MIT

#pragma once

#include "../Base/PrimitiveTypes.h"
#include <EASTL/set.h>

namespace Urho3D
{

/// Ordered set template class
template <class T, class Compare = eastl::less<T>>
class Set : public eastl::set<T, Compare>
{
public:
    using Base = eastl::set<T, Compare>;
    using Iterator = typename Base::iterator;
    using ConstIterator = typename Base::const_iterator;
    using ValueType = T;
    using CompareType = Compare;

    /// Construct empty
    Set() = default;

    /// Construct with compare function
    explicit Set(const Compare& comp) : Base(comp) {}

    /// Copy constructor
    Set(const Set<T, Compare>& set) : Base(set) {}

    /// Move constructor
    Set(Set<T, Compare>&& set) noexcept : Base(eastl::move(set)) {}

    /// Initializer list constructor
    Set(std::initializer_list<T> list) : Base(list) {}

    /// Assignment operator
    Set<T, Compare>& operator=(const Set<T, Compare>& rhs)
    {
        Base::operator=(rhs);
        return *this;
    }

    /// Move assignment operator
    Set<T, Compare>& operator=(Set<T, Compare>&& rhs) noexcept
    {
        Base::operator=(eastl::move(rhs));
        return *this;
    }

    /// Insert value. Return iterator and whether the value was inserted
    eastl::pair<Iterator, bool> Insert(const T& value)
    {
        return Base::insert(value);
    }

    /// Insert value (move). Return iterator and whether the value was inserted
    eastl::pair<Iterator, bool> Insert(T&& value)
    {
        return Base::insert(eastl::move(value));
    }

    /// Erase value. Return true if existed
    bool Erase(const T& value)
    {
        return Base::erase(value) != 0;
    }

    /// Erase value by iterator
    Iterator Erase(const Iterator& it)
    {
        return Base::erase(it);
    }

    /// Clear all values
    void Clear() { Base::clear(); }

    /// Return size
    i32 Size() const { return static_cast<i32>(Base::size()); }

    /// Return whether is empty
    bool Empty() const { return Base::empty(); }

    /// Return iterator to beginning
    Iterator Begin() { return Base::begin(); }
    ConstIterator Begin() const { return Base::begin(); }

    /// Return iterator to end
    Iterator End() { return Base::end(); }
    ConstIterator End() const { return Base::end(); }

    /// Find value and return iterator
    Iterator Find(const T& value) { return Base::find(value); }
    ConstIterator Find(const T& value) const { return Base::find(value); }

    /// Test whether contains a specific value
    bool Contains(const T& value) const
    {
        return Base::find(value) != Base::end();
    }

    /// Return lower bound iterator
    Iterator LowerBound(const T& value) { return Base::lower_bound(value); }
    ConstIterator LowerBound(const T& value) const { return Base::lower_bound(value); }

    /// Return upper bound iterator
    Iterator UpperBound(const T& value) { return Base::upper_bound(value); }
    ConstIterator UpperBound(const T& value) const { return Base::upper_bound(value); }
};

}