// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include <EASTL/list.h>
#include <EASTL/algorithm.h>

namespace Urho3D
{

// EASTL 版 List 适配器，保留旧接口命名
template <class T> class List
{
public:
    using Underlying = eastl::list<T>;
    using Iterator = typename Underlying::iterator;
    using ConstIterator = typename Underlying::const_iterator;

    List() = default;
    List(const List&) = default;
    List(List&&) noexcept = default;
    List& operator=(const List&) = default;
    List& operator=(List&&) noexcept = default;
    ~List() = default;

    void Push(const T& value) { data_.push_back(value); }
    // 兼容 STL/EASTL 接口名
    void push_back(const T& value) { data_.push_back(value); }
    void push_back(T&& value) { data_.push_back(eastl::move(value)); }
    void PushFront(const T& value) { data_.push_front(value); }
    void push_front(const T& value) { data_.push_front(value); }
    void Pop() { if (!data_.empty()) data_.pop_back(); }
    void pop_back() { if (!data_.empty()) data_.pop_back(); }
    void PopFront() { if (!data_.empty()) data_.pop_front(); }
    void pop_front() { if (!data_.empty()) data_.pop_front(); }

    Iterator Insert(const Iterator& dest, const T& value) { return data_.insert(dest, value); }
    void Insert(const Iterator& dest, const List& list) { data_.insert(dest, list.data_.begin(), list.data_.end()); }
    void Insert(const Iterator& dest, const ConstIterator& start, const ConstIterator& end) { data_.insert(dest, start, end); }

    Iterator Erase(Iterator it) { return data_.erase(it); }
    Iterator Erase(const Iterator& start, const Iterator& end) { return data_.erase(start, end); }
    void Clear() { data_.clear(); }
    void clear() { data_.clear(); }

    Iterator Find(const T& value) { return eastl::find(data_.begin(), data_.end(), value); }
    bool Contains(const T& value) const { return eastl::find(data_.begin(), data_.end(), value) != data_.end(); }

    Iterator Begin() { return data_.begin(); }
    ConstIterator Begin() const { return data_.begin(); }
    Iterator End() { return data_.end(); }
    ConstIterator End() const { return data_.end(); }

    T& Front() { return data_.front(); }
    const T& Front() const { return data_.front(); }
    T& Back() { return data_.back(); }
    const T& Back() const { return data_.back(); }

    int Size() const { return (int)data_.size(); }
    bool Empty() const { return data_.empty(); }

private:
    Underlying data_;
};

template <class T> typename Urho3D::List<T>::ConstIterator begin(const Urho3D::List<T>& v) { return v.Begin(); }
template <class T> typename Urho3D::List<T>::ConstIterator end(const Urho3D::List<T>& v) { return v.End(); }
template <class T> typename Urho3D::List<T>::Iterator begin(Urho3D::List<T>& v) { return v.Begin(); }
template <class T> typename Urho3D::List<T>::Iterator end(Urho3D::List<T>& v) { return v.End(); }

}
