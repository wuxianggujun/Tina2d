// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

// 基于 EASTL::hash_set 的轻量包装，提供与旧接口兼容的方法名
#include <EASTL/hash_set.h>
#include <EASTL/functional.h>
#include <EASTL/algorithm.h>
#include "../Container/Hash.h"

namespace Urho3D
{

template <class T>
class HashSet : public eastl::hash_set<T, Urho3D::HashAdapter<T>, eastl::equal_to<T>, eastl::allocator, false>
{
public:
    using Base = eastl::hash_set<T, Urho3D::HashAdapter<T>, eastl::equal_to<T>, eastl::allocator, false>;
    using Base::Base;

    using Iterator = typename Base::iterator;
    using ConstIterator = typename Base::const_iterator;

    unsigned Size() const { return (unsigned)Base::size(); }
    bool Empty() const { return Base::empty(); }
    void Clear() { Base::clear(); }

    Iterator Begin() { return Base::begin(); }
    ConstIterator Begin() const { return Base::begin(); }
    Iterator End() { return Base::end(); }
    ConstIterator End() const { return Base::end(); }

    Iterator Insert(const T& value) { return Base::insert(value).first; }
    size_t Erase(const T& value) { return Base::erase(value); }
    // 提供按迭代器擦除，保持与旧接口一致
    // 注意：对于 set 类容器，iterator 与 const_iterator 可能为同一类型（只读），因此只保留 const 版本避免重复定义。
    Iterator Erase(ConstIterator it) { return Base::erase(it); }

    bool Contains(const T& value) const { return Base::find(value) != Base::end(); }
};

} // namespace Urho3D
