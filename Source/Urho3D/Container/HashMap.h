// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

// 基于 EASTL::hash_map 的轻量包装，直接暴露 EASTL 迭代器与 pair(first/second)
#include <EASTL/hash_map.h>
#include <EASTL/functional.h>
#include <type_traits>
#include "../Container/Pair.h"
#include "../Container/Vector.h"
#include "../Container/Hash.h"

namespace Urho3D
{

template <class K, class V>
class HashMap : public eastl::hash_map<K, V, Urho3D::HashAdapter<K>, eastl::equal_to<K>, eastl::allocator, false>
{
public:
    using Base = eastl::hash_map<K, V, Urho3D::HashAdapter<K>, eastl::equal_to<K>, eastl::allocator, false>;
    using Base::Base;

    // 直接暴露 EASTL 迭代器与 pair(first/second)
    using Iterator = typename Base::iterator;
    using ConstIterator = typename Base::const_iterator;

    // 基本容量/状态
    unsigned Size() const { return (unsigned)Base::size(); }
    bool Empty() const { return Base::empty(); }
    void Clear() { Base::clear(); }

    // 迭代器
    Iterator Begin() { return Base::begin(); }
    ConstIterator Begin() const { return Base::begin(); }
    Iterator End() { return Base::end(); }
    ConstIterator End() const { return Base::end(); }

    // 查找
    Iterator Find(const K& key) { return Base::find(key); }
    ConstIterator Find(const K& key) const { return Base::find(key); }
    bool Contains(const K& key) const { return Base::find(key) != Base::end(); }

    // 擦除
    size_t Erase(const K& key) { return Base::erase(key); }
    // 为了尽量减少影响，保持同名接口，返回 end() 以便调用点可忽略返回值
    Iterator Erase(Iterator it) { Base::erase(it); return Base::end(); }
    Iterator Erase(ConstIterator it) { Base::erase(it); return Base::end(); }
    Iterator Erase(Iterator first, Iterator last) { Base::erase(first, last); return Base::end(); }

    // 访问/写入
    V& operator[](const K& key) { return Base::operator[](key); }

    // 插入 Pair，返回底层迭代器
    Iterator Insert(const Pair<K, V>& p)
    {
        auto result = Base::insert(eastl::pair<K, V>(p.first_, p.second_));
        return result.first;
    }

    // 返回所有键
    Vector<K> Keys() const
    {
        Vector<K> keys;
        keys.Reserve((unsigned)Base::size());
        for (auto it = Base::begin(); it != Base::end(); ++it)
            keys.Push(it->first);
        return keys;
    }

    // 无序容器排序为 no-op
    void Sort() {}

    bool operator==(const HashMap& rhs) const { return static_cast<const Base&>(*this) == static_cast<const Base&>(rhs); }
    bool operator!=(const HashMap& rhs) const { return !(*this == rhs); }
};

} // namespace Urho3D


