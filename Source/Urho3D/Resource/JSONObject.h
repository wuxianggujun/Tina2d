// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include <EASTL/hash_map.h>
#include <EASTL/functional.h>

#include "../Container/Str.h"

namespace Urho3D {

class JSONValue;

// 轻量包装：延迟持有 EASTL hash_map，提供与 EASTL 兼容的 API（begin/end/find/operator[]/size/clear/erase）
struct JSONObject
{
    using Map = eastl::hash_map<String, JSONValue, eastl::hash<String>, eastl::equal_to<String>, eastl::allocator, false>;
    using iterator = typename Map::iterator;
    using const_iterator = typename Map::const_iterator;
    using Iterator = typename Map::iterator;
    using ConstIterator = typename Map::const_iterator;

    JSONObject() : map_(new Map()) {}
    JSONObject(const JSONObject& rhs) : map_(new Map(*rhs.map_)) {}
    JSONObject& operator=(const JSONObject& rhs) { if (this!=&rhs) *map_ = *rhs.map_; return *this; }
    ~JSONObject() { delete map_; }

    // 容器访问
    JSONValue& operator[](const String& key) { return (*map_)[key]; }
    const JSONValue& at(const String& key) const { return map_->find(key)->second; }

    iterator begin() { return map_->begin(); }
    const_iterator begin() const { return map_->begin(); }
    iterator end() { return map_->end(); }
    const_iterator end() const { return map_->end(); }
    // 兼容旧接口命名
    Iterator Begin() { return map_->begin(); }
    ConstIterator Begin() const { return map_->begin(); }
    Iterator End() { return map_->end(); }
    ConstIterator End() const { return map_->end(); }

    iterator find(const String& key) { return map_->find(key); }
    const_iterator find(const String& key) const { return map_->find(key); }

    bool empty() const { return map_->empty(); }
    size_t size() const { return map_->size(); }
    void clear() { map_->clear(); }
    size_t erase(const String& key) { return map_->erase(key); }

    Map* map_{};
};

} // namespace Urho3D
