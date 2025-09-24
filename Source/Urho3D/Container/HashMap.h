// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include "../Base/PrimitiveTypes.h"
#include <EASTL/hash_map.h>
#include "../Container/Hash.h"
#include "../Container/Pair.h"
#include "../Container/Vector.h"

namespace Urho3D
{

/// Hash map template class
template <class K, class V>
class HashMap : public eastl::hash_map<K, V, Urho3D::HashAdapter<K>, eastl::equal_to<K>, eastl::allocator, false>
{
public:
    using Base = eastl::hash_map<K, V, Urho3D::HashAdapter<K>, eastl::equal_to<K>, eastl::allocator, false>;
    using Iterator = typename Base::iterator;
    using ConstIterator = typename Base::const_iterator;
    using KeyType = K;
    using ValueType = V;
    using PairType = eastl::pair<K, V>;

    /// Construct empty
    HashMap() = default;

    /// Copy constructor
    HashMap(const HashMap<K, V>& map) : Base(map) {}

    /// Move constructor
    HashMap(HashMap<K, V>&& map) noexcept : Base(eastl::move(map)) {}

    /// Initializer list constructor
    HashMap(std::initializer_list<PairType> list) : Base(list) {}

    /// Assignment operator
    HashMap<K, V>& operator=(const HashMap<K, V>& rhs)
    {
        Base::operator=(rhs);
        return *this;
    }

    /// Move assignment operator
    HashMap<K, V>& operator=(HashMap<K, V>&& rhs) noexcept
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
    eastl::pair<Iterator, bool> Insert(const K& key, const V& value)
    {
        return Base::insert(eastl::make_pair(key, value));
    }

    /// Insert from Urho3D::Pair
    eastl::pair<Iterator, bool> Insert(const Urho3D::Pair<K, V>& pair)
    {
        return Base::insert(eastl::make_pair(pair.first_, pair.second_));
    }

    /// Erase key. Return true if existed
    bool Erase(const K& key)
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
    Iterator Find(const K& key) { return Base::find(key); }
    ConstIterator Find(const K& key) const { return Base::find(key); }

    /// Test whether contains a specific key
    bool Contains(const K& key) const
    {
        return Base::find(key) != Base::end();
    }

    /// Return keys as a vector
    Vector<K> Keys() const
    {
        Vector<K> keys;
        keys.Reserve(Size());
        for (const auto& pair : *this)
            keys.Push(pair.first);
        return keys;
    }

    /// Return values as a vector
    Vector<V> Values() const
    {
        Vector<V> values;
        values.Reserve(Size());
        for (const auto& pair : *this)
            values.Push(pair.second);
        return values;
    }

    /// Return all the shader parameters as Urho3D HashMap (for compatibility)
    const HashMap<K, V>& GetShaderParameters() const { return *this; }

    /// Return all the textures as Urho3D HashMap (for compatibility) 
    const HashMap<K, V>& GetTextures() const { return *this; }
};

}