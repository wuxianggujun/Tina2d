// Copyright (c) 2025 Tina2D project
// License: MIT

#pragma once

#include "../Base/PrimitiveTypes.h"
#include <EASTL/map.h>
#include "Vector.h"

namespace Urho3D
{

/// Ordered map template class
template <class Key, class Value, class Compare = eastl::less<Key>>
class Map : public eastl::map<Key, Value, Compare>
{
public:
    using Base = eastl::map<Key, Value, Compare>;
    using Iterator = typename Base::iterator;
    using ConstIterator = typename Base::const_iterator;
    using KeyType = Key;
    using ValueType = Value;
    using CompareType = Compare;
    using PairType = eastl::pair<Key, Value>;

    /// Construct empty
    Map() = default;

    /// Construct with compare function
    explicit Map(const Compare& comp) : Base(comp) {}

    /// Copy constructor
    Map(const Map<Key, Value, Compare>& map) : Base(map) {}

    /// Move constructor
    Map(Map<Key, Value, Compare>&& map) noexcept : Base(eastl::move(map)) {}

    /// Initializer list constructor
    Map(std::initializer_list<PairType> list) : Base(list) {}

    /// Assignment operator
    Map<Key, Value, Compare>& operator=(const Map<Key, Value, Compare>& rhs)
    {
        Base::operator=(rhs);
        return *this;
    }

    /// Move assignment operator
    Map<Key, Value, Compare>& operator=(Map<Key, Value, Compare>&& rhs) noexcept
    {
        Base::operator=(eastl::move(rhs));
        return *this;
    }

    /// Insert key-value pair. Return iterator and whether the value was inserted
    eastl::pair<Iterator, bool> Insert(const PairType& pair)
    {
        return Base::insert(pair);
    }

    /// Insert key-value pair (move). Return iterator and whether the value was inserted
    eastl::pair<Iterator, bool> Insert(PairType&& pair)
    {
        return Base::insert(eastl::move(pair));
    }

    /// Insert key-value pair
    eastl::pair<Iterator, bool> Insert(const Key& key, const Value& value)
    {
        return Base::insert(eastl::make_pair(key, value));
    }

    /// Erase key. Return true if existed
    bool Erase(const Key& key)
    {
        return Base::erase(key) != 0;
    }

    /// Erase by iterator
    Iterator Erase(const Iterator& it)
    {
        return Base::erase(it);
    }

    /// Clear all key-value pairs
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

    /// Find key and return iterator
    Iterator Find(const Key& key) { return Base::find(key); }
    ConstIterator Find(const Key& key) const { return Base::find(key); }

    /// Test whether contains a specific key
    bool Contains(const Key& key) const
    {
        return Base::find(key) != Base::end();
    }

    /// Return lower bound iterator
    Iterator LowerBound(const Key& key) { return Base::lower_bound(key); }
    ConstIterator LowerBound(const Key& key) const { return Base::lower_bound(key); }

    /// Return upper bound iterator
    Iterator UpperBound(const Key& key) { return Base::upper_bound(key); }
    ConstIterator UpperBound(const Key& key) const { return Base::upper_bound(key); }

    /// Return keys as a vector
    Vector<Key> Keys() const
    {
        Vector<Key> keys;
        keys.Reserve(Size());
        for (const auto& pair : *this)
            keys.Push(pair.first);
        return keys;
    }

    /// Return values as a vector
    Vector<Value> Values() const
    {
        Vector<Value> values;
        values.Reserve(Size());
        for (const auto& pair : *this)
            values.Push(pair.second);
        return values;
    }
};

}