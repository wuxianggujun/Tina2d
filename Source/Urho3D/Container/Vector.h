// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include "../Base/PrimitiveTypes.h"
// 以 EASTL::vector 为底层，提供与旧 Urho3D Vector 接口兼容的轻量包装
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>

namespace Urho3D
{

template <class T>
class Vector : public eastl::vector<T>
{
public:
    using Base = eastl::vector<T>;
    using Base::Base;

    using Iterator = typename Base::iterator;
    using ConstIterator = typename Base::const_iterator;

    i32 Size() const { return static_cast<i32>(Base::size()); }
    bool Empty() const { return Base::empty(); }
    i32 Capacity() const { return static_cast<i32>(Base::capacity()); }

    void Clear() { Base::clear(); }
    void Reserve(i32 n) { Base::reserve(n); }
    void Resize(i32 n) { Base::resize(n); }

    void Push(const T& value) { Base::push_back(value); }
    void Push(T&& value) { Base::push_back(eastl::move(value)); }
    void Push(const Vector& other) { Base::insert(Base::end(), other.begin(), other.end()); }
    void Pop() { Base::pop_back(); }

    T& Front() { return Base::front(); }
    const T& Front() const { return Base::front(); }
    T& Back() { return Base::back(); }
    const T& Back() const { return Base::back(); }

    // 与旧接口一致
    T* Buffer() { return Base::data(); }
    const T* Buffer() const { return Base::data(); }

    Iterator Begin() { return Base::begin(); }
    ConstIterator Begin() const { return Base::begin(); }
    Iterator End() { return Base::end(); }
    ConstIterator End() const { return Base::end(); }

    void Insert(i32 pos, const T& value) { Base::insert(Base::begin() + pos, value); }
    // 迭代器版本插入，方便与 std/EASTL 算法协作（如 upper_bound 的返回值）
    void Insert(Iterator it, const T& value) { Base::insert(it, value); }
    // 范围插入：将 [first, last) 插入到 it 之前
    void Insert(Iterator it, ConstIterator first, ConstIterator last) { Base::insert(it, first, last); }
    void Erase(i32 pos) { Base::erase(Base::begin() + pos); }
    void Erase(i32 pos, i32 length) { Base::erase(Base::begin() + pos, Base::begin() + pos + length); }

    // 迭代器版本，兼容旧代码中以迭代器调用 Erase 的场景
    Iterator Erase(Iterator it) { return Base::erase(it); }
    Iterator Erase(Iterator first, Iterator last) { return Base::erase(first, last); }

    // 按值移除首个匹配项，返回是否移除成功
    bool Remove(const T& value)
    {
        auto it = eastl::find(this->begin(), this->end(), value);
        if (it != this->end())
        {
            Base::erase(it);
            return true;
        }
        return false;
    }

    // 兼容旧接口名
    T& At(i32 i) { return (*this)[i]; }
    const T& At(i32 i) const { return (*this)[i]; }

    bool operator==(const Vector& rhs) const { return static_cast<const Base&>(*this) == static_cast<const Base&>(rhs); }
    bool operator!=(const Vector& rhs) const { return !(*this == rhs); }

    bool Contains(const T& value) const
    {
        return eastl::find(this->begin(), this->end(), value) != this->end();
    }
};

// PODVector 保持为 Vector 的别名
template <class T>
using PODVector = Vector<T>;

} // namespace Urho3D
