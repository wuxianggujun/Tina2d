// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

// 基于 EASTL::hash_map 的轻量包装，尽可能提供与旧 Urho3D HashMap 接口/行为兼容的适配层
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

    // 迭代器代理，暴露 first_/second_ 命名以兼容旧代码
    struct NodeRef
    {
        const K& first_;
        V& second_;
    };
    struct ConstNodeRef
    {
        const K& first_;
        const V& second_;
    };

    class Iterator
    {
    public:
        using Underlying = typename Base::iterator;

        Iterator() = default;
        explicit Iterator(Underlying it) : it_(it) {}

        // 递增/递减
        Iterator& operator++() { ++it_; return *this; }
        Iterator operator++(int) { Iterator tmp(*this); ++it_; return tmp; }
        Iterator& operator--() { --it_; return *this; }
        Iterator operator--(int) { Iterator tmp(*this); --it_; return tmp; }

        // 比较
        bool operator==(const Iterator& rhs) const { return it_ == rhs.it_; }
        bool operator!=(const Iterator& rhs) const { return it_ != rhs.it_; }

        // 解引用
        NodeRef operator*() { return NodeRef{ it_->first, it_->second }; }
        NodeRef* operator->()
        {
            return new (&proxyStorage_) NodeRef{ it_->first, it_->second };
        }

        // 访问底层迭代器（包装内部使用）
        Underlying& Get() { return it_; }
        const Underlying& Get() const { return it_; }

    private:
        Underlying it_{};
        typename std::aligned_storage<sizeof(NodeRef), alignof(NodeRef)>::type proxyStorage_;
    };

    class ConstIterator
    {
    public:
        using Underlying = typename Base::const_iterator;

        ConstIterator() = default;
        explicit ConstIterator(Underlying it) : it_(it) {}
        // 允许从非 const 迭代器隐式转换
        /*implicit*/ ConstIterator(const Iterator& it) : it_(it.Get()) {}

        // 递增/递减
        ConstIterator& operator++() { ++it_; return *this; }
        ConstIterator operator++(int) { ConstIterator tmp(*this); ++it_; return tmp; }
        ConstIterator& operator--() { --it_; return *this; }
        ConstIterator operator--(int) { ConstIterator tmp(*this); --it_; return tmp; }

        // 比较
        bool operator==(const ConstIterator& rhs) const { return it_ == rhs.it_; }
        bool operator!=(const ConstIterator& rhs) const { return it_ != rhs.it_; }

        // 解引用
        ConstNodeRef operator*() const { return ConstNodeRef{ it_->first, it_->second }; }
        ConstNodeRef* operator->() const
        {
            return new (&proxyStorage_) ConstNodeRef{ it_->first, it_->second };
        }

        // 访问底层迭代器（包装内部使用）
        Underlying& Get() { return it_; }
        const Underlying& Get() const { return it_; }

    private:
        Underlying it_{};
        mutable typename std::aligned_storage<sizeof(ConstNodeRef), alignof(ConstNodeRef)>::type proxyStorage_;
    };

    // 基本容量/状态
    unsigned Size() const { return (unsigned)Base::size(); }
    bool Empty() const { return Base::empty(); }
    void Clear() { Base::clear(); }

    // 迭代器
    Iterator Begin() { return Iterator(Base::begin()); }
    ConstIterator Begin() const { return ConstIterator(Base::begin()); }
    Iterator End() { return Iterator(Base::end()); }
    ConstIterator End() const { return ConstIterator(Base::end()); }

    // 查找
    Iterator Find(const K& key) { return Iterator(Base::find(key)); }
    ConstIterator Find(const K& key) const { return ConstIterator(Base::find(key)); }
    bool Contains(const K& key) const { return Base::find(key) != Base::end(); }

    // 擦除
    size_t Erase(const K& key) { return Base::erase(key); }
    Iterator Erase(Iterator it)
    {
        // EASTL::hash_map::erase 返回下一个迭代器
        return Iterator(Base::erase(it.Get()));
    }
    // 兼容：当调用点通过 Base::find 得到底层迭代器时，允许直接擦除
    Iterator Erase(typename Base::iterator it)
    {
        return Iterator(Base::erase(it));
    }
    Iterator Erase(typename Base::const_iterator it)
    {
        return Iterator(Base::erase(it));
    }
    Iterator Erase(Iterator first, Iterator last)
    {
        return Iterator(Base::erase(first.Get(), last.Get()));
    }

    // 访问/写入
    V& operator[](const K& key) { return Base::operator[](key); }

    // 兼容接口：插入 Pair，返回迭代器
    Iterator Insert(const Pair<K, V>& p)
    {
        auto result = Base::insert(eastl::pair<K, V>(p.first_, p.second_));
        return Iterator(result.first);
    }

    // 兼容接口：返回所有键
    Vector<K> Keys() const
    {
        Vector<K> keys;
        keys.Reserve((unsigned)Base::size());
        for (auto it = Base::begin(); it != Base::end(); ++it)
            keys.Push(it->first);
        return keys;
    }

    // 兼容接口：排序（对无序容器无意义，这里为 no-op，占位保持源代码可编译）
    void Sort() {}

    bool operator==(const HashMap& rhs) const { return static_cast<const Base&>(*this) == static_cast<const Base&>(rhs); }
    bool operator!=(const HashMap& rhs) const { return !(*this == rhs); }
};

} // namespace Urho3D


