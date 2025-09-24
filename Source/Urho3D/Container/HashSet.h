// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include "../Base/PrimitiveTypes.h"
#include <EASTL/hash_set.h>
#include "../Container/Hash.h"
#include "../Container/Vector.h"

namespace Urho3D
{

/// Hash set template class
template <class T>
class HashSet : public eastl::hash_set<T, Urho3D::HashAdapter<T>, eastl::equal_to<T>, eastl::allocator, false>
{
public:
    using Base = eastl::hash_set<T, Urho3D::HashAdapter<T>, eastl::equal_to<T>, eastl::allocator, false>;
    using Iterator = typename Base::iterator;
    using ConstIterator = typename Base::const_iterator;
    using ValueType = T;

    /// Construct empty
    HashSet() = default;

    /// Copy constructor
    HashSet(const HashSet<T>& set) : Base(set) {}

    /// Move constructor
    HashSet(HashSet<T>&& set) noexcept : Base(eastl::move(set)) {}

    /// Initializer list constructor
    HashSet(std::initializer_list<T> list) : Base(list) {}

    /// Assignment operator
    HashSet<T>& operator=(const HashSet<T>& rhs)
    {
        Base::operator=(rhs);
        return *this;
    }

    /// Move assignment operator
    HashSet<T>& operator=(HashSet<T>&& rhs) noexcept
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

    /// Erase by iterator
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

    /// Return all values as a vector
    Vector<T> Values() const
    {
        Vector<T> values;
        values.Reserve(Size());
        for (const auto& value : *this)
            values.Push(value);
        return values;
    }
};

}