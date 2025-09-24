// Copyright (c) 2025 Tina2D project
// License: MIT

#pragma once

#include "../Base/PrimitiveTypes.h"
#include <EASTL/array.h>
#include <EASTL/algorithm.h>

namespace Urho3D
{

/// Fixed-size array template class
template <class T, size_t N>
class Array : public eastl::array<T, N>
{
public:
    using Base = eastl::array<T, N>;
    using Iterator = typename Base::iterator;
    using ConstIterator = typename Base::const_iterator;
    using ValueType = T;

    /// Default constructor
    Array() = default;

    /// Copy constructor
    Array(const Array<T, N>& array) : Base(array) {}

    /// Move constructor
    Array(Array<T, N>&& array) noexcept : Base(eastl::move(array)) {}

    /// Initialize from initializer list
    Array(std::initializer_list<T> list)
    {
        auto it = list.begin();
        for (size_t i = 0; i < N && it != list.end(); ++i, ++it)
            Base::operator[](i) = *it;
    }

    /// Assignment operator
    Array<T, N>& operator=(const Array<T, N>& rhs)
    {
        Base::operator=(rhs);
        return *this;
    }

    /// Move assignment operator
    Array<T, N>& operator=(Array<T, N>&& rhs) noexcept
    {
        Base::operator=(eastl::move(rhs));
        return *this;
    }

    /// Return size (always N)
    constexpr i32 Size() const { return static_cast<i32>(N); }

    /// Return whether is empty (always false unless N==0)
    constexpr bool Empty() const { return N == 0; }

    /// Fill with value
    void Fill(const T& value) { Base::fill(value); }

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

    /// Get raw data pointer
    T* Data() { return Base::data(); }
    const T* Data() const { return Base::data(); }
};

}