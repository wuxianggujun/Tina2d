// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include "../Container/HashMap.h"
#include "../Container/Str.h"

namespace Urho3D
{

class JSONValue;

/// JSON object type, thin wrapper around HashMap<String, JSONValue>
struct JSONObject : public HashMap<String, JSONValue>
{
    using Base = HashMap<String, JSONValue>;
    using Base::Base;
    
    // Import base class constructors and methods
    using ConstIterator = Base::ConstIterator;
    using Iterator = Base::Iterator;
    
    using Base::Begin;
    using Base::End;
    using Base::Find;
    using Base::Contains;
    using Base::Empty;
    using Base::Size;
    using Base::Clear;
    using Base::Insert;
    using Base::Erase;
    using Base::operator[];
};

}